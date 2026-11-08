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
typedef struct { void *data; int64_t len; } fg163_gg_slice;
typedef struct { unsigned char *base; int64_t len; } fg163_gg_header;
typedef struct { unsigned char *base; size_t top, committed; } fg163_gg_stack;
typedef struct { fg163_gg_header *header; fg163_gg_stack *stack; } fg163_gg_reference;
#define GG_RESERVATION ((size_t)268435456)
#define GG_STACK_COUNT 8192
static fg163_gg_stack fg163_gg_stacks[GG_STACK_COUNT];
static int fg163_gg_argc;
static char **fg163_gg_argv;
static void fg163_gg_fail(const char *message) {
    fprintf(stderr, "goose runtime error: %s\n", message); exit(1);
}
static fg163_gg_stack *fg163_gg_get_stack(int64_t id) {
    fg163_gg_stack *s;
    if (id < 0 || id >= GG_STACK_COUNT) fg163_gg_fail("data stack limit exceeded");
    s = &fg163_gg_stacks[id];
    if (!s->base) {
#ifdef _WIN32
        s->base = VirtualAlloc(NULL, GG_RESERVATION + 65536, MEM_RESERVE, PAGE_NOACCESS);
#else
        s->base = mmap(NULL, GG_RESERVATION + 65536, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (s->base == MAP_FAILED) s->base = NULL;
#endif
        if (!s->base) fg163_gg_fail("cannot reserve data stack");
    }
    return s;
}
static void *fg163_gg_allocate(fg163_gg_stack *s, size_t size) {
    size_t end, committed;
    void *result;
    if (size > GG_RESERVATION - s->top) fg163_gg_fail("data stack reservation exceeded");
    result = s->base + s->top; end = s->top + size;
    if (end > s->committed) {
        committed = (end + 65535) & ~(size_t)65535;
#ifdef _WIN32
        if (!VirtualAlloc(s->base + s->committed, committed - s->committed, MEM_COMMIT, PAGE_READWRITE)) fg163_gg_fail("cannot commit data stack");
#else
        if (mprotect(s->base + s->committed, committed - s->committed, PROT_READ | PROT_WRITE)) fg163_gg_fail("cannot commit data stack");
#endif
        s->committed = committed;
    }
    s->top = end; return result;
}
static fg163_gg_reference fg163_gg_ref(fg163_gg_header *h, fg163_gg_stack *s) { fg163_gg_reference r; r.header=h; r.stack=s; return r; }
static fg163_gg_slice fg163_gg_view(void *p, int64_t n) { fg163_gg_slice v; v.data=p; v.len=n; return v; }
static fg163_gg_slice fg163_gg_byte_view(fg163_gg_slice a, size_t width) {
    if (a.len < 0 || (width && (uint64_t)a.len > (uint64_t)INT64_MAX / width)) fg163_gg_fail("byte view size overflow");
    return fg163_gg_view(a.data, (int64_t)((uint64_t)a.len * width));
}
static fg163_gg_slice fg163_gg_sub(fg163_gg_slice a, int64_t lo, int64_t hi, size_t element) {
    if (lo < 0 || hi < lo || hi > a.len) fg163_gg_fail("slice out of bounds");
    return fg163_gg_view((unsigned char *)a.data + (size_t)lo * element, hi - lo);
}
static int64_t fg163_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg163_gg_fail("array index out of bounds"); return i; }
static void fg163_gg_append(fg163_gg_reference a, const void *p, int64_t n, size_t element) {
    void *to;
    if (n < 0 || (uint64_t)n > GG_RESERVATION / (element ? element : 1)) fg163_gg_fail("invalid array size");
    to=fg163_gg_allocate(a.stack, (size_t)n * element);
    if (n && element) memmove(to, p, (size_t)n * element);
    a.header->len += n;
}
static void fg163_gg_resize(fg163_gg_reference a, int64_t n, size_t element, const void *fill) {
    int64_t old=a.header->len, i;
    if (n < 0) fg163_gg_fail("negative array length");
    if (n > old) {
        unsigned char *p;
        if (!fill) fg163_gg_fail("resize without fill may only shrink");
        if ((uint64_t)(n-old) > GG_RESERVATION / (element ? element : 1)) fg163_gg_fail("invalid array size");
        p=fg163_gg_allocate(a.stack, (size_t)(n-old)*element);
        for (i=0; i<n-old; i++) if(element) memcpy(p+(size_t)i*element, fill, element);
    } else { a.stack->top = (size_t)(a.header->base-a.stack->base) + (size_t)n*element; }
    a.header->len=n;
}
static int64_t fg163_gg_div(int64_t a, int64_t b) { if (!b) fg163_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg163_gg_fail("division overflow"); return a/b; }
static int64_t fg163_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg163_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static uint64_t fg163_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg163_gg_fail("division by zero");return a/b; }
static uint64_t fg163_gg_umod(uint64_t a,uint64_t b) { if(!b)fg163_gg_fail("division by zero");return a%b; }
static int fg163_gg_equal(fg163_gg_slice a, fg163_gg_slice b, size_t element) { return a.len==b.len && (!a.len || !memcmp(a.data,b.data,(size_t)a.len*element)); }
static void fg163_gg_text(fg163_gg_reference out, fg163_gg_slice v) { fg163_gg_append(out,v.data,v.len,1); }
static void fg163_gg_integer(fg163_gg_reference out,int64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%lld",(long long)n);fg163_gg_append(out,b,k,1); }
static void fg163_gg_unsigned(fg163_gg_reference out,uint64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%llu",(unsigned long long)n);fg163_gg_append(out,b,k,1); }
static double fg163_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static int64_t fg163_ggf_fmt_f64(uint8_t *out,double n);
static int64_t fg163_ggf_fmt_f32(uint8_t *out,float n);
static void fg163_gg_float(fg163_gg_reference out,double n) { uint8_t b[64];int64_t k=fg163_ggf_fmt_f64(b,n);fg163_gg_append(out,b,k,1); }
static void fg163_gg_float32(fg163_gg_reference out,float n) { uint8_t b[64];int64_t k=fg163_ggf_fmt_f32(b,n);fg163_gg_append(out,b,k,1); }
static void fg163_gg_bool(fg163_gg_reference out,uint8_t n) { fg163_gg_append(out,n?"true":"false",n?4:5,1); }
static void fg163_gg_write(FILE *file,fg163_gg_slice text) { if(text.len && fwrite(text.data,1,(size_t)text.len,file)!=(size_t)text.len)fg163_gg_fail("write failed"); }
static char *fg163_gg_cstring(fg163_gg_slice path) {
    char *s=malloc((size_t)path.len+1); if(!s)fg163_gg_fail("out of memory in file shim");
    if(path.len)memcpy(s,path.data,(size_t)path.len);s[path.len]=0;return s;
}
static uint8_t fg163_gs_os_read_file(fg163_gg_slice path,fg163_gg_reference out) {
    char *p=fg163_gg_cstring(path);FILE *f=fopen(p,"rb");unsigned char b[16384];size_t n;int ok;free(p);
    if(!f)return 0;while((n=fread(b,1,sizeof b,f))!=0)fg163_gg_append(out,b,(int64_t)n,1);ok=!ferror(f);fclose(f);return (uint8_t)ok;
}
static uint8_t fg163_gs_os_write_file(fg163_gg_slice path,fg163_gg_slice data) {
    char *p=fg163_gg_cstring(path);FILE *f=fopen(p,"wb");int ok;free(p);if(!f)return 0;
    ok=!data.len || fwrite(data.data,1,(size_t)data.len,f)==(size_t)data.len;if(fclose(f))ok=0;return (uint8_t)ok;
}
static uint8_t fg163_gs_os_file_exists(fg163_gg_slice path) { char*p=fg163_gg_cstring(path);FILE*f=fopen(p,"rb");free(p);if(!f)return 0;fclose(f);return 1; }
static void fg163_gs_os_write_stdout(fg163_gg_slice s) { fg163_gg_write(stdout,s); }
static void fg163_gs_os_write_stderr(fg163_gg_slice s) { fg163_gg_write(stderr,s); }
static void fg163_gs_os_flush_stdout(void) { fflush(stdout); }
static int64_t fg163_gs_os_arg_count(void) { return fg163_gg_argc; }
static void fg163_gs_os_arg(int64_t i,fg163_gg_reference out) { if(i>=0 && i<fg163_gg_argc)fg163_gg_append(out,fg163_gg_argv[i],(int64_t)strlen(fg163_gg_argv[i]),1); }
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
typedef struct { int n; uint32_t d[36]; } fg163_ggf_big;

/* a = v << sh, for v > 0. */
static void fg163_ggf_big_set(fg163_ggf_big *a, uint64_t v, int sh) {
    int w = sh >> 5, b = sh & 31, i;
    uint64_t lo = v << b, hi = b ? v >> (64 - b) : 0;
    for (i = 0; i < w; i++) a->d[i] = 0;
    a->d[w] = (uint32_t)lo;
    a->d[w + 1] = (uint32_t)(lo >> 32);
    a->d[w + 2] = (uint32_t)hi;
    a->n = w + 3;
    while (!a->d[a->n - 1]) a->n--;
}

static void fg163_ggf_big_mul(fg163_ggf_big *a, uint32_t m) {
    uint64_t c = 0;
    for (int i = 0; i < a->n; i++) {
        c += (uint64_t)a->d[i] * m;
        a->d[i] = (uint32_t)c;
        c >>= 32;
    }
    if (c) a->d[a->n++] = (uint32_t)c;
}

static void fg163_ggf_big_mul_pow10(fg163_ggf_big *a, int k) {
    for (; k >= 9; k -= 9) fg163_ggf_big_mul(a, 1000000000u);
    for (; k > 0; k--) fg163_ggf_big_mul(a, 10);
}

static int fg163_ggf_big_cmp(const fg163_ggf_big *a, const fg163_ggf_big *b) {
    if (a->n != b->n) return a->n < b->n ? -1 : 1;
    for (int i = a->n - 1; i >= 0; i--)
        if (a->d[i] != b->d[i]) return a->d[i] < b->d[i] ? -1 : 1;
    return 0;
}

/* t = a + b */
static void fg163_ggf_big_add(fg163_ggf_big *t, const fg163_ggf_big *a, const fg163_ggf_big *b) {
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
static void fg163_ggf_big_sub(fg163_ggf_big *a, const fg163_ggf_big *b) {
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
static int fg163_ggf_float_digits(uint64_t f, int e, int bits, int emin, char *dig, int *k) {
    fg163_ggf_big r, s, mp, mlo, t;
    /* Reading rounds a tie to the even significand, so the interval that
       reads back as f * 2^e includes its ends when f is even. */
    int even = !(f & 1);
    /* Below a power of two, the gap to the next float down is half the gap up. */
    int asym = f == (uint64_t)1 << (bits - 1) && e > emin;
    int up = e > 0 ? e : 0, lg = e - 1, n = 0, c;
    /* The value is r / s, and what reads back as it reaches from (r - *mm) / s
       to (r + mp) / s: half the gap to each neighbour. */
    fg163_ggf_big *mm = asym ? &mlo : &mp;
    fg163_ggf_big_set(&r, f, up + 1 + asym);
    fg163_ggf_big_set(&s, 1, up - e + 1 + asym);
    fg163_ggf_big_set(&mp, 1, up + asym);
    fg163_ggf_big_set(&mlo, 1, up);
    /* Scale to 10^*k, the least power of ten above the interval: with
       lg = floor(log2 value), ceil(lg log10 2) is it or one short, and
       78913 / 2^18 is log10 2 closely enough to compute that for |lg| < 1650. */
    for (uint64_t g = f; g; g >>= 1) lg++;
    *k = lg > 0 ? ((lg * 78913) >> 18) + 1 : -((-lg * 78913) >> 18);
    if (*k >= 0) fg163_ggf_big_mul_pow10(&s, *k);
    else {
        fg163_ggf_big_mul_pow10(&r, -*k);
        fg163_ggf_big_mul_pow10(&mp, -*k);
        if (asym) fg163_ggf_big_mul_pow10(&mlo, -*k);
    }
    fg163_ggf_big_add(&t, &r, &mp);
    c = fg163_ggf_big_cmp(&t, &s);
    if (even ? c >= 0 : c > 0) {
        fg163_ggf_big_mul(&s, 10);
        ++*k;
    }
    /* Each digit d leaves the remainder r: stop where the digits so far
       followed by d (r within *mm) or by d + 1 (r + mp past s) read back. */
    for (;;) {
        int d = 0, low, high;
        fg163_ggf_big_mul(&r, 10);
        fg163_ggf_big_mul(&mp, 10);
        if (asym) fg163_ggf_big_mul(&mlo, 10);
        while (fg163_ggf_big_cmp(&r, &s) >= 0) {
            fg163_ggf_big_sub(&r, &s);
            d++;
        }
        c = fg163_ggf_big_cmp(&r, mm);
        low = even ? c <= 0 : c < 0;
        fg163_ggf_big_add(&t, &r, &mp);
        c = fg163_ggf_big_cmp(&t, &s);
        high = even ? c >= 0 : c > 0;
        if (!low && !high) {
            dig[n++] = (char)('0' + d);
            continue;
        }
        if (low && high) {
            /* Both read back: the nearer, d + 1 when r is past half of s,
               and of two as near, the even one. */
            fg163_ggf_big_add(&t, &r, &r);
            c = fg163_ggf_big_cmp(&t, &s);
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
static int64_t fg163_ggf_fmt_float(uint8_t *dst, int neg, uint64_t frac, int bexp, int bits,
                            int emin) {
    char dig[20], *p = (char *)dst;
    int k, n, x, i;
    if (neg) *p++ = '-';
    if (!frac && !bexp) {
        memcpy(p, "0.0", 3);
        return p + 3 - (char *)dst;
    }
    n = fg163_ggf_float_digits(bexp ? frac | (uint64_t)1 << (bits - 1) : frac,
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

static int64_t fg163_ggf_fmt_f64(uint8_t *dst, double v) {
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
    return fg163_ggf_fmt_float(dst, (int)(b >> 63), b & (((uint64_t)1 << 52) - 1),
                        (int)(b >> 52) & 0x7FF, 53, -1074);
}

/* The digits are the f32's own, laid out as an f64's are, so 0.1 as an f32
   prints as 0.1, not as the digits of the f64 it widens to. */
static int64_t fg163_ggf_fmt_f32(uint8_t *dst, float v) {
    double d = v;
    uint32_t b;
    if (d != d || isinf(d)) return fg163_ggf_fmt_f64(dst, d);
    memcpy(&b, &v, sizeof b);
    return fg163_ggf_fmt_float(dst, (int)(b >> 31), b & 0x7FFFFF, (int)(b >> 23) & 0xFF, 24, -149);
}


static void fg163_gg_quote(fg163_gg_reference out,fg163_gg_slice text) { int64_t i; unsigned char c; fg163_gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){fg163_gg_append(out,"\\",1,1);fg163_gg_append(out,&c,1,1);} else if(c=='\n')fg163_gg_append(out,"\\n",2,1);else if(c=='\r')fg163_gg_append(out,"\\r",2,1);else if(c=='\t')fg163_gg_append(out,"\\t",2,1);else fg163_gg_append(out,&c,1,1);}fg163_gg_append(out,"\"",1,1);}
static uint8_t fg163_gg_eq_1(int64_t a, int64_t b);
static uint8_t fg163_gg_equal_elements_1(fg163_gg_slice a, fg163_gg_slice b);
static void fg163_gg_render_1(fg163_gg_reference out, int64_t value);
static uint8_t fg163_gg_eq_2(uint64_t a, uint64_t b);
static uint8_t fg163_gg_equal_elements_2(fg163_gg_slice a, fg163_gg_slice b);
static void fg163_gg_render_2(fg163_gg_reference out, uint64_t value);
static uint8_t fg163_gg_eq_4(uint8_t a, uint8_t b);
static uint8_t fg163_gg_equal_elements_4(fg163_gg_slice a, fg163_gg_slice b);
static void fg163_gg_render_4(fg163_gg_reference out, uint8_t value);
static uint8_t fg163_gg_eq_5(uint8_t a, uint8_t b);
static uint8_t fg163_gg_equal_elements_5(fg163_gg_slice a, fg163_gg_slice b);
static void fg163_gg_render_5(fg163_gg_reference out, uint8_t value);
static uint8_t fg163_gg_equal_elements_1(fg163_gg_slice a,fg163_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg163_gg_eq_1(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg163_gg_eq_1(int64_t a, int64_t b) {
return a == b;
}
static void fg163_gg_render_1(fg163_gg_reference out, int64_t value) {
fg163_gg_integer(out,value);
}
static uint8_t fg163_gg_equal_elements_2(fg163_gg_slice a,fg163_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg163_gg_eq_2(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg163_gg_eq_2(uint64_t a, uint64_t b) {
return a == b;
}
static void fg163_gg_render_2(fg163_gg_reference out, uint64_t value) {
fg163_gg_unsigned(out,value);
}
static uint8_t fg163_gg_equal_elements_4(fg163_gg_slice a,fg163_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg163_gg_eq_4(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg163_gg_eq_4(uint8_t a, uint8_t b) {
return a == b;
}
static void fg163_gg_render_4(fg163_gg_reference out, uint8_t value) {
fg163_gg_bool(out,value);
}
static uint8_t fg163_gg_equal_elements_5(fg163_gg_slice a,fg163_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg163_gg_eq_5(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg163_gg_eq_5(uint8_t a, uint8_t b) {
return a == b;
}
static void fg163_gg_render_5(fg163_gg_reference out, uint8_t value) {
fg163_gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} fg163_gg_globals;
static void fg163_gg_function_0(int64_t base);
static uint64_t fg163_gg_function_1(int64_t p0, int64_t base);
static int64_t fg163_gg_function_3(uint64_t p0, int64_t base);
static void fg163_gg_function_7(uint8_t p0, int64_t p1, int64_t base);
static uint64_t fg163_gg_function_10(int64_t p0, int64_t p1, int64_t p2, uint64_t p3, uint64_t p4, int64_t base);
static void fg163_gg_function_13(int64_t p0, int64_t p1, int64_t p2, uint64_t p3, uint64_t p4, uint8_t p5, int64_t base);
static uint64_t fg163_gg_function_14(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static void fg163_gg_function_15(int64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int64_t base);
static uint64_t fg163_gg_function_16(uint64_t p0, uint64_t p1, int64_t base);
static uint64_t fg163_gg_function_17(int64_t base);
static uint64_t fg163_gg_function_18(uint64_t p0, uint64_t p1, int64_t base);
static void fg163_gg_function_20(int64_t p0, int64_t base);
static void fg163_gg_function_21(int64_t p0, int64_t p1, uint64_t p2, uint64_t p3, uint8_t p4, int64_t p5, int64_t base);
static int64_t fg163_gg_function_22(int64_t p0, int64_t p1, uint64_t p2, uint64_t p3, uint8_t p4, int64_t base);
static int64_t fg163_gg_function_23(int64_t p0, int64_t base);
static void fg163_gg_function_25(int64_t p0, int64_t p1, int64_t base);
static void fg163_gg_function_27(int64_t p0, int64_t p1, int64_t p2, uint64_t p3, uint64_t p4, uint64_t p5, uint64_t p6, uint64_t p7, uint8_t p8, uint8_t p9, int64_t base);
static void fg163_gg_function_28(int64_t p0, int64_t p1, int64_t base);
static uint8_t fg163_gg_function_29(int64_t p0, int64_t p1, int64_t base);
static void fg163_gg_function_34(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static void fg163_gg_function_7(uint8_t p0, int64_t p1, int64_t base) {
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
  fg163_fail(t2);
  }
  }
  }
fg163_gg_done:;
}
static uint64_t fg163_gg_function_10(int64_t p0, int64_t p1, int64_t p2, uint64_t p3, uint64_t p4, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint64_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  int64_t t18;
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
  uint64_t t29;
  uint64_t t30;
  uint64_t t31;
  uint8_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  uint64_t t36;
  uint64_t t37;
  uint64_t t38;
  uint8_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
  int64_t t44;
  int64_t t45;
  uint8_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  uint64_t t50;
  uint64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  uint64_t t55;
  uint64_t t56;
  uint64_t t57;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  uint64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(12))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) == (t5));
  t7 = t6;
  if (!t7) {
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(((int64_t)UINT64_C(2))));
  t10 = ((int64_t)(((int64_t)UINT64_C(12))));
  t11 = fg163_callback(t8, t9, t10);
  t12 = ((uint64_t)(t11));
  t13 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t12) == (t13));
  }
  t14 = t7;
  if (t14) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg163_gg_done;
  }
  }
  }
  } else {
  {
  t15 = ((int64_t)(v1));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t15) != (t16));
  t18 = ((int64_t)(((int64_t)UINT64_C(21))));
  fg163_gg_function_7(t17, t18, base + 0);
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(7))));
  t21 = ((t19) == (t20));
  t22 = t21;
  if (!t22) {
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(((int64_t)UINT64_C(11))));
  t22 = ((t23) == (t24));
  }
  t25 = t22;
  if (t25) {
  {
  t26 = ((int64_t)(v1));
  t27 = ((int64_t)(((int64_t)UINT64_C(2))));
  t28 = ((int64_t)(v0));
  t29 = fg163_callback(t26, t27, t28);
  t30 = ((uint64_t)(t29));
  t31 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t32 = ((t30) == (t31));
  if (t32) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg163_gg_done;
  }
  }
  }
  }
  t33 = ((int64_t)(v1));
  t34 = ((int64_t)(((int64_t)UINT64_C(2))));
  t35 = ((int64_t)(v0));
  t36 = fg163_callback(t33, t34, t35);
  t37 = ((uint64_t)(t36));
  t38 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t39 = ((t37) != (t38));
  t40 = ((int64_t)(((int64_t)UINT64_C(22))));
  fg163_gg_function_7(t39, t40, base + 0);
  }
  }
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(((int64_t)UINT64_C(4))));
  t43 = ((t41) == (t42));
  if (t43) {
  {
  t44 = ((int64_t)(v2));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = ((t44) != (t45));
  t47 = ((int64_t)(((int64_t)UINT64_C(23))));
  fg163_gg_function_7(t46, t47, base + 0);
  t48 = ((int64_t)(v2));
  t49 = ((int64_t)(((int64_t)UINT64_C(3))));
  t50 = fg163_constant(t49);
  t51 = ((uint64_t)(t50));
  fg163_zero(t48, t51);
  }
  }
  t52 = ((int64_t)(v0));
  t53 = ((int64_t)(v1));
  t54 = ((int64_t)(v2));
  t55 = ((uint64_t)(v3));
  t56 = ((uint64_t)(v4));
  t57 = fg163_device_call(t52, t53, t54, t55, t56);
  t0 = ((uint64_t)(t57));
  goto fg163_gg_done;
  }
