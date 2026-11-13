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
typedef struct { void *data; int64_t len; } fg167h_gg_slice;
typedef struct { unsigned char *base; int64_t len; } fg167h_gg_header;
typedef struct { unsigned char *base; size_t top, committed; } fg167h_gg_stack;
typedef struct { fg167h_gg_header *header; fg167h_gg_stack *stack; } fg167h_gg_reference;
#define GG_RESERVATION ((size_t)268435456)
#define GG_STACK_COUNT 8192
static fg167h_gg_stack fg167h_gg_stacks[GG_STACK_COUNT];
static int fg167h_gg_argc;
static char **fg167h_gg_argv;
static void fg167h_gg_fail(const char *message) {
    fprintf(stderr, "goose runtime error: %s\n", message); exit(1);
}
static fg167h_gg_stack *fg167h_gg_get_stack(int64_t id) {
    fg167h_gg_stack *s;
    if (id < 0 || id >= GG_STACK_COUNT) fg167h_gg_fail("data stack limit exceeded");
    s = &fg167h_gg_stacks[id];
    if (!s->base) {
#ifdef _WIN32
        s->base = VirtualAlloc(NULL, GG_RESERVATION + 65536, MEM_RESERVE, PAGE_NOACCESS);
#else
        s->base = mmap(NULL, GG_RESERVATION + 65536, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (s->base == MAP_FAILED) s->base = NULL;
#endif
        if (!s->base) fg167h_gg_fail("cannot reserve data stack");
    }
    return s;
}
static void *fg167h_gg_allocate(fg167h_gg_stack *s, size_t size) {
    size_t end, committed;
    void *result;
    if (size > GG_RESERVATION - s->top) fg167h_gg_fail("data stack reservation exceeded");
    result = s->base + s->top; end = s->top + size;
    if (end > s->committed) {
        committed = (end + 65535) & ~(size_t)65535;
#ifdef _WIN32
        if (!VirtualAlloc(s->base + s->committed, committed - s->committed, MEM_COMMIT, PAGE_READWRITE)) fg167h_gg_fail("cannot commit data stack");
#else
        if (mprotect(s->base + s->committed, committed - s->committed, PROT_READ | PROT_WRITE)) fg167h_gg_fail("cannot commit data stack");
#endif
        s->committed = committed;
    }
    s->top = end; return result;
}
static fg167h_gg_reference fg167h_gg_ref(fg167h_gg_header *h, fg167h_gg_stack *s) { fg167h_gg_reference r; r.header=h; r.stack=s; return r; }
static fg167h_gg_slice fg167h_gg_view(void *p, int64_t n) { fg167h_gg_slice v; v.data=p; v.len=n; return v; }
static fg167h_gg_slice fg167h_gg_byte_view(fg167h_gg_slice a, size_t width) {
    if (a.len < 0 || (width && (uint64_t)a.len > (uint64_t)INT64_MAX / width)) fg167h_gg_fail("byte view size overflow");
    return fg167h_gg_view(a.data, (int64_t)((uint64_t)a.len * width));
}
static fg167h_gg_slice fg167h_gg_sub(fg167h_gg_slice a, int64_t lo, int64_t hi, size_t element) {
    if (lo < 0 || hi < lo || hi > a.len) fg167h_gg_fail("slice out of bounds");
    return fg167h_gg_view((unsigned char *)a.data + (size_t)lo * element, hi - lo);
}
static int64_t fg167h_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg167h_gg_fail("array index out of bounds"); return i; }
static void fg167h_gg_append(fg167h_gg_reference a, const void *p, int64_t n, size_t element) {
    void *to;
    if (n < 0 || (uint64_t)n > GG_RESERVATION / (element ? element : 1)) fg167h_gg_fail("invalid array size");
    to=fg167h_gg_allocate(a.stack, (size_t)n * element);
    if (n && element) memmove(to, p, (size_t)n * element);
    a.header->len += n;
}
static void fg167h_gg_resize(fg167h_gg_reference a, int64_t n, size_t element, const void *fill) {
    int64_t old=a.header->len, i;
    if (n < 0) fg167h_gg_fail("negative array length");
    if (n > old) {
        unsigned char *p;
        if (!fill) fg167h_gg_fail("resize without fill may only shrink");
        if ((uint64_t)(n-old) > GG_RESERVATION / (element ? element : 1)) fg167h_gg_fail("invalid array size");
        p=fg167h_gg_allocate(a.stack, (size_t)(n-old)*element);
        for (i=0; i<n-old; i++) if(element) memcpy(p+(size_t)i*element, fill, element);
    } else { a.stack->top = (size_t)(a.header->base-a.stack->base) + (size_t)n*element; }
    a.header->len=n;
}
static int64_t fg167h_gg_div(int64_t a, int64_t b) { if (!b) fg167h_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg167h_gg_fail("division overflow"); return a/b; }
static int64_t fg167h_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg167h_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static uint64_t fg167h_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg167h_gg_fail("division by zero");return a/b; }
static uint64_t fg167h_gg_umod(uint64_t a,uint64_t b) { if(!b)fg167h_gg_fail("division by zero");return a%b; }
static int fg167h_gg_equal(fg167h_gg_slice a, fg167h_gg_slice b, size_t element) { return a.len==b.len && (!a.len || !memcmp(a.data,b.data,(size_t)a.len*element)); }
static void fg167h_gg_text(fg167h_gg_reference out, fg167h_gg_slice v) { fg167h_gg_append(out,v.data,v.len,1); }
static void fg167h_gg_integer(fg167h_gg_reference out,int64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%lld",(long long)n);fg167h_gg_append(out,b,k,1); }
static void fg167h_gg_unsigned(fg167h_gg_reference out,uint64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%llu",(unsigned long long)n);fg167h_gg_append(out,b,k,1); }
static double fg167h_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static int64_t fg167h_ggf_fmt_f64(uint8_t *out,double n);
static int64_t fg167h_ggf_fmt_f32(uint8_t *out,float n);
static void fg167h_gg_float(fg167h_gg_reference out,double n) { uint8_t b[64];int64_t k=fg167h_ggf_fmt_f64(b,n);fg167h_gg_append(out,b,k,1); }
static void fg167h_gg_float32(fg167h_gg_reference out,float n) { uint8_t b[64];int64_t k=fg167h_ggf_fmt_f32(b,n);fg167h_gg_append(out,b,k,1); }
static void fg167h_gg_bool(fg167h_gg_reference out,uint8_t n) { fg167h_gg_append(out,n?"true":"false",n?4:5,1); }
static void fg167h_gg_write(FILE *file,fg167h_gg_slice text) { if(text.len && fwrite(text.data,1,(size_t)text.len,file)!=(size_t)text.len)fg167h_gg_fail("write failed"); }
static char *fg167h_gg_cstring(fg167h_gg_slice path) {
    char *s=malloc((size_t)path.len+1); if(!s)fg167h_gg_fail("out of memory in file shim");
    if(path.len)memcpy(s,path.data,(size_t)path.len);s[path.len]=0;return s;
}
static uint8_t fg167h_gs_os_read_file(fg167h_gg_slice path,fg167h_gg_reference out) {
    char *p=fg167h_gg_cstring(path);FILE *f=fopen(p,"rb");unsigned char b[16384];size_t n;int ok;free(p);
    if(!f)return 0;while((n=fread(b,1,sizeof b,f))!=0)fg167h_gg_append(out,b,(int64_t)n,1);ok=!ferror(f);fclose(f);return (uint8_t)ok;
}
static uint8_t fg167h_gs_os_write_file(fg167h_gg_slice path,fg167h_gg_slice data) {
    char *p=fg167h_gg_cstring(path);FILE *f=fopen(p,"wb");int ok;free(p);if(!f)return 0;
    ok=!data.len || fwrite(data.data,1,(size_t)data.len,f)==(size_t)data.len;if(fclose(f))ok=0;return (uint8_t)ok;
}
static uint8_t fg167h_gs_os_file_exists(fg167h_gg_slice path) { char*p=fg167h_gg_cstring(path);FILE*f=fopen(p,"rb");free(p);if(!f)return 0;fclose(f);return 1; }
static void fg167h_gs_os_write_stdout(fg167h_gg_slice s) { fg167h_gg_write(stdout,s); }
static void fg167h_gs_os_write_stderr(fg167h_gg_slice s) { fg167h_gg_write(stderr,s); }
static void fg167h_gs_os_flush_stdout(void) { fflush(stdout); }
static int64_t fg167h_gs_os_arg_count(void) { return fg167h_gg_argc; }
static void fg167h_gs_os_arg(int64_t i,fg167h_gg_reference out) { if(i>=0 && i<fg167h_gg_argc)fg167h_gg_append(out,fg167h_gg_argv[i],(int64_t)strlen(fg167h_gg_argv[i]),1); }
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
typedef struct { int n; uint32_t d[36]; } fg167h_ggf_big;

/* a = v << sh, for v > 0. */
static void fg167h_ggf_big_set(fg167h_ggf_big *a, uint64_t v, int sh) {
    int w = sh >> 5, b = sh & 31, i;
    uint64_t lo = v << b, hi = b ? v >> (64 - b) : 0;
    for (i = 0; i < w; i++) a->d[i] = 0;
    a->d[w] = (uint32_t)lo;
    a->d[w + 1] = (uint32_t)(lo >> 32);
    a->d[w + 2] = (uint32_t)hi;
    a->n = w + 3;
    while (!a->d[a->n - 1]) a->n--;
}

static void fg167h_ggf_big_mul(fg167h_ggf_big *a, uint32_t m) {
    uint64_t c = 0;
    for (int i = 0; i < a->n; i++) {
        c += (uint64_t)a->d[i] * m;
        a->d[i] = (uint32_t)c;
        c >>= 32;
    }
    if (c) a->d[a->n++] = (uint32_t)c;
}

static void fg167h_ggf_big_mul_pow10(fg167h_ggf_big *a, int k) {
    for (; k >= 9; k -= 9) fg167h_ggf_big_mul(a, 1000000000u);
    for (; k > 0; k--) fg167h_ggf_big_mul(a, 10);
}

static int fg167h_ggf_big_cmp(const fg167h_ggf_big *a, const fg167h_ggf_big *b) {
    if (a->n != b->n) return a->n < b->n ? -1 : 1;
    for (int i = a->n - 1; i >= 0; i--)
        if (a->d[i] != b->d[i]) return a->d[i] < b->d[i] ? -1 : 1;
    return 0;
}

/* t = a + b */
static void fg167h_ggf_big_add(fg167h_ggf_big *t, const fg167h_ggf_big *a, const fg167h_ggf_big *b) {
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
static void fg167h_ggf_big_sub(fg167h_ggf_big *a, const fg167h_ggf_big *b) {
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
static int fg167h_ggf_float_digits(uint64_t f, int e, int bits, int emin, char *dig, int *k) {
    fg167h_ggf_big r, s, mp, mlo, t;
    /* Reading rounds a tie to the even significand, so the interval that
       reads back as f * 2^e includes its ends when f is even. */
    int even = !(f & 1);
    /* Below a power of two, the gap to the next float down is half the gap up. */
    int asym = f == (uint64_t)1 << (bits - 1) && e > emin;
    int up = e > 0 ? e : 0, lg = e - 1, n = 0, c;
    /* The value is r / s, and what reads back as it reaches from (r - *mm) / s
       to (r + mp) / s: half the gap to each neighbour. */
    fg167h_ggf_big *mm = asym ? &mlo : &mp;
    fg167h_ggf_big_set(&r, f, up + 1 + asym);
    fg167h_ggf_big_set(&s, 1, up - e + 1 + asym);
    fg167h_ggf_big_set(&mp, 1, up + asym);
    fg167h_ggf_big_set(&mlo, 1, up);
    /* Scale to 10^*k, the least power of ten above the interval: with
       lg = floor(log2 value), ceil(lg log10 2) is it or one short, and
       78913 / 2^18 is log10 2 closely enough to compute that for |lg| < 1650. */
    for (uint64_t g = f; g; g >>= 1) lg++;
    *k = lg > 0 ? ((lg * 78913) >> 18) + 1 : -((-lg * 78913) >> 18);
    if (*k >= 0) fg167h_ggf_big_mul_pow10(&s, *k);
    else {
        fg167h_ggf_big_mul_pow10(&r, -*k);
        fg167h_ggf_big_mul_pow10(&mp, -*k);
        if (asym) fg167h_ggf_big_mul_pow10(&mlo, -*k);
    }
    fg167h_ggf_big_add(&t, &r, &mp);
    c = fg167h_ggf_big_cmp(&t, &s);
    if (even ? c >= 0 : c > 0) {
        fg167h_ggf_big_mul(&s, 10);
        ++*k;
    }
    /* Each digit d leaves the remainder r: stop where the digits so far
       followed by d (r within *mm) or by d + 1 (r + mp past s) read back. */
    for (;;) {
        int d = 0, low, high;
        fg167h_ggf_big_mul(&r, 10);
        fg167h_ggf_big_mul(&mp, 10);
        if (asym) fg167h_ggf_big_mul(&mlo, 10);
        while (fg167h_ggf_big_cmp(&r, &s) >= 0) {
            fg167h_ggf_big_sub(&r, &s);
            d++;
        }
        c = fg167h_ggf_big_cmp(&r, mm);
        low = even ? c <= 0 : c < 0;
        fg167h_ggf_big_add(&t, &r, &mp);
        c = fg167h_ggf_big_cmp(&t, &s);
        high = even ? c >= 0 : c > 0;
        if (!low && !high) {
            dig[n++] = (char)('0' + d);
            continue;
        }
        if (low && high) {
            /* Both read back: the nearer, d + 1 when r is past half of s,
               and of two as near, the even one. */
            fg167h_ggf_big_add(&t, &r, &r);
            c = fg167h_ggf_big_cmp(&t, &s);
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
static int64_t fg167h_ggf_fmt_float(uint8_t *dst, int neg, uint64_t frac, int bexp, int bits,
                            int emin) {
    char dig[20], *p = (char *)dst;
    int k, n, x, i;
    if (neg) *p++ = '-';
    if (!frac && !bexp) {
        memcpy(p, "0.0", 3);
        return p + 3 - (char *)dst;
    }
    n = fg167h_ggf_float_digits(bexp ? frac | (uint64_t)1 << (bits - 1) : frac,
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

static int64_t fg167h_ggf_fmt_f64(uint8_t *dst, double v) {
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
    return fg167h_ggf_fmt_float(dst, (int)(b >> 63), b & (((uint64_t)1 << 52) - 1),
                        (int)(b >> 52) & 0x7FF, 53, -1074);
}

/* The digits are the f32's own, laid out as an f64's are, so 0.1 as an f32
   prints as 0.1, not as the digits of the f64 it widens to. */
static int64_t fg167h_ggf_fmt_f32(uint8_t *dst, float v) {
    double d = v;
    uint32_t b;
    if (d != d || isinf(d)) return fg167h_ggf_fmt_f64(dst, d);
    memcpy(&b, &v, sizeof b);
    return fg167h_ggf_fmt_float(dst, (int)(b >> 31), b & 0x7FFFFF, (int)(b >> 23) & 0xFF, 24, -149);
}


static void fg167h_gg_quote(fg167h_gg_reference out,fg167h_gg_slice text) { int64_t i; unsigned char c; fg167h_gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){fg167h_gg_append(out,"\\",1,1);fg167h_gg_append(out,&c,1,1);} else if(c=='\n')fg167h_gg_append(out,"\\n",2,1);else if(c=='\r')fg167h_gg_append(out,"\\r",2,1);else if(c=='\t')fg167h_gg_append(out,"\\t",2,1);else fg167h_gg_append(out,&c,1,1);}fg167h_gg_append(out,"\"",1,1);}
static uint8_t fg167h_gg_eq_1(int64_t a, int64_t b);
static uint8_t fg167h_gg_equal_elements_1(fg167h_gg_slice a, fg167h_gg_slice b);
static void fg167h_gg_render_1(fg167h_gg_reference out, int64_t value);
static uint8_t fg167h_gg_eq_2(uint8_t a, uint8_t b);
static uint8_t fg167h_gg_equal_elements_2(fg167h_gg_slice a, fg167h_gg_slice b);
static void fg167h_gg_render_2(fg167h_gg_reference out, uint8_t value);
static uint8_t fg167h_gg_eq_3(uint64_t a, uint64_t b);
static uint8_t fg167h_gg_equal_elements_3(fg167h_gg_slice a, fg167h_gg_slice b);
static void fg167h_gg_render_3(fg167h_gg_reference out, uint64_t value);
static uint8_t fg167h_gg_eq_5(uint8_t a, uint8_t b);
static uint8_t fg167h_gg_equal_elements_5(fg167h_gg_slice a, fg167h_gg_slice b);
static void fg167h_gg_render_5(fg167h_gg_reference out, uint8_t value);
static uint8_t fg167h_gg_equal_elements_1(fg167h_gg_slice a,fg167h_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg167h_gg_eq_1(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg167h_gg_eq_1(int64_t a, int64_t b) {
return a == b;
}
static void fg167h_gg_render_1(fg167h_gg_reference out, int64_t value) {
fg167h_gg_integer(out,value);
}
static uint8_t fg167h_gg_equal_elements_2(fg167h_gg_slice a,fg167h_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg167h_gg_eq_2(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg167h_gg_eq_2(uint8_t a, uint8_t b) {
return a == b;
}
static void fg167h_gg_render_2(fg167h_gg_reference out, uint8_t value) {
fg167h_gg_bool(out,value);
}
static uint8_t fg167h_gg_equal_elements_3(fg167h_gg_slice a,fg167h_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg167h_gg_eq_3(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg167h_gg_eq_3(uint64_t a, uint64_t b) {
return a == b;
}
static void fg167h_gg_render_3(fg167h_gg_reference out, uint64_t value) {
fg167h_gg_unsigned(out,value);
}
static uint8_t fg167h_gg_equal_elements_5(fg167h_gg_slice a,fg167h_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg167h_gg_eq_5(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg167h_gg_eq_5(uint8_t a, uint8_t b) {
return a == b;
}
static void fg167h_gg_render_5(fg167h_gg_reference out, uint8_t value) {
fg167h_gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} fg167h_gg_globals;
static void fg167h_gg_function_0(int64_t base);
static void fg167h_gg_function_1(int64_t p0, int64_t base);
static void fg167h_gg_function_4(uint8_t p0, int64_t p1, int64_t base);
static uint64_t fg167h_gg_function_7(int64_t p0, int64_t base);
static void fg167h_gg_function_10(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static uint64_t fg167h_gg_function_13(uint64_t p0, uint64_t p1, int64_t base);
static uint64_t fg167h_gg_function_14(int64_t base);
static uint64_t fg167h_gg_function_15(uint64_t p0, uint64_t p1, int64_t base);
static void fg167h_gg_function_17(int64_t p0, int64_t base);
static uint64_t fg167h_gg_function_18(uint64_t p0, int64_t base);
static uint64_t fg167h_gg_function_19(uint64_t p0, uint64_t p1, int64_t base);
static void fg167h_gg_function_20(int64_t p0, int64_t p1, uint64_t p2, int64_t base);
static void fg167h_gg_function_4(uint8_t p0, int64_t p1, int64_t base) {
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
  fg167_fail(t2);
  }
  }
  }
fg167h_gg_done:;
}
static uint64_t fg167h_gg_function_14(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((uint64_t)UINT64_C(18446744073709551615))))));
  goto fg167h_gg_done;
  }
fg167h_gg_done:;
  return t0;
}
static uint64_t fg167h_gg_function_13(uint64_t p0, uint64_t p1, int64_t base) {
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
  t6 = fg167h_gg_function_14(base + 0);
  t7 = ((uint64_t)(t6));
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(((uint64_t)fg167h_gg_udiv(t7, t8))));
  t4 = ((t5) <= (t9));
  }
  t10 = t4;
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
  fg167h_gg_function_4(t10, t11, base + 0);
  t12 = ((uint64_t)(v0));
  t13 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  goto fg167h_gg_done;
  }
fg167h_gg_done:;
  return t0;
}
static void fg167h_gg_function_20(int64_t p0, int64_t p1, uint64_t p2, int64_t base) {
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
  t7 = ((int64_t)(((int64_t)UINT64_C(5))));
  fg167h_gg_function_4(t6, t7, base + 0);
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
  t28 = ((uint64_t)(((uint64_t)fg167h_gg_udiv(t26, t27))));
  t29 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v4 = ((uint64_t)(((uint64_t)((uint64_t)(t28) + (uint64_t)(t29)))));
  t30 = ((uint64_t)(v4));
  t31 = ((uint64_t)(((int64_t)UINT64_C(4294967295))));
  t32 = ((t30) <= (t31));
  t33 = ((int64_t)(((int64_t)UINT64_C(6))));
  fg167h_gg_function_4(t32, t33, base + 0);
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(((int64_t)UINT64_C(7))));
  t36 = ((int64_t)(v1));
  t37 = ((int64_t)(((int64_t)UINT64_C(3))));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t37)))));
  t39 = ((uint64_t)(v4));
  fg167_hput(t34, t35, t38, t39);
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(7))));
  t42 = ((int64_t)(v1));
  t43 = ((int64_t)(((int64_t)UINT64_C(3))));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) * (uint64_t)(t43)))));
  t45 = ((int64_t)(((int64_t)UINT64_C(1))));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) + (uint64_t)(t45)))));
  t47 = ((uint64_t)(v3));
  fg167_hput(t40, t41, t46, t47);
  t48 = ((int64_t)(v0));
  t49 = ((int64_t)(((int64_t)UINT64_C(7))));
  t50 = ((int64_t)(v1));
  t51 = ((int64_t)(((int64_t)UINT64_C(3))));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) * (uint64_t)(t51)))));
  t53 = ((int64_t)(((int64_t)UINT64_C(2))));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t53)))));
  t55 = ((uint64_t)(v2));
  fg167_hput(t48, t49, t54, t55);
  }