fg163_gg_done:;
  return t0;
}
static int64_t fg163_gg_function_3(uint64_t p0, int64_t base) {
  int64_t t0;
  uint64_t v0;
  v0 = p0;
  {
  t0 = ((int64_t)(((int64_t)(v0))));
  goto fg163_gg_done;
  }
fg163_gg_done:;
  return t0;
}
static void fg163_gg_function_25(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint64_t t2;
  int64_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = fg163_constant(t1);
  t3 = ((int64_t)(((int64_t)(t2))));
  t4 = ((t0) != (t3));
  if (t4) {
  {
  t5 = ((int64_t)(v1));
  fg163_drop(t5);
  t6 = ((int64_t)(((int64_t)UINT64_C(5))));
  fg163_fail(t6);
  }
  }
  }
fg163_gg_done:;
}
static uint64_t fg163_gg_function_17(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((uint64_t)UINT64_C(18446744073709551615))))));
  goto fg163_gg_done;
  }
fg163_gg_done:;
  return t0;
}
static uint64_t fg163_gg_function_18(uint64_t p0, uint64_t p1, int64_t base) {
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
  t2 = fg163_gg_function_17(base + 0);
  t3 = ((uint64_t)(t2));
  t4 = ((uint64_t)(v1));
  t5 = ((uint64_t)(((uint64_t)((uint64_t)(t3) - (uint64_t)(t4)))));
  t6 = ((t1) <= (t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg163_gg_function_7(t6, t7, base + 0);
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  goto fg163_gg_done;
  }
fg163_gg_done:;
  return t0;
}
static uint64_t fg163_gg_function_16(uint64_t p0, uint64_t p1, int64_t base) {
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
  t6 = fg163_gg_function_17(base + 0);
  t7 = ((uint64_t)(t6));
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(((uint64_t)fg163_gg_udiv(t7, t8))));
  t4 = ((t5) <= (t9));
  }
  t10 = t4;
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
  fg163_gg_function_7(t10, t11, base + 0);
  t12 = ((uint64_t)(v0));
  t13 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  goto fg163_gg_done;
  }