fg167h_gg_done:;
}
static uint64_t fg167h_gg_function_15(uint64_t p0, uint64_t p1, int64_t base) {
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
  t4 = ((int64_t)(((int64_t)UINT64_C(3))));
  fg167h_gg_function_4(t3, t4, base + 0);
  t5 = ((uint64_t)(v0));
  t6 = ((uint64_t)(v1));
  t7 = ((uint64_t)(((uint64_t)fg167h_gg_udiv(t5, t6))));
  t9 = ((uint64_t)(v0));
  t10 = ((uint64_t)(v1));
  t11 = ((uint64_t)(((uint64_t)fg167h_gg_umod(t9, t10))));
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
  goto fg167h_gg_done;
  }
fg167h_gg_done:;
  return t0;
}
static uint64_t fg167h_gg_function_19(uint64_t p0, uint64_t p1, int64_t base) {
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
  goto fg167h_gg_done;
  }
fg167h_gg_done:;
  return t0;
}
static uint64_t fg167h_gg_function_18(uint64_t p0, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint8_t t3;
  uint64_t v0;
  v0 = p0;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2))))));
  }
  }
  goto fg167h_gg_done;
  }
fg167h_gg_done:;
  return t0;
}
static void fg167h_gg_function_17(int64_t p0, int64_t base) {
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
  uint64_t t14;
  uint64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint64_t t21;
  uint64_t t22;
  uint64_t t23;
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
  uint64_t t36;
  uint64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint64_t t43;
  uint64_t t44;
  uint64_t t45;
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
  uint64_t t56;
  uint64_t t57;
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
  uint64_t t68;
  uint64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  uint64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  {
  v1 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(((int64_t)UINT64_C(3))));
  if (!(((t0) < (t1)))) goto label2;
  {
  v2 = ((int64_t)(((int64_t)UINT64_C(1))));
label4:;
  t2 = ((int64_t)(v2));
  t3 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t2) < (t3)))) goto label5;
  {
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(4))));
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t5) + (uint64_t)(t6)))));
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(4))));
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))));
  t13 = ((int64_t)(v2));
  t14 = fg167_hget(t9, t12, t13);
  t15 = ((uint64_t)(t14));
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(1))));
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t18)))));
  t20 = ((int64_t)(v2));
  t21 = fg167_hget(t16, t19, t20);
  t22 = ((uint64_t)(t21));
  t23 = fg167h_gg_function_13(t15, t22, base + 0);
  fg167_hput(t4, t7, t8, t23);
  t24 = v2;
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t24) + (uint64_t)(t25)));
  }
  goto label4;
label5:;
  t26 = ((int64_t)(v0));
  t27 = ((int64_t)(((int64_t)UINT64_C(1))));
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t28)))));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(1))));
  t33 = ((int64_t)(v1));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) + (uint64_t)(t33)))));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = fg167_hget(t31, t34, t35);
  t37 = ((uint64_t)(t36));
  t38 = ((int64_t)(v0));
  t39 = ((int64_t)(((int64_t)UINT64_C(1))));
  t40 = ((int64_t)(v1));
  t41 = ((int64_t)(((int64_t)((uint64_t)(t39) + (uint64_t)(t40)))));
  t42 = ((int64_t)(((int64_t)UINT64_C(1))));
  t43 = fg167_hget(t38, t41, t42);
  t44 = ((uint64_t)(t43));
  t45 = fg167h_gg_function_13(t37, t44, base + 0);
  fg167_hput(t26, t29, t30, t45);
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(((int64_t)UINT64_C(1))));
  t48 = ((int64_t)(v1));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) + (uint64_t)(t48)))));
  t50 = ((int64_t)(((int64_t)UINT64_C(1))));
  t51 = ((int64_t)(v0));
  t52 = ((int64_t)(((int64_t)UINT64_C(1))));
  t53 = ((int64_t)(v1));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t53)))));
  t55 = ((int64_t)(((int64_t)UINT64_C(2))));
  t56 = fg167_hget(t51, t54, t55);
  t57 = ((uint64_t)(t56));
  fg167_hput(t46, t49, t50, t57);
  t58 = ((int64_t)(v0));
  t59 = ((int64_t)(((int64_t)UINT64_C(1))));
  t60 = ((int64_t)(v1));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t59) + (uint64_t)(t60)))));
  t62 = ((int64_t)(((int64_t)UINT64_C(2))));
  t63 = ((int64_t)(v0));
  t64 = ((int64_t)(((int64_t)UINT64_C(1))));
  t65 = ((int64_t)(v1));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t64) + (uint64_t)(t65)))));
  t67 = ((int64_t)(((int64_t)UINT64_C(3))));
  t68 = fg167_hget(t63, t66, t67);
  t69 = ((uint64_t)(t68));
  fg167_hput(t58, t61, t62, t69);
  t70 = ((int64_t)(v0));
  t71 = ((int64_t)(((int64_t)UINT64_C(1))));
  t72 = ((int64_t)(v1));
  t73 = ((int64_t)(((int64_t)((uint64_t)(t71) + (uint64_t)(t72)))));
  t74 = ((int64_t)(((int64_t)UINT64_C(3))));
  t75 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  fg167_hput(t70, t73, t74, t75);
  t76 = v1;
  t77 = ((int64_t)(((int64_t)UINT64_C(1))));
  v1 = ((int64_t)((uint64_t)(t76) + (uint64_t)(t77)));
  }
  goto label1;
label2:;
  }
fg167h_gg_done:;
}
static uint64_t fg167h_gg_function_7(int64_t p0, int64_t base) {
  uint64_t t0;
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
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  uint8_t t16;
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
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  uint8_t t32;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(2))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg167_ti(t1, t2, t3);
  v1 = ((int64_t)(t4));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = fg167_constant(t6);
  t8 = ((int64_t)(t7));
  t9 = ((t5) == (t8));
  t10 = t9;
  if (!t10) {
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)UINT64_C(1))));
  t13 = fg167_constant(t12);
  t14 = ((int64_t)(t13));
  t10 = ((t11) == (t14));
  }
  t15 = t10;
  t16 = t15;
  if (!t16) {
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(((int64_t)UINT64_C(2))));
  t19 = fg167_constant(t18);
  t20 = ((int64_t)(t19));
  t16 = ((t17) == (t20));
  }
  t21 = t16;
  t22 = ((int64_t)(((int64_t)UINT64_C(4))));
  fg167h_gg_function_4(t21, t22, base + 0);
  t23 = ((int64_t)(v1));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = fg167_constant(t24);
  t26 = ((int64_t)(t25));
  t27 = ((t23) == (t26));
  if (t27) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  } else {
  {
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  t30 = fg167_constant(t29);
  t31 = ((int64_t)(t30));
  t32 = ((t28) == (t31));
  if (t32) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2))))));
  }
  }
  }
  }
  goto fg167h_gg_done;
  }
fg167h_gg_done:;
  return t0;
}
static void fg167h_gg_function_10(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
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
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint8_t t24;
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  uint64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  uint64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
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
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) != (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = fg167_ti(t4, t5, t6);
  t8 = ((int64_t)(t7));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t8) != (t9));
  }
  t10 = t3;
  t11 = ((int64_t)(((int64_t)UINT64_C(7))));
  fg167h_gg_function_4(t10, t11, base + 0);
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t12) < (t13)))) goto label2;
  {
  t14 = ((int64_t)(v1));
  t15 = ((int64_t)(((int64_t)UINT64_C(3))));
  t16 = ((int64_t)(v3));
  t17 = fg167_ti(t14, t15, t16);
  v4 = ((int64_t)(t17));
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(((int64_t)UINT64_C(4))));
  t20 = ((int64_t)(v3));
  t21 = fg167_ti(t18, t19, t20);
  v5 = ((int64_t)(t21));
  t22 = ((int64_t)(v4));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((t22) > (t23));
  t25 = t24;
  if (t25) {
  t26 = ((int64_t)(v5));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((t26) >= (t27));
  }
  t28 = t25;
  t29 = ((int64_t)(((int64_t)UINT64_C(8))));
  fg167h_gg_function_4(t28, t29, base + 0);
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  t32 = ((int64_t)(v2));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t32)))));
  t34 = ((int64_t)(v3));
  t35 = ((uint64_t)(((uint64_t)(v4))));
  fg167_hput(t30, t33, t34, t35);
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(((int64_t)UINT64_C(4))));
  t38 = ((int64_t)(v2));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t38)))));
  t40 = ((int64_t)(v3));
  t41 = ((uint64_t)(((uint64_t)(v5))));
  fg167_hput(t36, t39, t40, t41);
  t42 = v3;
  t43 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t42) + (uint64_t)(t43)));
  }
  goto label1;
label2:;
  t44 = ((int64_t)(v1));
  t45 = ((int64_t)(((int64_t)UINT64_C(9))));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = fg167_ti(t44, t45, t46);
  (void)(t47);
  }