fg163_gg_done:;
  return t0;
}
static void fg163_gg_function_15(int64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint8_t t2;
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint64_t t8;
  int64_t t9;
  uint64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint8_t t13;
  uint8_t t14;
  uint64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint64_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  uint64_t v5;
  uint64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t0 = ((uint64_t)(v3));
  t1 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  goto fg163_gg_done;
  }
  }
  t3 = ((uint64_t)(v1));
  t4 = ((uint64_t)(v3));
  t5 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t6 = ((uint64_t)(((uint64_t)((uint64_t)(t4) - (uint64_t)(t5)))));
  t7 = ((uint64_t)(v4));
  t8 = fg163_gg_function_16(t6, t7, base + 0);
  v5 = fg163_gg_function_18(t3, t8, base + 0);
  t9 = ((int64_t)(v0));
  t10 = fg163_nbytes(t9);
  v6 = ((uint64_t)(t10));
  t11 = ((uint64_t)(v5));
  t12 = ((uint64_t)(v6));
  t13 = ((t11) <= (t12));
  t14 = t13;
  if (t14) {
  t15 = ((uint64_t)(v2));
  t16 = ((uint64_t)(v6));
  t17 = ((uint64_t)(v5));
  t18 = ((uint64_t)(((uint64_t)((uint64_t)(t16) - (uint64_t)(t17)))));
  t14 = ((t15) <= (t18));
  }
  t19 = t14;
  t20 = ((int64_t)(((int64_t)UINT64_C(3))));
  fg163_gg_function_7(t19, t20, base + 0);
  }
fg163_gg_done:;
}
static uint64_t fg163_gg_function_14(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint64_t t5;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(v2));
  t5 = fg163_get(t1, t2, t3, t4);
  t0 = ((uint64_t)(t5));
  goto fg163_gg_done;
  }
fg163_gg_done:;
  return t0;
}
static int64_t fg163_gg_function_23(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(2))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg163_gg_function_14(t1, t2, t3, base + 0);
  v1 = fg163_gg_function_3(t4, base + 0);
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t6) != (t7));
  if (t8) {
  {
  t5 = ((int64_t)(v1));
  }
  } else {
  {
  t5 = ((int64_t)(v0));
  }
  }
  t9 = ((int64_t)(t5));
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fg163_gg_function_14(t9, t10, t11, base + 0);
  t0 = fg163_gg_function_3(t12, base + 0);
  goto fg163_gg_done;
  }
fg163_gg_done:;
  return t0;
}
static int64_t fg163_gg_function_22(int64_t p0, int64_t p1, uint64_t p2, uint64_t p3, uint8_t p4, int64_t base) {
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
  uint64_t t10;
  uint64_t t11;
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint64_t t18;
  uint8_t t19;
  uint8_t t20;
  int64_t t21;
  int64_t t22;
  uint8_t t23;
  int64_t t24;
  int64_t t25;
  uint64_t t26;
  uint64_t t27;
  uint64_t t28;
  uint64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  uint8_t t33;
  int64_t t34;
  uint64_t t35;
  uint64_t t36;
  uint64_t t37;
  uint8_t t38;
  int64_t t39;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint8_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(6))));
  fg163_gg_function_7(t3, t4, base + 0);
  t5 = ((int64_t)(v0));
  v5 = fg163_gg_function_23(t5, base + 0);
  t6 = ((int64_t)(v5));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t6) != (t7));
  t9 = ((int64_t)(((int64_t)UINT64_C(7))));
  fg163_gg_function_7(t8, t9, base + 0);
  t10 = ((uint64_t)(v3));
  t11 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t10) == (t11));
  if (t12) {
  {
  t0 = ((int64_t)(v5));
  goto fg163_gg_done;
  }
  }
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = fg163_gg_function_14(t13, t14, t15, base + 0);
  t17 = ((uint64_t)(t16));
  t18 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t17) != (t18));
  t20 = t19;
  if (t20) {
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((t21) != (t22));
  }
  t23 = t20;
  t24 = ((int64_t)(((int64_t)UINT64_C(8))));
  fg163_gg_function_7(t23, t24, base + 0);
  t25 = ((int64_t)(v0));
  t26 = ((uint64_t)(v2));
  t27 = ((uint64_t)(v3));
  t28 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t29 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg163_gg_function_15(t25, t26, t27, t28, t29, base + 0);
  t30 = ((int64_t)(v5));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = v4;
  if (t33) {
  {
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  t32 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t34 = ((int64_t)(t32));
  t35 = fg163_callback(t30, t31, t34);
  t36 = ((uint64_t)(t35));
  t37 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t38 = ((t36) != (t37));
  t39 = ((int64_t)(((int64_t)UINT64_C(9))));
  fg163_gg_function_7(t38, t39, base + 0);
  t0 = ((int64_t)(v5));
  goto fg163_gg_done;
  }
fg163_gg_done:;
  return t0;
}
static void fg163_gg_function_21(int64_t p0, int64_t p1, uint64_t p2, uint64_t p3, uint8_t p4, int64_t p5, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint64_t t2;
  uint64_t t3;
  uint8_t t4;
  uint64_t t5;
  uint64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  uint64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint64_t t18;
  uint64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint8_t v4;
  int64_t v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(v1));
  t2 = ((uint64_t)(v2));
  t3 = ((uint64_t)(v3));
  t4 = v4;
  v6 = fg163_gg_function_22(t0, t1, t2, t3, t4, base + 0);
  t5 = ((uint64_t)(v3));
  t6 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) == (t6));
  if (t7) {
  {
  goto fg163_gg_done;
  }
  }
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = v4;
  if (t10) {
  {
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t11 = ((int64_t)(t9));
  t12 = ((int64_t)(v6));
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(v1));
  t15 = ((uint64_t)(v2));
  t16 = ((uint64_t)(v3));
  t17 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t18 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t19 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t20 = fg163_io(t8, t11, t12, t13, t14, t15, t16, t17, t18, t19);
  t21 = ((int64_t)(t20));
  t22 = ((int64_t)(v5));
  fg163_gg_function_25(t21, t22, base + 0);
  }
fg163_gg_done:;
}
static uint8_t fg163_gg_function_29(int64_t p0, int64_t p1, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  uint64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  uint64_t t17;
  uint64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  uint64_t t22;
  uint64_t t23;
  uint8_t t24;
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  uint64_t t29;
  uint64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  uint64_t t34;
  uint64_t t35;
  uint8_t t36;
  int64_t t37;
  int64_t t38;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(3))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg163_gg_function_14(t1, t2, t3, base + 0);
  t5 = ((uint64_t)(t4));
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(3))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = fg163_gg_function_14(t6, t7, t8, base + 0);
  t10 = ((uint64_t)(t9));
  t11 = ((t5) != (t10));
  if (t11) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg163_gg_done;
  }
  }
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t12) < (t13)))) goto label2;
  {
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(4))));
  t16 = ((int64_t)(v2));
  t17 = fg163_gg_function_14(t14, t15, t16, base + 0);
  t18 = ((uint64_t)(t17));
  t19 = ((int64_t)(v1));
  t20 = ((int64_t)(((int64_t)UINT64_C(4))));
  t21 = ((int64_t)(v2));
  t22 = fg163_gg_function_14(t19, t20, t21, base + 0);
  t23 = ((uint64_t)(t22));
  t24 = ((t18) != (t23));
  t25 = t24;
  if (!t25) {
  t26 = ((int64_t)(v0));
  t27 = ((int64_t)(((int64_t)UINT64_C(5))));
  t28 = ((int64_t)(v2));
  t29 = fg163_gg_function_14(t26, t27, t28, base + 0);
  t30 = ((uint64_t)(t29));
  t31 = ((int64_t)(v1));
  t32 = ((int64_t)(((int64_t)UINT64_C(5))));
  t33 = ((int64_t)(v2));
  t34 = fg163_gg_function_14(t31, t32, t33, base + 0);
  t35 = ((uint64_t)(t34));
  t25 = ((t30) != (t35));
  }
  t36 = t25;
  if (t36) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg163_gg_done;
  }
  }
  t37 = v2;
  t38 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t37) + (uint64_t)(t38)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto fg163_gg_done;
  }