fg167h_gg_done:;
}
static void fg167h_gg_function_1(int64_t p0, int64_t base) {
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
  uint8_t t11;
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
  uint8_t t37;
  int64_t t38;
  int64_t t39;
  uint8_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
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
  uint64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  uint64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  uint64_t t66;
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
  uint64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  uint64_t t87;
  int64_t t88;
  int64_t t89;
  uint8_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  uint64_t t95;
  int64_t t96;
  int64_t t97;
  uint8_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  uint8_t t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  uint8_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  uint64_t t113;
  int64_t t114;
  int64_t t115;
  uint8_t t116;
  int64_t t117;
  int64_t t118;
  uint8_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  uint64_t t123;
  int64_t t124;
  int64_t t125;
  uint8_t t126;
  uint64_t t127;
  uint64_t t128;
  uint8_t t129;
  uint8_t t130;
  uint64_t t131;
  uint64_t t132;
  uint8_t t133;
  uint8_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  uint8_t t138;
  uint8_t t139;
  uint8_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  uint8_t t144;
  uint8_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  uint64_t t152;
  uint64_t t153;
  uint64_t t154;
  uint64_t t155;
  uint64_t t156;
  uint8_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  uint64_t t164;
  uint64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  uint64_t t169;
  uint64_t t170;
  uint64_t t171;
  uint64_t t172;
  uint8_t t173;
  uint8_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  uint64_t t178;
  uint64_t t179;
  uint64_t t180;
  uint8_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  int64_t t185;
  uint64_t t186;
  int64_t t187;
  int64_t t188;
  int64_t t189;
  uint64_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  uint64_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  uint64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t t204;
  uint64_t t205;
  uint64_t t206;
  uint64_t t207;
  uint64_t t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  uint64_t t215;
  uint64_t t216;
  int64_t t217;
  int64_t t218;
  int64_t t219;
  uint64_t t220;
  int64_t t221;
  int64_t t222;
  uint64_t t223;
  uint64_t t224;
  uint8_t t225;
  uint8_t t226;
  uint64_t t227;
  uint64_t t228;
  uint8_t t229;
  uint8_t t230;
  uint64_t t231;
  uint64_t t232;
  uint8_t t233;
  uint64_t t234;
  uint64_t t235;
  uint8_t t236;
  uint8_t t237;
  uint64_t t238;
  uint64_t t239;
  uint8_t t240;
  uint8_t t241;
  uint64_t t242;
  uint64_t t243;
  uint8_t t244;
  uint64_t t245;
  uint64_t t246;
  uint8_t t247;
  uint8_t t248;
  uint64_t t249;
  uint64_t t250;
  uint8_t t251;
  uint8_t t252;
  uint64_t t253;
  uint64_t t254;
  uint8_t t255;
  uint64_t t256;
  uint64_t t257;
  uint8_t t258;
  uint8_t t259;
  uint64_t t260;
  uint64_t t261;
  uint8_t t262;
  uint8_t t263;
  uint64_t t264;
  uint64_t t265;
  uint8_t t266;
  uint64_t t267;
  uint64_t t268;
  uint8_t t269;
  uint8_t t270;
  uint64_t t271;
  uint64_t t272;
  uint8_t t273;
  uint8_t t274;
  uint64_t t275;
  uint64_t t276;
  uint8_t t277;
  uint64_t t278;
  uint64_t t279;
  uint8_t t280;
  uint8_t t281;
  uint64_t t282;
  uint64_t t283;
  uint8_t t284;
  uint8_t t285;
  uint64_t t286;
  uint64_t t287;
  uint8_t t288;
  uint64_t t289;
  uint64_t t290;
  uint8_t t291;
  uint8_t t292;
  uint64_t t293;
  uint64_t t294;
  uint8_t t295;
  uint8_t t296;
  uint64_t t297;
  uint64_t t298;
  uint8_t t299;
  uint64_t t300;
  uint64_t t301;
  uint8_t t302;
  uint8_t t303;
  uint64_t t304;
  uint64_t t305;
  uint8_t t306;
  uint8_t t307;
  uint64_t t308;
  uint64_t t309;
  uint8_t t310;
  int64_t t311;
  int64_t t312;
  uint8_t t313;
  int64_t t314;
  int64_t t315;
  int64_t t316;
  int64_t t317;
  uint64_t t318;
  int64_t t319;
  int64_t t320;
  int64_t t321;
  int64_t t322;
  int64_t t323;
  uint64_t t324;
  uint64_t t325;
  int64_t t326;
  int64_t t327;
  int64_t t328;
  uint64_t t329;
  uint64_t t330;
  uint8_t t331;
  uint8_t t332;
  int64_t t333;
  int64_t t334;
  int64_t t335;
  uint64_t t336;
  uint64_t t337;
  int64_t t338;
  int64_t t339;
  int64_t t340;
  uint64_t t341;
  uint64_t t342;
  uint64_t t343;
  uint64_t t344;
  uint8_t t345;
  int64_t t346;
  int64_t t347;
  int64_t t348;
  int64_t t349;
  int64_t t350;
  int64_t t351;
  int64_t t352;
  uint8_t t353;
  int64_t t354;
  int64_t t355;
  int64_t t356;
  int64_t t357;
  int64_t t358;
  int64_t t359;
  int64_t t360;
  int64_t t361;
  uint8_t t362;
  uint8_t t363;
  int64_t t364;
  int64_t t365;
  int64_t t366;
  int64_t t367;
  int64_t t368;
  int64_t t369;
  uint8_t t370;
  uint8_t t371;
  int64_t t372;
  uint64_t t373;
  uint64_t t374;
  uint64_t t375;
  uint8_t t376;
  int64_t t377;
  int64_t t378;
  int64_t t379;
  int64_t t380;
  int64_t t381;
  int64_t t382;
  int64_t t383;
  uint64_t t384;
  int64_t t385;
  int64_t t386;
  int64_t t387;
  uint64_t t388;
  uint64_t t389;
  uint8_t t390;
  uint8_t t391;
  int64_t t392;
  int64_t t393;
  int64_t t394;
  int64_t t395;
  uint64_t t396;
  int64_t t397;
  int64_t t398;
  int64_t t399;
  uint64_t t400;
  uint64_t t401;
  uint8_t t402;
  int64_t t403;
  int64_t t404;
  int64_t t405;
  int64_t t406;
  int64_t t407;
  int64_t t408;
  int64_t t409;
  int64_t t410;
  int64_t t411;
  int64_t t412;
  int64_t t413;
  int64_t t414;
  uint64_t t415;
  int64_t t416;
  int64_t t417;
  int64_t t418;
  int64_t t419;
  int64_t t420;
  uint8_t t421;
  uint8_t t422;
  uint8_t t423;
  int64_t t424;
  int64_t t425;
  int64_t t426;
  uint8_t t427;
  uint8_t t428;
  uint8_t t429;
  int64_t t430;
  int64_t t431;
  int64_t t432;
  uint8_t t433;
  uint8_t t434;
  uint8_t t435;
  int64_t t436;
  int64_t t437;
  int64_t t438;
  uint8_t t439;
  uint8_t t440;
  uint8_t t441;
  uint8_t t442;
  int64_t t443;
  int64_t t444;
  int64_t t445;
  uint8_t t446;
  uint8_t t447;
  uint8_t t448;
  int64_t t449;
  int64_t t450;
  int64_t t451;
  int64_t t452;
  int64_t t453;
  int64_t t454;
  int64_t t455;
  int64_t t456;
  int64_t t457;
  int64_t t458;
  int64_t t459;
  int64_t t460;
  int64_t t461;
  int64_t t462;
  uint8_t t463;
  int64_t t464;
  int64_t t465;
  uint8_t t466;
  int64_t t467;
  int64_t t468;
  int64_t t469;
  int64_t t470;
  int64_t t471;
  int64_t t472;
  int64_t t473;
  int64_t t474;
  uint64_t t475;
  uint64_t t476;
  uint64_t t477;
  uint8_t t478;
  uint8_t t479;
  int64_t t480;
  int64_t t481;
  int64_t t482;
  uint64_t t483;
  uint64_t t484;
  uint64_t t485;
  uint8_t t486;
  int64_t t487;
  int64_t t488;
  int64_t t489;
  uint64_t t490;
  int64_t t491;
  int64_t t492;
  uint8_t t493;
  int64_t t494;
  int64_t t495;
  uint8_t t496;
  uint64_t t497;
  int64_t t498;
  int64_t t499;
  int64_t t500;
  int64_t t501;
  int64_t t502;
  uint64_t t503;
  uint64_t t504;
  uint64_t t505;
  uint64_t t506;
  uint64_t t507;
  uint8_t t508;
  uint8_t t509;
  uint64_t t510;
  uint64_t t511;
  uint64_t t512;
  uint64_t t513;
  uint8_t t514;
  int64_t t515;
  int64_t t516;
  int64_t t517;
  int64_t t518;
  int64_t t519;
  int64_t t520;
  int64_t t521;
  int64_t t522;
  uint64_t t523;
  int64_t t524;
  int64_t t525;
  int64_t t526;
  uint64_t t527;
  int64_t t528;
  int64_t t529;
  int64_t t530;
  uint64_t t531;
  uint64_t t532;
  int64_t t533;
  int64_t t534;
  int64_t t535;
  uint64_t t536;
  uint64_t t537;
  uint64_t t538;
  uint64_t t539;
  uint8_t t540;
  int64_t t541;
  uint64_t t542;
  uint64_t t543;
  uint64_t t544;
  uint64_t t545;
  uint8_t t546;
  uint64_t t547;
  uint64_t t548;
  uint64_t t549;
  uint64_t t550;
  uint64_t t551;
  uint64_t t552;
  uint64_t t553;
  uint64_t t554;
  uint64_t t555;
  uint64_t t556;
  uint64_t t557;
  uint64_t t558;
  uint64_t t559;
  uint64_t t560;
  uint64_t t561;
  uint64_t t562;
  uint64_t t563;
  uint64_t t564;
  uint64_t t565;
  uint64_t t566;
  uint64_t t567;
  uint64_t t568;
  int64_t t569;
  int64_t t570;
  int64_t t571;
  uint64_t t572;
  int64_t t573;
  int64_t t574;
  int64_t t575;
  uint64_t t576;
  int64_t t577;
  int64_t t578;
  int64_t t579;
  uint64_t t580;
  int64_t t581;
  int64_t t582;
  int64_t t583;
  uint64_t t584;
  int64_t t585;
  int64_t t586;
  int64_t t587;
  uint64_t t588;
  int64_t t589;
  int64_t t590;
  int64_t t591;
  uint64_t t592;
  int64_t t593;
  int64_t t594;
  int64_t t595;
  int64_t t596;
  int64_t t597;
  uint64_t t598;
  uint64_t t599;
  int64_t t600;
  int64_t t601;
  int64_t t602;
  int64_t t603;
  int64_t t604;
  int64_t t605;
  int64_t t606;
  int64_t t607;
  int64_t t608;
  uint64_t t609;
  uint64_t t610;
  int64_t t611;
  int64_t t612;
  uint64_t t613;
  uint64_t t614;
  uint8_t t615;
  uint8_t t616;
  uint64_t t617;
  uint64_t t618;
  uint8_t t619;
  uint64_t t620;
  uint64_t t621;
  uint64_t t622;
  int64_t t623;
  int64_t t624;
  int64_t t625;
  uint64_t t626;
  uint64_t t627;
  uint64_t t628;
  int64_t t629;
  int64_t t630;
  int64_t t631;
  uint64_t t632;
  uint64_t t633;
  uint64_t t634;
  uint64_t t635;
  uint64_t t636;
  uint64_t t637;
  uint8_t t638;
  uint8_t t639;
  uint64_t t640;
  uint64_t t641;
  uint8_t t642;
  uint8_t t643;
  uint64_t t644;
  uint64_t t645;
  uint64_t t646;
  uint64_t t647;
  uint64_t t648;
  uint8_t t649;
  int64_t t650;
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
  int64_t t663;
  int64_t t664;
  int64_t t665;
  uint64_t t666;
  int64_t t667;
  int64_t t668;
  int64_t t669;
  uint64_t t670;
  int64_t t671;
  int64_t t672;
  int64_t t673;
  uint64_t t674;
  int64_t t675;
  int64_t t676;
  int64_t t677;
  uint64_t t678;
  int64_t t679;
  int64_t t680;
  uint64_t t681;
  int64_t t682;
  int64_t t683;
  uint64_t t684;
  int64_t t685;
  int64_t t686;
  uint64_t t687;
  uint64_t t688;
  uint64_t t689;
  uint64_t t690;
  uint64_t t691;
  uint64_t t692;
  uint8_t t693;
  uint8_t t694;
  uint64_t t695;
  uint64_t t696;
  uint64_t t697;
  uint64_t t698;
  uint64_t t699;
  uint8_t t700;
  uint8_t t701;
  uint64_t t702;
  uint64_t t703;
  uint64_t t704;
  uint64_t t705;
  uint64_t t706;
  uint8_t t707;
  int64_t t708;
  int64_t t709;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  uint64_t v7;
  uint64_t v8;
  uint64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  uint64_t v13;
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
  uint64_t v27;
  uint64_t v28;
  uint64_t v29;
  uint64_t v30;
  uint64_t v31;
  uint64_t v32;
  uint64_t v33;
  uint64_t v34;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = fg167_arg(t0, t1);
  v1 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = fg167_plan_ptr(t3);
  v2 = ((int64_t)(t4));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(2))));
  t7 = fg167_arg(t5, t6);
  v3 = ((int64_t)(t7));
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t8) != (t9));
  t11 = t10;
  if (t11) {
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  t14 = fg167_arg(t12, t13);
  t15 = ((int64_t)(t14));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t15) != (t16));
  }
  t17 = t11;
  t18 = t17;
  if (t18) {
  t19 = ((int64_t)(v1));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t19) >= (t20));
  }
  t21 = t18;
  t22 = t21;
  if (t22) {
  t23 = ((int64_t)(v1));
  t24 = ((int64_t)(((int64_t)UINT64_C(8))));
  t22 = ((t23) < (t24));
  }
  t25 = t22;
  t26 = ((int64_t)(((int64_t)UINT64_C(9))));
  fg167h_gg_function_4(t25, t26, base + 0);
  t27 = ((int64_t)(v3));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = fg167_ti(t27, t28, t29);
  v4 = ((int64_t)(t30));
  t31 = ((int64_t)(v4));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = ((t31) != (t32));
  t34 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg167h_gg_function_4(t33, t34, base + 0);
  t35 = ((int64_t)(v1));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = ((t35) == (t36));
  if (t37) {
  {
  v5 = ((int64_t)(v3));
  }
  } else {
  {
  v5 = ((int64_t)(v4));
  }
  }
  t38 = ((int64_t)(v1));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = ((t38) == (t39));
  if (t40) {
  {
  v6 = ((int64_t)(v4));
  }
  } else {
  {
  t41 = ((int64_t)(v1));
  t42 = ((int64_t)(((int64_t)UINT64_C(5))));
  t43 = ((t41) == (t42));
  if (t43) {
  {
  v6 = ((int64_t)(v4));
  }
  } else {
  {
  t44 = ((int64_t)(v3));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = ((int64_t)(((int64_t)UINT64_C(1))));
  t47 = fg167_ti(t44, t45, t46);
  v6 = ((int64_t)(t47));
  }
  }
  }
  }
  t48 = ((int64_t)(v6));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = ((t48) != (t49));
  t51 = ((int64_t)(((int64_t)UINT64_C(11))));
  fg167h_gg_function_4(t50, t51, base + 0);
  t52 = ((int64_t)(v5));
  v7 = fg167h_gg_function_7(t52, base + 0);
  t53 = ((int64_t)(v6));
  v8 = fg167h_gg_function_7(t53, base + 0);
  t54 = ((int64_t)(v3));
  v9 = fg167h_gg_function_7(t54, base + 0);
  t55 = ((int64_t)(v2));
  t56 = ((int64_t)(((int64_t)UINT64_C(10))));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = ((uint64_t)(v7));
  fg167_hput(t55, t56, t57, t58);
  t59 = ((int64_t)(v2));
  t60 = ((int64_t)(((int64_t)UINT64_C(11))));
  t61 = ((int64_t)(((int64_t)UINT64_C(0))));
  t62 = ((uint64_t)(v8));
  fg167_hput(t59, t60, t61, t62);
  t63 = ((int64_t)(v2));
  t64 = ((int64_t)(((int64_t)UINT64_C(12))));
  t65 = ((int64_t)(((int64_t)UINT64_C(0))));
  t66 = ((uint64_t)(v9));
  fg167_hput(t63, t64, t65, t66);
  t67 = ((int64_t)(v2));
  t68 = ((int64_t)(v3));
  t69 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg167h_gg_function_10(t67, t68, t69, base + 0);
  t70 = ((int64_t)(v2));
  t71 = ((int64_t)(v5));
  t72 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg167h_gg_function_10(t70, t71, t72, base + 0);
  t73 = ((int64_t)(v2));
  t74 = ((int64_t)(v6));
  t75 = ((int64_t)(((int64_t)UINT64_C(2))));
  fg167h_gg_function_10(t73, t74, t75, base + 0);
  t76 = ((int64_t)(v2));
  t77 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = ((int64_t)(((int64_t)UINT64_C(9))));
  t79 = ((int64_t)(v3));
  t80 = ((int64_t)(((int64_t)UINT64_C(1))));
  t81 = ((int64_t)(((int64_t)UINT64_C(0))));
  t82 = fg167_ti(t79, t80, t81);
  t83 = ((uint64_t)(((uint64_t)(t82))));
  fg167_hput(t76, t77, t78, t83);
  t84 = ((int64_t)(v2));
  t85 = ((int64_t)(((int64_t)UINT64_C(0))));
  t86 = ((int64_t)(((int64_t)UINT64_C(0))));
  t88 = ((int64_t)(v1));
  t89 = ((int64_t)(((int64_t)UINT64_C(0))));
  t90 = ((t88) == (t89));
  if (t90) {
  {
  t87 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  } else {
  {
  t91 = ((int64_t)(v5));
  t92 = ((int64_t)(((int64_t)UINT64_C(1))));
  t93 = ((int64_t)(((int64_t)UINT64_C(0))));
  t94 = fg167_ti(t91, t92, t93);
  t87 = ((uint64_t)(((uint64_t)(t94))));
  }
  }
  t95 = ((uint64_t)(t87));
  fg167_hput(t84, t85, t86, t95);
  v10 = ((int64_t)(((int64_t)UINT64_C(1))));
  t96 = ((int64_t)(v1));
  t97 = ((int64_t)(((int64_t)UINT64_C(6))));
  t98 = ((t96) >= (t97));
  if (t98) {
  {
  t99 = ((int64_t)(v0));
  t100 = ((int64_t)(((int64_t)UINT64_C(3))));
  t101 = fg167_arg(t99, t100);
  v10 = ((int64_t)(t101));
  t102 = ((int64_t)(v10));
  t103 = ((int64_t)(((int64_t)UINT64_C(2))));
  t104 = ((t102) >= (t103));
  t105 = t104;
  if (t105) {
  t106 = ((int64_t)(v10));
  t107 = ((int64_t)(((int64_t)UINT64_C(8))));
  t105 = ((t106) <= (t107));
  }
  t108 = t105;
  t109 = ((int64_t)(((int64_t)UINT64_C(12))));
  fg167h_gg_function_4(t108, t109, base + 0);
  }
  }
  t110 = ((int64_t)(v2));
  t111 = ((int64_t)(((int64_t)UINT64_C(14))));
  t112 = ((int64_t)(((int64_t)UINT64_C(0))));
  t113 = ((uint64_t)(((uint64_t)(v10))));
  fg167_hput(t110, t111, t112, t113);
  t114 = ((int64_t)(v1));
  t115 = ((int64_t)(((int64_t)UINT64_C(6))));
  t116 = ((t114) == (t115));
  if (t116) {
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t117 = ((int64_t)(v1));
  t118 = ((int64_t)(((int64_t)UINT64_C(7))));
  t119 = ((t117) == (t118));
  if (t119) {
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(3))));
  }
  } else {
  {
  v11 = ((int64_t)(v1));
  }
  }
  }
  }
  t120 = ((int64_t)(v2));
  t121 = ((int64_t)(((int64_t)UINT64_C(13))));
  t122 = ((int64_t)(((int64_t)UINT64_C(0))));
  t123 = ((uint64_t)(((uint64_t)(v11))));
  fg167_hput(t120, t121, t122, t123);
  t124 = ((int64_t)(v1));
  t125 = ((int64_t)(((int64_t)UINT64_C(5))));
  t126 = ((t124) == (t125));
  if (t126) {
  {
  t127 = ((uint64_t)(v7));
  t128 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t129 = ((t127) == (t128));
  t130 = t129;
  if (t130) {
  t131 = ((uint64_t)(v9));
  t132 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t130 = ((t131) == (t132));
  }
  t133 = t130;
  t134 = t133;
  if (t134) {
  t135 = ((int64_t)(((int64_t)UINT64_C(0))));
  t136 = ((int64_t)(v3));
  t137 = ((int64_t)(((int64_t)UINT64_C(0))));
  t138 = fg167_pred(t135, t136, t137);
  t134 = t138;
  }
  t139 = t134;
  t140 = t139;
  if (t140) {
  t141 = ((int64_t)(((int64_t)UINT64_C(3))));
  t142 = ((int64_t)(v3));
  t143 = ((int64_t)(v5));
  t144 = fg167_pred(t141, t142, t143);
  t140 = t144;
  }
  t145 = t140;
  t146 = ((int64_t)(((int64_t)UINT64_C(13))));
  fg167h_gg_function_4(t145, t146, base + 0);
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t147 = ((int64_t)(v12));
  t148 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t147) < (t148)))) goto label2;
  {
  t149 = ((int64_t)(v2));
  t150 = ((int64_t)(((int64_t)UINT64_C(5))));
  t151 = ((int64_t)(v12));
  t152 = fg167_hget(t149, t150, t151);
  t153 = ((uint64_t)(t152));
  t154 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t155 = ((uint64_t)(((uint64_t)fg167h_gg_umod(t153, t154))));
  t156 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t157 = ((t155) == (t156));
  t158 = ((int64_t)(((int64_t)UINT64_C(14))));
  fg167h_gg_function_4(t157, t158, base + 0);
  t159 = v12;
  t160 = ((int64_t)(((int64_t)UINT64_C(1))));
  v12 = ((int64_t)((uint64_t)(t159) + (uint64_t)(t160)));
  }
  goto label1;
label2:;
  t161 = ((int64_t)(v2));
  t162 = ((int64_t)(((int64_t)UINT64_C(1))));
  t163 = ((int64_t)(((int64_t)UINT64_C(2))));
  t164 = fg167_hget(t161, t162, t163);
  t165 = ((uint64_t)(t164));
  t166 = ((int64_t)(v2));
  t167 = ((int64_t)(((int64_t)UINT64_C(1))));
  t168 = ((int64_t)(((int64_t)UINT64_C(3))));
  t169 = fg167_hget(t166, t167, t168);
  t170 = ((uint64_t)(t169));
  v13 = fg167h_gg_function_13(t165, t170, base + 0);
  t171 = ((uint64_t)(v13));
  t172 = ((uint64_t)(((int64_t)UINT64_C(32768))));
  t173 = ((t171) <= (t172));
  t174 = t173;
  if (t174) {
  t175 = ((int64_t)(v2));
  t176 = ((int64_t)(((int64_t)UINT64_C(1))));
  t177 = ((int64_t)(((int64_t)UINT64_C(1))));
  t178 = fg167_hget(t175, t176, t177);
  t179 = ((uint64_t)(t178));
  t180 = ((uint64_t)(((int64_t)UINT64_C(65535))));
  t174 = ((t179) <= (t180));
  }
  t181 = t174;
  t182 = ((int64_t)(((int64_t)UINT64_C(15))));
  fg167h_gg_function_4(t181, t182, base + 0);
  t183 = ((int64_t)(v2));
  t184 = ((int64_t)(((int64_t)UINT64_C(15))));
  t185 = ((int64_t)(((int64_t)UINT64_C(0))));
  t186 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2))))));
  fg167_hput(t183, t184, t185, t186);
  t187 = ((int64_t)(v2));
  t188 = ((int64_t)(((int64_t)UINT64_C(9))));
  t189 = ((int64_t)(((int64_t)UINT64_C(0))));
  t190 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(32))))));
  fg167_hput(t187, t188, t189, t190);
  t191 = ((int64_t)(v2));
  t192 = ((int64_t)(((int64_t)UINT64_C(9))));
  t193 = ((int64_t)(((int64_t)UINT64_C(1))));
  t194 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  fg167_hput(t191, t192, t193, t194);
  t195 = ((int64_t)(v2));
  t196 = ((int64_t)(((int64_t)UINT64_C(9))));
  t197 = ((int64_t)(((int64_t)UINT64_C(2))));
  t198 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  fg167_hput(t195, t196, t197, t198);
  t199 = ((int64_t)(v2));
  t200 = ((int64_t)(((int64_t)UINT64_C(8))));
  t201 = ((int64_t)(((int64_t)UINT64_C(0))));
  t202 = ((int64_t)(v2));
  t203 = ((int64_t)(((int64_t)UINT64_C(1))));
  t204 = ((int64_t)(((int64_t)UINT64_C(0))));
  t205 = fg167_hget(t202, t203, t204);
  t206 = ((uint64_t)(t205));
  t207 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(32))))));
  t208 = fg167h_gg_function_15(t206, t207, base + 0);
  fg167_hput(t199, t200, t201, t208);
  t209 = ((int64_t)(v2));
  t210 = ((int64_t)(((int64_t)UINT64_C(8))));
  t211 = ((int64_t)(((int64_t)UINT64_C(1))));
  t212 = ((int64_t)(v2));
  t213 = ((int64_t)(((int64_t)UINT64_C(1))));
  t214 = ((int64_t)(((int64_t)UINT64_C(1))));
  t215 = fg167_hget(t212, t213, t214);
  t216 = ((uint64_t)(t215));
  fg167_hput(t209, t210, t211, t216);
  t217 = ((int64_t)(v2));
  t218 = ((int64_t)(((int64_t)UINT64_C(8))));
  t219 = ((int64_t)(((int64_t)UINT64_C(2))));
  t220 = ((uint64_t)(v13));
  fg167_hput(t217, t218, t219, t220);
  t221 = ((int64_t)(v0));
  fg167_launch(t221);
  goto fg167h_gg_done;
  }
  }
  t222 = ((int64_t)UINT64_C(1));
  v14 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t222)))));
  t223 = ((uint64_t)(v7));
  t224 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t225 = ((t223) == (t224));
  t226 = t225;
  if (t226) {
  t227 = ((uint64_t)(v8));
  t228 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t226 = ((t227) == (t228));
  }
  t229 = t226;
  t230 = t229;
  if (t230) {
  t231 = ((uint64_t)(v9));
  t232 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t230 = ((t231) == (t232));
  }
  t233 = t230;
  if (t233) {
  {
  v14 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t234 = ((uint64_t)(v7));
  t235 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t236 = ((t234) == (t235));
  t237 = t236;
  if (t237) {
  t238 = ((uint64_t)(v8));
  t239 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t237 = ((t238) == (t239));
  }
  t240 = t237;
  t241 = t240;
  if (t241) {
  t242 = ((uint64_t)(v9));
  t243 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t241 = ((t242) == (t243));
  }
  t244 = t241;
  if (t244) {
  {
  v14 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t245 = ((uint64_t)(v7));
  t246 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t247 = ((t245) == (t246));
  t248 = t247;
  if (t248) {
  t249 = ((uint64_t)(v8));
  t250 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t248 = ((t249) == (t250));
  }
  t251 = t248;
  t252 = t251;
  if (t252) {
  t253 = ((uint64_t)(v9));
  t254 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t252 = ((t253) == (t254));
  }
  t255 = t252;
  if (t255) {
  {
  v14 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  }
  t256 = ((uint64_t)(v7));
  t257 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t258 = ((t256) == (t257));
  t259 = t258;
  if (t259) {
  t260 = ((uint64_t)(v8));
  t261 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t259 = ((t260) == (t261));
  }
  t262 = t259;
  t263 = t262;
  if (t263) {
  t264 = ((uint64_t)(v9));
  t265 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t263 = ((t264) == (t265));
  }
  t266 = t263;
  if (t266) {
  {
  v14 = ((int64_t)(((int64_t)UINT64_C(3))));
  }
  }
  t267 = ((uint64_t)(v7));
  t268 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t269 = ((t267) == (t268));
  t270 = t269;
  if (t270) {
  t271 = ((uint64_t)(v8));
  t272 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t270 = ((t271) == (t272));
  }
  t273 = t270;
  t274 = t273;
  if (t274) {
  t275 = ((uint64_t)(v9));
  t276 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t274 = ((t275) == (t276));
  }
  t277 = t274;
  if (t277) {
  {
  v14 = ((int64_t)(((int64_t)UINT64_C(4))));
  }
  }
  t278 = ((uint64_t)(v7));
  t279 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t280 = ((t278) == (t279));
  t281 = t280;
  if (t281) {
  t282 = ((uint64_t)(v8));
  t283 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t281 = ((t282) == (t283));
  }
  t284 = t281;
  t285 = t284;
  if (t285) {
  t286 = ((uint64_t)(v9));
  t287 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t285 = ((t286) == (t287));
  }
  t288 = t285;
  if (t288) {
  {
  v14 = ((int64_t)(((int64_t)UINT64_C(5))));
  }
  }
  t289 = ((uint64_t)(v7));
  t290 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t291 = ((t289) == (t290));
  t292 = t291;
  if (t292) {
  t293 = ((uint64_t)(v8));
  t294 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t292 = ((t293) == (t294));
  }
  t295 = t292;
  t296 = t295;
  if (t296) {
  t297 = ((uint64_t)(v9));
  t298 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t296 = ((t297) == (t298));
  }
  t299 = t296;
  if (t299) {
  {
  v14 = ((int64_t)(((int64_t)UINT64_C(6))));
  }
  }
  t300 = ((uint64_t)(v7));
  t301 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t302 = ((t300) == (t301));
  t303 = t302;
  if (t303) {
  t304 = ((uint64_t)(v8));
  t305 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t303 = ((t304) == (t305));
  }
  t306 = t303;
  t307 = t306;
  if (t307) {
  t308 = ((uint64_t)(v9));
  t309 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t307 = ((t308) == (t309));
  }
  t310 = t307;
  if (t310) {
  {
  v14 = ((int64_t)(((int64_t)UINT64_C(7))));
  }
  }
  t311 = ((int64_t)(v14));
  t312 = ((int64_t)(((int64_t)UINT64_C(0))));
  t313 = ((t311) >= (t312));
  t314 = ((int64_t)(((int64_t)UINT64_C(16))));
  fg167h_gg_function_4(t313, t314, base + 0);
  t315 = ((int64_t)(v2));
  t316 = ((int64_t)(((int64_t)UINT64_C(16))));
  t317 = ((int64_t)(((int64_t)UINT64_C(0))));
  t318 = ((uint64_t)(((uint64_t)(v14))));
  fg167_hput(t315, t316, t317, t318);
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t319 = ((int64_t)(v15));
  t320 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t319) < (t320)))) goto label5;
  {
  t321 = ((int64_t)(v2));
  t322 = ((int64_t)(((int64_t)UINT64_C(1))));
  t323 = ((int64_t)(v15));
  t324 = fg167_hget(t321, t322, t323);
  t325 = ((uint64_t)(t324));
  t326 = ((int64_t)(v2));
  t327 = ((int64_t)(((int64_t)UINT64_C(2))));
  t328 = ((int64_t)(v15));
  t329 = fg167_hget(t326, t327, t328);
  t330 = ((uint64_t)(t329));
  t331 = ((t325) == (t330));
  t332 = t331;
  if (t332) {
  t333 = ((int64_t)(v2));
  t334 = ((int64_t)(((int64_t)UINT64_C(1))));
  t335 = ((int64_t)(v15));
  t336 = fg167_hget(t333, t334, t335);
  t337 = ((uint64_t)(t336));
  t338 = ((int64_t)(v2));
  t339 = ((int64_t)(((int64_t)UINT64_C(3))));
  t340 = ((int64_t)(v15));
  t341 = fg167_hget(t338, t339, t340);
  t342 = ((uint64_t)(t341));
  t343 = ((uint64_t)(((uint64_t)fg167h_gg_umod(t337, t342))));
  t344 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t332 = ((t343) == (t344));
  }
  t345 = t332;
  t346 = ((int64_t)(((int64_t)UINT64_C(17))));
  fg167h_gg_function_4(t345, t346, base + 0);
  t347 = v15;
  t348 = ((int64_t)(((int64_t)UINT64_C(1))));
  v15 = ((int64_t)((uint64_t)(t347) + (uint64_t)(t348)));
  }
  goto label4;
label5:;
  v16 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t349 = ((int64_t)(v16));
  t350 = ((int64_t)(v10));
  if (!(((t349) < (t350)))) goto label8;
  {
  t351 = ((int64_t)(v1));
  t352 = ((int64_t)(((int64_t)UINT64_C(0))));
  t353 = ((t351) == (t352));
  if (t353) {
  {
  v17 = ((int64_t)(v6));
  }
  } else {
  {
  t354 = ((int64_t)(v3));
  t355 = ((int64_t)(((int64_t)UINT64_C(0))));
  t356 = ((int64_t)(v16));
  t357 = ((int64_t)(((int64_t)UINT64_C(1))));
  t358 = ((int64_t)(((int64_t)((uint64_t)(t356) + (uint64_t)(t357)))));
  t359 = fg167_ti(t354, t355, t358);
  v17 = ((int64_t)(t359));
  }
  }
  t360 = ((int64_t)(v17));
  t361 = ((int64_t)(((int64_t)UINT64_C(0))));
  t362 = ((t360) != (t361));
  t363 = t362;
  if (t363) {
  t364 = ((int64_t)(v17));
  t365 = ((int64_t)(((int64_t)UINT64_C(1))));
  t366 = ((int64_t)(((int64_t)UINT64_C(0))));
  t367 = fg167_ti(t364, t365, t366);
  t368 = ((int64_t)(t367));
  t369 = ((int64_t)(((int64_t)UINT64_C(0))));
  t363 = ((t368) != (t369));
  }
  t370 = t363;
  t371 = t370;
  if (t371) {
  t372 = ((int64_t)(v17));
  t373 = fg167h_gg_function_7(t372, base + 0);
  t374 = ((uint64_t)(t373));
  t375 = ((uint64_t)(v8));
  t371 = ((t374) == (t375));
  }
  t376 = t371;
  t377 = ((int64_t)(((int64_t)UINT64_C(18))));
  fg167h_gg_function_4(t376, t377, base + 0);
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t378 = ((int64_t)(v15));
  t379 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t378) < (t379)))) goto label11;
  {
  t380 = ((int64_t)(v17));
  t381 = ((int64_t)(((int64_t)UINT64_C(3))));
  t382 = ((int64_t)(v15));
  t383 = fg167_ti(t380, t381, t382);
  t384 = ((uint64_t)(((uint64_t)(t383))));
  t385 = ((int64_t)(v2));
  t386 = ((int64_t)(((int64_t)UINT64_C(3))));
  t387 = ((int64_t)(v15));
  t388 = fg167_hget(t385, t386, t387);
  t389 = ((uint64_t)(t388));
  t390 = ((t384) == (t389));
  t391 = t390;
  if (t391) {
  t392 = ((int64_t)(v17));
  t393 = ((int64_t)(((int64_t)UINT64_C(4))));
  t394 = ((int64_t)(v15));
  t395 = fg167_ti(t392, t393, t394);
  t396 = ((uint64_t)(((uint64_t)(t395))));
  t397 = ((int64_t)(v2));
  t398 = ((int64_t)(((int64_t)UINT64_C(6))));
  t399 = ((int64_t)(v15));
  t400 = fg167_hget(t397, t398, t399);
  t401 = ((uint64_t)(t400));
  t391 = ((t396) == (t401));
  }
  t402 = t391;
  t403 = ((int64_t)(((int64_t)UINT64_C(19))));
  fg167h_gg_function_4(t402, t403, base + 0);
  t404 = v15;
  t405 = ((int64_t)(((int64_t)UINT64_C(1))));
  v15 = ((int64_t)((uint64_t)(t404) + (uint64_t)(t405)));
  }
  goto label10;