fg163_gg_done:;
  return t0;
}
static void fg163_gg_function_28(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint64_t t19;
  int64_t t20;
  int64_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  uint64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint8_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  uint64_t t35;
  int64_t t36;
  uint64_t t37;
  uint64_t t38;
  uint8_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  uint64_t t44;
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
  uint64_t t58;
  int64_t t59;
  uint64_t t60;
  uint64_t t61;
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
  uint64_t t74;
  int64_t t75;
  uint64_t t76;
  uint64_t t77;
  uint8_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  uint64_t t84;
  int64_t t85;
  uint64_t t86;
  uint64_t t87;
  uint8_t t88;
  int64_t t89;
  int64_t t90;
  uint64_t t91;
  uint64_t t92;
  uint8_t t93;
  uint64_t t94;
  int64_t t95;
  uint64_t t96;
  uint64_t t97;
  uint8_t t98;
  uint8_t t99;
  int64_t t100;
  int64_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  uint64_t t106;
  uint64_t t107;
  uint8_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  uint64_t t112;
  uint64_t t113;
  uint8_t t114;
  int64_t t115;
  int64_t t116;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) != (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) != (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(v1));
  t10 = fg163_gg_function_29(t8, t9, base + 0);
  t7 = t10;
  }
  t11 = t7;
  t12 = ((int64_t)(((int64_t)UINT64_C(16))));
  fg163_gg_function_7(t11, t12, base + 0);
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(v1));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  goto fg163_gg_done;
  }
  }
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(1))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = fg163_gg_function_14(t16, t17, t18, base + 0);
  v2 = fg163_gg_function_3(t19, base + 0);
  t20 = ((int64_t)(v2));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t20) != (t21));
  t23 = ((int64_t)(((int64_t)UINT64_C(17))));
  fg163_gg_function_7(t22, t23, base + 0);
  t24 = ((int64_t)(v0));
  t25 = fg163_nbytes(t24);
  v3 = ((uint64_t)(t25));
  t26 = ((int64_t)(v2));
  t27 = fg163_is_host(t26);
  t28 = ((int64_t)(t27));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = ((t28) != (t29));
  if (t30) {
  {
  t31 = ((int64_t)(v1));
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = fg163_gg_function_14(t32, t33, t34, base + 0);
  t36 = fg163_gg_function_3(t35, base + 0);
  t37 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t38 = ((uint64_t)(v3));
  t39 = ((uint8_t)UINT64_C(1));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg163_gg_function_21(t31, t36, t37, t38, t39, t40, base + 0);
  goto fg163_gg_done;
  }
  }
  t41 = ((int64_t)(v1));
  t42 = ((int64_t)(((int64_t)UINT64_C(1))));
  t43 = ((int64_t)(((int64_t)UINT64_C(0))));
  t44 = fg163_gg_function_14(t41, t42, t43, base + 0);
  v4 = fg163_gg_function_3(t44, base + 0);
  t45 = ((int64_t)(v4));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = ((t45) != (t46));
  t48 = ((int64_t)(((int64_t)UINT64_C(17))));
  fg163_gg_function_7(t47, t48, base + 0);
  t49 = ((int64_t)(v4));
  t50 = fg163_is_host(t49);
  t51 = ((int64_t)(t50));
  t52 = ((int64_t)(((int64_t)UINT64_C(0))));
  t53 = ((t51) != (t52));
  if (t53) {
  {
  t54 = ((int64_t)(v0));
  t55 = ((int64_t)(v1));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = fg163_gg_function_14(t55, t56, t57, base + 0);
  t59 = fg163_gg_function_3(t58, base + 0);
  t60 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t61 = ((uint64_t)(v3));
  t62 = ((uint8_t)UINT64_C(0));
  t63 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg163_gg_function_21(t54, t59, t60, t61, t62, t63, base + 0);
  goto fg163_gg_done;
  }
  }
  t64 = ((int64_t)(v0));
  t65 = ((int64_t)(v1));
  t66 = fg163_buffer_copy(t64, t65);
  t67 = ((int64_t)(t66));
  t68 = ((int64_t)(((int64_t)UINT64_C(0))));
  t69 = ((t67) != (t68));
  if (t69) {
  {
  goto fg163_gg_done;
  }
  }
  t70 = ((int64_t)(v0));
  t71 = ((int64_t)(v0));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  t73 = ((int64_t)(((int64_t)UINT64_C(0))));
  t74 = fg163_gg_function_14(t71, t72, t73, base + 0);
  t75 = fg163_gg_function_3(t74, base + 0);
  t76 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t77 = ((uint64_t)(v3));
  t78 = ((uint8_t)UINT64_C(0));
  t79 = fg163_gg_function_22(t70, t75, t76, t77, t78, base + 0);
  (void)(t79);
  t80 = ((int64_t)(v1));
  t81 = ((int64_t)(v1));
  t82 = ((int64_t)(((int64_t)UINT64_C(0))));
  t83 = ((int64_t)(((int64_t)UINT64_C(0))));
  t84 = fg163_gg_function_14(t81, t82, t83, base + 0);
  t85 = fg163_gg_function_3(t84, base + 0);
  t86 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t87 = ((uint64_t)(v3));
  t88 = ((uint8_t)UINT64_C(1));
  t89 = fg163_gg_function_22(t80, t85, t86, t87, t88, base + 0);
  (void)(t89);
  t90 = ((int64_t)(((int64_t)UINT64_C(50))));
  fg163_mark(t90);
  t91 = ((uint64_t)(v3));
  t92 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t93 = ((t91) != (t92));
  if (t93) {
  {
  t94 = ((uint64_t)(v3));
  t95 = fg163_heap(t94);
  v5 = ((int64_t)(t95));
  }
  } else {
  {
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t96 = ((uint64_t)(v3));
  t97 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t98 = ((t96) == (t97));
  t99 = t98;
  if (!t99) {
  t100 = ((int64_t)(v5));
  t101 = ((int64_t)(((int64_t)UINT64_C(0))));
  t99 = ((t100) != (t101));
  }
  t102 = t99;
  t103 = ((int64_t)(((int64_t)UINT64_C(18))));
  fg163_gg_function_7(t102, t103, base + 0);
  t104 = ((int64_t)(v0));
  t105 = ((int64_t)(v5));
  t106 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t107 = ((uint64_t)(v3));
  t108 = ((uint8_t)UINT64_C(0));
  t109 = ((int64_t)(v5));
  fg163_gg_function_21(t104, t105, t106, t107, t108, t109, base + 0);
  t110 = ((int64_t)(v1));
  t111 = ((int64_t)(v5));
  t112 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t113 = ((uint64_t)(v3));
  t114 = ((uint8_t)UINT64_C(1));
  t115 = ((int64_t)(v5));
  fg163_gg_function_21(t110, t111, t112, t113, t114, t115, base + 0);
  t116 = ((int64_t)(v5));
  fg163_drop(t116);
  }
fg163_gg_done:;
}
static void fg163_gg_function_20(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint64_t t16;
  int64_t v0;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) != (t1));
  t3 = ((int64_t)(((int64_t)UINT64_C(4))));
  fg163_gg_function_7(t2, t3, base + 0);
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  t6 = ((int64_t)(((int64_t)UINT64_C(7))));
  t7 = fg163_callback(t4, t5, t6);
  t8 = ((uint64_t)(t7));
  t9 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t8) != (t9));
  if (t10) {
  {
  t11 = ((int64_t)(((int64_t)UINT64_C(7))));
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = fg163_backend_call(t11, t12, t13, t14, t15);
  (void)(t16);
  }
  }
  }
fg163_gg_done:;
}
static void fg163_gg_function_34(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  int64_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint64_t t23;
  uint64_t t24;
  uint64_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  uint64_t t32;
  uint64_t t33;
  uint64_t t34;
  uint8_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) != (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) != (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(v3));
  t10 = fg163_gg_function_29(t8, t9, base + 0);
  t7 = t10;
  }
  t11 = t7;
  t12 = ((int64_t)(((int64_t)UINT64_C(19))));
  fg163_gg_function_7(t11, t12, base + 0);
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(v3));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  goto fg163_gg_done;
  }
  }
  t16 = ((int64_t)(v1));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) != (t17));
  t19 = ((int64_t)(((int64_t)UINT64_C(20))));
  fg163_gg_function_7(t18, t19, base + 0);
  t20 = ((int64_t)(v1));
  t21 = ((int64_t)(((int64_t)UINT64_C(1))));
  t22 = ((int64_t)(((int64_t)UINT64_C(6))));
  t23 = fg163_callback(t20, t21, t22);
  t24 = ((uint64_t)(t23));
  t25 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t26 = ((t24) != (t25));
  if (t26) {
  {
  t27 = ((int64_t)(((int64_t)UINT64_C(6))));
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(v2));
  t31 = ((int64_t)(v3));
  t32 = fg163_backend_call(t27, t28, t29, t30, t31);
  t33 = ((uint64_t)(t32));
  t34 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t35 = ((t33) != (t34));
  if (t35) {
  {
  goto fg163_gg_done;
  }
  }
  }
  }
  t36 = ((int64_t)(((int64_t)UINT64_C(51))));
  fg163_mark(t36);
  t37 = ((int64_t)(v0));
  fg163_gg_function_20(t37, base + 0);
  t38 = ((int64_t)(v1));
  fg163_gg_function_20(t38, base + 0);
  t39 = ((int64_t)(v2));
  t40 = ((int64_t)(v3));
  fg163_gg_function_28(t39, t40, base + 0);
  }
fg163_gg_done:;
}
static void fg163_gg_function_13(int64_t p0, int64_t p1, int64_t p2, uint64_t p3, uint64_t p4, uint8_t p5, int64_t base) {
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
  int64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint64_t t13;
  uint8_t t14;
  int64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint8_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  uint64_t t25;
  uint64_t t26;
  uint64_t t27;
  uint64_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  uint64_t t33;
  uint64_t t34;
  uint64_t t35;
  uint8_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint64_t t40;
  uint64_t t41;
  uint8_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  uint64_t t49;
  uint64_t t50;
  uint64_t t51;
  uint64_t t52;
  uint64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  uint64_t v4;
  uint8_t v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) != (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) != (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = fg163_gg_function_14(t8, t9, t10, base + 0);
  t12 = ((uint64_t)(t11));
  t13 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t12) != (t13));
  }
  t14 = t7;
  t15 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg163_gg_function_7(t14, t15, base + 0);
  t16 = ((uint64_t)(v4));
  t17 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) == (t17));
  t19 = t18;
  if (!t19) {
  t20 = ((int64_t)(v2));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t20) != (t21));
  }
  t22 = t19;
  t23 = ((int64_t)(((int64_t)UINT64_C(11))));
  fg163_gg_function_7(t22, t23, base + 0);
  t24 = ((int64_t)(v1));
  t25 = ((uint64_t)(v3));
  t26 = ((uint64_t)(v4));
  t27 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t28 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg163_gg_function_15(t24, t25, t26, t27, t28, base + 0);
  t29 = v5;
  if (t29) {
  {
  v6 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  } else {
  {
  v6 = ((int64_t)(((int64_t)UINT64_C(3))));
  }
  }
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  t32 = ((int64_t)(v6));
  t33 = fg163_callback(t30, t31, t32);
  t34 = ((uint64_t)(t33));
  t35 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t36 = ((t34) == (t35));
  if (t36) {
  {
  t37 = ((int64_t)(v0));
  fg163_gg_function_20(t37, base + 0);
  t38 = ((int64_t)(v1));
  t39 = ((int64_t)(v2));
  t40 = ((uint64_t)(v3));
  t41 = ((uint64_t)(v4));
  t42 = v5;
  t43 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg163_gg_function_21(t38, t39, t40, t41, t42, t43, base + 0);
  }
  } else {
  {
  t44 = ((int64_t)(((int64_t)UINT64_C(1))));
  t45 = ((int64_t)(v6));
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(v1));
  t48 = ((int64_t)(v2));
  t49 = ((uint64_t)(v3));
  t50 = ((uint64_t)(v4));
  t51 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t52 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t53 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t54 = fg163_io(t44, t45, t46, t47, t48, t49, t50, t51, t52, t53);
  t55 = ((int64_t)(t54));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg163_gg_function_25(t55, t56, base + 0);
  }
  }
  }