label11:;
  t406 = ((int64_t)(v2));
  t407 = ((int64_t)(((int64_t)UINT64_C(0))));
  t408 = ((int64_t)(v16));
  t409 = ((int64_t)(((int64_t)UINT64_C(1))));
  t410 = ((int64_t)(((int64_t)((uint64_t)(t408) + (uint64_t)(t409)))));
  t411 = ((int64_t)(v17));
  t412 = ((int64_t)(((int64_t)UINT64_C(1))));
  t413 = ((int64_t)(((int64_t)UINT64_C(0))));
  t414 = fg167_ti(t411, t412, t413);
  t415 = ((uint64_t)(((uint64_t)(t414))));
  fg167_hput(t406, t407, t410, t415);
  t416 = v16;
  t417 = ((int64_t)(((int64_t)UINT64_C(1))));
  v16 = ((int64_t)((uint64_t)(t416) + (uint64_t)(t417)));
  }
  goto label7;
label8:;
  t418 = ((int64_t)(((int64_t)UINT64_C(0))));
  t419 = ((int64_t)(v5));
  t420 = ((int64_t)(((int64_t)UINT64_C(0))));
  t421 = fg167_pred(t418, t419, t420);
  t422 = t421;
  t423 = t422;
  if (t423) {
  t424 = ((int64_t)(((int64_t)UINT64_C(0))));
  t425 = ((int64_t)(v6));
  t426 = ((int64_t)(((int64_t)UINT64_C(0))));
  t427 = fg167_pred(t424, t425, t426);
  t423 = t427;
  }
  t428 = t423;
  t429 = t428;
  if (t429) {
  t430 = ((int64_t)(((int64_t)UINT64_C(0))));
  t431 = ((int64_t)(v3));
  t432 = ((int64_t)(((int64_t)UINT64_C(0))));
  t433 = fg167_pred(t430, t431, t432);
  t429 = t433;
  }
  t434 = t429;
  t435 = t434;
  if (t435) {
  t436 = ((int64_t)(((int64_t)UINT64_C(1))));
  t437 = ((int64_t)(v5));
  t438 = ((int64_t)(((int64_t)UINT64_C(0))));
  t439 = fg167_pred(t436, t437, t438);
  t440 = t439;
  t435 = (!(t440));
  }
  t441 = t435;
  t442 = t441;
  if (t442) {
  t443 = ((int64_t)(((int64_t)UINT64_C(1))));
  t444 = ((int64_t)(v6));
  t445 = ((int64_t)(((int64_t)UINT64_C(0))));
  t446 = fg167_pred(t443, t444, t445);
  t447 = t446;
  t442 = (!(t447));
  }
  t448 = t442;
  if (t448) {
  {
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t449 = ((int64_t)(v15));
  t450 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t449) < (t450)))) goto label14;
  {
  t451 = ((int64_t)(v6));
  t452 = ((int64_t)(((int64_t)UINT64_C(3))));
  t453 = ((int64_t)(v15));
  t454 = fg167_ti(t451, t452, t453);
  t455 = ((int64_t)(t454));
  t456 = ((int64_t)(v3));
  t457 = ((int64_t)(((int64_t)UINT64_C(3))));
  t458 = ((int64_t)(v15));
  t459 = fg167_ti(t456, t457, t458);
  t460 = ((int64_t)(t459));
  t461 = ((int64_t)(((int64_t)fg167h_gg_div(t455, t460))));
  t462 = ((int64_t)(((int64_t)UINT64_C(1))));
  t463 = ((t461) != (t462));
  if (t463) {
  {
  goto label14;
  }
  }
  t464 = ((int64_t)(v15));
  t465 = ((int64_t)(((int64_t)UINT64_C(0))));
  t466 = ((t464) > (t465));
  if (t466) {
  {
  t467 = ((int64_t)(v2));
  fg167h_gg_function_17(t467, base + 0);
  }
  }
  t468 = v15;
  t469 = ((int64_t)(((int64_t)UINT64_C(1))));
  v15 = ((int64_t)((uint64_t)(t468) + (uint64_t)(t469)));
  }
  goto label13;
label14:;
  }
  }
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
label16:;
  t470 = ((int64_t)(v15));
  t471 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t470) < (t471)))) goto label17;
  {
  t472 = ((int64_t)(v2));
  t473 = ((int64_t)(((int64_t)UINT64_C(1))));
  t474 = ((int64_t)(v15));
  t475 = fg167_hget(t472, t473, t474);
  t476 = ((uint64_t)(t475));
  t477 = ((uint64_t)(((int64_t)UINT64_C(4294967295))));
  t478 = ((t476) <= (t477));
  t479 = t478;
  if (t479) {
  t480 = ((int64_t)(v2));
  t481 = ((int64_t)(((int64_t)UINT64_C(3))));
  t482 = ((int64_t)(v15));
  t483 = fg167_hget(t480, t481, t482);
  t484 = ((uint64_t)(t483));
  t485 = ((uint64_t)(((int64_t)UINT64_C(4294967295))));
  t479 = ((t484) <= (t485));
  }
  t486 = t479;
  t487 = ((int64_t)(((int64_t)UINT64_C(20))));
  fg167h_gg_function_4(t486, t487, base + 0);
  v18 = ((int64_t)(((int64_t)UINT64_C(0))));
label19:;
  t488 = ((int64_t)(v18));
  t489 = ((int64_t)(((int64_t)UINT64_C(3))));
  if (!(((t488) < (t489)))) goto label20;
  {
  t491 = ((int64_t)(v18));
  t492 = ((int64_t)(((int64_t)UINT64_C(0))));
  t493 = ((t491) == (t492));
  if (t493) {
  {
  t490 = ((uint64_t)(v9));
  }
  } else {
  {
  t494 = ((int64_t)(v18));
  t495 = ((int64_t)(((int64_t)UINT64_C(1))));
  t496 = ((t494) == (t495));
  if (t496) {
  {
  t490 = ((uint64_t)(v7));
  }
  } else {
  {
  t490 = ((uint64_t)(v8));
  }
  }
  }
  }
  t497 = ((uint64_t)(t490));
  v19 = fg167h_gg_function_18(t497, base + 0);
  t498 = ((int64_t)(v2));
  t499 = ((int64_t)(((int64_t)UINT64_C(4))));
  t500 = ((int64_t)(v18));
  t501 = ((int64_t)(((int64_t)((uint64_t)(t499) + (uint64_t)(t500)))));
  t502 = ((int64_t)(v15));
  t503 = fg167_hget(t498, t501, t502);
  v20 = ((uint64_t)(t503));
  t504 = ((uint64_t)(v20));
  t505 = ((uint64_t)(v19));
  t506 = ((uint64_t)(((uint64_t)fg167h_gg_umod(t504, t505))));
  t507 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t508 = ((t506) == (t507));
  t509 = t508;
  if (t509) {
  t510 = ((uint64_t)(v20));
  t511 = ((uint64_t)(v19));
  t512 = ((uint64_t)(((uint64_t)fg167h_gg_udiv(t510, t511))));
  t513 = ((uint64_t)(((int64_t)UINT64_C(4294967295))));
  t509 = ((t512) <= (t513));
  }
  t514 = t509;
  t515 = ((int64_t)(((int64_t)UINT64_C(21))));
  fg167h_gg_function_4(t514, t515, base + 0);
  t516 = v18;
  t517 = ((int64_t)(((int64_t)UINT64_C(1))));
  v18 = ((int64_t)((uint64_t)(t516) + (uint64_t)(t517)));
  }
  goto label19;
label20:;
  t518 = v15;
  t519 = ((int64_t)(((int64_t)UINT64_C(1))));
  v15 = ((int64_t)((uint64_t)(t518) + (uint64_t)(t519)));
  }
  goto label16;