fg163_gg_done:;
}
static void fg163_gg_function_27(int64_t p0, int64_t p1, int64_t p2, uint64_t p3, uint64_t p4, uint64_t p5, uint64_t p6, uint64_t p7, uint8_t p8, uint8_t p9, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  uint8_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  uint8_t t20;
  int64_t t21;
  uint8_t t22;
  uint8_t t23;
  uint8_t t24;
  uint8_t t25;
  uint8_t t26;
  uint64_t t27;
  uint64_t t28;
  uint8_t t29;
  uint8_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  uint64_t t34;
  uint64_t t35;
  uint64_t t36;
  uint8_t t37;
  uint64_t t38;
  uint64_t t39;
  uint8_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
  uint8_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  uint64_t t48;
  uint64_t t49;
  uint64_t t50;
  uint8_t t51;
  int64_t t52;
  int64_t t53;
  uint64_t t54;
  uint64_t t55;
  uint64_t t56;
  uint64_t t57;
  uint64_t t58;
  uint64_t t59;
  uint64_t t60;
  uint64_t t61;
  uint64_t t62;
  uint64_t t63;
  uint64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  uint64_t t70;
  uint64_t t71;
  uint64_t t72;
  uint64_t t73;
  uint64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  uint64_t t78;
  uint64_t t79;
  uint8_t t80;
  uint8_t t81;
  uint8_t t82;
  uint8_t t83;
  uint64_t t84;
  uint64_t t85;
  uint8_t t86;
  uint64_t t87;
  uint64_t t88;
  uint8_t t89;
  uint8_t t90;
  uint8_t t91;
  int64_t t92;
  uint64_t t93;
  uint64_t t94;
  uint64_t t95;
  uint64_t t96;
  uint64_t t97;
  uint64_t t98;
  uint64_t t99;
  uint64_t t100;
  uint64_t t101;
  uint64_t t102;
  uint64_t t103;
  uint64_t t104;
  uint64_t t105;
  uint64_t t106;
  uint64_t t107;
  uint64_t t108;
  uint64_t t109;
  uint64_t t110;
  uint64_t t111;
  uint64_t t112;
  uint64_t t113;
  uint64_t t114;
  uint8_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  uint64_t t119;
  uint64_t t120;
  uint8_t t121;
  int64_t t122;
  int64_t t123;
  uint64_t t124;
  uint64_t t125;
  uint8_t t126;
  int64_t t127;
  uint64_t t128;
  uint64_t t129;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  uint64_t v4;
  uint64_t v5;
  uint64_t v6;
  uint64_t v7;
  uint8_t v8;
  uint8_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  uint64_t v14;
  int64_t v15;
  uint64_t v16;
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
  {
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) != (t1));
  t3 = ((int64_t)(((int64_t)UINT64_C(12))));
  fg163_gg_function_7(t2, t3, base + 0);
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = v9;
  if (t4) {
  {
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) != (t6));
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fg163_gg_function_14(t9, t10, t11, base + 0);
  t13 = ((uint64_t)(t12));
  t14 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t13) != (t14));
  }
  t15 = t8;
  t16 = ((int64_t)(((int64_t)UINT64_C(13))));
  fg163_gg_function_7(t15, t16, base + 0);
  }
  } else {
  {
  t17 = ((int64_t)(v1));
  v10 = fg163_gg_function_23(t17, base + 0);
  t18 = ((int64_t)(v10));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((t18) != (t19));
  t21 = ((int64_t)(((int64_t)UINT64_C(14))));
  fg163_gg_function_7(t20, t21, base + 0);
  }
  }
  t22 = v9;
  if (t22) {
  {
  t23 = v8;
  if (t23) {
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(4))));
  }
  } else {
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(5))));
  }
  }
  }
  } else {
  {
  t24 = v8;
  if (t24) {
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  } else {
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(3))));
  }
  }
  }
  }
  t25 = v9;
  if (t25) {
  {
  v12 = ((int64_t)(v0));
  }
  } else {
  {
  v12 = ((int64_t)(v10));
  }
  }
  t26 = v9;
  if (t26) {
  {
  v13 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v13 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t27 = ((uint64_t)(v5));
  t28 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t29 = ((t27) > (t28));
  t30 = t29;
  if (t30) {
  t31 = ((int64_t)(v12));
  t32 = ((int64_t)(v13));
  t33 = ((int64_t)(v11));
  t34 = fg163_callback(t31, t32, t33);
  t35 = ((uint64_t)(t34));
  t36 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t30 = ((t35) != (t36));
  }
  t37 = t30;
  if (t37) {
  {
  t38 = ((uint64_t)(v4));
  t39 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t40 = ((t38) == (t39));
  if (t40) {
  {
  goto fg163_gg_done;
  }
  }
  t41 = ((int64_t)(v2));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = ((t41) != (t42));
  t44 = t43;
  if (t44) {
  t45 = ((int64_t)(v1));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = fg163_gg_function_14(t45, t46, t47, base + 0);
  t49 = ((uint64_t)(t48));
  t50 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t44 = ((t49) != (t50));
  }
  t51 = t44;
  t52 = ((int64_t)(((int64_t)UINT64_C(15))));
  fg163_gg_function_7(t51, t52, base + 0);
  t53 = ((int64_t)(v1));
  t54 = ((uint64_t)(v3));
  t55 = ((uint64_t)(v4));
  t56 = ((uint64_t)(v5));
  t57 = ((uint64_t)(v6));
  fg163_gg_function_15(t53, t54, t55, t56, t57, base + 0);
  t58 = ((uint64_t)(((uint64_t)(v2))));
  t59 = ((uint64_t)(v5));
  t60 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t61 = ((uint64_t)(((uint64_t)((uint64_t)(t59) - (uint64_t)(t60)))));
  t62 = ((uint64_t)(v7));
  t63 = fg163_gg_function_16(t61, t62, base + 0);
  t64 = fg163_gg_function_18(t58, t63, base + 0);
  (void)(t64);
  t65 = ((int64_t)(v13));
  t66 = ((int64_t)(v11));
  t67 = ((int64_t)(v12));
  t68 = ((int64_t)(v1));
  t69 = ((int64_t)(v2));
  t70 = ((uint64_t)(v3));
  t71 = ((uint64_t)(v4));
  t72 = ((uint64_t)(v5));
  t73 = ((uint64_t)(v6));
  t74 = ((uint64_t)(v7));
  t75 = fg163_io(t65, t66, t67, t68, t69, t70, t71, t72, t73, t74);
  t76 = ((int64_t)(t75));
  t77 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg163_gg_function_25(t76, t77, base + 0);
  goto fg163_gg_done;
  }
  }
  t78 = ((uint64_t)(v5));
  t79 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t80 = ((t78) == (t79));
  if (t80) {
  {
  goto fg163_gg_done;
  }
  }
  t81 = v9;
  t82 = (!(t81));
  t83 = t82;
  if (t83) {
  t84 = ((uint64_t)(v4));
  t85 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t83 = ((t84) == (t85));
  }
  t86 = t83;
  if (t86) {
  {
  goto fg163_gg_done;
  }
  }
  t87 = ((uint64_t)(v4));
  t88 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t89 = ((t87) > (t88));
  t90 = t89;
  if (!t90) {
  t90 = v9;
  }
  t91 = t90;
  if (t91) {
  {
  t92 = ((int64_t)(v1));
  t93 = ((uint64_t)(v3));
  t94 = ((uint64_t)(v4));
  t95 = ((uint64_t)(v5));
  t96 = ((uint64_t)(v6));
  fg163_gg_function_15(t92, t93, t94, t95, t96, base + 0);
  t97 = ((uint64_t)(((uint64_t)(v2))));
  t98 = ((uint64_t)(v5));
  t99 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t100 = ((uint64_t)(((uint64_t)((uint64_t)(t98) - (uint64_t)(t99)))));
  t101 = ((uint64_t)(v7));
  t102 = fg163_gg_function_16(t100, t101, base + 0);
  t103 = fg163_gg_function_18(t97, t102, base + 0);
  (void)(t103);
  }
  }
  v14 = ((uint64_t)(((int64_t)UINT64_C(0))));
label1:;
  t104 = ((uint64_t)(v14));
  t105 = ((uint64_t)(v5));
  if (!(((t104) < (t105)))) goto label2;
  {
  t106 = ((uint64_t)(((uint64_t)(v2))));
  t107 = ((uint64_t)(v14));
  t108 = ((uint64_t)(v7));
  t109 = fg163_gg_function_16(t107, t108, base + 0);
  t110 = fg163_gg_function_18(t106, t109, base + 0);
  v15 = fg163_gg_function_3(t110, base + 0);
  t111 = ((uint64_t)(v3));
  t112 = ((uint64_t)(v14));
  t113 = ((uint64_t)(v6));
  t114 = fg163_gg_function_16(t112, t113, base + 0);
  v16 = fg163_gg_function_18(t111, t114, base + 0);
  t115 = v9;
  if (t115) {
  {
  t116 = ((int64_t)(v0));
  t117 = ((int64_t)(v1));
  t118 = ((int64_t)(v15));
  t119 = ((uint64_t)(v16));
  t120 = ((uint64_t)(v4));
  t121 = v8;
  fg163_gg_function_13(t116, t117, t118, t119, t120, t121, base + 0);
  }
  } else {
  {
  t122 = ((int64_t)(v1));
  t123 = ((int64_t)(v15));
  t124 = ((uint64_t)(v16));
  t125 = ((uint64_t)(v4));
  t126 = v8;
  t127 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg163_gg_function_21(t122, t123, t124, t125, t126, t127, base + 0);
  }
  }
  t128 = v14;
  t129 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v14 = ((uint64_t)((uint64_t)(t128) + (uint64_t)(t129)));
  }
  goto label1;
label2:;
  }
fg163_gg_done:;
}
static uint64_t fg163_gg_function_1(int64_t p0, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  uint64_t t3;
  uint64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  uint64_t t8;
  int64_t t9;
  int64_t t10;
  uint64_t t11;
  uint64_t t12;
  int64_t t13;
  int64_t t14;
  uint64_t t15;
  uint64_t t16;
  int64_t t17;
  int64_t t18;
  uint64_t t19;
  int64_t t20;
  int64_t t21;
  uint64_t t22;
  int64_t t23;
  int64_t t24;
  uint64_t t25;
  int64_t t26;
  int64_t t27;
  uint64_t t28;
  int64_t t29;
  int64_t t30;
  uint64_t t31;
  int64_t t32;
  int64_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  uint8_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  uint64_t t42;
  int64_t t43;
  int64_t t44;
  uint8_t t45;
  int64_t t46;
  int64_t t47;
  uint8_t t48;
  int64_t t49;
  uint64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  uint64_t t54;
  uint64_t t55;
  uint64_t t56;
  uint8_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  uint64_t t64;
  int64_t t65;
  int64_t t66;
  uint8_t t67;
  int64_t t68;
  int64_t t69;
  uint8_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  uint64_t t74;
  uint64_t t75;
  uint64_t t76;
  uint8_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  uint64_t t84;
  int64_t t85;
  int64_t t86;
  uint8_t t87;
  int64_t t88;
  int64_t t89;
  uint8_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  uint64_t t97;
  uint64_t t98;
  int64_t t99;
  int64_t t100;
  uint64_t t101;
  uint64_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  uint8_t t106;
  int64_t t107;
  int64_t t108;
  uint8_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  uint64_t t113;
  uint64_t t114;
  int64_t t115;
  int64_t t116;
  uint8_t t117;
  int64_t t118;
  int64_t t119;
  uint8_t t120;
  uint8_t t121;
  int64_t t122;
  int64_t t123;
  uint8_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  uint64_t t128;
  uint64_t t129;
  uint64_t t130;
  uint64_t t131;
  uint64_t t132;
  int64_t t133;
  int64_t t134;
  uint8_t t135;
  uint8_t t136;
  int64_t t137;
  int64_t t138;
  uint8_t t139;
  uint8_t t140;
  int64_t t141;
  int64_t t142;
  uint8_t t143;
  int64_t t144;
  int64_t t145;
  uint64_t t146;
  uint64_t t147;
  int64_t t148;
  int64_t t149;
  uint8_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  uint8_t t154;
  uint8_t t155;
  int64_t t156;
  int64_t t157;
  uint8_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  uint64_t t162;
  uint64_t t163;
  uint64_t t164;
  uint64_t t165;
  uint64_t t166;
  int64_t t167;
  int64_t t168;
  uint8_t t169;
  uint8_t t170;
  int64_t t171;
  int64_t t172;
  uint8_t t173;
  int64_t t174;
  int64_t t175;
  uint8_t t176;
  int64_t t177;
  int64_t t178;
  uint64_t t179;
  uint64_t t180;
  uint8_t t181;
  int64_t t182;
  int64_t t183;
  uint8_t t184;
  uint8_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  uint64_t t189;
  uint64_t t190;
  uint64_t t191;
  uint8_t t192;
  uint8_t t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  uint64_t t197;
  uint64_t t198;
  uint64_t t199;
  uint8_t t200;
  int64_t t201;
  int64_t t202;
  uint64_t t203;
  uint64_t t204;
  uint64_t t205;
  uint64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  uint64_t t212;
  uint64_t t213;
  uint64_t t214;
  uint64_t t215;
  uint64_t t216;
  int64_t t217;
  int64_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  uint8_t t222;
  int64_t t223;
  int64_t t224;
  int64_t t225;
  uint8_t t226;
  uint8_t t227;
  int64_t t228;
  int64_t t229;
  uint8_t t230;
  int64_t t231;
  int64_t t232;
  uint8_t t233;
  int64_t t234;
  int64_t t235;
  int64_t t236;
  uint8_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  uint64_t t241;
  uint64_t t242;
  uint64_t t243;
  uint8_t t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  uint64_t t251;
  int64_t t252;
  int64_t t253;
  int64_t t254;
  uint8_t t255;
  int64_t t256;
  int64_t t257;
  uint8_t t258;
  int64_t t259;
  int64_t t260;
  uint8_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  uint64_t t265;
  uint64_t t266;
  uint64_t t267;
  uint8_t t268;
  int64_t t269;
  int64_t t270;
  int64_t t271;
  int64_t t272;
  int64_t t273;
  int64_t t274;
  uint64_t t275;
  int64_t t276;
  int64_t t277;
  uint8_t t278;
  uint8_t t279;
  int64_t t280;
  int64_t t281;
  uint8_t t282;
  int64_t t283;
  int64_t t284;
  uint8_t t285;
  int64_t t286;
  int64_t t287;
  int64_t t288;
  int64_t t289;
  uint8_t t290;
  int64_t t291;
  int64_t t292;
  uint8_t t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  int64_t t297;
  int64_t t298;
  uint64_t t299;
  uint64_t t300;
  int64_t t301;
  int64_t t302;
  uint64_t t303;
  uint64_t t304;
  int64_t t305;
  int64_t t306;
  uint8_t t307;
  int64_t t308;
  int64_t t309;
  uint8_t t310;
  int64_t t311;
  int64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t t315;
  uint64_t t316;
  int64_t t317;
  int64_t t318;
  uint8_t t319;
  int64_t t320;
  int64_t t321;
  int64_t t322;
  int64_t t323;
  uint8_t t324;
  int64_t t325;
  int64_t t326;
  int64_t t327;
  uint64_t t328;
  int64_t t329;
  int64_t t330;
  int64_t t331;
  uint8_t t332;
  int64_t t333;
  int64_t t334;
  int64_t t335;
  uint64_t t336;
  uint64_t t337;
  int64_t t338;
  int64_t t339;
  uint8_t t340;
  int64_t t341;
  int64_t t342;
  uint8_t t343;
  int64_t t344;
  int64_t t345;
  int64_t t346;
  int64_t t347;
  int64_t t348;
  uint64_t t349;
  uint64_t t350;
  int64_t t351;
  int64_t t352;
  uint64_t t353;
  uint64_t t354;
  uint64_t t355;
  int64_t t356;
  int64_t t357;
  uint8_t t358;
  uint8_t t359;
  int64_t t360;
  int64_t t361;
  uint8_t t362;
  int64_t t363;
  int64_t t364;
  uint8_t t365;
  int64_t t366;
  int64_t t367;
  int64_t t368;
  uint8_t t369;
  int64_t t370;
  int64_t t371;
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
  uint64_t t383;
  int64_t t384;
  int64_t t385;
  uint8_t t386;
  int64_t t387;
  int64_t t388;
  uint8_t t389;
  int64_t t390;
  int64_t t391;
  int64_t t392;
  int64_t t393;
  int64_t t394;
  int64_t t395;
  uint64_t t396;
  uint64_t t397;
  int64_t t398;
  int64_t t399;
  uint64_t t400;
  uint64_t t401;
  uint64_t t402;
  int64_t t403;
  int64_t t404;
  uint8_t t405;
  int64_t t406;
  int64_t t407;
  uint8_t t408;
  int64_t t409;
  int64_t t410;
  int64_t t411;
  int64_t t412;
  uint64_t t413;
  uint64_t t414;
  uint64_t t415;
  uint8_t t416;
  int64_t t417;
  int64_t t418;
  int64_t t419;
  int64_t t420;
  int64_t t421;
  uint64_t t422;
  int64_t t423;
  int64_t t424;
  uint8_t t425;
  uint8_t t426;
  int64_t t427;
  int64_t t428;
  uint8_t t429;
  int64_t t430;
  int64_t t431;
  int64_t t432;
  int64_t t433;
  int64_t t434;
  uint64_t t435;
  uint64_t t436;
  int64_t t437;
  int64_t t438;
  uint8_t t439;
  int64_t t440;
  int64_t t441;
  uint8_t t442;
  int64_t t443;
  int64_t t444;
  int64_t t445;
  int64_t t446;
  int64_t t447;
  uint64_t t448;
  int64_t t449;
  int64_t t450;
  uint8_t t451;
  uint8_t t452;
  int64_t t453;
  int64_t t454;
  uint8_t t455;
  int64_t t456;
  int64_t t457;
  int64_t t458;
  int64_t t459;
  int64_t t460;
  uint64_t t461;
  uint64_t t462;
  int64_t t463;
  int64_t t464;
  uint8_t t465;
  uint8_t t466;
  int64_t t467;
  int64_t t468;
  uint8_t t469;
  int64_t t470;
  int64_t t471;
  uint8_t t472;
  int64_t t473;
  int64_t t474;
  int64_t t475;
  int64_t t476;
  int64_t t477;
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
  uint64_t t491;
  uint64_t t492;
  uint8_t t493;
  int64_t t494;
  int64_t t495;
  int64_t t496;
  int64_t t497;
  int64_t t498;
  uint64_t t499;
  uint64_t t500;
  uint64_t t501;
  int64_t t502;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  uint64_t v5;
  uint64_t v6;
  uint64_t v7;
  uint64_t v8;
  uint64_t v9;
  int64_t v10;
  uint64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg163_arg(t1, t2);
  t4 = ((uint64_t)(t3));
  v1 = fg163_gg_function_3(t4, base + 0);
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = fg163_arg(t5, t6);
  t8 = ((uint64_t)(t7));
  v2 = fg163_gg_function_3(t8, base + 0);
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = fg163_arg(t9, t10);
  t12 = ((uint64_t)(t11));
  v3 = fg163_gg_function_3(t12, base + 0);
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(3))));
  t15 = fg163_arg(t13, t14);
  t16 = ((uint64_t)(t15));
  v4 = fg163_gg_function_3(t16, base + 0);
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(4))));
  t19 = fg163_arg(t17, t18);
  v5 = ((uint64_t)(t19));
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(5))));
  t22 = fg163_arg(t20, t21);
  v6 = ((uint64_t)(t22));
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(((int64_t)UINT64_C(6))));
  t25 = fg163_arg(t23, t24);
  v7 = ((uint64_t)(t25));
  t26 = ((int64_t)(v0));
  t27 = ((int64_t)(((int64_t)UINT64_C(7))));
  t28 = fg163_arg(t26, t27);
  v8 = ((uint64_t)(t28));
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(8))));
  t31 = fg163_arg(t29, t30);
  v9 = ((uint64_t)(t31));
  t32 = ((int64_t)(v1));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = ((t32) == (t33));
  if (t34) {
  {
  t35 = ((int64_t)(v2));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = ((t35) != (t36));
  if (t37) {
  {
  t38 = ((int64_t)(v2));
  t39 = ((int64_t)(((int64_t)UINT64_C(1))));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = fg163_get(t38, t39, t40, t41);
  t0 = ((uint64_t)(t42));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  goto fg163_gg_done;
  }
  }
  t43 = ((int64_t)(v1));
  t44 = ((int64_t)(((int64_t)UINT64_C(1))));
  t45 = ((t43) == (t44));
  if (t45) {
  {
  t46 = ((int64_t)(v2));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = ((t46) == (t47));
  if (t48) {
  {
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = fg163_constant(t49);
  t0 = ((uint64_t)(t50));
  goto fg163_gg_done;
  }
  }
  t51 = ((int64_t)(v2));
  t52 = ((int64_t)(((int64_t)UINT64_C(1))));
  t53 = ((int64_t)(((int64_t)UINT64_C(0))));
  t54 = fg163_callback(t51, t52, t53);
  t55 = ((uint64_t)(t54));
  t56 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t57 = ((t55) != (t56));
  t58 = ((int64_t)(((int64_t)UINT64_C(24))));
  fg163_gg_function_7(t57, t58, base + 0);
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
  t60 = ((int64_t)(v2));
  t61 = ((int64_t)(((int64_t)UINT64_C(0))));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t63 = ((int64_t)(((int64_t)UINT64_C(0))));
  t64 = fg163_backend_call(t59, t60, t61, t62, t63);
  t0 = ((uint64_t)(t64));
  goto fg163_gg_done;
  }
  }
  t65 = ((int64_t)(v1));
  t66 = ((int64_t)(((int64_t)UINT64_C(2))));
  t67 = ((t65) == (t66));
  if (t67) {
  {
  t68 = ((int64_t)(v2));
  t69 = ((int64_t)(((int64_t)UINT64_C(0))));
  t70 = ((t68) == (t69));
  if (t70) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg163_gg_done;
  }
  }
  t71 = ((int64_t)(v2));
  t72 = ((int64_t)(((int64_t)UINT64_C(1))));
  t73 = ((int64_t)(((int64_t)UINT64_C(1))));
  t74 = fg163_callback(t71, t72, t73);
  t75 = ((uint64_t)(t74));
  t76 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t77 = ((t75) != (t76));
  t78 = ((int64_t)(((int64_t)UINT64_C(25))));
  fg163_gg_function_7(t77, t78, base + 0);
  t79 = ((int64_t)(((int64_t)UINT64_C(1))));
  t80 = ((int64_t)(v2));
  t81 = ((int64_t)(((int64_t)UINT64_C(0))));
  t82 = ((int64_t)(((int64_t)UINT64_C(0))));
  t83 = ((int64_t)(((int64_t)UINT64_C(0))));
  t84 = fg163_backend_call(t79, t80, t81, t82, t83);
  (void)(t84);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg163_gg_done;
  }
  }
  t85 = ((int64_t)(v1));
  t86 = ((int64_t)(((int64_t)UINT64_C(3))));
  t87 = ((t85) == (t86));
  if (t87) {
  {
  t88 = ((int64_t)(v2));
  t89 = ((int64_t)(((int64_t)UINT64_C(0))));
  t90 = ((t88) != (t89));
  t91 = ((int64_t)(((int64_t)UINT64_C(26))));
  fg163_gg_function_7(t90, t91, base + 0);
  t92 = ((int64_t)(((int64_t)UINT64_C(6))));
  t93 = ((int64_t)(v2));
  t94 = ((int64_t)(((int64_t)UINT64_C(1))));
  t95 = ((int64_t)(((int64_t)UINT64_C(1))));
  t96 = ((int64_t)(((int64_t)UINT64_C(0))));
  t97 = fg163_get(t93, t94, t95, t96);
  t98 = ((uint64_t)(t97));
  t99 = fg163_gg_function_3(t98, base + 0);
  t100 = ((int64_t)(((int64_t)UINT64_C(0))));
  t101 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t102 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t0 = fg163_gg_function_10(t92, t99, t100, t101, t102, base + 0);
  goto fg163_gg_done;
  }
  }
  t103 = ((int64_t)(v1));
  t104 = ((int64_t)(((int64_t)UINT64_C(4))));
  t105 = ((t103) == (t104));
  t106 = t105;
  if (!t106) {
  t107 = ((int64_t)(v1));
  t108 = ((int64_t)(((int64_t)UINT64_C(5))));
  t106 = ((t107) == (t108));
  }
  t109 = t106;
  if (t109) {
  {
  t110 = ((int64_t)(v2));
  t111 = ((int64_t)(v3));
  t112 = ((int64_t)(v4));
  t113 = ((uint64_t)(v5));
  t114 = ((uint64_t)(v6));
  t115 = ((int64_t)(v1));
  t116 = ((int64_t)(((int64_t)UINT64_C(4))));
  t117 = ((t115) == (t116));
  fg163_gg_function_13(t110, t111, t112, t113, t114, t117, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg163_gg_done;
  }
  }
  t118 = ((int64_t)(v1));
  t119 = ((int64_t)(((int64_t)UINT64_C(6))));
  t120 = ((t118) == (t119));
  t121 = t120;
  if (!t121) {
  t122 = ((int64_t)(v1));
  t123 = ((int64_t)(((int64_t)UINT64_C(7))));
  t121 = ((t122) == (t123));
  }
  t124 = t121;
  if (t124) {
  {
  t125 = ((int64_t)(v2));
  t126 = ((int64_t)(v3));
  t127 = ((int64_t)(v4));
  t128 = ((uint64_t)(v5));
  t129 = ((uint64_t)(v6));
  t130 = ((uint64_t)(v7));
  t131 = ((uint64_t)(v8));
  t132 = ((uint64_t)(v9));
  t133 = ((int64_t)(v1));
  t134 = ((int64_t)(((int64_t)UINT64_C(6))));
  t135 = ((t133) == (t134));
  t136 = ((uint8_t)UINT64_C(1));
  fg163_gg_function_27(t125, t126, t127, t128, t129, t130, t131, t132, t135, t136, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg163_gg_done;
  }
  }
  t137 = ((int64_t)(v1));
  t138 = ((int64_t)(((int64_t)UINT64_C(8))));
  t139 = ((t137) == (t138));
  t140 = t139;
  if (!t140) {
  t141 = ((int64_t)(v1));
  t142 = ((int64_t)(((int64_t)UINT64_C(9))));
  t140 = ((t141) == (t142));
  }
  t143 = t140;
  if (t143) {
  {
  t144 = ((int64_t)(v3));
  t145 = ((int64_t)(v4));
  t146 = ((uint64_t)(v5));
  t147 = ((uint64_t)(v6));
  t148 = ((int64_t)(v1));
  t149 = ((int64_t)(((int64_t)UINT64_C(8))));
  t150 = ((t148) == (t149));
  t151 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg163_gg_function_21(t144, t145, t146, t147, t150, t151, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg163_gg_done;
  }
  }
  t152 = ((int64_t)(v1));
  t153 = ((int64_t)(((int64_t)UINT64_C(10))));
  t154 = ((t152) == (t153));
  t155 = t154;
  if (!t155) {
  t156 = ((int64_t)(v1));
  t157 = ((int64_t)(((int64_t)UINT64_C(11))));
  t155 = ((t156) == (t157));
  }
  t158 = t155;
  if (t158) {
  {
  t159 = ((int64_t)(((int64_t)UINT64_C(0))));
  t160 = ((int64_t)(v3));
  t161 = ((int64_t)(v4));
  t162 = ((uint64_t)(v5));
  t163 = ((uint64_t)(v6));
  t164 = ((uint64_t)(v7));
  t165 = ((uint64_t)(v8));
  t166 = ((uint64_t)(v9));
  t167 = ((int64_t)(v1));
  t168 = ((int64_t)(((int64_t)UINT64_C(10))));
  t169 = ((t167) == (t168));
  t170 = ((uint8_t)UINT64_C(0));
  fg163_gg_function_27(t159, t160, t161, t162, t163, t164, t165, t166, t169, t170, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg163_gg_done;
  }
  }
  t171 = ((int64_t)(v1));
  t172 = ((int64_t)(((int64_t)UINT64_C(12))));
  t173 = ((t171) == (t172));
  if (t173) {
  {
  t174 = ((int64_t)(v3));
  t175 = ((int64_t)(((int64_t)UINT64_C(0))));
  t176 = ((t174) != (t175));
  t177 = ((int64_t)(((int64_t)UINT64_C(27))));
  fg163_gg_function_7(t176, t177, base + 0);
  t178 = ((int64_t)(v3));
  v10 = fg163_gg_function_23(t178, base + 0);
  t179 = ((uint64_t)(v6));
  t180 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t181 = ((t179) == (t180));
  if (t181) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg163_gg_done;
  }
  }
  t182 = ((int64_t)(v10));
  t183 = ((int64_t)(((int64_t)UINT64_C(0))));
  t184 = ((t182) != (t183));
  t185 = t184;
  if (t185) {
  t186 = ((int64_t)(v3));
  t187 = ((int64_t)(((int64_t)UINT64_C(0))));
  t188 = ((int64_t)(((int64_t)UINT64_C(0))));
  t189 = fg163_gg_function_14(t186, t187, t188, base + 0);
  t190 = ((uint64_t)(t189));
  t191 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t185 = ((t190) != (t191));
  }
  t192 = t185;
  t193 = t192;
  if (t193) {
  t194 = ((int64_t)(v10));
  t195 = ((int64_t)(((int64_t)UINT64_C(0))));
  t196 = ((int64_t)(((int64_t)UINT64_C(4))));
  t197 = fg163_callback(t194, t195, t196);
  t198 = ((uint64_t)(t197));
  t199 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t193 = ((t198) != (t199));
  }
  t200 = t193;
  t201 = ((int64_t)(((int64_t)UINT64_C(28))));
  fg163_gg_function_7(t200, t201, base + 0);
  t202 = ((int64_t)(v3));
  t203 = ((uint64_t)(v5));
  t204 = ((uint64_t)(v6));
  t205 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t206 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg163_gg_function_15(t202, t203, t204, t205, t206, base + 0);
  t207 = ((int64_t)(((int64_t)UINT64_C(0))));
  t208 = ((int64_t)(((int64_t)UINT64_C(4))));
  t209 = ((int64_t)(v10));
  t210 = ((int64_t)(v3));
  t211 = ((int64_t)(v4));
  t212 = ((uint64_t)(v5));
  t213 = ((uint64_t)(v6));
  t214 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t215 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t216 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t217 = fg163_io(t207, t208, t209, t210, t211, t212, t213, t214, t215, t216);
  t218 = ((int64_t)(t217));
  t219 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg163_gg_function_25(t218, t219, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg163_gg_done;
  }
  }
  t220 = ((int64_t)(v1));
  t221 = ((int64_t)(((int64_t)UINT64_C(13))));
  t222 = ((t220) == (t221));
  if (t222) {
  {
  t223 = ((int64_t)(v2));
  fg163_gg_function_20(t223, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg163_gg_done;
  }
  }
  t224 = ((int64_t)(v1));
  t225 = ((int64_t)(((int64_t)UINT64_C(14))));
  t226 = ((t224) >= (t225));
  t227 = t226;
  if (t227) {
  t228 = ((int64_t)(v1));
  t229 = ((int64_t)(((int64_t)UINT64_C(18))));
  t227 = ((t228) <= (t229));
  }
  t230 = t227;
  if (t230) {
  {
  t231 = ((int64_t)(v2));
  t232 = ((int64_t)(((int64_t)UINT64_C(0))));
  t233 = ((t231) != (t232));
  t234 = ((int64_t)(((int64_t)UINT64_C(29))));
  fg163_gg_function_7(t233, t234, base + 0);
  t235 = ((int64_t)(v1));
  t236 = ((int64_t)(((int64_t)UINT64_C(18))));
  t237 = ((t235) == (t236));
  if (t237) {
  {
  t238 = ((int64_t)(v2));
  t239 = ((int64_t)(((int64_t)UINT64_C(1))));
  t240 = ((int64_t)(((int64_t)UINT64_C(11))));
  t241 = fg163_callback(t238, t239, t240);
  t242 = ((uint64_t)(t241));
  t243 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t244 = ((t242) != (t243));
  t245 = ((int64_t)(((int64_t)UINT64_C(30))));
  fg163_gg_function_7(t244, t245, base + 0);
  t246 = ((int64_t)(((int64_t)UINT64_C(11))));
  t247 = ((int64_t)(v2));
  t248 = ((int64_t)(v3));
  t249 = ((int64_t)(((int64_t)UINT64_C(0))));
  t250 = ((int64_t)(((int64_t)UINT64_C(0))));
  t251 = fg163_backend_call(t246, t247, t248, t249, t250);
  v11 = ((uint64_t)(t251));
  t252 = ((int64_t)(v2));
  fg163_gg_function_20(t252, base + 0);
  t0 = ((uint64_t)(v11));
  goto fg163_gg_done;
  }
  }
  t253 = ((int64_t)(v1));
  t254 = ((int64_t)(((int64_t)UINT64_C(14))));
  t255 = ((t253) == (t254));
  if (t255) {
  {
  v12 = ((int64_t)(((int64_t)UINT64_C(8))));
  }
  } else {
  {
  t256 = ((int64_t)(v1));
  t257 = ((int64_t)(((int64_t)UINT64_C(15))));
  t258 = ((t256) == (t257));
  if (t258) {
  {
  v12 = ((int64_t)(((int64_t)UINT64_C(9))));
  }
  } else {
  {
  t259 = ((int64_t)(v1));
  t260 = ((int64_t)(((int64_t)UINT64_C(16))));
  t261 = ((t259) == (t260));
  if (t261) {
  {
  v12 = ((int64_t)(((int64_t)UINT64_C(10))));
  }
  } else {
  {
  v12 = ((int64_t)(((int64_t)UINT64_C(11))));
  }
  }
  }
  }
  }
  }
  t262 = ((int64_t)(v2));
  t263 = ((int64_t)(((int64_t)UINT64_C(1))));
  t264 = ((int64_t)(v12));
  t265 = fg163_callback(t262, t263, t264);
  t266 = ((uint64_t)(t265));
  t267 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t268 = ((t266) != (t267));
  t269 = ((int64_t)(((int64_t)UINT64_C(31))));
  fg163_gg_function_7(t268, t269, base + 0);
  t270 = ((int64_t)(v12));
  t271 = ((int64_t)(v2));
  t272 = ((int64_t)(v3));
  t273 = ((int64_t)(((int64_t)UINT64_C(0))));
  t274 = ((int64_t)(((int64_t)UINT64_C(0))));
  t275 = fg163_backend_call(t270, t271, t272, t273, t274);
  t0 = ((uint64_t)(t275));
  goto fg163_gg_done;
  }
  }
  t276 = ((int64_t)(v1));
  t277 = ((int64_t)(((int64_t)UINT64_C(19))));
  t278 = ((t276) >= (t277));
  t279 = t278;
  if (t279) {
  t280 = ((int64_t)(v1));
  t281 = ((int64_t)(((int64_t)UINT64_C(21))));
  t279 = ((t280) <= (t281));
  }
  t282 = t279;
  if (t282) {
  {
  t283 = ((int64_t)(v2));
  t284 = ((int64_t)(((int64_t)UINT64_C(0))));
  t285 = ((t283) != (t284));
  t286 = ((int64_t)(((int64_t)UINT64_C(32))));
  fg163_gg_function_7(t285, t286, base + 0);
  t288 = ((int64_t)(v1));
  t289 = ((int64_t)(((int64_t)UINT64_C(19))));
  t290 = ((t288) == (t289));
  if (t290) {
  {
  t287 = ((int64_t)(((int64_t)UINT64_C(9))));
  }
  } else {
  {
  t291 = ((int64_t)(v1));
  t292 = ((int64_t)(((int64_t)UINT64_C(20))));
  t293 = ((t291) == (t292));
  if (t293) {
  {
  t287 = ((int64_t)(((int64_t)UINT64_C(10))));
  }
  } else {
  {
  t287 = ((int64_t)(((int64_t)UINT64_C(11))));
  }
  }
  }
  }
  t294 = ((int64_t)(t287));
  t295 = ((int64_t)(v2));
  t296 = ((int64_t)(((int64_t)UINT64_C(1))));
  t297 = ((int64_t)(((int64_t)UINT64_C(1))));
  t298 = ((int64_t)(((int64_t)UINT64_C(0))));
  t299 = fg163_get(t295, t296, t297, t298);
  t300 = ((uint64_t)(t299));
  t301 = fg163_gg_function_3(t300, base + 0);
  t302 = ((int64_t)(v3));
  t303 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t304 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t0 = fg163_gg_function_10(t294, t301, t302, t303, t304, base + 0);
  goto fg163_gg_done;
  }
  }
  t305 = ((int64_t)(v1));
  t306 = ((int64_t)(((int64_t)UINT64_C(22))));
  t307 = ((t305) == (t306));
  if (t307) {
  {
  t308 = ((int64_t)(v2));
  t309 = ((int64_t)(((int64_t)UINT64_C(0))));
  t310 = ((t308) != (t309));
  t311 = ((int64_t)(((int64_t)UINT64_C(33))));
  fg163_gg_function_7(t310, t311, base + 0);
  t312 = ((int64_t)(v2));
  t313 = ((int64_t)(((int64_t)UINT64_C(1))));
  t314 = ((int64_t)(((int64_t)UINT64_C(1))));
  t315 = ((int64_t)(((int64_t)UINT64_C(0))));
  t316 = fg163_get(t312, t313, t314, t315);
  t0 = ((uint64_t)(t316));
  goto fg163_gg_done;
  }
  }
  t317 = ((int64_t)(v1));
  t318 = ((int64_t)(((int64_t)UINT64_C(23))));
  t319 = ((t317) == (t318));
  if (t319) {
  {
  t320 = ((int64_t)(v2));
  t321 = ((int64_t)(v3));
  fg163_gg_function_28(t320, t321, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg163_gg_done;
  }
  }
  t322 = ((int64_t)(v1));
  t323 = ((int64_t)(((int64_t)UINT64_C(24))));
  t324 = ((t322) == (t323));
  if (t324) {
  {
  t325 = ((int64_t)(v2));
  t326 = ((int64_t)(v3));
  t327 = ((int64_t)(v4));
  t328 = ((uint64_t)(v5));
  t329 = fg163_gg_function_3(t328, base + 0);
  fg163_gg_function_34(t325, t326, t327, t329, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg163_gg_done;
  }
  }
  t330 = ((int64_t)(v1));
  t331 = ((int64_t)(((int64_t)UINT64_C(25))));
  t332 = ((t330) == (t331));
  if (t332) {
  {
  t333 = ((int64_t)(((int64_t)UINT64_C(12))));
  t334 = ((int64_t)(v2));
  t335 = ((int64_t)(((int64_t)UINT64_C(0))));
  t336 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t337 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t0 = fg163_gg_function_10(t333, t334, t335, t336, t337, base + 0);
  goto fg163_gg_done;
  }
  }
  t338 = ((int64_t)(v1));
  t339 = ((int64_t)(((int64_t)UINT64_C(26))));
  t340 = ((t338) == (t339));
  if (t340) {
  {
  t341 = ((int64_t)(v2));
  t342 = ((int64_t)(((int64_t)UINT64_C(0))));
  t343 = ((t341) == (t342));
  if (t343) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg163_gg_done;
  }
  }
  t344 = ((int64_t)(((int64_t)UINT64_C(13))));
  t345 = ((int64_t)(v2));
  t346 = ((int64_t)(((int64_t)UINT64_C(2))));
  t347 = ((int64_t)(((int64_t)UINT64_C(0))));
  t348 = ((int64_t)(((int64_t)UINT64_C(0))));
  t349 = fg163_get(t345, t346, t347, t348);
  t350 = ((uint64_t)(t349));
  t351 = fg163_gg_function_3(t350, base + 0);
  t352 = ((int64_t)(v2));
  t353 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t354 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t355 = fg163_gg_function_10(t344, t351, t352, t353, t354, base + 0);
  (void)(t355);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg163_gg_done;
  }
  }
  t356 = ((int64_t)(v1));
  t357 = ((int64_t)(((int64_t)UINT64_C(27))));
  t358 = ((t356) == (t357));
  t359 = t358;
  if (!t359) {
  t360 = ((int64_t)(v1));
  t361 = ((int64_t)(((int64_t)UINT64_C(29))));
  t359 = ((t360) == (t361));
  }
  t362 = t359;
  if (t362) {
  {
  t363 = ((int64_t)(v2));
  t364 = ((int64_t)(((int64_t)UINT64_C(0))));
  t365 = ((t363) != (t364));
  t366 = ((int64_t)(((int64_t)UINT64_C(34))));
  fg163_gg_function_7(t365, t366, base + 0);
  t367 = ((int64_t)(v1));
  t368 = ((int64_t)(((int64_t)UINT64_C(27))));
  t369 = ((t367) == (t368));
  if (t369) {
  {
  v13 = ((int64_t)(((int64_t)UINT64_C(12))));
  }
  } else {
  {
  v13 = ((int64_t)(((int64_t)UINT64_C(13))));
  }
  }
  t370 = ((int64_t)(v2));
  t371 = ((int64_t)(((int64_t)UINT64_C(1))));
  t372 = ((int64_t)(v13));
  t373 = fg163_callback(t370, t371, t372);
  t374 = ((uint64_t)(t373));
  t375 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t376 = ((t374) != (t375));
  t377 = ((int64_t)(((int64_t)UINT64_C(35))));
  fg163_gg_function_7(t376, t377, base + 0);
  t378 = ((int64_t)(v13));
  t379 = ((int64_t)(v2));
  t380 = ((int64_t)(v3));
  t381 = ((int64_t)(((int64_t)UINT64_C(0))));
  t382 = ((int64_t)(((int64_t)UINT64_C(0))));
  t383 = fg163_backend_call(t378, t379, t380, t381, t382);
  (void)(t383);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg163_gg_done;
  }
  }
  t384 = ((int64_t)(v1));
  t385 = ((int64_t)(((int64_t)UINT64_C(28))));
  t386 = ((t384) == (t385));
  if (t386) {
  {
  t387 = ((int64_t)(v2));
  t388 = ((int64_t)(((int64_t)UINT64_C(0))));
  t389 = ((t387) != (t388));
  t390 = ((int64_t)(((int64_t)UINT64_C(36))));
  fg163_gg_function_7(t389, t390, base + 0);
  t391 = ((int64_t)(((int64_t)UINT64_C(14))));
  t392 = ((int64_t)(v2));
  t393 = ((int64_t)(((int64_t)UINT64_C(2))));
  t394 = ((int64_t)(((int64_t)UINT64_C(0))));
  t395 = ((int64_t)(((int64_t)UINT64_C(0))));
  t396 = fg163_get(t392, t393, t394, t395);
  t397 = ((uint64_t)(t396));
  t398 = fg163_gg_function_3(t397, base + 0);
  t399 = ((int64_t)(v2));
  t400 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t401 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t402 = fg163_gg_function_10(t391, t398, t399, t400, t401, base + 0);
  (void)(t402);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg163_gg_done;
  }
  }
  t403 = ((int64_t)(v1));
  t404 = ((int64_t)(((int64_t)UINT64_C(30))));
  t405 = ((t403) == (t404));
  if (t405) {
  {
  t406 = ((int64_t)(v2));
  t407 = ((int64_t)(((int64_t)UINT64_C(0))));
  t408 = ((t406) != (t407));
  t409 = ((int64_t)(((int64_t)UINT64_C(37))));
  fg163_gg_function_7(t408, t409, base + 0);
  t410 = ((int64_t)(v2));
  t411 = ((int64_t)(((int64_t)UINT64_C(1))));
  t412 = ((int64_t)(((int64_t)UINT64_C(14))));
  t413 = fg163_callback(t410, t411, t412);
  t414 = ((uint64_t)(t413));
  t415 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t416 = ((t414) != (t415));
  if (t416) {
  {
  t417 = ((int64_t)(((int64_t)UINT64_C(14))));
  t418 = ((int64_t)(v2));
  t419 = ((int64_t)(v3));
  t420 = ((int64_t)(v4));
  t421 = ((int64_t)(((int64_t)UINT64_C(0))));
  t422 = fg163_backend_call(t417, t418, t419, t420, t421);
  (void)(t422);
  }
  }
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg163_gg_done;
  }
  }
  t423 = ((int64_t)(v1));
  t424 = ((int64_t)(((int64_t)UINT64_C(31))));
  t425 = ((t423) >= (t424));
  t426 = t425;
  if (t426) {
  t427 = ((int64_t)(v1));
  t428 = ((int64_t)(((int64_t)UINT64_C(35))));
  t426 = ((t427) <= (t428));
  }
  t429 = t426;
  if (t429) {
  {
  t430 = ((int64_t)(v1));
  t431 = ((int64_t)(((int64_t)UINT64_C(31))));
  t432 = ((int64_t)(((int64_t)((uint64_t)(t430) - (uint64_t)(t431)))));
  t433 = ((int64_t)(v2));
  t434 = ((int64_t)(v3));
  t435 = ((uint64_t)(((uint64_t)(v4))));
  t436 = ((uint64_t)(v5));
  t0 = fg163_gg_function_10(t432, t433, t434, t435, t436, base + 0);
  goto fg163_gg_done;
  }
  }
  t437 = ((int64_t)(v1));
  t438 = ((int64_t)(((int64_t)UINT64_C(36))));
  t439 = ((t437) == (t438));
  if (t439) {
  {
  t440 = ((int64_t)(v2));
  t441 = ((int64_t)(((int64_t)UINT64_C(0))));
  t442 = ((t440) != (t441));
  t443 = ((int64_t)(((int64_t)UINT64_C(38))));
  fg163_gg_function_7(t442, t443, base + 0);
  t444 = ((int64_t)(v2));
  t445 = ((int64_t)(((int64_t)UINT64_C(3))));
  t446 = ((int64_t)(((int64_t)UINT64_C(0))));
  t447 = ((int64_t)(((int64_t)UINT64_C(0))));
  t448 = fg163_get(t444, t445, t446, t447);
  t0 = ((uint64_t)(t448));
  goto fg163_gg_done;
  }
  }
  t449 = ((int64_t)(v1));
  t450 = ((int64_t)(((int64_t)UINT64_C(37))));
  t451 = ((t449) >= (t450));
  t452 = t451;
  if (t452) {
  t453 = ((int64_t)(v1));
  t454 = ((int64_t)(((int64_t)UINT64_C(43))));
  t452 = ((t453) <= (t454));
  }
  t455 = t452;
  if (t455) {
  {
  t456 = ((int64_t)(v1));
  t457 = ((int64_t)(((int64_t)UINT64_C(32))));
  t458 = ((int64_t)(((int64_t)((uint64_t)(t456) - (uint64_t)(t457)))));
  t459 = ((int64_t)(v2));
  t460 = ((int64_t)(v3));
  t461 = ((uint64_t)(((uint64_t)(v4))));
  t462 = ((uint64_t)(v5));
  t0 = fg163_gg_function_10(t458, t459, t460, t461, t462, base + 0);
  goto fg163_gg_done;
  }
  }
  t463 = ((int64_t)(v1));
  t464 = ((int64_t)(((int64_t)UINT64_C(44))));
  t465 = ((t463) >= (t464));
  t466 = t465;
  if (t466) {
  t467 = ((int64_t)(v1));
  t468 = ((int64_t)(((int64_t)UINT64_C(47))));
  t466 = ((t467) <= (t468));
  }
  t469 = t466;
  if (t469) {
  {
  t470 = ((int64_t)(v2));
  t471 = ((int64_t)(((int64_t)UINT64_C(0))));
  t472 = ((t470) != (t471));
  t473 = ((int64_t)(((int64_t)UINT64_C(39))));
  fg163_gg_function_7(t472, t473, base + 0);
  t474 = ((int64_t)(v1));
  t475 = ((int64_t)(((int64_t)UINT64_C(44))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t474) - (uint64_t)(t475)))));
  t476 = ((int64_t)(v14));
  t477 = ((int64_t)(((int64_t)UINT64_C(3))));
  t478 = ((t476) == (t477));
  t479 = t478;
  if (t479) {
  t480 = ((int64_t)(v2));
  t481 = ((int64_t)(((int64_t)UINT64_C(3))));
  t482 = ((int64_t)(v14));
  t483 = fg163_callback(t480, t481, t482);
  t484 = ((uint64_t)(t483));
  t485 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t479 = ((t484) == (t485));
  }
  t486 = t479;
  if (t486) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg163_gg_done;
  }
  }
  t487 = ((int64_t)(v2));
  t488 = ((int64_t)(((int64_t)UINT64_C(3))));
  t489 = ((int64_t)(v14));
  t490 = fg163_callback(t487, t488, t489);
  t491 = ((uint64_t)(t490));
  t492 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t493 = ((t491) != (t492));
  t494 = ((int64_t)(((int64_t)UINT64_C(40))));
  fg163_gg_function_7(t493, t494, base + 0);
  t495 = ((int64_t)(v14));
  t496 = ((int64_t)(v2));
  t497 = ((int64_t)(v0));
  t498 = ((int64_t)(((int64_t)UINT64_C(2))));
  t499 = fg163_arg(t497, t498);
  t500 = ((uint64_t)(t499));
  t501 = fg163_reg_call(t495, t496, t500);
  t0 = ((uint64_t)(t501));
  goto fg163_gg_done;
  }
  }
  t502 = ((int64_t)(((int64_t)UINT64_C(99))));
  fg163_fail(t502);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg163_gg_done;
  }