label17:;
  t520 = ((int64_t)(v2));
  t521 = ((int64_t)(((int64_t)UINT64_C(1))));
  t522 = ((int64_t)(((int64_t)UINT64_C(0))));
  t523 = fg167_hget(t520, t521, t522);
  v21 = ((uint64_t)(t523));
  t524 = ((int64_t)(v2));
  t525 = ((int64_t)(((int64_t)UINT64_C(1))));
  t526 = ((int64_t)(((int64_t)UINT64_C(1))));
  t527 = fg167_hget(t524, t525, t526);
  v22 = ((uint64_t)(t527));
  t528 = ((int64_t)(v2));
  t529 = ((int64_t)(((int64_t)UINT64_C(1))));
  t530 = ((int64_t)(((int64_t)UINT64_C(2))));
  t531 = fg167_hget(t528, t529, t530);
  t532 = ((uint64_t)(t531));
  t533 = ((int64_t)(v2));
  t534 = ((int64_t)(((int64_t)UINT64_C(1))));
  t535 = ((int64_t)(((int64_t)UINT64_C(3))));
  t536 = fg167_hget(t533, t534, t535);
  t537 = ((uint64_t)(t536));
  v23 = fg167h_gg_function_13(t532, t537, base + 0);
  t538 = ((uint64_t)(v23));
  t539 = ((uint64_t)(((int64_t)UINT64_C(4294967295))));
  t540 = ((t538) <= (t539));
  t541 = ((int64_t)(((int64_t)UINT64_C(22))));
  fg167h_gg_function_4(t540, t541, base + 0);
  t542 = ((uint64_t)(v21));
  t543 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t544 = ((uint64_t)(((uint64_t)fg167h_gg_udiv(t542, t543))));
  t545 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t546 = ((t544) > (t545));
  if (t546) {
  {
  t547 = ((uint64_t)(v21));
  t548 = ((uint64_t)(((int64_t)UINT64_C(2))));
  v24 = ((uint64_t)(((uint64_t)fg167h_gg_udiv(t547, t548))));
  }
  } else {
  {
  v24 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  }
  t549 = ((uint64_t)(v24));
  t550 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(128))))));
  v25 = fg167h_gg_function_19(t549, t550, base + 0);
  t551 = ((uint64_t)(v22));
  t552 = ((uint64_t)(((int64_t)UINT64_C(128))));
  t553 = ((uint64_t)(v25));
  t554 = ((uint64_t)(((uint64_t)fg167h_gg_udiv(t552, t553))));
  v26 = fg167h_gg_function_19(t551, t554, base + 0);
  t555 = ((uint64_t)(v23));
  t556 = ((uint64_t)(((int64_t)UINT64_C(128))));
  t557 = ((uint64_t)(v25));
  t558 = ((uint64_t)(((uint64_t)fg167h_gg_udiv(t556, t557))));
  t559 = ((uint64_t)(v26));
  t560 = ((uint64_t)(((uint64_t)fg167h_gg_udiv(t558, t559))));
  t561 = fg167h_gg_function_19(t555, t560, base + 0);
  t562 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(64))))));
  v27 = fg167h_gg_function_19(t561, t562, base + 0);
  t563 = ((uint64_t)(v24));
  t564 = ((uint64_t)(v25));
  v28 = fg167h_gg_function_15(t563, t564, base + 0);
  t565 = ((uint64_t)(v22));
  t566 = ((uint64_t)(v26));
  v29 = fg167h_gg_function_15(t565, t566, base + 0);
  t567 = ((uint64_t)(v23));
  t568 = ((uint64_t)(v27));
  v30 = fg167h_gg_function_15(t567, t568, base + 0);
  t569 = ((int64_t)(v2));
  t570 = ((int64_t)(((int64_t)UINT64_C(9))));
  t571 = ((int64_t)(((int64_t)UINT64_C(0))));
  t572 = ((uint64_t)(v25));
  fg167_hput(t569, t570, t571, t572);
  t573 = ((int64_t)(v2));
  t574 = ((int64_t)(((int64_t)UINT64_C(9))));
  t575 = ((int64_t)(((int64_t)UINT64_C(1))));
  t576 = ((uint64_t)(v26));
  fg167_hput(t573, t574, t575, t576);
  t577 = ((int64_t)(v2));
  t578 = ((int64_t)(((int64_t)UINT64_C(9))));
  t579 = ((int64_t)(((int64_t)UINT64_C(2))));
  t580 = ((uint64_t)(v27));
  fg167_hput(t577, t578, t579, t580);
  t581 = ((int64_t)(v2));
  t582 = ((int64_t)(((int64_t)UINT64_C(8))));
  t583 = ((int64_t)(((int64_t)UINT64_C(0))));
  t584 = ((uint64_t)(v28));
  fg167_hput(t581, t582, t583, t584);
  t585 = ((int64_t)(v2));
  t586 = ((int64_t)(((int64_t)UINT64_C(8))));
  t587 = ((int64_t)(((int64_t)UINT64_C(1))));
  t588 = ((uint64_t)(v29));
  fg167_hput(t585, t586, t587, t588);
  t589 = ((int64_t)(v2));
  t590 = ((int64_t)(((int64_t)UINT64_C(8))));
  t591 = ((int64_t)(((int64_t)UINT64_C(2))));
  t592 = ((uint64_t)(v30));
  fg167_hput(t589, t590, t591, t592);
  t593 = ((int64_t)(v2));
  t594 = ((int64_t)(((int64_t)UINT64_C(0))));
  t595 = ((int64_t)(v2));
  t596 = ((int64_t)(((int64_t)UINT64_C(1))));
  t597 = ((int64_t)(((int64_t)UINT64_C(3))));
  t598 = fg167_hget(t595, t596, t597);
  t599 = ((uint64_t)(t598));
  fg167h_gg_function_20(t593, t594, t599, base + 0);
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
label22:;
  t600 = ((int64_t)(v15));
  t601 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t600) < (t601)))) goto label23;
  {
  t602 = ((int64_t)(v2));
  t603 = ((int64_t)(v15));
  t604 = ((int64_t)(((int64_t)UINT64_C(1))));
  t605 = ((int64_t)(((int64_t)((uint64_t)(t603) + (uint64_t)(t604)))));
  t606 = ((int64_t)(v2));
  t607 = ((int64_t)(((int64_t)UINT64_C(3))));
  t608 = ((int64_t)(v15));
  t609 = fg167_hget(t606, t607, t608);
  t610 = ((uint64_t)(t609));
  fg167h_gg_function_20(t602, t605, t610, base + 0);
  t611 = v15;
  t612 = ((int64_t)(((int64_t)UINT64_C(1))));
  v15 = ((int64_t)((uint64_t)(t611) + (uint64_t)(t612)));
  }
  goto label22;
label23:;
  t613 = ((uint64_t)(v29));
  t614 = ((uint64_t)(((int64_t)UINT64_C(65535))));
  t615 = ((t613) > (t614));
  t616 = t615;
  if (!t616) {
  t617 = ((uint64_t)(v30));
  t618 = ((uint64_t)(((int64_t)UINT64_C(65535))));
  t616 = ((t617) > (t618));
  }
  t619 = t616;
  if (t619) {
  {
  t620 = ((uint64_t)(v21));
  t621 = ((uint64_t)(v22));
  v31 = fg167h_gg_function_13(t620, t621, base + 0);
  t622 = ((uint64_t)(v31));
  t623 = ((int64_t)(v2));
  t624 = ((int64_t)(((int64_t)UINT64_C(1))));
  t625 = ((int64_t)(((int64_t)UINT64_C(2))));
  t626 = fg167_hget(t623, t624, t625);
  t627 = ((uint64_t)(t626));
  v32 = fg167h_gg_function_13(t622, t627, base + 0);
  t628 = ((uint64_t)(v32));
  t629 = ((int64_t)(v2));
  t630 = ((int64_t)(((int64_t)UINT64_C(1))));
  t631 = ((int64_t)(((int64_t)UINT64_C(3))));
  t632 = fg167_hget(t629, t630, t631);
  t633 = ((uint64_t)(t632));
  v33 = fg167h_gg_function_13(t628, t633, base + 0);
  t634 = ((uint64_t)(v33));
  t635 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(128))))));
  v34 = fg167h_gg_function_15(t634, t635, base + 0);
  t636 = ((uint64_t)(v31));
  t637 = ((uint64_t)(((int64_t)UINT64_C(4294967295))));
  t638 = ((t636) <= (t637));
  t639 = t638;
  if (t639) {
  t640 = ((uint64_t)(v32));
  t641 = ((uint64_t)(((int64_t)UINT64_C(4294967295))));
  t639 = ((t640) <= (t641));
  }
  t642 = t639;
  t643 = t642;
  if (t643) {
  t644 = ((uint64_t)(v34));
  t645 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(128))))));
  t646 = fg167h_gg_function_13(t644, t645, base + 0);
  t647 = ((uint64_t)(t646));
  t648 = ((uint64_t)(((int64_t)UINT64_C(4294967295))));
  t643 = ((t647) <= (t648));
  }
  t649 = t643;
  t650 = ((int64_t)(((int64_t)UINT64_C(23))));
  fg167h_gg_function_4(t649, t650, base + 0);
  t651 = ((int64_t)(v2));
  t652 = ((int64_t)(((int64_t)UINT64_C(15))));
  t653 = ((int64_t)(((int64_t)UINT64_C(0))));
  t654 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  fg167_hput(t651, t652, t653, t654);
  t655 = ((int64_t)(v2));
  t656 = ((int64_t)(((int64_t)UINT64_C(9))));
  t657 = ((int64_t)(((int64_t)UINT64_C(0))));
  t658 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(128))))));
  fg167_hput(t655, t656, t657, t658);
  t659 = ((int64_t)(v2));
  t660 = ((int64_t)(((int64_t)UINT64_C(9))));
  t661 = ((int64_t)(((int64_t)UINT64_C(1))));
  t662 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  fg167_hput(t659, t660, t661, t662);
  t663 = ((int64_t)(v2));
  t664 = ((int64_t)(((int64_t)UINT64_C(9))));
  t665 = ((int64_t)(((int64_t)UINT64_C(2))));
  t666 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  fg167_hput(t663, t664, t665, t666);
  t667 = ((int64_t)(v2));
  t668 = ((int64_t)(((int64_t)UINT64_C(8))));
  t669 = ((int64_t)(((int64_t)UINT64_C(0))));
  t670 = ((uint64_t)(v34));
  fg167_hput(t667, t668, t669, t670);
  t671 = ((int64_t)(v2));
  t672 = ((int64_t)(((int64_t)UINT64_C(8))));
  t673 = ((int64_t)(((int64_t)UINT64_C(1))));
  t674 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  fg167_hput(t671, t672, t673, t674);
  t675 = ((int64_t)(v2));
  t676 = ((int64_t)(((int64_t)UINT64_C(8))));
  t677 = ((int64_t)(((int64_t)UINT64_C(2))));
  t678 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  fg167_hput(t675, t676, t677, t678);
  t679 = ((int64_t)(v2));
  t680 = ((int64_t)(((int64_t)UINT64_C(5))));
  t681 = ((uint64_t)(v21));
  fg167h_gg_function_20(t679, t680, t681, base + 0);
  t682 = ((int64_t)(v2));
  t683 = ((int64_t)(((int64_t)UINT64_C(6))));
  t684 = ((uint64_t)(v31));
  fg167h_gg_function_20(t682, t683, t684, base + 0);
  t685 = ((int64_t)(v2));
  t686 = ((int64_t)(((int64_t)UINT64_C(7))));
  t687 = ((uint64_t)(v32));
  fg167h_gg_function_20(t685, t686, t687, base + 0);
  }
  } else {
  {
  t688 = ((uint64_t)(v28));
  t689 = ((uint64_t)(v25));
  t690 = fg167h_gg_function_13(t688, t689, base + 0);
  t691 = ((uint64_t)(t690));
  t692 = ((uint64_t)(((int64_t)UINT64_C(4294967295))));
  t693 = ((t691) <= (t692));
  t694 = t693;
  if (t694) {
  t695 = ((uint64_t)(v29));
  t696 = ((uint64_t)(v26));
  t697 = fg167h_gg_function_13(t695, t696, base + 0);
  t698 = ((uint64_t)(t697));
  t699 = ((uint64_t)(((int64_t)UINT64_C(4294967295))));
  t694 = ((t698) <= (t699));
  }
  t700 = t694;
  t701 = t700;
  if (t701) {
  t702 = ((uint64_t)(v30));
  t703 = ((uint64_t)(v27));
  t704 = fg167h_gg_function_13(t702, t703, base + 0);
  t705 = ((uint64_t)(t704));
  t706 = ((uint64_t)(((int64_t)UINT64_C(4294967295))));
  t701 = ((t705) <= (t706));
  }
  t707 = t701;
  t708 = ((int64_t)(((int64_t)UINT64_C(24))));
  fg167h_gg_function_4(t707, t708, base + 0);
  }
  }
  t709 = ((int64_t)(v0));
  fg167_launch(t709);
  }
fg167h_gg_done:;
}
static void fg167h_gg_function_0(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg167h_gg_function_1(t0, base + 0);
  }
fg167h_gg_done:;
}
static int fg167h_gg_program(int argc, char **argv) {
  struct { int64_t base; } frame;
  frame.base = 0;
  fg167h_gg_argc = argc; fg167h_gg_argv = argv;
  fg167h_gg_function_0(frame.base + 0);
  return 0;
}
int main(int argc, char **argv) {
#ifdef _MSC_VER
  __try { return fg167h_gg_program(argc, argv); }
  __except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    _resetstkoflw(); fg167h_gg_fail("native call stack overflow"); return 1;
  }
#else
  return fg167h_gg_program(argc, argv);
#endif
}