fg163_gg_done:;
  return t0;
}
static void fg163_gg_function_0(int64_t base) {
  fg163_gg_header t0;
  int64_t t1;
  uint64_t t2;
  int64_t t3;
  {
  t0.base = fg163_gg_get_stack(base + 0)->base + fg163_gg_get_stack(base + 0)->top; t0.len = 0;
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = fg163_gg_function_1(t1, base + 1);
  fg163_gg_unsigned(fg163_gg_ref(&t0, fg163_gg_get_stack(base + 0)), t2);
  fg163_gg_append(fg163_gg_ref(&t0, fg163_gg_get_stack(base + 0)), "\n", 1, 1);
  fg163_gg_write(stdout, fg163_gg_view((t0).base, (t0).len));
  }
fg163_gg_done:;
  for (t3 = 0; t3 < 1; t3++) fg163_gg_stacks[base + t3].top = 0;
}
static int fg163_gg_program(int argc, char **argv) {
  struct { int64_t base; } frame;
  frame.base = 0;
  fg163_gg_argc = argc; fg163_gg_argv = argv;
  fg163_gg_function_0(frame.base + 0);
  return 0;
}
int main(int argc, char **argv) {
#ifdef _MSC_VER
  __try { return fg163_gg_program(argc, argv); }
  __except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    _resetstkoflw(); fg163_gg_fail("native call stack overflow"); return 1;
  }
#else
  return fg163_gg_program(argc, argv);
#endif
}
