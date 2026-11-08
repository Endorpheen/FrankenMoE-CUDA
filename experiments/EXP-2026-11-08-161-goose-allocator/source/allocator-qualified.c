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
typedef struct { void *data; int64_t len; } fg161_gg_slice;
typedef struct { unsigned char *base; int64_t len; } fg161_gg_header;
typedef struct { unsigned char *base; size_t top, committed; } fg161_gg_stack;
typedef struct { fg161_gg_header *header; fg161_gg_stack *stack; } fg161_gg_reference;
#define GG_RESERVATION ((size_t)268435456)
#define GG_STACK_COUNT 8192
static fg161_gg_stack fg161_gg_stacks[GG_STACK_COUNT];
static int fg161_gg_argc;
static char **fg161_gg_argv;
static void fg161_gg_fail(const char *message) {
    fprintf(stderr, "goose runtime error: %s\n", message); exit(1);
}
static fg161_gg_stack *fg161_gg_get_stack(int64_t id) {
    fg161_gg_stack *s;
    if (id < 0 || id >= GG_STACK_COUNT) fg161_gg_fail("data stack limit exceeded");
    s = &fg161_gg_stacks[id];
    if (!s->base) {
#ifdef _WIN32
        s->base = VirtualAlloc(NULL, GG_RESERVATION + 65536, MEM_RESERVE, PAGE_NOACCESS);
#else
        s->base = mmap(NULL, GG_RESERVATION + 65536, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (s->base == MAP_FAILED) s->base = NULL;
#endif
        if (!s->base) fg161_gg_fail("cannot reserve data stack");
    }
    return s;
}
static void *fg161_gg_allocate(fg161_gg_stack *s, size_t size) {
    size_t end, committed;
    void *result;
    if (size > GG_RESERVATION - s->top) fg161_gg_fail("data stack reservation exceeded");
    result = s->base + s->top; end = s->top + size;
    if (end > s->committed) {
        committed = (end + 65535) & ~(size_t)65535;
#ifdef _WIN32
        if (!VirtualAlloc(s->base + s->committed, committed - s->committed, MEM_COMMIT, PAGE_READWRITE)) fg161_gg_fail("cannot commit data stack");
#else
        if (mprotect(s->base + s->committed, committed - s->committed, PROT_READ | PROT_WRITE)) fg161_gg_fail("cannot commit data stack");
#endif
        s->committed = committed;
    }
    s->top = end; return result;
}
static fg161_gg_reference fg161_gg_ref(fg161_gg_header *h, fg161_gg_stack *s) { fg161_gg_reference r; r.header=h; r.stack=s; return r; }
static fg161_gg_slice fg161_gg_view(void *p, int64_t n) { fg161_gg_slice v; v.data=p; v.len=n; return v; }
static fg161_gg_slice fg161_gg_byte_view(fg161_gg_slice a, size_t width) {
    if (a.len < 0 || (width && (uint64_t)a.len > (uint64_t)INT64_MAX / width)) fg161_gg_fail("byte view size overflow");
    return fg161_gg_view(a.data, (int64_t)((uint64_t)a.len * width));
}
static fg161_gg_slice fg161_gg_sub(fg161_gg_slice a, int64_t lo, int64_t hi, size_t element) {
    if (lo < 0 || hi < lo || hi > a.len) fg161_gg_fail("slice out of bounds");
    return fg161_gg_view((unsigned char *)a.data + (size_t)lo * element, hi - lo);
}
static int64_t fg161_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg161_gg_fail("array index out of bounds"); return i; }
static void fg161_gg_append(fg161_gg_reference a, const void *p, int64_t n, size_t element) {
    void *to;
    if (n < 0 || (uint64_t)n > GG_RESERVATION / (element ? element : 1)) fg161_gg_fail("invalid array size");
    to=fg161_gg_allocate(a.stack, (size_t)n * element);
    if (n && element) memmove(to, p, (size_t)n * element);
    a.header->len += n;
}
static void fg161_gg_resize(fg161_gg_reference a, int64_t n, size_t element, const void *fill) {
    int64_t old=a.header->len, i;
    if (n < 0) fg161_gg_fail("negative array length");
    if (n > old) {
        unsigned char *p;
        if (!fill) fg161_gg_fail("resize without fill may only shrink");
        if ((uint64_t)(n-old) > GG_RESERVATION / (element ? element : 1)) fg161_gg_fail("invalid array size");
        p=fg161_gg_allocate(a.stack, (size_t)(n-old)*element);
        for (i=0; i<n-old; i++) if(element) memcpy(p+(size_t)i*element, fill, element);
    } else { a.stack->top = (size_t)(a.header->base-a.stack->base) + (size_t)n*element; }
    a.header->len=n;
}
static int64_t fg161_gg_div(int64_t a, int64_t b) { if (!b) fg161_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg161_gg_fail("division overflow"); return a/b; }
static int64_t fg161_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg161_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static uint64_t fg161_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg161_gg_fail("division by zero");return a/b; }
static uint64_t fg161_gg_umod(uint64_t a,uint64_t b) { if(!b)fg161_gg_fail("division by zero");return a%b; }
static int fg161_gg_equal(fg161_gg_slice a, fg161_gg_slice b, size_t element) { return a.len==b.len && (!a.len || !memcmp(a.data,b.data,(size_t)a.len*element)); }
static void fg161_gg_text(fg161_gg_reference out, fg161_gg_slice v) { fg161_gg_append(out,v.data,v.len,1); }
static void fg161_gg_integer(fg161_gg_reference out,int64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%lld",(long long)n);fg161_gg_append(out,b,k,1); }
static void fg161_gg_unsigned(fg161_gg_reference out,uint64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%llu",(unsigned long long)n);fg161_gg_append(out,b,k,1); }
static double fg161_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static int64_t fg161_ggf_fmt_f64(uint8_t *out,double n);
static int64_t fg161_ggf_fmt_f32(uint8_t *out,float n);
static void fg161_gg_float(fg161_gg_reference out,double n) { uint8_t b[64];int64_t k=fg161_ggf_fmt_f64(b,n);fg161_gg_append(out,b,k,1); }
static void fg161_gg_float32(fg161_gg_reference out,float n) { uint8_t b[64];int64_t k=fg161_ggf_fmt_f32(b,n);fg161_gg_append(out,b,k,1); }
static void fg161_gg_bool(fg161_gg_reference out,uint8_t n) { fg161_gg_append(out,n?"true":"false",n?4:5,1); }
static void fg161_gg_write(FILE *file,fg161_gg_slice text) { if(text.len && fwrite(text.data,1,(size_t)text.len,file)!=(size_t)text.len)fg161_gg_fail("write failed"); }
static char *fg161_gg_cstring(fg161_gg_slice path) {
    char *s=malloc((size_t)path.len+1); if(!s)fg161_gg_fail("out of memory in file shim");
    if(path.len)memcpy(s,path.data,(size_t)path.len);s[path.len]=0;return s;
}
static uint8_t fg161_gs_os_read_file(fg161_gg_slice path,fg161_gg_reference out) {
    char *p=fg161_gg_cstring(path);FILE *f=fopen(p,"rb");unsigned char b[16384];size_t n;int ok;free(p);
    if(!f)return 0;while((n=fread(b,1,sizeof b,f))!=0)fg161_gg_append(out,b,(int64_t)n,1);ok=!ferror(f);fclose(f);return (uint8_t)ok;
}
static uint8_t fg161_gs_os_write_file(fg161_gg_slice path,fg161_gg_slice data) {
    char *p=fg161_gg_cstring(path);FILE *f=fopen(p,"wb");int ok;free(p);if(!f)return 0;
    ok=!data.len || fwrite(data.data,1,(size_t)data.len,f)==(size_t)data.len;if(fclose(f))ok=0;return (uint8_t)ok;
}
static uint8_t fg161_gs_os_file_exists(fg161_gg_slice path) { char*p=fg161_gg_cstring(path);FILE*f=fopen(p,"rb");free(p);if(!f)return 0;fclose(f);return 1; }
static void fg161_gs_os_write_stdout(fg161_gg_slice s) { fg161_gg_write(stdout,s); }
static void fg161_gs_os_write_stderr(fg161_gg_slice s) { fg161_gg_write(stderr,s); }
static void fg161_gs_os_flush_stdout(void) { fflush(stdout); }
static int64_t fg161_gs_os_arg_count(void) { return fg161_gg_argc; }
static void fg161_gs_os_arg(int64_t i,fg161_gg_reference out) { if(i>=0 && i<fg161_gg_argc)fg161_gg_append(out,fg161_gg_argv[i],(int64_t)strlen(fg161_gg_argv[i]),1); }
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
typedef struct { int n; uint32_t d[36]; } fg161_ggf_big;

/* a = v << sh, for v > 0. */
static void fg161_ggf_big_set(fg161_ggf_big *a, uint64_t v, int sh) {
    int w = sh >> 5, b = sh & 31, i;
    uint64_t lo = v << b, hi = b ? v >> (64 - b) : 0;
    for (i = 0; i < w; i++) a->d[i] = 0;
    a->d[w] = (uint32_t)lo;
    a->d[w + 1] = (uint32_t)(lo >> 32);
    a->d[w + 2] = (uint32_t)hi;
    a->n = w + 3;
    while (!a->d[a->n - 1]) a->n--;
}

static void fg161_ggf_big_mul(fg161_ggf_big *a, uint32_t m) {
    uint64_t c = 0;
    for (int i = 0; i < a->n; i++) {
        c += (uint64_t)a->d[i] * m;
        a->d[i] = (uint32_t)c;
        c >>= 32;
    }
    if (c) a->d[a->n++] = (uint32_t)c;
}

static void fg161_ggf_big_mul_pow10(fg161_ggf_big *a, int k) {
    for (; k >= 9; k -= 9) fg161_ggf_big_mul(a, 1000000000u);
    for (; k > 0; k--) fg161_ggf_big_mul(a, 10);
}

static int fg161_ggf_big_cmp(const fg161_ggf_big *a, const fg161_ggf_big *b) {
    if (a->n != b->n) return a->n < b->n ? -1 : 1;
    for (int i = a->n - 1; i >= 0; i--)
        if (a->d[i] != b->d[i]) return a->d[i] < b->d[i] ? -1 : 1;
    return 0;
}

/* t = a + b */
static void fg161_ggf_big_add(fg161_ggf_big *t, const fg161_ggf_big *a, const fg161_ggf_big *b) {
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
static void fg161_ggf_big_sub(fg161_ggf_big *a, const fg161_ggf_big *b) {
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
static int fg161_ggf_float_digits(uint64_t f, int e, int bits, int emin, char *dig, int *k) {
    fg161_ggf_big r, s, mp, mlo, t;
    /* Reading rounds a tie to the even significand, so the interval that
       reads back as f * 2^e includes its ends when f is even. */
    int even = !(f & 1);
    /* Below a power of two, the gap to the next float down is half the gap up. */
    int asym = f == (uint64_t)1 << (bits - 1) && e > emin;
    int up = e > 0 ? e : 0, lg = e - 1, n = 0, c;
    /* The value is r / s, and what reads back as it reaches from (r - *mm) / s
       to (r + mp) / s: half the gap to each neighbour. */
    fg161_ggf_big *mm = asym ? &mlo : &mp;
    fg161_ggf_big_set(&r, f, up + 1 + asym);
    fg161_ggf_big_set(&s, 1, up - e + 1 + asym);
    fg161_ggf_big_set(&mp, 1, up + asym);
    fg161_ggf_big_set(&mlo, 1, up);
    /* Scale to 10^*k, the least power of ten above the interval: with
       lg = floor(log2 value), ceil(lg log10 2) is it or one short, and
       78913 / 2^18 is log10 2 closely enough to compute that for |lg| < 1650. */
    for (uint64_t g = f; g; g >>= 1) lg++;
    *k = lg > 0 ? ((lg * 78913) >> 18) + 1 : -((-lg * 78913) >> 18);
    if (*k >= 0) fg161_ggf_big_mul_pow10(&s, *k);
    else {
        fg161_ggf_big_mul_pow10(&r, -*k);
        fg161_ggf_big_mul_pow10(&mp, -*k);
        if (asym) fg161_ggf_big_mul_pow10(&mlo, -*k);
    }
    fg161_ggf_big_add(&t, &r, &mp);
    c = fg161_ggf_big_cmp(&t, &s);
    if (even ? c >= 0 : c > 0) {
        fg161_ggf_big_mul(&s, 10);
        ++*k;
    }
    /* Each digit d leaves the remainder r: stop where the digits so far
       followed by d (r within *mm) or by d + 1 (r + mp past s) read back. */
    for (;;) {
        int d = 0, low, high;
        fg161_ggf_big_mul(&r, 10);
        fg161_ggf_big_mul(&mp, 10);
        if (asym) fg161_ggf_big_mul(&mlo, 10);
        while (fg161_ggf_big_cmp(&r, &s) >= 0) {
            fg161_ggf_big_sub(&r, &s);
            d++;
        }
        c = fg161_ggf_big_cmp(&r, mm);
        low = even ? c <= 0 : c < 0;
        fg161_ggf_big_add(&t, &r, &mp);
        c = fg161_ggf_big_cmp(&t, &s);
        high = even ? c >= 0 : c > 0;
        if (!low && !high) {
            dig[n++] = (char)('0' + d);
            continue;
        }
        if (low && high) {
            /* Both read back: the nearer, d + 1 when r is past half of s,
               and of two as near, the even one. */
            fg161_ggf_big_add(&t, &r, &r);
            c = fg161_ggf_big_cmp(&t, &s);
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
static int64_t fg161_ggf_fmt_float(uint8_t *dst, int neg, uint64_t frac, int bexp, int bits,
                            int emin) {
    char dig[20], *p = (char *)dst;
    int k, n, x, i;
    if (neg) *p++ = '-';
    if (!frac && !bexp) {
        memcpy(p, "0.0", 3);
        return p + 3 - (char *)dst;
    }
    n = fg161_ggf_float_digits(bexp ? frac | (uint64_t)1 << (bits - 1) : frac,
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

static int64_t fg161_ggf_fmt_f64(uint8_t *dst, double v) {
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
    return fg161_ggf_fmt_float(dst, (int)(b >> 63), b & (((uint64_t)1 << 52) - 1),
                        (int)(b >> 52) & 0x7FF, 53, -1074);
}

/* The digits are the f32's own, laid out as an f64's are, so 0.1 as an f32
   prints as 0.1, not as the digits of the f64 it widens to. */
static int64_t fg161_ggf_fmt_f32(uint8_t *dst, float v) {
    double d = v;
    uint32_t b;
    if (d != d || isinf(d)) return fg161_ggf_fmt_f64(dst, d);
    memcpy(&b, &v, sizeof b);
    return fg161_ggf_fmt_float(dst, (int)(b >> 31), b & 0x7FFFFF, (int)(b >> 23) & 0xFF, 24, -149);
}


static void fg161_gg_quote(fg161_gg_reference out,fg161_gg_slice text) { int64_t i; unsigned char c; fg161_gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){fg161_gg_append(out,"\\",1,1);fg161_gg_append(out,&c,1,1);} else if(c=='\n')fg161_gg_append(out,"\\n",2,1);else if(c=='\r')fg161_gg_append(out,"\\r",2,1);else if(c=='\t')fg161_gg_append(out,"\\t",2,1);else fg161_gg_append(out,&c,1,1);}fg161_gg_append(out,"\"",1,1);}
static uint8_t fg161_gg_eq_1(int64_t a, int64_t b);
static uint8_t fg161_gg_equal_elements_1(fg161_gg_slice a, fg161_gg_slice b);
static void fg161_gg_render_1(fg161_gg_reference out, int64_t value);
static uint8_t fg161_gg_eq_2(uint64_t a, uint64_t b);
static uint8_t fg161_gg_equal_elements_2(fg161_gg_slice a, fg161_gg_slice b);
static void fg161_gg_render_2(fg161_gg_reference out, uint64_t value);
static uint8_t fg161_gg_eq_4(uint8_t a, uint8_t b);
static uint8_t fg161_gg_equal_elements_4(fg161_gg_slice a, fg161_gg_slice b);
static void fg161_gg_render_4(fg161_gg_reference out, uint8_t value);
static uint8_t fg161_gg_eq_5(uint8_t a, uint8_t b);
static uint8_t fg161_gg_equal_elements_5(fg161_gg_slice a, fg161_gg_slice b);
static void fg161_gg_render_5(fg161_gg_reference out, uint8_t value);
static uint8_t fg161_gg_equal_elements_1(fg161_gg_slice a,fg161_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg161_gg_eq_1(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg161_gg_eq_1(int64_t a, int64_t b) {
return a == b;
}
static void fg161_gg_render_1(fg161_gg_reference out, int64_t value) {
fg161_gg_integer(out,value);
}
static uint8_t fg161_gg_equal_elements_2(fg161_gg_slice a,fg161_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg161_gg_eq_2(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg161_gg_eq_2(uint64_t a, uint64_t b) {
return a == b;
}
static void fg161_gg_render_2(fg161_gg_reference out, uint64_t value) {
fg161_gg_unsigned(out,value);
}
static uint8_t fg161_gg_equal_elements_4(fg161_gg_slice a,fg161_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg161_gg_eq_4(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg161_gg_eq_4(uint8_t a, uint8_t b) {
return a == b;
}
static void fg161_gg_render_4(fg161_gg_reference out, uint8_t value) {
fg161_gg_bool(out,value);
}
static uint8_t fg161_gg_equal_elements_5(fg161_gg_slice a,fg161_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg161_gg_eq_5(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg161_gg_eq_5(uint8_t a, uint8_t b) {
return a == b;
}
static void fg161_gg_render_5(fg161_gg_reference out, uint8_t value) {
fg161_gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} fg161_gg_globals;
static void fg161_gg_function_0(int64_t base);
static uint64_t fg161_gg_function_1(int64_t p0, int64_t base);
static int64_t fg161_gg_function_3(uint64_t p0, int64_t base);
static uint64_t fg161_gg_function_4(uint64_t p0, uint64_t p1, uint64_t p2, int64_t base);
static void fg161_gg_function_5(uint64_t p0, int64_t base);
static void fg161_gg_function_6(uint8_t p0, int64_t p1, int64_t base);
static uint64_t fg161_gg_function_8(uint64_t p0, uint64_t p1, int64_t base);
static uint64_t fg161_gg_function_9(int64_t base);
static uint8_t fg161_gg_function_10(int64_t p0, int64_t base);
static void fg161_gg_function_15(int64_t p0, uint64_t p1, uint64_t p2, int64_t base);
static uint64_t fg161_gg_function_16(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static void fg161_gg_function_18(int64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base);
static void fg161_gg_function_19(int64_t p0, int64_t p1, int64_t base);
static int64_t fg161_gg_function_20(int64_t p0, uint64_t p1, int64_t base);
static uint64_t fg161_gg_function_21(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static void fg161_gg_function_23(int64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base);
static void fg161_gg_function_24(int64_t p0, uint64_t p1, int64_t p2, int64_t base);
static void fg161_gg_function_25(int64_t p0, int64_t p1, uint64_t p2, int64_t base);
static void fg161_gg_function_26(int64_t p0, int64_t p1, uint64_t p2, uint64_t p3, int64_t base);
static void fg161_gg_function_27(int64_t p0, int64_t base);
static int64_t fg161_gg_function_29(uint64_t p0, uint64_t p1, int64_t base);
static int64_t fg161_gg_function_30(uint64_t p0, uint64_t p1, int64_t base);
static void fg161_gg_function_31(int64_t p0, int64_t base);
static uint64_t fg161_gg_function_32(int64_t p0, int64_t p1, int64_t base);
static void fg161_gg_function_33(int64_t p0, int64_t base);
static int64_t fg161_gg_function_34(int64_t p0, int64_t p1, int64_t base);
static uint64_t fg161_gg_function_36(int64_t p0, int64_t p1, int64_t base);
static uint64_t fg161_gg_function_37(int64_t p0, int64_t base);
static uint64_t fg161_gg_function_38(int64_t p0, uint64_t p1, uint64_t p2, int64_t base);
static int64_t fg161_gg_function_40(int64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base);
static void fg161_gg_function_43(int64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base);
static void fg161_gg_function_44(int64_t p0, int64_t base);
static int64_t fg161_gg_function_46(int64_t p0, int64_t p1, int64_t base);
static uint64_t fg161_gg_function_47(uint64_t p0, uint64_t p1, int64_t base);
static void fg161_gg_function_48(int64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base);
static void fg161_gg_function_49(int64_t p0, int64_t base);
static uint64_t fg161_gg_function_50(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static void fg161_gg_function_52(int64_t p0, int64_t p1, int64_t base);
static int64_t fg161_gg_function_53(int64_t p0, int64_t p1, int64_t base);
static uint8_t fg161_gg_function_56(int64_t p0, int64_t p1, int64_t base);
static uint64_t fg161_gg_function_57(int64_t p0, int64_t p1, int64_t base);
static uint8_t fg161_gg_function_58(int64_t p0, int64_t p1, int64_t base);
static uint64_t fg161_gg_function_59(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static void fg161_gg_function_60(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static void fg161_gg_function_61(int64_t p0, int64_t p1, int64_t base);
static void fg161_gg_function_62(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static void fg161_gg_function_63(int64_t p0, int64_t p1, uint64_t p2, int64_t base);
static uint8_t fg161_gg_function_64(int64_t p0, int64_t p1, int64_t base);
static void fg161_gg_function_65(int64_t p0, int64_t p1, int64_t base);
static void fg161_gg_function_66(int64_t p0, int64_t p1, int64_t base);
static int64_t fg161_gg_function_67(int64_t p0, int64_t p1, int64_t base);
static void fg161_gg_function_69(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static void fg161_gg_function_72(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static uint8_t fg161_gg_function_73(int64_t p0, int64_t p1, int64_t p2, int64_t p3, uint8_t p4, int64_t base);
static void fg161_gg_function_75(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static void fg161_gg_function_77(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static uint8_t fg161_gg_function_79(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static uint8_t fg161_gg_function_80(int64_t p0, int64_t p1, int64_t base);
static uint8_t fg161_gg_function_81(int64_t p0, int64_t p1, int64_t base);
static int64_t fg161_gg_function_82(int64_t p0, int64_t p1, int64_t base);
static uint64_t fg161_gg_function_86(int64_t p0, int64_t p1, uint8_t p2, int64_t p3, int64_t base);
static void fg161_gg_function_6(uint8_t p0, int64_t p1, int64_t base) {
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
  fg161_fail(t2);
  }
  }
  }
fg161_gg_done:;
}
static uint64_t fg161_gg_function_9(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((uint64_t)UINT64_C(18446744073709551615))))));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static uint64_t fg161_gg_function_47(uint64_t p0, uint64_t p1, int64_t base) {
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
  t6 = fg161_gg_function_9(base + 0);
  t7 = ((uint64_t)(t6));
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(((uint64_t)fg161_gg_udiv(t7, t8))));
  t4 = ((t5) <= (t9));
  }
  t10 = t4;
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
  fg161_gg_function_6(t10, t11, base + 0);
  t12 = ((uint64_t)(v0));
  t13 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static int64_t fg161_gg_function_82(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  uint64_t t20;
  uint64_t t21;
  uint64_t t22;
  int64_t t23;
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
  uint64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  {
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = ((int64_t)(v0));
  t2 = fg161_first(t1);
  v3 = ((int64_t)(t2));
label1:;
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t3) != (t4)))) goto label2;
  {
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  t7 = ((t5) < (t6));
  t8 = ((int64_t)(((int64_t)UINT64_C(33))));
  fg161_gg_function_6(t7, t8, base + 0);
  t9 = v2;
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t9) + (uint64_t)(t10)));
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(v3));
  t13 = fg161_next(t11, t12);
  v3 = ((int64_t)(t13));
  }
  goto label1;
label2:;
  t14 = ((int64_t)(v1));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((int64_t)(v2));
  fg161_i32_put(t14, t15, t16);
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t17) == (t18));
  if (t19) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg161_gg_done;
  }
  }
  t20 = ((uint64_t)(((uint64_t)(v2))));
  t21 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  t22 = fg161_gg_function_47(t20, t21, base + 0);
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = fg161_heap(t22, t23);
  v4 = ((int64_t)(t24));
  t25 = ((int64_t)(v4));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg161_gg_done;
  }
  }
  t28 = ((int64_t)(v0));
  t29 = fg161_first(t28);
  v3 = ((int64_t)(t29));
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t30 = ((int64_t)(v3));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t30) != (t31)))) goto label5;
  {
  t32 = ((int64_t)(v4));
  t33 = ((int64_t)(v5));
  t34 = ((uint64_t)(((uint64_t)(v3))));
  fg161_ptr_put(t32, t33, t34);
  t35 = v5;
  t36 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t35) + (uint64_t)(t36)));
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(v3));
  t39 = fg161_next(t37, t38);
  v3 = ((int64_t)(t39));
  }
  goto label4;
label5:;
  t0 = ((int64_t)(v4));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static uint64_t fg161_gg_function_86(int64_t p0, int64_t p1, uint8_t p2, int64_t p3, int64_t base) {
  uint64_t t0;
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
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  int64_t t20;
  uint64_t t21;
  int64_t t22;
  int64_t v0;
  int64_t v1;
  uint8_t v2;
  int64_t v3;
  int64_t v4;
  uint64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v0));
  t2 = fg161_no_alloc(t1);
  t3 = ((int64_t)(t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) != (t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(34))));
  fg161_gg_function_6(t5, t6, base + 0);
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(v3));
  v4 = fg161_gg_function_82(t7, t8, base + 0);
  t9 = ((int64_t)(v4));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) == (t10));
  if (t11) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg161_gg_done;
  }
  }
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(v4));
  t14 = ((int64_t)(v3));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = fg161_i32(t14, t15);
  t17 = ((int64_t)(t16));
  t19 = v2;
  if (t19) {
  {
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t20 = ((int64_t)(t18));
  t21 = fg161_bulk(t12, t13, t17, t20);
  v5 = ((uint64_t)(t21));
  t22 = ((int64_t)(v4));
  fg161_release(t22);
  t0 = ((uint64_t)(v5));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static uint64_t fg161_gg_function_8(uint64_t p0, uint64_t p1, int64_t base) {
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
  t2 = fg161_gg_function_9(base + 0);
  t3 = ((uint64_t)(t2));
  t4 = ((uint64_t)(v1));
  t5 = ((uint64_t)(((uint64_t)((uint64_t)(t3) - (uint64_t)(t4)))));
  t6 = ((t1) <= (t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg161_gg_function_6(t6, t7, base + 0);
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static int64_t fg161_gg_function_3(uint64_t p0, int64_t base) {
  int64_t t0;
  uint64_t v0;
  v0 = p0;
  {
  t0 = ((int64_t)(((int64_t)(v0))));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static int64_t fg161_gg_function_34(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint64_t t5;
  uint64_t t6;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(3))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(v1));
  t5 = fg161_get(t1, t2, t3, t4);
  t6 = ((uint64_t)(t5));
  t0 = fg161_gg_function_3(t6, base + 0);
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static uint64_t fg161_gg_function_37(int64_t p0, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  int64_t t2;
  uint64_t t3;
  uint64_t t4;
  uint8_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  uint64_t t18;
  uint64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t v0;
  uint64_t v1;
  int64_t v2;
  v0 = p0;
  {
  v1 = ((uint64_t)(((int64_t)UINT64_C(0))));
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t1 = ((uint64_t)(((uint64_t)(v2))));
  t2 = ((int64_t)(((int64_t)UINT64_C(10))));
  t3 = fg161_constant(t2);
  t4 = ((uint64_t)(t3));
  t5 = ((t1) < (t4));
  t6 = t5;
  if (t6) {
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(v2));
  t9 = fg161_gg_function_34(t7, t8, base + 0);
  t10 = ((int64_t)(t9));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t10) != (t11));
  }
  if (!(t6)) goto label2;
  {
  t12 = ((uint64_t)(v1));
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(v2));
  t15 = fg161_gg_function_34(t13, t14, base + 0);
  t16 = ((int64_t)(((int64_t)UINT64_C(2))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = fg161_buffer(t15, t16, t17);
  t19 = ((uint64_t)(t18));
  v1 = fg161_gg_function_8(t12, t19, base + 0);
  t20 = v2;
  t21 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t20) + (uint64_t)(t21)));
  }
  goto label1;
label2:;
  t0 = ((uint64_t)(v1));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static uint64_t fg161_gg_function_50(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  t2 = ((int64_t)(((int64_t)UINT64_C(4))));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(v2));
  t5 = fg161_get(t1, t2, t3, t4);
  t0 = ((uint64_t)(t5));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static void fg161_gg_function_61(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg161_gg_function_50(t5, t6, t7, base + 0);
  t9 = fg161_gg_function_3(t8, base + 0);
  t10 = ((int64_t)(t9));
  t3 = ((t4) < (t10));
  }
  t11 = t3;
  t12 = ((int64_t)(((int64_t)UINT64_C(23))));
  fg161_gg_function_6(t11, t12, base + 0);
  }
fg161_gg_done:;
}
static void fg161_gg_function_43(int64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  uint64_t t4;
  int64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint8_t t8;
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
  uint64_t t19;
  uint64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint64_t t24;
  uint64_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint64_t t30;
  uint64_t t31;
  uint64_t t32;
  int64_t t33;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  int64_t v4;
  uint64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((uint64_t)(((uint64_t)(v2))));
  t5 = ((int64_t)(((int64_t)UINT64_C(10))));
  t6 = fg161_constant(t5);
  t7 = ((uint64_t)(t6));
  t3 = ((t4) < (t7));
  }
  t8 = t3;
  t9 = ((int64_t)(((int64_t)UINT64_C(15))));
  fg161_gg_function_6(t8, t9, base + 0);
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(v2));
  v4 = fg161_gg_function_34(t10, t11, base + 0);
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) != (t13));
  t15 = ((int64_t)(((int64_t)UINT64_C(16))));
  fg161_gg_function_6(t14, t15, base + 0);
  t16 = ((int64_t)(v4));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = fg161_buffer(t16, t17, t18);
  v5 = ((uint64_t)(t19));
  t20 = ((uint64_t)(v3));
  t21 = ((int64_t)(v4));
  t22 = ((int64_t)(((int64_t)UINT64_C(2))));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = fg161_buffer(t21, t22, t23);
  t25 = ((uint64_t)(t24));
  t26 = ((t20) <= (t25));
  t27 = ((int64_t)(((int64_t)UINT64_C(17))));
  fg161_gg_function_6(t26, t27, base + 0);
  t28 = ((int64_t)(v4));
  t29 = ((int64_t)(v1));
  t30 = ((uint64_t)(v5));
  t31 = ((uint64_t)(v3));
  t32 = fg161_gg_function_8(t30, t31, base + 0);
  t33 = fg161_tensor_alloc(t28, t29, t32);
  (void)(t33);
  }
fg161_gg_done:;
}
static uint64_t fg161_gg_function_59(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  t2 = ((int64_t)(((int64_t)UINT64_C(7))));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(v2));
  t5 = fg161_get(t1, t2, t3, t4);
  t0 = ((uint64_t)(t5));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static void fg161_gg_function_77(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  uint64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint64_t t16;
  uint8_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint64_t t21;
  uint64_t t22;
  uint64_t t23;
  uint8_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint64_t t28;
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
  uint64_t t40;
  uint64_t t41;
  uint64_t t42;
  uint8_t t43;
  int64_t t44;
  int64_t t45;
  uint64_t t46;
  uint64_t t47;
  uint64_t t48;
  uint8_t t49;
  uint8_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  uint64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  uint64_t t58;
  uint64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  uint64_t t64;
  uint64_t t65;
  uint8_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  uint64_t t71;
  int64_t t72;
  int64_t t73;
  uint8_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  uint64_t t82;
  uint64_t t83;
  int64_t t84;
  uint64_t t85;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  uint64_t v4;
  int64_t v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v2));
  t1 = ((int64_t)(((int64_t)UINT64_C(6))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg161_get(t0, t1, t2, t3);
  t5 = ((uint64_t)(t4));
  v3 = fg161_gg_function_3(t5, base + 0);
  t6 = ((int64_t)(v2));
  t7 = ((int64_t)(((int64_t)UINT64_C(6))));
  t8 = ((int64_t)(((int64_t)UINT64_C(2))));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = fg161_get(t6, t7, t8, t9);
  v4 = ((uint64_t)(t10));
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)UINT64_C(7))));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = fg161_gg_function_59(t11, t12, t13, base + 0);
  t15 = ((uint64_t)(t14));
  t16 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t15) != (t16));
  if (t17) {
  {
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(((int64_t)UINT64_C(5))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = fg161_gg_function_59(t18, t19, t20, base + 0);
  t22 = ((uint64_t)(t21));
  t23 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t24 = ((t22) == (t23));
  if (t24) {
  {
  t25 = ((int64_t)(v1));
  t26 = ((int64_t)(((int64_t)UINT64_C(7))));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = fg161_gg_function_59(t25, t26, t27, base + 0);
  v5 = fg161_gg_function_3(t28, base + 0);
  t29 = ((int64_t)(v5));
  t30 = ((int64_t)(((int64_t)UINT64_C(5))));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = fg161_gg_function_59(t29, t30, t31, base + 0);
  t33 = ((uint64_t)(t32));
  t34 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t35 = ((t33) == (t34));
  if (t35) {
  {
  goto fg161_gg_done;
  }
  }
  t36 = ((int64_t)(v1));
  fg161_view_init(t36);
  }
  }
  goto fg161_gg_done;
  }
  }
  t37 = ((int64_t)(v1));
  t38 = ((int64_t)(((int64_t)UINT64_C(6))));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = fg161_gg_function_59(t37, t38, t39, base + 0);
  t41 = ((uint64_t)(t40));
  t42 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t43 = ((t41) == (t42));
  if (t43) {
  {
  t44 = ((int64_t)(v0));
  t45 = ((int64_t)(v3));
  fg161_gg_function_61(t44, t45, base + 0);
  t46 = ((uint64_t)(v4));
  t47 = fg161_gg_function_9(base + 0);
  t48 = ((uint64_t)(t47));
  t49 = ((t46) != (t48));
  t50 = t49;
  if (t50) {
  t51 = ((int64_t)(v0));
  t52 = ((int64_t)(((int64_t)UINT64_C(1))));
  t53 = ((int64_t)(v3));
  t54 = fg161_gg_function_50(t51, t52, t53, base + 0);
  t55 = fg161_gg_function_3(t54, base + 0);
  t56 = ((int64_t)(((int64_t)UINT64_C(2))));
  t57 = ((int64_t)(v1));
  t58 = fg161_buft(t55, t56, t57);
  t59 = ((uint64_t)(t58));
  t60 = ((int64_t)(v2));
  t61 = ((int64_t)(((int64_t)UINT64_C(6))));
  t62 = ((int64_t)(((int64_t)UINT64_C(3))));
  t63 = ((int64_t)(((int64_t)UINT64_C(0))));
  t64 = fg161_get(t60, t61, t62, t63);
  t65 = ((uint64_t)(t64));
  t50 = ((t59) <= (t65));
  }
  t66 = t50;
  t67 = ((int64_t)(((int64_t)UINT64_C(31))));
  fg161_gg_function_6(t66, t67, base + 0);
  t68 = ((int64_t)(v0));
  t69 = ((int64_t)(((int64_t)UINT64_C(2))));
  t70 = ((int64_t)(v3));
  t71 = fg161_gg_function_50(t68, t69, t70, base + 0);
  v6 = fg161_gg_function_3(t71, base + 0);
  t72 = ((int64_t)(v6));
  t73 = ((int64_t)(((int64_t)UINT64_C(0))));
  t74 = ((t72) != (t73));
  t75 = ((int64_t)(((int64_t)UINT64_C(32))));
  fg161_gg_function_6(t74, t75, base + 0);
  t76 = ((int64_t)(v6));
  t77 = ((int64_t)(v1));
  t78 = ((int64_t)(v2));
  t79 = ((int64_t)(((int64_t)UINT64_C(6))));
  t80 = ((int64_t)(((int64_t)UINT64_C(1))));
  t81 = ((int64_t)(((int64_t)UINT64_C(0))));
  t82 = fg161_get(t78, t79, t80, t81);
  t83 = ((uint64_t)(t82));
  t84 = fg161_gg_function_3(t83, base + 0);
  t85 = ((uint64_t)(v4));
  fg161_gg_function_43(t76, t77, t84, t85, base + 0);
  }
  }
  }
fg161_gg_done:;
}
static void fg161_gg_function_44(int64_t p0, int64_t base) {
  uint64_t t0;
  int64_t t1;
  uint64_t t2;
  uint64_t t3;
  uint8_t t4;
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
  int64_t v0;
  int64_t v1;
  v0 = p0;
  {
  v1 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t0 = ((uint64_t)(((uint64_t)(v1))));
  t1 = ((int64_t)(((int64_t)UINT64_C(10))));
  t2 = fg161_constant(t1);
  t3 = ((uint64_t)(t2));
  t4 = ((t0) < (t3));
  t5 = t4;
  if (t5) {
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(v1));
  t8 = fg161_gg_function_34(t6, t7, base + 0);
  t9 = ((int64_t)(t8));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t9) != (t10));
  }
  if (!(t5)) goto label2;
  {
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(v1));
  t13 = fg161_gg_function_34(t11, t12, base + 0);
  fg161_reset_buffer(t13);
  t14 = v1;
  t15 = ((int64_t)(((int64_t)UINT64_C(1))));
  v1 = ((int64_t)((uint64_t)(t14) + (uint64_t)(t15)));
  }
  goto label1;
label2:;
  }
fg161_gg_done:;
}
static void fg161_gg_function_48(int64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(4))));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(v2));
  t4 = ((uint64_t)(v3));
  fg161_put(t0, t1, t2, t3, t4);
  }
fg161_gg_done:;
}
static void fg161_gg_function_33(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint64_t t3;
  int64_t t4;
  uint64_t t5;
  uint64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  goto fg161_gg_done;
  }
  }
  v1 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t3 = ((uint64_t)(((uint64_t)(v1))));
  t4 = ((int64_t)(((int64_t)UINT64_C(10))));
  t5 = fg161_constant(t4);
  t6 = ((uint64_t)(t5));
  if (!(((t3) < (t6)))) goto label2;
  {
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(v1));
  t9 = fg161_gg_function_34(t7, t8, base + 0);
  fg161_free_buffer(t9);
  t10 = v1;
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  v1 = ((int64_t)((uint64_t)(t10) + (uint64_t)(t11)));
  }
  goto label1;
label2:;
  t12 = ((int64_t)(v0));
  fg161_release(t12);
  }
fg161_gg_done:;
}
static uint64_t fg161_gg_function_38(int64_t p0, uint64_t p1, uint64_t p2, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint8_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint8_t t11;
  uint64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  uint64_t t20;
  uint64_t t21;
  uint64_t t22;
  uint8_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  uint64_t t27;
  uint64_t t28;
  uint64_t t29;
  uint8_t t30;
  int64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((uint64_t)(v2));
  t2 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((uint64_t)(v1));
  goto fg161_gg_done;
  }
  }
  t4 = ((uint64_t)(v1));
  t5 = fg161_gg_function_9(base + 0);
  t6 = ((uint64_t)(t5));
  t7 = ((uint64_t)(((int64_t)UINT64_C(3))));
  t8 = ((uint64_t)(((uint64_t)((uint64_t)(t6) - (uint64_t)(t7)))));
  t9 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t10 = ((uint64_t)(((uint64_t)fg161_gg_udiv(t8, t9))));
  t11 = ((t4) > (t10));
  if (t11) {
  {
  v3 = fg161_gg_function_9(base + 0);
  }
  } else {
  {
  t12 = ((uint64_t)(v1));
  t13 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v3 = ((uint64_t)(((uint64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  }
  }
  t14 = ((uint64_t)(v2));
  t15 = ((uint64_t)(v3));
  t16 = ((t14) < (t15));
  if (t16) {
  {
  v3 = ((uint64_t)(v2));
  }
  }
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = fg161_buft(t17, t18, t19);
  v4 = ((uint64_t)(t20));
  t21 = ((uint64_t)(v4));
  t22 = ((uint64_t)(v3));
  t23 = ((t21) < (t22));
  if (t23) {
  {
  t24 = ((int64_t)(v0));
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = fg161_buft(t24, t25, t26);
  v3 = ((uint64_t)(t27));
  }
  }
  t28 = ((uint64_t)(v3));
  t29 = ((uint64_t)(v1));
  t30 = ((t28) > (t29));
  if (t30) {
  {
  t0 = ((uint64_t)(v3));
  }
  } else {
  {
  t0 = ((uint64_t)(v1));
  }
  }
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static uint64_t fg161_gg_function_21(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(v2));
  t5 = fg161_get(t1, t2, t3, t4);
  t0 = ((uint64_t)(t5));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static uint64_t fg161_gg_function_16(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  t2 = ((int64_t)(((int64_t)UINT64_C(2))));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(v2));
  t5 = fg161_get(t1, t2, t3, t4);
  t0 = ((uint64_t)(t5));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static int64_t fg161_gg_function_40(int64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint64_t t2;
  uint64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  uint64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint64_t t23;
  int64_t t24;
  uint64_t t25;
  uint64_t t26;
  uint8_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  uint64_t t32;
  uint64_t t33;
  uint64_t t34;
  uint8_t t35;
  uint8_t t36;
  uint64_t t37;
  uint64_t t38;
  uint8_t t39;
  int64_t t40;
  uint64_t t41;
  uint64_t t42;
  int64_t t43;
  uint64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  uint8_t t48;
  uint8_t t49;
  uint64_t t50;
  uint64_t t51;
  uint8_t t52;
  int64_t t53;
  uint64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  uint64_t t60;
  int64_t t61;
  int64_t t62;
  uint8_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  int64_t v4;
  int64_t v5;
  uint64_t v6;
  uint64_t v7;
  int64_t v8;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(3))));
  t2 = fg161_constant(t1);
  t3 = ((uint64_t)(t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(1))));
  t5 = fg161_heap(t3, t4);
  v4 = ((int64_t)(t5));
  t6 = ((int64_t)(v4));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t6) == (t7));
  if (t8) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg161_gg_done;
  }
  }
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t9 = ((int64_t)(v5));
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = fg161_gg_function_21(t10, t11, t12, base + 0);
  t14 = fg161_gg_function_3(t13, base + 0);
  t15 = ((int64_t)(t14));
  if (!(((t9) < (t15)))) goto label2;
  {
  t16 = ((int64_t)(v1));
  t17 = ((int64_t)(((int64_t)UINT64_C(3))));
  t18 = ((int64_t)(v5));
  t19 = fg161_gg_function_21(t16, t17, t18, base + 0);
  t20 = fg161_gg_function_3(t19, base + 0);
  t21 = ((int64_t)(((int64_t)UINT64_C(1))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  v6 = fg161_gg_function_16(t20, t21, t22, base + 0);
  v7 = ((uint64_t)(v6));
  t23 = ((uint64_t)(((uint64_t)(v2))));
  t24 = ((int64_t)(((int64_t)UINT64_C(15))));
  t25 = fg161_constant(t24);
  t26 = ((uint64_t)(t25));
  t27 = ((t23) == (t26));
  t28 = t27;
  if (t28) {
  t29 = ((int64_t)(v1));
  t30 = ((int64_t)(((int64_t)UINT64_C(2))));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = fg161_gg_function_21(t29, t30, t31, base + 0);
  t33 = ((uint64_t)(t32));
  t34 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t28 = ((t33) == (t34));
  }
  t35 = t28;
  t36 = t35;
  if (t36) {
  t37 = ((uint64_t)(v3));
  t38 = ((uint64_t)(v6));
  t36 = ((t37) > (t38));
  }
  t39 = t36;
  if (t39) {
  {
  t40 = ((int64_t)(v0));
  t41 = ((uint64_t)(v6));
  t42 = ((uint64_t)(v3));
  v7 = fg161_gg_function_38(t40, t41, t42, base + 0);
  }
  }
  t43 = ((int64_t)(v0));
  t44 = ((uint64_t)(v7));
  t45 = fg161_alloc_buffer(t43, t44);
  v8 = ((int64_t)(t45));
  t46 = ((int64_t)(v8));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = ((t46) == (t47));
  t49 = t48;
  if (t49) {
  t50 = ((uint64_t)(v7));
  t51 = ((uint64_t)(v6));
  t49 = ((t50) > (t51));
  }
  t52 = t49;
  if (t52) {
  {
  t53 = ((int64_t)(v0));
  t54 = ((uint64_t)(v6));
  t55 = fg161_alloc_buffer(t53, t54);
  v8 = ((int64_t)(t55));
  }
  }
  t56 = ((int64_t)(v4));
  t57 = ((int64_t)(((int64_t)UINT64_C(3))));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = ((int64_t)(v5));
  t60 = ((uint64_t)(((uint64_t)(v8))));
  fg161_put(t56, t57, t58, t59, t60);
  t61 = ((int64_t)(v8));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t63 = ((t61) == (t62));
  if (t63) {
  {
  t64 = ((int64_t)(v4));
  fg161_gg_function_33(t64, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg161_gg_done;
  }
  }
  t65 = ((int64_t)(v8));
  t66 = ((int64_t)(v2));
  fg161_usage(t65, t66);
  t67 = v5;
  t68 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t67) + (uint64_t)(t68)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v4));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static uint64_t fg161_gg_function_32(int64_t p0, int64_t p1, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint64_t t9;
  int64_t t10;
  int64_t t11;
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(9))));
  fg161_gg_function_6(t3, t4, base + 0);
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = fg161_gg_function_21(t6, t7, t8, base + 0);
  t10 = fg161_gg_function_3(t9, base + 0);
  t11 = ((int64_t)(t10));
  t12 = ((t5) < (t11));
  if (t12) {
  {
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(3))));
  t15 = ((int64_t)(v1));
  t16 = fg161_gg_function_21(t13, t14, t15, base + 0);
  t17 = fg161_gg_function_3(t16, base + 0);
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t0 = fg161_gg_function_16(t17, t18, t19, base + 0);
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static uint64_t fg161_gg_function_36(int64_t p0, int64_t p1, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint64_t t9;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  v2 = fg161_gg_function_34(t1, t2, base + 0);
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) != (t4));
  if (t5) {
  {
  t6 = ((int64_t)(v2));
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = fg161_buffer(t6, t7, t8);
  t0 = ((uint64_t)(t9));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static uint64_t fg161_gg_function_57(int64_t p0, int64_t p1, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint64_t t5;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(5))));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = fg161_get(t1, t2, t3, t4);
  t0 = ((uint64_t)(t5));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static int64_t fg161_gg_function_53(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint64_t t3;
  uint64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = fg161_hash_slot(t1, t2);
  v2 = ((uint64_t)(t3));
  t4 = ((uint64_t)(v2));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(5))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg161_gg_function_50(t5, t6, t7, base + 0);
  t9 = fg161_gg_function_3(t8, base + 0);
  t10 = ((int64_t)(((int64_t)UINT64_C(9))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = fg161_get(t9, t10, t11, t12);
  t14 = ((uint64_t)(t13));
  t15 = ((t4) < (t14));
  t16 = ((int64_t)(((int64_t)UINT64_C(4))));
  fg161_gg_function_6(t15, t16, base + 0);
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(((int64_t)(v2))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = fg161_record(t17, t18, t19, t20);
  t0 = ((int64_t)(t21));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static void fg161_gg_function_75(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint8_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  uint64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  uint64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  uint64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  uint64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  uint64_t t72;
  int64_t t73;
  uint64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  uint64_t t78;
  uint64_t t79;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (!t3) {
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(7))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = fg161_gg_function_59(t4, t5, t6, base + 0);
  t8 = ((uint64_t)(t7));
  t9 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t8) != (t9));
  }
  t10 = t3;
  t11 = t10;
  if (!t11) {
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(((int64_t)UINT64_C(6))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = fg161_gg_function_59(t12, t13, t14, base + 0);
  t16 = ((uint64_t)(t15));
  t17 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t16) != (t17));
  }
  t18 = t11;
  if (t18) {
  {
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)(((int64_t)UINT64_C(6))));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((int64_t)UINT64_C(1));
  t24 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t23)))))));
  fg161_put(t19, t20, t21, t22, t24);
  t25 = ((int64_t)(v2));
  t26 = ((int64_t)(((int64_t)UINT64_C(6))));
  t27 = ((int64_t)(((int64_t)UINT64_C(1))));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = ((int64_t)UINT64_C(1));
  t30 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t29)))))));
  fg161_put(t25, t26, t27, t28, t30);
  t31 = ((int64_t)(v2));
  t32 = ((int64_t)(((int64_t)UINT64_C(6))));
  t33 = ((int64_t)(((int64_t)UINT64_C(2))));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = fg161_gg_function_9(base + 0);
  fg161_put(t31, t32, t33, t34, t35);
  t36 = ((int64_t)(v2));
  t37 = ((int64_t)(((int64_t)UINT64_C(6))));
  t38 = ((int64_t)(((int64_t)UINT64_C(3))));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg161_put(t36, t37, t38, t39, t40);
  goto fg161_gg_done;
  }
  }
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(v1));
  v3 = fg161_gg_function_53(t41, t42, base + 0);
  t43 = ((int64_t)(v2));
  t44 = ((int64_t)(((int64_t)UINT64_C(6))));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = ((int64_t)(v3));
  t48 = ((int64_t)(((int64_t)UINT64_C(2))));
  t49 = fg161_gg_function_57(t47, t48, base + 0);
  fg161_put(t43, t44, t45, t46, t49);
  t50 = ((int64_t)(v2));
  t51 = ((int64_t)(((int64_t)UINT64_C(6))));
  t52 = ((int64_t)(((int64_t)UINT64_C(1))));
  t53 = ((int64_t)(((int64_t)UINT64_C(0))));
  t54 = ((int64_t)(v3));
  t55 = ((int64_t)(((int64_t)UINT64_C(3))));
  t56 = fg161_gg_function_57(t54, t55, base + 0);
  fg161_put(t50, t51, t52, t53, t56);
  t57 = ((int64_t)(v2));
  t58 = ((int64_t)(((int64_t)UINT64_C(6))));
  t59 = ((int64_t)(((int64_t)UINT64_C(2))));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = ((int64_t)(v3));
  t62 = ((int64_t)(((int64_t)UINT64_C(4))));
  t63 = fg161_gg_function_57(t61, t62, base + 0);
  fg161_put(t57, t58, t59, t60, t63);
  t64 = ((int64_t)(v2));
  t65 = ((int64_t)(((int64_t)UINT64_C(6))));
  t66 = ((int64_t)(((int64_t)UINT64_C(3))));
  t67 = ((int64_t)(((int64_t)UINT64_C(0))));
  t68 = ((int64_t)(v0));
  t69 = ((int64_t)(((int64_t)UINT64_C(1))));
  t70 = ((int64_t)(v3));
  t71 = ((int64_t)(((int64_t)UINT64_C(2))));
  t72 = fg161_gg_function_57(t70, t71, base + 0);
  t73 = fg161_gg_function_3(t72, base + 0);
  t74 = fg161_gg_function_50(t68, t69, t73, base + 0);
  t75 = fg161_gg_function_3(t74, base + 0);
  t76 = ((int64_t)(((int64_t)UINT64_C(2))));
  t77 = ((int64_t)(v1));
  t78 = fg161_buft(t75, t76, t77);
  t79 = ((uint64_t)(t78));
  fg161_put(t64, t65, t66, t67, t79);
  }
fg161_gg_done:;
}
static void fg161_gg_function_63(int64_t p0, int64_t p1, uint64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(5))));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((uint64_t)(v2));
  fg161_put(t0, t1, t2, t3, t4);
  }
fg161_gg_done:;
}
static void fg161_gg_function_18(int64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(2))));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(v2));
  t4 = ((uint64_t)(v3));
  fg161_put(t0, t1, t2, t3, t4);
  }
fg161_gg_done:;
}
static void fg161_gg_function_15(int64_t p0, uint64_t p1, uint64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  uint8_t t7;
  uint64_t t8;
  int64_t t9;
  uint64_t t10;
  uint64_t t11;
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  uint8_t t17;
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
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  uint64_t t45;
  int64_t t46;
  int64_t t47;
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
  int64_t t59;
  int64_t t60;
  uint64_t t61;
  int64_t v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg161_gg_function_16(t0, t1, t2, base + 0);
  v3 = fg161_gg_function_3(t3, base + 0);
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) >= (t5));
  t7 = t6;
  if (t7) {
  t8 = ((uint64_t)(((uint64_t)(v3))));
  t9 = ((int64_t)(((int64_t)UINT64_C(11))));
  t10 = fg161_constant(t9);
  t11 = ((uint64_t)(t10));
  t7 = ((t8) < (t11));
  }
  t12 = t7;
  t13 = ((int64_t)(((int64_t)UINT64_C(6))));
  fg161_gg_function_6(t12, t13, base + 0);
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t14 = ((int64_t)(v4));
  t15 = ((int64_t)(v3));
  t16 = ((t14) < (t15));
  t17 = t16;
  if (t17) {
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(2))));
  t20 = ((int64_t)(v4));
  t21 = fg161_gg_function_16(t18, t19, t20, base + 0);
  t22 = ((uint64_t)(t21));
  t23 = ((uint64_t)(v1));
  t17 = ((t22) < (t23));
  }
  if (!(t17)) goto label2;
  {
  t24 = v4;
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t24) + (uint64_t)(t25)));
  }
  goto label1;
label2:;
  v5 = ((int64_t)(v3));
label4:;
  t26 = ((int64_t)(v5));
  t27 = ((int64_t)(v4));
  if (!(((t26) > (t27)))) goto label5;
  {
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(2))));
  t30 = ((int64_t)(v5));
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(2))));
  t33 = ((int64_t)(v5));
  t34 = ((int64_t)(((int64_t)UINT64_C(1))));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t33) - (uint64_t)(t34)))));
  t36 = fg161_gg_function_16(t31, t32, t35, base + 0);
  fg161_gg_function_18(t28, t29, t30, t36, base + 0);
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(3))));
  t39 = ((int64_t)(v5));
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(3))));
  t42 = ((int64_t)(v5));
  t43 = ((int64_t)(((int64_t)UINT64_C(1))));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) - (uint64_t)(t43)))));
  t45 = fg161_gg_function_16(t40, t41, t44, base + 0);
  fg161_gg_function_18(t37, t38, t39, t45, base + 0);
  t46 = v5;
  t47 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t46) - (uint64_t)(t47)));
  }
  goto label4;
label5:;
  t48 = ((int64_t)(v0));
  t49 = ((int64_t)(((int64_t)UINT64_C(2))));
  t50 = ((int64_t)(v4));
  t51 = ((uint64_t)(v1));
  fg161_gg_function_18(t48, t49, t50, t51, base + 0);
  t52 = ((int64_t)(v0));
  t53 = ((int64_t)(((int64_t)UINT64_C(3))));
  t54 = ((int64_t)(v4));
  t55 = ((uint64_t)(v2));
  fg161_gg_function_18(t52, t53, t54, t55, base + 0);
  t56 = ((int64_t)(v0));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = ((int64_t)(v3));
  t60 = ((int64_t)(((int64_t)UINT64_C(1))));
  t61 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t59) + (uint64_t)(t60)))))));
  fg161_gg_function_18(t56, t57, t58, t61, base + 0);
  }
fg161_gg_done:;
}
static void fg161_gg_function_19(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
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
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  uint64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  uint64_t t41;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg161_gg_function_16(t0, t1, t2, base + 0);
  v2 = fg161_gg_function_3(t3, base + 0);
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) >= (t5));
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(v2));
  t7 = ((t8) < (t9));
  }
  t10 = t7;
  t11 = ((int64_t)(((int64_t)UINT64_C(5))));
  fg161_gg_function_6(t10, t11, base + 0);
  v3 = ((int64_t)(v1));
label1:;
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) - (uint64_t)(t14)))));
  if (!(((t12) < (t15)))) goto label2;
  {
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(2))));
  t18 = ((int64_t)(v3));
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(2))));
  t21 = ((int64_t)(v3));
  t22 = ((int64_t)(((int64_t)UINT64_C(1))));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t22)))));
  t24 = fg161_gg_function_16(t19, t20, t23, base + 0);
  fg161_gg_function_18(t16, t17, t18, t24, base + 0);
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(3))));
  t27 = ((int64_t)(v3));
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(3))));
  t30 = ((int64_t)(v3));
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t31)))));
  t33 = fg161_gg_function_16(t28, t29, t32, base + 0);
  fg161_gg_function_18(t25, t26, t27, t33, base + 0);
  t34 = v3;
  t35 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t34) + (uint64_t)(t35)));
  }
  goto label1;
label2:;
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = ((int64_t)(v2));
  t40 = ((int64_t)(((int64_t)UINT64_C(1))));
  t41 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t39) - (uint64_t)(t40)))))));
  fg161_gg_function_18(t36, t37, t38, t41, base + 0);
  }
fg161_gg_done:;
}
static void fg161_gg_function_5(uint64_t p0, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint8_t t2;
  uint8_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint8_t t10;
  int64_t t11;
  uint64_t v0;
  v0 = p0;
  {
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((uint64_t)(v0));
  t5 = ((uint64_t)(v0));
  t6 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t7 = ((uint64_t)(((uint64_t)((uint64_t)(t5) - (uint64_t)(t6)))));
  t8 = ((uint64_t)(((t4) & (t7))));
  t9 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t8) == (t9));
  }
  t10 = t3;
  t11 = ((int64_t)(((int64_t)UINT64_C(3))));
  fg161_gg_function_6(t10, t11, base + 0);
  }
fg161_gg_done:;
}
static uint64_t fg161_gg_function_4(uint64_t p0, uint64_t p1, uint64_t p2, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint64_t t11;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((uint64_t)(v2));
  fg161_gg_function_5(t1, base + 0);
  t2 = ((uint64_t)(v0));
  t3 = ((uint64_t)(v1));
  v3 = fg161_gg_function_8(t2, t3, base + 0);
  t4 = ((uint64_t)(v2));
  t5 = ((uint64_t)(v3));
  t6 = ((uint64_t)(v2));
  t7 = ((uint64_t)(((uint64_t)fg161_gg_umod(t5, t6))));
  t8 = ((uint64_t)(((uint64_t)((uint64_t)(t4) - (uint64_t)(t7)))));
  t9 = ((uint64_t)(v2));
  v4 = ((uint64_t)(((uint64_t)fg161_gg_umod(t8, t9))));
  t10 = ((uint64_t)(v1));
  t11 = ((uint64_t)(v4));
  t0 = fg161_gg_function_8(t10, t11, base + 0);
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static void fg161_gg_function_26(int64_t p0, int64_t p1, uint64_t p2, uint64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  int64_t t12;
  uint64_t t13;
  uint64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  uint64_t t18;
  uint64_t t19;
  uint64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint64_t t24;
  uint64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  uint64_t t29;
  uint64_t t30;
  uint8_t t31;
  int64_t t32;
  uint64_t t33;
  uint64_t t34;
  uint8_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  uint64_t t49;
  uint64_t t50;
  uint8_t t51;
  uint64_t t52;
  uint64_t t53;
  uint64_t t54;
  uint64_t t55;
  uint8_t t56;
  uint8_t t57;
  uint64_t t58;
  uint64_t t59;
  uint8_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  uint64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  uint64_t t77;
  uint64_t t78;
  uint64_t t79;
  uint64_t t80;
  uint64_t t81;
  uint8_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  uint64_t t86;
  uint64_t t87;
  uint64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  uint64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  uint8_t t98;
  uint8_t t99;
  uint64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  uint64_t t104;
  uint64_t t105;
  uint64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  uint64_t t112;
  uint64_t t113;
  uint8_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
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
  uint64_t t133;
  uint64_t t134;
  uint8_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  uint64_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  uint64_t t143;
  uint64_t t144;
  uint64_t t145;
  int64_t t146;
  int64_t t147;
  uint8_t t148;
  uint8_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  uint64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  uint64_t t161;
  uint64_t t162;
  uint64_t t163;
  uint64_t t164;
  uint8_t t165;
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
  uint64_t t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  uint64_t t180;
  uint64_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  uint64_t t187;
  uint64_t t188;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  uint64_t v5;
  int64_t v6;
  int64_t v7;
  uint64_t v8;
  uint64_t v9;
  uint64_t v10;
  uint64_t v11;
  uint64_t v12;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(2))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg161_gg_function_21(t5, t6, t7, base + 0);
  t9 = fg161_gg_function_3(t8, base + 0);
  t10 = ((int64_t)(t9));
  t3 = ((t4) < (t10));
  }
  t11 = t3;
  t12 = ((int64_t)(((int64_t)UINT64_C(12))));
  fg161_gg_function_6(t11, t12, base + 0);
  t13 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t14 = ((uint64_t)(v3));
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = fg161_gg_function_21(t15, t16, t17, base + 0);
  v4 = fg161_gg_function_4(t13, t14, t18, base + 0);
  t19 = ((uint64_t)(v2));
  t20 = ((uint64_t)(v4));
  v5 = fg161_gg_function_8(t19, t20, base + 0);
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(3))));
  t23 = ((int64_t)(v1));
  t24 = fg161_gg_function_21(t21, t22, t23, base + 0);
  v6 = fg161_gg_function_3(t24, base + 0);
  t25 = ((uint64_t)(v5));
  t26 = ((int64_t)(v6));
  t27 = ((int64_t)(((int64_t)UINT64_C(1))));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = fg161_gg_function_16(t26, t27, t28, base + 0);
  t30 = ((uint64_t)(t29));
  t31 = ((t25) <= (t30));
  t32 = ((int64_t)(((int64_t)UINT64_C(13))));
  fg161_gg_function_6(t31, t32, base + 0);
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = ((uint64_t)(v4));
  t34 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t35 = ((t33) != (t34));
  if (t35) {
  {
label1:;
  t36 = ((int64_t)(v7));
  t37 = ((int64_t)(v6));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = fg161_gg_function_16(t37, t38, t39, base + 0);
  t41 = fg161_gg_function_3(t40, base + 0);
  t42 = ((int64_t)(t41));
  if (!(((t36) < (t42)))) goto label2;
  {
  t43 = ((int64_t)(v6));
  t44 = ((int64_t)(((int64_t)UINT64_C(2))));
  t45 = ((int64_t)(v7));
  v8 = fg161_gg_function_16(t43, t44, t45, base + 0);
  t46 = ((int64_t)(v6));
  t47 = ((int64_t)(((int64_t)UINT64_C(3))));
  t48 = ((int64_t)(v7));
  v9 = fg161_gg_function_16(t46, t47, t48, base + 0);
  t49 = ((uint64_t)(v9));
  t50 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t51 = ((t49) != (t50));
  if (t51) {
  {
  t52 = ((uint64_t)(v8));
  t53 = ((uint64_t)(v9));
  v10 = fg161_gg_function_8(t52, t53, base + 0);
  t54 = ((uint64_t)(v5));
  t55 = ((uint64_t)(v8));
  t56 = ((t54) <= (t55));
  t57 = t56;
  if (!t57) {
  t58 = ((uint64_t)(v2));
  t59 = ((uint64_t)(v10));
  t57 = ((t58) >= (t59));
  }
  t60 = t57;
  t61 = ((int64_t)(((int64_t)UINT64_C(14))));
  fg161_gg_function_6(t60, t61, base + 0);
  }
  }
  t62 = v7;
  t63 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)((uint64_t)(t62) + (uint64_t)(t63)));
  }
  goto label1;
label2:;
  }
  }
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t64 = ((int64_t)(v7));
  t65 = ((int64_t)(v6));
  t66 = ((int64_t)(((int64_t)UINT64_C(0))));
  t67 = ((int64_t)(((int64_t)UINT64_C(0))));
  t68 = fg161_gg_function_16(t65, t66, t67, base + 0);
  t69 = fg161_gg_function_3(t68, base + 0);
  t70 = ((int64_t)(t69));
  if (!(((t64) < (t70)))) goto label5;
  {
  t71 = ((int64_t)(v6));
  t72 = ((int64_t)(((int64_t)UINT64_C(2))));
  t73 = ((int64_t)(v7));
  v11 = fg161_gg_function_16(t71, t72, t73, base + 0);
  t74 = ((int64_t)(v6));
  t75 = ((int64_t)(((int64_t)UINT64_C(3))));
  t76 = ((int64_t)(v7));
  v12 = fg161_gg_function_16(t74, t75, t76, base + 0);
  t77 = ((uint64_t)(v11));
  t78 = ((uint64_t)(v12));
  t79 = fg161_gg_function_8(t77, t78, base + 0);
  t80 = ((uint64_t)(t79));
  t81 = ((uint64_t)(v2));
  t82 = ((t80) == (t81));
  if (t82) {
  {
  t83 = ((int64_t)(v6));
  t84 = ((int64_t)(((int64_t)UINT64_C(3))));
  t85 = ((int64_t)(v7));
  t86 = ((uint64_t)(v12));
  t87 = ((uint64_t)(v4));
  t88 = fg161_gg_function_8(t86, t87, base + 0);
  fg161_gg_function_18(t83, t84, t85, t88, base + 0);
  t89 = ((int64_t)(v7));
  t90 = ((int64_t)(v6));
  t91 = ((int64_t)(((int64_t)UINT64_C(0))));
  t92 = ((int64_t)(((int64_t)UINT64_C(0))));
  t93 = fg161_gg_function_16(t90, t91, t92, base + 0);
  t94 = fg161_gg_function_3(t93, base + 0);
  t95 = ((int64_t)(t94));
  t96 = ((int64_t)(((int64_t)UINT64_C(1))));
  t97 = ((int64_t)(((int64_t)((uint64_t)(t95) - (uint64_t)(t96)))));
  t98 = ((t89) < (t97));
  t99 = t98;
  if (t99) {
  t100 = ((uint64_t)(v11));
  t101 = ((int64_t)(v6));
  t102 = ((int64_t)(((int64_t)UINT64_C(3))));
  t103 = ((int64_t)(v7));
  t104 = fg161_gg_function_16(t101, t102, t103, base + 0);
  t105 = fg161_gg_function_8(t100, t104, base + 0);
  t106 = ((uint64_t)(t105));
  t107 = ((int64_t)(v6));
  t108 = ((int64_t)(((int64_t)UINT64_C(2))));
  t109 = ((int64_t)(v7));
  t110 = ((int64_t)(((int64_t)UINT64_C(1))));
  t111 = ((int64_t)(((int64_t)((uint64_t)(t109) + (uint64_t)(t110)))));
  t112 = fg161_gg_function_16(t107, t108, t111, base + 0);
  t113 = ((uint64_t)(t112));
  t99 = ((t106) == (t113));
  }
  t114 = t99;
  if (t114) {
  {
  t115 = ((int64_t)(v6));
  t116 = ((int64_t)(((int64_t)UINT64_C(3))));
  t117 = ((int64_t)(v7));
  t118 = ((int64_t)(v6));
  t119 = ((int64_t)(((int64_t)UINT64_C(3))));
  t120 = ((int64_t)(v7));
  t121 = fg161_gg_function_16(t118, t119, t120, base + 0);
  t122 = ((int64_t)(v6));
  t123 = ((int64_t)(((int64_t)UINT64_C(3))));
  t124 = ((int64_t)(v7));
  t125 = ((int64_t)(((int64_t)UINT64_C(1))));
  t126 = ((int64_t)(((int64_t)((uint64_t)(t124) + (uint64_t)(t125)))));
  t127 = fg161_gg_function_16(t122, t123, t126, base + 0);
  t128 = fg161_gg_function_8(t121, t127, base + 0);
  fg161_gg_function_18(t115, t116, t117, t128, base + 0);
  t129 = ((int64_t)(v6));
  t130 = ((int64_t)(v7));
  t131 = ((int64_t)(((int64_t)UINT64_C(1))));
  t132 = ((int64_t)(((int64_t)((uint64_t)(t130) + (uint64_t)(t131)))));
  fg161_gg_function_19(t129, t132, base + 0);
  }
  }
  goto fg161_gg_done;
  }
  }
  t133 = ((uint64_t)(v5));
  t134 = ((uint64_t)(v11));
  t135 = ((t133) == (t134));
  if (t135) {
  {
  t136 = ((int64_t)(v6));
  t137 = ((int64_t)(((int64_t)UINT64_C(2))));
  t138 = ((int64_t)(v7));
  t139 = ((uint64_t)(v2));
  fg161_gg_function_18(t136, t137, t138, t139, base + 0);
  t140 = ((int64_t)(v6));
  t141 = ((int64_t)(((int64_t)UINT64_C(3))));
  t142 = ((int64_t)(v7));
  t143 = ((uint64_t)(v12));
  t144 = ((uint64_t)(v4));
  t145 = fg161_gg_function_8(t143, t144, base + 0);
  fg161_gg_function_18(t140, t141, t142, t145, base + 0);
  t146 = ((int64_t)(v7));
  t147 = ((int64_t)(((int64_t)UINT64_C(0))));
  t148 = ((t146) > (t147));
  t149 = t148;
  if (t149) {
  t150 = ((int64_t)(v6));
  t151 = ((int64_t)(((int64_t)UINT64_C(2))));
  t152 = ((int64_t)(v7));
  t153 = ((int64_t)(((int64_t)UINT64_C(1))));
  t154 = ((int64_t)(((int64_t)((uint64_t)(t152) - (uint64_t)(t153)))));
  t155 = fg161_gg_function_16(t150, t151, t154, base + 0);
  t156 = ((int64_t)(v6));
  t157 = ((int64_t)(((int64_t)UINT64_C(3))));
  t158 = ((int64_t)(v7));
  t159 = ((int64_t)(((int64_t)UINT64_C(1))));
  t160 = ((int64_t)(((int64_t)((uint64_t)(t158) - (uint64_t)(t159)))));
  t161 = fg161_gg_function_16(t156, t157, t160, base + 0);
  t162 = fg161_gg_function_8(t155, t161, base + 0);
  t163 = ((uint64_t)(t162));
  t164 = ((uint64_t)(v2));
  t149 = ((t163) == (t164));
  }
  t165 = t149;
  if (t165) {
  {
  t166 = ((int64_t)(v6));
  t167 = ((int64_t)(((int64_t)UINT64_C(3))));
  t168 = ((int64_t)(v7));
  t169 = ((int64_t)(((int64_t)UINT64_C(1))));
  t170 = ((int64_t)(((int64_t)((uint64_t)(t168) - (uint64_t)(t169)))));
  t171 = ((int64_t)(v6));
  t172 = ((int64_t)(((int64_t)UINT64_C(3))));
  t173 = ((int64_t)(v7));
  t174 = ((int64_t)(((int64_t)UINT64_C(1))));
  t175 = ((int64_t)(((int64_t)((uint64_t)(t173) - (uint64_t)(t174)))));
  t176 = fg161_gg_function_16(t171, t172, t175, base + 0);
  t177 = ((int64_t)(v6));
  t178 = ((int64_t)(((int64_t)UINT64_C(3))));
  t179 = ((int64_t)(v7));
  t180 = fg161_gg_function_16(t177, t178, t179, base + 0);
  t181 = fg161_gg_function_8(t176, t180, base + 0);
  fg161_gg_function_18(t166, t167, t170, t181, base + 0);
  t182 = ((int64_t)(v6));
  t183 = ((int64_t)(v7));
  fg161_gg_function_19(t182, t183, base + 0);
  }
  }
  goto fg161_gg_done;
  }
  }
  t184 = v7;
  t185 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)((uint64_t)(t184) + (uint64_t)(t185)));
  }
  goto label4;
label5:;
  t186 = ((int64_t)(v6));
  t187 = ((uint64_t)(v2));
  t188 = ((uint64_t)(v4));
  fg161_gg_function_15(t186, t187, t188, base + 0);
  }
fg161_gg_done:;
}
static void fg161_gg_function_66(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint64_t t3;
  uint64_t t4;
  int64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  uint64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  uint64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  uint64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  uint64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  uint64_t t37;
  uint64_t t38;
  int64_t t39;
  int64_t t40;
  uint64_t t41;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(((int64_t)UINT64_C(4))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg161_gg_function_59(t0, t1, t2, base + 0);
  t4 = ((uint64_t)(t3));
  t5 = ((int64_t)(((int64_t)UINT64_C(14))));
  t6 = fg161_constant(t5);
  t7 = ((uint64_t)(t6));
  t8 = ((uint64_t)(((t4) & (t7))));
  t9 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t8) != (t9));
  if (t10) {
  {
  goto fg161_gg_done;
  }
  }
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(v1));
  v2 = fg161_gg_function_53(t11, t12, base + 0);
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(((int64_t)UINT64_C(2))));
  t15 = fg161_gg_function_57(t13, t14, base + 0);
  v3 = fg161_gg_function_3(t15, base + 0);
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(v3));
  fg161_gg_function_61(t16, t17, base + 0);
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(3))));
  t20 = ((int64_t)(v3));
  t21 = fg161_gg_function_50(t18, t19, t20, base + 0);
  t22 = fg161_gg_function_3(t21, base + 0);
  t23 = ((int64_t)(v2));
  t24 = ((int64_t)(((int64_t)UINT64_C(3))));
  t25 = fg161_gg_function_57(t23, t24, base + 0);
  t26 = fg161_gg_function_3(t25, base + 0);
  t27 = ((int64_t)(v2));
  t28 = ((int64_t)(((int64_t)UINT64_C(4))));
  t29 = fg161_gg_function_57(t27, t28, base + 0);
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  t32 = ((int64_t)(v3));
  t33 = fg161_gg_function_50(t30, t31, t32, base + 0);
  t34 = fg161_gg_function_3(t33, base + 0);
  t35 = ((int64_t)(((int64_t)UINT64_C(2))));
  t36 = ((int64_t)(v1));
  t37 = fg161_buft(t34, t35, t36);
  t38 = ((uint64_t)(t37));
  fg161_gg_function_26(t22, t26, t29, t38, base + 0);
  t39 = ((int64_t)(v2));
  t40 = ((int64_t)(((int64_t)UINT64_C(5))));
  t41 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg161_gg_function_63(t39, t40, t41, base + 0);
  }
fg161_gg_done:;
}
static void fg161_gg_function_72(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  uint8_t t14;
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
  uint64_t t26;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(v1));
  t2 = fg161_gg_function_57(t0, t1, base + 0);
  v3 = fg161_gg_function_3(t2, base + 0);
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) > (t4));
  t6 = t5;
  if (t6) {
  t7 = ((int64_t)(v3));
  t8 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  t6 = ((t7) < (t8));
  }
  t9 = t6;
  t10 = t9;
  if (!t10) {
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t11) < (t12));
  t14 = t13;
  if (t14) {
  t15 = ((int64_t)(v3));
  t16 = ((int64_t)UINT64_C(2147483647));
  t17 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t16)))));
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) - (uint64_t)(t18)))));
  t14 = ((t15) > (t19));
  }
  t10 = t14;
  }
  t20 = t10;
  t21 = ((int64_t)(((int64_t)UINT64_C(25))));
  fg161_gg_function_6(t20, t21, base + 0);
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(v1));
  t24 = ((int64_t)(v3));
  t25 = ((int64_t)(v2));
  t26 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t24) + (uint64_t)(t25)))))));
  fg161_gg_function_63(t22, t23, t26, base + 0);
  }
fg161_gg_done:;
}
static void fg161_gg_function_25(int64_t p0, int64_t p1, uint64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint64_t t9;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(10))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((uint64_t)(((uint64_t)(v1))));
  fg161_put(t0, t1, t2, t3, t4);
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(10))));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((uint64_t)(v2));
  fg161_put(t5, t6, t7, t8, t9);
  }
fg161_gg_done:;
}
static void fg161_gg_function_23(int64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(v2));
  t4 = ((uint64_t)(v3));
  fg161_put(t0, t1, t2, t3, t4);
  }
fg161_gg_done:;
}
static int64_t fg161_gg_function_20(int64_t p0, uint64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  uint64_t t5;
  int64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  uint64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  uint64_t t27;
  uint64_t t28;
  uint8_t t29;
  uint64_t t30;
  int64_t t31;
  uint64_t t32;
  uint64_t t33;
  uint64_t t34;
  uint64_t t35;
  uint8_t t36;
  uint64_t t37;
  uint64_t t38;
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
  int64_t t51;
  int64_t t52;
  uint64_t t53;
  int64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  uint64_t v4;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(2))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg161_gg_function_21(t1, t2, t3, base + 0);
  v2 = fg161_gg_function_3(t4, base + 0);
  t5 = ((uint64_t)(((uint64_t)(v2))));
  t6 = ((int64_t)(((int64_t)UINT64_C(10))));
  t7 = fg161_constant(t6);
  t8 = ((uint64_t)(t7));
  t9 = ((t5) >= (t8));
  if (t9) {
  {
  t10 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t10)))));
  goto fg161_gg_done;
  }
  }
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
  t12 = fg161_constant(t11);
  t13 = ((uint64_t)(t12));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = fg161_heap(t13, t14);
  v3 = ((int64_t)(t15));
  t16 = ((int64_t)(v3));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) != (t17));
  t19 = ((int64_t)(((int64_t)UINT64_C(7))));
  fg161_gg_function_6(t18, t19, base + 0);
  t20 = ((int64_t)(v3));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  fg161_gg_function_18(t20, t21, t22, t23, base + 0);
  t24 = ((int64_t)(v0));
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  v4 = fg161_gg_function_21(t24, t25, t26, base + 0);
  t27 = ((uint64_t)(v1));
  t28 = ((uint64_t)(v4));
  t29 = ((t27) > (t28));
  if (t29) {
  {
  v4 = ((uint64_t)(v1));
  }
  }
  t30 = ((uint64_t)(((uint64_t)(v2))));
  t31 = ((int64_t)(((int64_t)UINT64_C(10))));
  t32 = fg161_constant(t31);
  t33 = ((uint64_t)(t32));
  t34 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t35 = ((uint64_t)(((uint64_t)((uint64_t)(t33) - (uint64_t)(t34)))));
  t36 = ((t30) == (t35));
  if (t36) {
  {
  t37 = fg161_gg_function_9(base + 0);
  t38 = ((uint64_t)(t37));
  t39 = ((uint64_t)(((int64_t)UINT64_C(2))));
  v4 = ((uint64_t)(((uint64_t)fg161_gg_udiv(t38, t39))));
  }
  }
  t40 = ((int64_t)(v3));
  t41 = ((int64_t)(((int64_t)UINT64_C(3))));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = ((uint64_t)(v4));
  fg161_gg_function_18(t40, t41, t42, t43, base + 0);
  t44 = ((int64_t)(v0));
  t45 = ((int64_t)(((int64_t)UINT64_C(3))));
  t46 = ((int64_t)(v2));
  t47 = ((uint64_t)(((uint64_t)(v3))));
  fg161_gg_function_23(t44, t45, t46, t47, base + 0);
  t48 = ((int64_t)(v0));
  t49 = ((int64_t)(((int64_t)UINT64_C(2))));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = ((int64_t)(v2));
  t52 = ((int64_t)(((int64_t)UINT64_C(1))));
  t53 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t51) + (uint64_t)(t52)))))));
  fg161_gg_function_23(t48, t49, t50, t53, base + 0);
  t0 = ((int64_t)(v2));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static void fg161_gg_function_24(int64_t p0, uint64_t p1, int64_t p2, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  uint64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  uint64_t t31;
  uint64_t t32;
  uint8_t t33;
  uint8_t t34;
  uint64_t t35;
  uint64_t t36;
  uint8_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  uint8_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  uint64_t t52;
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
  uint8_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  uint64_t t71;
  uint64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  uint64_t t76;
  uint64_t t77;
  uint64_t t78;
  uint64_t t79;
  int64_t t80;
  int64_t t81;
  uint8_t t82;
  uint8_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  uint8_t t88;
  uint8_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  uint64_t t95;
  uint64_t t96;
  uint64_t t97;
  uint8_t t98;
  uint8_t t99;
  uint8_t t100;
  uint8_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  uint8_t t107;
  int64_t t108;
  uint64_t t109;
  int64_t t110;
  int64_t t111;
  uint8_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  uint64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  uint64_t t121;
  uint64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  uint64_t t126;
  uint64_t t127;
  uint64_t t128;
  uint8_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  uint64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  uint64_t t141;
  uint64_t t142;
  uint64_t t143;
  uint64_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  uint64_t t148;
  uint64_t t149;
  uint64_t t150;
  uint8_t t151;
  int64_t t152;
  int64_t t153;
  uint64_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  uint64_t t158;
  uint64_t t159;
  uint8_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  uint64_t t164;
  int64_t t165;
  int64_t t166;
  uint64_t t167;
  int64_t v0;
  uint64_t v1;
  int64_t v2;
  uint64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  uint64_t v8;
  int64_t v9;
  uint64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  uint8_t v16;
  uint8_t v17;
  int64_t v18;
  uint64_t v19;
  uint64_t v20;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t1 = ((uint64_t)(v1));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = fg161_gg_function_21(t2, t3, t4, base + 0);
  v3 = fg161_gg_function_4(t0, t1, t5, base + 0);
  t6 = ((int64_t)UINT64_C(1));
  v4 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t6)))));
  t7 = ((int64_t)UINT64_C(1));
  v5 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t7)))));
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t8 = ((int64_t)(v6));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fg161_gg_function_21(t9, t10, t11, base + 0);
  t13 = fg161_gg_function_3(t12, base + 0);
  t14 = ((int64_t)(t13));
  if (!(((t8) < (t14)))) goto label2;
  {
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(3))));
  t17 = ((int64_t)(v6));
  t18 = fg161_gg_function_21(t15, t16, t17, base + 0);
  v7 = fg161_gg_function_3(t18, base + 0);
  v8 = fg161_gg_function_9(base + 0);
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t19 = ((int64_t)(v9));
  t20 = ((int64_t)(v7));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = fg161_gg_function_16(t20, t21, t22, base + 0);
  t24 = fg161_gg_function_3(t23, base + 0);
  t25 = ((int64_t)(t24));
  t26 = ((int64_t)(((int64_t)UINT64_C(1))));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) - (uint64_t)(t26)))));
  if (!(((t19) < (t27)))) goto label5;
  {
  t28 = ((int64_t)(v7));
  t29 = ((int64_t)(((int64_t)UINT64_C(3))));
  t30 = ((int64_t)(v9));
  v10 = fg161_gg_function_16(t28, t29, t30, base + 0);
  t31 = ((uint64_t)(v10));
  t32 = ((uint64_t)(v3));
  t33 = ((t31) >= (t32));
  t34 = t33;
  if (t34) {
  t35 = ((uint64_t)(v10));
  t36 = ((uint64_t)(v8));
  t34 = ((t35) <= (t36));
  }
  t37 = t34;
  if (t37) {
  {
  v4 = ((int64_t)(v6));
  v5 = ((int64_t)(v9));
  v8 = ((uint64_t)(v10));
  }
  }
  t38 = v9;
  t39 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)((uint64_t)(t38) + (uint64_t)(t39)));
  }
  goto label4;
label5:;
  t40 = v6;
  t41 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t40) + (uint64_t)(t41)));
  }
  goto label1;
label2:;
  t42 = ((int64_t)(v5));
  t43 = ((int64_t)(((int64_t)UINT64_C(0))));
  t44 = ((t42) < (t43));
  if (t44) {
  {
  t45 = ((int64_t)UINT64_C(9223372036854775807));
  t46 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t45)))));
  t47 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t46) - (uint64_t)(t47)))));
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t48 = ((int64_t)(v6));
  t49 = ((int64_t)(v0));
  t50 = ((int64_t)(((int64_t)UINT64_C(2))));
  t51 = ((int64_t)(((int64_t)UINT64_C(0))));
  t52 = fg161_gg_function_21(t49, t50, t51, base + 0);
  t53 = fg161_gg_function_3(t52, base + 0);
  t54 = ((int64_t)(t53));
  if (!(((t48) < (t54)))) goto label8;
  {
  t55 = ((int64_t)(v0));
  t56 = ((int64_t)(((int64_t)UINT64_C(3))));
  t57 = ((int64_t)(v6));
  t58 = fg161_gg_function_21(t55, t56, t57, base + 0);
  v12 = fg161_gg_function_3(t58, base + 0);
  t59 = ((int64_t)(v12));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = ((int64_t)(((int64_t)UINT64_C(0))));
  t62 = fg161_gg_function_16(t59, t60, t61, base + 0);
  v13 = fg161_gg_function_3(t62, base + 0);
  t63 = ((int64_t)(v13));
  t64 = ((int64_t)(((int64_t)UINT64_C(0))));
  t65 = ((t63) > (t64));
  if (t65) {
  {
  t66 = ((int64_t)(v13));
  t67 = ((int64_t)(((int64_t)UINT64_C(1))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t66) - (uint64_t)(t67)))));
  t68 = ((int64_t)(v12));
  t69 = ((int64_t)(((int64_t)UINT64_C(1))));
  t70 = ((int64_t)(((int64_t)UINT64_C(0))));
  t71 = fg161_gg_function_16(t68, t69, t70, base + 0);
  t72 = ((uint64_t)(t71));
  t73 = ((int64_t)(v12));
  t74 = ((int64_t)(((int64_t)UINT64_C(2))));
  t75 = ((int64_t)(v14));
  t76 = fg161_gg_function_16(t73, t74, t75, base + 0);
  t77 = ((uint64_t)(t76));
  t78 = ((uint64_t)(((uint64_t)((uint64_t)(t72) - (uint64_t)(t77)))));
  t79 = ((uint64_t)(v3));
  v15 = ((int64_t)(((int64_t)(((uint64_t)((uint64_t)(t78) - (uint64_t)(t79)))))));
  t80 = ((int64_t)(v11));
  t81 = ((int64_t)(((int64_t)UINT64_C(0))));
  t82 = ((t80) < (t81));
  t83 = t82;
  if (t83) {
  t84 = ((int64_t)(v15));
  t85 = ((int64_t)(v11));
  t83 = ((t84) > (t85));
  }
  v16 = t83;
  t86 = ((int64_t)(v15));
  t87 = ((int64_t)(((int64_t)UINT64_C(0))));
  t88 = ((t86) >= (t87));
  t89 = t88;
  if (t89) {
  t90 = ((int64_t)(v15));
  t91 = ((int64_t)(v11));
  t89 = ((t90) < (t91));
  }
  v17 = t89;
  t92 = ((int64_t)(v12));
  t93 = ((int64_t)(((int64_t)UINT64_C(3))));
  t94 = ((int64_t)(v14));
  t95 = fg161_gg_function_16(t92, t93, t94, base + 0);
  t96 = ((uint64_t)(t95));
  t97 = ((uint64_t)(v3));
  t98 = ((t96) >= (t97));
  t99 = t98;
  if (t99) {
  t100 = v16;
  t101 = t100;
  if (!t101) {
  t101 = v17;
  }
  t99 = t101;
  }
  t102 = t99;
  if (t102) {
  {
  v4 = ((int64_t)(v6));
  v5 = ((int64_t)(v14));
  v11 = ((int64_t)(v15));
  }
  }
  }
  }
  t103 = v6;
  t104 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t103) + (uint64_t)(t104)));
  }
  goto label7;
label8:;
  }
  }
  t105 = ((int64_t)(v5));
  t106 = ((int64_t)(((int64_t)UINT64_C(0))));
  t107 = ((t105) < (t106));
  if (t107) {
  {
  t108 = ((int64_t)(v0));
  t109 = ((uint64_t)(v3));
  v4 = fg161_gg_function_20(t108, t109, base + 0);
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t110 = ((int64_t)(v4));
  t111 = ((int64_t)(((int64_t)UINT64_C(0))));
  t112 = ((t110) >= (t111));
  t113 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg161_gg_function_6(t112, t113, base + 0);
  t114 = ((int64_t)(v0));
  t115 = ((int64_t)(((int64_t)UINT64_C(3))));
  t116 = ((int64_t)(v4));
  t117 = fg161_gg_function_21(t114, t115, t116, base + 0);
  v18 = fg161_gg_function_3(t117, base + 0);
  t118 = ((int64_t)(v18));
  t119 = ((int64_t)(((int64_t)UINT64_C(2))));
  t120 = ((int64_t)(v5));
  v19 = fg161_gg_function_16(t118, t119, t120, base + 0);
  t121 = ((uint64_t)(v19));
  t122 = ((uint64_t)(v3));
  v20 = fg161_gg_function_8(t121, t122, base + 0);
  t123 = ((int64_t)(v18));
  t124 = ((int64_t)(((int64_t)UINT64_C(3))));
  t125 = ((int64_t)(v5));
  t126 = fg161_gg_function_16(t123, t124, t125, base + 0);
  t127 = ((uint64_t)(t126));
  t128 = ((uint64_t)(v3));
  t129 = ((t127) >= (t128));
  t130 = ((int64_t)(((int64_t)UINT64_C(11))));
  fg161_gg_function_6(t129, t130, base + 0);
  t131 = ((int64_t)(v18));
  t132 = ((int64_t)(((int64_t)UINT64_C(2))));
  t133 = ((int64_t)(v5));
  t134 = ((uint64_t)(v20));
  fg161_gg_function_18(t131, t132, t133, t134, base + 0);
  t135 = ((int64_t)(v18));
  t136 = ((int64_t)(((int64_t)UINT64_C(3))));
  t137 = ((int64_t)(v5));
  t138 = ((int64_t)(v18));
  t139 = ((int64_t)(((int64_t)UINT64_C(3))));
  t140 = ((int64_t)(v5));
  t141 = fg161_gg_function_16(t138, t139, t140, base + 0);
  t142 = ((uint64_t)(t141));
  t143 = ((uint64_t)(v3));
  t144 = ((uint64_t)(((uint64_t)((uint64_t)(t142) - (uint64_t)(t143)))));
  fg161_gg_function_18(t135, t136, t137, t144, base + 0);
  t145 = ((int64_t)(v18));
  t146 = ((int64_t)(((int64_t)UINT64_C(3))));
  t147 = ((int64_t)(v5));
  t148 = fg161_gg_function_16(t145, t146, t147, base + 0);
  t149 = ((uint64_t)(t148));
  t150 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t151 = ((t149) == (t150));
  if (t151) {
  {
  t152 = ((int64_t)(v18));
  t153 = ((int64_t)(v5));
  fg161_gg_function_19(t152, t153, base + 0);
  }
  }
  t154 = ((uint64_t)(v20));
  t155 = ((int64_t)(v18));
  t156 = ((int64_t)(((int64_t)UINT64_C(1))));
  t157 = ((int64_t)(((int64_t)UINT64_C(0))));
  t158 = fg161_gg_function_16(t155, t156, t157, base + 0);
  t159 = ((uint64_t)(t158));
  t160 = ((t154) > (t159));
  if (t160) {
  {
  t161 = ((int64_t)(v18));
  t162 = ((int64_t)(((int64_t)UINT64_C(1))));
  t163 = ((int64_t)(((int64_t)UINT64_C(0))));
  t164 = ((uint64_t)(v20));
  fg161_gg_function_18(t161, t162, t163, t164, base + 0);
  }
  }
  t165 = ((int64_t)(v2));
  t166 = ((int64_t)(v4));
  t167 = ((uint64_t)(v19));
  fg161_gg_function_25(t165, t166, t167, base + 0);
  }
fg161_gg_done:;
}
static void fg161_gg_function_60(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint64_t t6;
  int64_t t7;
  int64_t t8;
  uint64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  uint64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  uint64_t t25;
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
  int64_t t36;
  uint64_t t37;
  uint64_t t38;
  uint64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint64_t t43;
  uint64_t t44;
  uint64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  uint64_t t49;
  uint64_t t50;
  uint64_t t51;
  uint8_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  uint64_t t60;
  uint64_t t61;
  uint64_t t62;
  uint64_t t63;
  uint64_t t64;
  uint64_t t65;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(v1));
  v3 = fg161_gg_function_53(t0, t1, base + 0);
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(v2));
  v4 = fg161_gg_function_53(t2, t3, base + 0);
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(((int64_t)UINT64_C(2))));
  t6 = fg161_gg_function_57(t4, t5, base + 0);
  v5 = fg161_gg_function_3(t6, base + 0);
  t7 = ((int64_t)(v3));
  t8 = ((int64_t)(((int64_t)UINT64_C(2))));
  t9 = fg161_gg_function_57(t7, t8, base + 0);
  v6 = fg161_gg_function_3(t9, base + 0);
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(v5));
  fg161_gg_function_61(t10, t11, base + 0);
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(v6));
  fg161_gg_function_61(t12, t13, base + 0);
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(1))));
  t16 = ((int64_t)(v5));
  t17 = fg161_gg_function_50(t14, t15, t16, base + 0);
  t18 = fg161_gg_function_3(t17, base + 0);
  t19 = ((int64_t)(((int64_t)UINT64_C(2))));
  t20 = ((int64_t)(v2));
  t21 = fg161_buft(t18, t19, t20);
  v7 = ((uint64_t)(t21));
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = ((int64_t)(v6));
  t25 = fg161_gg_function_50(t22, t23, t24, base + 0);
  t26 = fg161_gg_function_3(t25, base + 0);
  t27 = ((int64_t)(((int64_t)UINT64_C(2))));
  t28 = ((int64_t)(v1));
  t29 = fg161_buft(t26, t27, t28);
  v8 = ((uint64_t)(t29));
  t30 = ((uint64_t)(v7));
  t31 = ((uint64_t)(v8));
  t32 = ((t30) >= (t31));
  t33 = ((int64_t)(((int64_t)UINT64_C(24))));
  fg161_gg_function_6(t32, t33, base + 0);
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(((int64_t)UINT64_C(3))));
  t36 = ((int64_t)(v5));
  t37 = fg161_gg_function_50(t34, t35, t36, base + 0);
  v9 = fg161_gg_function_3(t37, base + 0);
  t38 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t39 = ((uint64_t)(v7));
  t40 = ((int64_t)(v9));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = fg161_gg_function_21(t40, t41, t42, base + 0);
  v7 = fg161_gg_function_4(t38, t39, t43, base + 0);
  t44 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t45 = ((uint64_t)(v8));
  t46 = ((int64_t)(v9));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = fg161_gg_function_21(t46, t47, t48, base + 0);
  v8 = fg161_gg_function_4(t44, t45, t49, base + 0);
  t50 = ((uint64_t)(v7));
  t51 = ((uint64_t)(v8));
  t52 = ((t50) > (t51));
  if (t52) {
  {
  t53 = ((int64_t)(v9));
  t54 = ((int64_t)(v4));
  t55 = ((int64_t)(((int64_t)UINT64_C(3))));
  t56 = fg161_gg_function_57(t54, t55, base + 0);
  t57 = fg161_gg_function_3(t56, base + 0);
  t58 = ((int64_t)(v4));
  t59 = ((int64_t)(((int64_t)UINT64_C(4))));
  t60 = fg161_gg_function_57(t58, t59, base + 0);
  t61 = ((uint64_t)(v8));
  t62 = fg161_gg_function_8(t60, t61, base + 0);
  t63 = ((uint64_t)(v7));
  t64 = ((uint64_t)(v8));
  t65 = ((uint64_t)(((uint64_t)((uint64_t)(t63) - (uint64_t)(t64)))));
  fg161_gg_function_26(t53, t57, t62, t65, base + 0);
  }
  }
  }
fg161_gg_done:;
}
static void fg161_gg_function_65(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint64_t t14;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(2))));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)UINT64_C(2))));
  t4 = fg161_gg_function_57(t2, t3, base + 0);
  fg161_gg_function_63(t0, t1, t4, base + 0);
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(3))));
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(((int64_t)UINT64_C(3))));
  t9 = fg161_gg_function_57(t7, t8, base + 0);
  fg161_gg_function_63(t5, t6, t9, base + 0);
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(4))));
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(((int64_t)UINT64_C(4))));
  t14 = fg161_gg_function_57(t12, t13, base + 0);
  fg161_gg_function_63(t10, t11, t14, base + 0);
  }
fg161_gg_done:;
}
static uint8_t fg161_gg_function_64(int64_t p0, int64_t p1, int64_t base) {
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
  t2 = ((int64_t)(((int64_t)UINT64_C(2))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg161_gg_function_59(t1, t2, t3, base + 0);
  t5 = ((uint64_t)(t4));
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = fg161_gg_function_59(t6, t7, t8, base + 0);
  t10 = ((uint64_t)(t9));
  t11 = ((t5) != (t10));
  if (t11) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg161_gg_done;
  }
  }
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t12) < (t13)))) goto label2;
  {
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((int64_t)(v2));
  t17 = fg161_gg_function_59(t14, t15, t16, base + 0);
  t18 = ((uint64_t)(t17));
  t19 = ((int64_t)(v1));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((int64_t)(v2));
  t22 = fg161_gg_function_59(t19, t20, t21, base + 0);
  t23 = ((uint64_t)(t22));
  t24 = ((t18) != (t23));
  t25 = t24;
  if (!t25) {
  t26 = ((int64_t)(v0));
  t27 = ((int64_t)(((int64_t)UINT64_C(1))));
  t28 = ((int64_t)(v2));
  t29 = fg161_gg_function_59(t26, t27, t28, base + 0);
  t30 = ((uint64_t)(t29));
  t31 = ((int64_t)(v1));
  t32 = ((int64_t)(((int64_t)UINT64_C(1))));
  t33 = ((int64_t)(v2));
  t34 = fg161_gg_function_59(t31, t32, t33, base + 0);
  t35 = ((uint64_t)(t34));
  t25 = ((t30) != (t35));
  }
  t36 = t25;
  if (t36) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg161_gg_done;
  }
  }
  t37 = v2;
  t38 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t37) + (uint64_t)(t38)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static uint8_t fg161_gg_function_56(int64_t p0, int64_t p1, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = fg161_gg_function_53(t1, t2, base + 0);
  t4 = ((int64_t)(((int64_t)UINT64_C(5))));
  t5 = fg161_gg_function_57(t3, t4, base + 0);
  t6 = ((uint64_t)(t5));
  t7 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t0 = ((t6) != (t7));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static uint8_t fg161_gg_function_10(int64_t p0, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  int64_t t22;
  int64_t t23;
  uint8_t t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  uint8_t t30;
  int64_t t31;
  int64_t t32;
  uint8_t t33;
  int64_t t34;
  int64_t t35;
  uint8_t t36;
  int64_t t37;
  int64_t t38;
  uint8_t t39;
  int64_t t40;
  int64_t t41;
  uint8_t t42;
  int64_t t43;
  int64_t t44;
  uint8_t t45;
  int64_t t46;
  int64_t t47;
  uint8_t t48;
  int64_t t49;
  int64_t t50;
  uint8_t t51;
  int64_t t52;
  int64_t t53;
  uint8_t t54;
  int64_t t55;
  int64_t t56;
  uint8_t t57;
  int64_t t58;
  int64_t t59;
  uint8_t t60;
  int64_t t61;
  int64_t t62;
  uint8_t t63;
  int64_t t64;
  int64_t t65;
  uint8_t t66;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(73))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(32))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(45))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(44))));
  t12 = ((t10) == (t11));
  if (t12) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(2))));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(3))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(4))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(6))));
  t24 = ((t22) == (t23));
  if (t24) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(7))));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(8))));
  t30 = ((t28) == (t29));
  if (t30) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(9))));
  t33 = ((t31) == (t32));
  if (t33) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(((int64_t)UINT64_C(10))));
  t36 = ((t34) == (t35));
  if (t36) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(11))));
  t39 = ((t37) == (t38));
  if (t39) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(91))));
  t42 = ((t40) == (t41));
  if (t42) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  t43 = ((int64_t)(v0));
  t44 = ((int64_t)(((int64_t)UINT64_C(48))));
  t45 = ((t43) == (t44));
  if (t45) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(((int64_t)UINT64_C(49))));
  t48 = ((t46) == (t47));
  if (t48) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  t49 = ((int64_t)(v0));
  t50 = ((int64_t)(((int64_t)UINT64_C(23))));
  t51 = ((t49) == (t50));
  if (t51) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  t52 = ((int64_t)(v0));
  t53 = ((int64_t)(((int64_t)UINT64_C(25))));
  t54 = ((t52) == (t53));
  if (t54) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  t55 = ((int64_t)(v0));
  t56 = ((int64_t)(((int64_t)UINT64_C(26))));
  t57 = ((t55) == (t56));
  if (t57) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  t58 = ((int64_t)(v0));
  t59 = ((int64_t)(((int64_t)UINT64_C(50))));
  t60 = ((t58) == (t59));
  if (t60) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  t61 = ((int64_t)(v0));
  t62 = ((int64_t)(((int64_t)UINT64_C(46))));
  t63 = ((t61) == (t62));
  if (t63) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  t64 = ((int64_t)(v0));
  t65 = ((int64_t)(((int64_t)UINT64_C(47))));
  t66 = ((t64) == (t65));
  if (t66) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  t0 = ((uint8_t)UINT64_C(0));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static uint8_t fg161_gg_function_58(int64_t p0, int64_t p1, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint8_t t7;
  uint8_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(6))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg161_gg_function_59(t1, t2, t3, base + 0);
  t5 = ((uint64_t)(t4));
  t6 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) != (t6));
  t8 = t7;
  if (!t8) {
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(5))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fg161_gg_function_59(t9, t10, t11, base + 0);
  t13 = ((uint64_t)(t12));
  t14 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t13) != (t14));
  }
  t15 = t8;
  t16 = t15;
  if (!t16) {
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(v1));
  t19 = fg161_gg_function_56(t17, t18, base + 0);
  t16 = t19;
  }
  t0 = t16;
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static void fg161_gg_function_62(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
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
  uint64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  uint64_t t22;
  int64_t t23;
  uint8_t t24;
  uint8_t t25;
  uint64_t t26;
  int64_t t27;
  uint64_t t28;
  uint64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  uint64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint8_t t38;
  int64_t t39;
  int64_t t40;
  uint8_t t41;
  uint8_t t42;
  uint8_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  uint64_t t51;
  uint64_t t52;
  int64_t t53;
  uint64_t t54;
  uint64_t t55;
  uint64_t t56;
  uint64_t t57;
  uint8_t t58;
  uint8_t t59;
  int64_t t60;
  int64_t t61;
  uint8_t t62;
  uint8_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  uint64_t t67;
  uint64_t t68;
  int64_t t69;
  uint64_t t70;
  uint64_t t71;
  uint64_t t72;
  uint64_t t73;
  uint8_t t74;
  int64_t t75;
  int64_t t76;
  uint8_t t77;
  uint8_t t78;
  uint8_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  uint64_t t84;
  uint64_t t85;
  uint64_t t86;
  uint8_t t87;
  uint8_t t88;
  int64_t t89;
  int64_t t90;
  uint64_t t91;
  uint64_t t92;
  uint64_t t93;
  uint8_t t94;
  int64_t t95;
  int64_t t96;
  uint8_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  uint64_t t102;
  uint64_t t103;
  uint64_t t104;
  uint8_t t105;
  uint8_t t106;
  int64_t t107;
  int64_t t108;
  uint64_t t109;
  uint64_t t110;
  uint64_t t111;
  uint8_t t112;
  uint8_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  uint64_t t117;
  uint64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  uint64_t t122;
  uint64_t t123;
  uint8_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  uint64_t t129;
  int64_t t130;
  int64_t t131;
  uint64_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  uint64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  uint64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  uint64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  uint64_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  uint64_t t164;
  uint64_t t165;
  int64_t t166;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(v2));
  fg161_gg_function_61(t0, t1, base + 0);
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(v1));
  v3 = fg161_gg_function_53(t2, t3, base + 0);
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(v1));
  t6 = fg161_gg_function_58(t4, t5, base + 0);
  t7 = t6;
  t8 = t7;
  if (!t8) {
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(7))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fg161_gg_function_59(t9, t10, t11, base + 0);
  t13 = ((uint64_t)(t12));
  t14 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t13) != (t14));
  }
  t15 = t8;
  if (t15) {
  {
  goto fg161_gg_done;
  }
  }
  t16 = ((int64_t)(v3));
  t17 = ((int64_t)(((int64_t)UINT64_C(5))));
  t18 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  fg161_gg_function_63(t16, t17, t18, base + 0);
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(v1));
  t20 = ((int64_t)(((int64_t)UINT64_C(3))));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = fg161_gg_function_59(t19, t20, t21, base + 0);
  t23 = fg161_gg_function_3(t22, base + 0);
  t24 = fg161_gg_function_10(t23, base + 0);
  t25 = t24;
  if (t25) {
  {
label1:;
  t26 = ((uint64_t)(((uint64_t)(v4))));
  t27 = ((int64_t)(((int64_t)UINT64_C(12))));
  t28 = fg161_constant(t27);
  t29 = ((uint64_t)(t28));
  if (!(((t26) < (t29)))) goto label2;
  {
  t30 = ((int64_t)(v1));
  t31 = ((int64_t)(((int64_t)UINT64_C(9))));
  t32 = ((int64_t)(v4));
  t33 = fg161_gg_function_59(t30, t31, t32, base + 0);
  v5 = fg161_gg_function_3(t33, base + 0);
  t34 = v4;
  t35 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t34) + (uint64_t)(t35)));
  t36 = ((int64_t)(v5));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  t38 = ((t36) == (t37));
  if (t38) {
  {
  goto label1;
  }
  }
  t39 = ((int64_t)(v0));
  t40 = ((int64_t)(v5));
  t41 = fg161_gg_function_56(t39, t40, base + 0);
  t42 = t41;
  t43 = (!(t42));
  if (t43) {
  {
  goto label1;
  }
  }
  t44 = ((int64_t)(v5));
  t45 = ((int64_t)(((int64_t)UINT64_C(7))));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = fg161_gg_function_59(t44, t45, t46, base + 0);
  v6 = fg161_gg_function_3(t47, base + 0);
  t48 = ((int64_t)(v5));
  t49 = ((int64_t)(((int64_t)UINT64_C(4))));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = fg161_gg_function_59(t48, t49, t50, base + 0);
  t52 = ((uint64_t)(t51));
  t53 = ((int64_t)(((int64_t)UINT64_C(14))));
  t54 = fg161_constant(t53);
  t55 = ((uint64_t)(t54));
  t56 = ((uint64_t)(((t52) & (t55))));
  t57 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t58 = ((t56) != (t57));
  t59 = t58;
  if (!t59) {
  t60 = ((int64_t)(v6));
  t61 = ((int64_t)(((int64_t)UINT64_C(0))));
  t62 = ((t60) != (t61));
  t63 = t62;
  if (t63) {
  t64 = ((int64_t)(v6));
  t65 = ((int64_t)(((int64_t)UINT64_C(4))));
  t66 = ((int64_t)(((int64_t)UINT64_C(0))));
  t67 = fg161_gg_function_59(t64, t65, t66, base + 0);
  t68 = ((uint64_t)(t67));
  t69 = ((int64_t)(((int64_t)UINT64_C(14))));
  t70 = fg161_constant(t69);
  t71 = ((uint64_t)(t70));
  t72 = ((uint64_t)(((t68) & (t71))));
  t73 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t63 = ((t72) != (t73));
  }
  t59 = t63;
  }
  t74 = t59;
  if (t74) {
  {
  goto label1;
  }
  }
  t75 = ((int64_t)(v1));
  t76 = ((int64_t)(v5));
  t77 = fg161_gg_function_64(t75, t76, base + 0);
  t78 = t77;
  t79 = (!(t78));
  if (t79) {
  {
  goto label1;
  }
  }
  t80 = ((int64_t)(v0));
  t81 = ((int64_t)(v5));
  v7 = fg161_gg_function_53(t80, t81, base + 0);
  t82 = ((int64_t)(v7));
  t83 = ((int64_t)(((int64_t)UINT64_C(0))));
  t84 = fg161_gg_function_57(t82, t83, base + 0);
  t85 = ((uint64_t)(t84));
  t86 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t87 = ((t85) == (t86));
  t88 = t87;
  if (t88) {
  t89 = ((int64_t)(v7));
  t90 = ((int64_t)(((int64_t)UINT64_C(1))));
  t91 = fg161_gg_function_57(t89, t90, base + 0);
  t92 = ((uint64_t)(t91));
  t93 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t88 = ((t92) == (t93));
  }
  t94 = t88;
  if (t94) {
  {
  t95 = ((int64_t)(v6));
  t96 = ((int64_t)(((int64_t)UINT64_C(0))));
  t97 = ((t95) != (t96));
  if (t97) {
  {
  t98 = ((int64_t)(v0));
  t99 = ((int64_t)(v6));
  v8 = fg161_gg_function_53(t98, t99, base + 0);
  t100 = ((int64_t)(v8));
  t101 = ((int64_t)(((int64_t)UINT64_C(1))));
  t102 = fg161_gg_function_57(t100, t101, base + 0);
  t103 = ((uint64_t)(t102));
  t104 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t105 = ((t103) == (t104));
  t106 = t105;
  if (t106) {
  t107 = ((int64_t)(v8));
  t108 = ((int64_t)(((int64_t)UINT64_C(0))));
  t109 = fg161_gg_function_57(t107, t108, base + 0);
  t110 = ((uint64_t)(t109));
  t111 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t106 = ((t110) == (t111));
  }
  t112 = t106;
  t113 = t112;
  if (t113) {
  t114 = ((int64_t)(v6));
  t115 = ((int64_t)(((int64_t)UINT64_C(6))));
  t116 = ((int64_t)(((int64_t)UINT64_C(0))));
  t117 = fg161_gg_function_59(t114, t115, t116, base + 0);
  t118 = ((uint64_t)(t117));
  t119 = ((int64_t)(v5));
  t120 = ((int64_t)(((int64_t)UINT64_C(6))));
  t121 = ((int64_t)(((int64_t)UINT64_C(0))));
  t122 = fg161_gg_function_59(t119, t120, t121, base + 0);
  t123 = ((uint64_t)(t122));
  t113 = ((t118) == (t123));
  }
  t124 = t113;
  if (t124) {
  {
  t125 = ((int64_t)(v3));
  t126 = ((int64_t)(v7));
  fg161_gg_function_65(t125, t126, base + 0);
  t127 = ((int64_t)(v7));
  t128 = ((int64_t)(((int64_t)UINT64_C(5))));
  t129 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg161_gg_function_63(t127, t128, t129, base + 0);
  t130 = ((int64_t)(v8));
  t131 = ((int64_t)(((int64_t)UINT64_C(5))));
  t132 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg161_gg_function_63(t130, t131, t132, base + 0);
  t133 = ((int64_t)(v0));
  t134 = ((int64_t)(v1));
  t135 = ((int64_t)(v6));
  fg161_gg_function_60(t133, t134, t135, base + 0);
  goto fg161_gg_done;
  }
  }
  }
  } else {
  {
  t136 = ((int64_t)(v3));
  t137 = ((int64_t)(v7));
  fg161_gg_function_65(t136, t137, base + 0);
  t138 = ((int64_t)(v7));
  t139 = ((int64_t)(((int64_t)UINT64_C(5))));
  t140 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg161_gg_function_63(t138, t139, t140, base + 0);
  t141 = ((int64_t)(v0));
  t142 = ((int64_t)(v1));
  t143 = ((int64_t)(v5));
  fg161_gg_function_60(t141, t142, t143, base + 0);
  goto fg161_gg_done;
  }
  }
  }
  }
  }
  goto label1;
label2:;
  }
  }
  t144 = ((int64_t)(v3));
  t145 = ((int64_t)(((int64_t)UINT64_C(2))));
  t146 = ((uint64_t)(((uint64_t)(v2))));
  fg161_gg_function_63(t144, t145, t146, base + 0);
  t147 = ((int64_t)(v3));
  t148 = ((int64_t)(((int64_t)UINT64_C(5))));
  t149 = ((int64_t)(((int64_t)UINT64_C(0))));
  t150 = ((int64_t)(((int64_t)UINT64_C(0))));
  t151 = fg161_record(t147, t148, t149, t150);
  v9 = ((int64_t)(t151));
  t152 = ((int64_t)(v0));
  t153 = ((int64_t)(((int64_t)UINT64_C(3))));
  t154 = ((int64_t)(v2));
  t155 = fg161_gg_function_50(t152, t153, t154, base + 0);
  t156 = fg161_gg_function_3(t155, base + 0);
  t157 = ((int64_t)(v0));
  t158 = ((int64_t)(((int64_t)UINT64_C(1))));
  t159 = ((int64_t)(v2));
  t160 = fg161_gg_function_50(t157, t158, t159, base + 0);
  t161 = fg161_gg_function_3(t160, base + 0);
  t162 = ((int64_t)(((int64_t)UINT64_C(2))));
  t163 = ((int64_t)(v1));
  t164 = fg161_buft(t161, t162, t163);
  t165 = ((uint64_t)(t164));
  t166 = ((int64_t)(v9));
  fg161_gg_function_24(t156, t165, t166, base + 0);
  }
fg161_gg_done:;
}
static int64_t fg161_gg_function_67(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  if (t3) {
  {
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(v1));
  t6 = fg161_i32(t4, t5);
  t0 = ((int64_t)(t6));
  }
  } else {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static void fg161_gg_function_69(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  uint64_t t26;
  uint64_t t27;
  int64_t t28;
  uint64_t t29;
  uint64_t t30;
  uint64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  uint64_t t37;
  uint64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  uint64_t t46;
  uint64_t t47;
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
  uint64_t t59;
  uint64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  uint64_t t67;
  uint64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  uint64_t t72;
  int64_t t73;
  int64_t t74;
  uint8_t t75;
  uint8_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  uint64_t t80;
  uint64_t t81;
  int64_t t82;
  uint64_t t83;
  uint64_t t84;
  uint8_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  uint64_t t94;
  uint64_t t95;
  int64_t t96;
  uint64_t t97;
  uint64_t t98;
  uint64_t t99;
  uint64_t t100;
  uint8_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  uint64_t t107;
  int64_t t108;
  uint64_t t109;
  uint64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  uint64_t t114;
  int64_t t115;
  int64_t t116;
  uint8_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  uint64_t t126;
  uint64_t t127;
  int64_t t128;
  uint64_t t129;
  uint64_t t130;
  uint64_t t131;
  uint64_t t132;
  uint8_t t133;
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
  uint64_t t148;
  uint64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  uint64_t t156;
  uint64_t t157;
  int64_t t158;
  int64_t t159;
  uint64_t t160;
  int64_t t161;
  uint64_t t162;
  uint64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  uint64_t t167;
  int64_t t168;
  int64_t t169;
  uint8_t t170;
  int64_t t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  uint64_t t179;
  int64_t t180;
  uint64_t t181;
  uint64_t t182;
  int64_t t183;
  int64_t t184;
  int64_t t185;
  uint64_t t186;
  int64_t t187;
  int64_t t188;
  uint8_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  uint64_t t198;
  uint64_t t199;
  uint64_t t200;
  uint8_t t201;
  uint8_t t202;
  int64_t t203;
  int64_t t204;
  uint64_t t205;
  uint64_t t206;
  uint64_t t207;
  uint8_t t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  uint64_t t212;
  int64_t t213;
  int64_t t214;
  uint8_t t215;
  int64_t t216;
  int64_t t217;
  int64_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  int64_t t223;
  uint64_t t224;
  uint64_t t225;
  uint64_t t226;
  uint8_t t227;
  uint8_t t228;
  int64_t t229;
  int64_t t230;
  uint64_t t231;
  uint64_t t232;
  uint64_t t233;
  uint8_t t234;
  uint8_t t235;
  int64_t t236;
  int64_t t237;
  uint64_t t238;
  uint64_t t239;
  uint64_t t240;
  uint8_t t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  uint64_t t246;
  uint64_t t247;
  uint64_t t248;
  uint8_t t249;
  int64_t t250;
  int64_t t251;
  int64_t t252;
  int64_t t253;
  int64_t t254;
  int64_t t255;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(5))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg161_gg_function_50(t0, t1, t2, base + 0);
  t4 = fg161_gg_function_3(t3, base + 0);
  t5 = ((int64_t)(((int64_t)UINT64_C(9))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg161_get(t4, t5, t6, t7);
  t9 = ((uint64_t)(t8));
  t10 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) > (t10));
  if (t11) {
  {
  t12 = ((int64_t)(v0));
  fg161_hash_reset(t12);
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(6))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = fg161_gg_function_50(t13, t14, t15, base + 0);
  t17 = fg161_gg_function_3(t16, base + 0);
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(5))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = fg161_gg_function_50(t18, t19, t20, base + 0);
  t22 = fg161_gg_function_3(t21, base + 0);
  t23 = ((int64_t)(((int64_t)UINT64_C(9))));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = fg161_get(t22, t23, t24, t25);
  t27 = ((uint64_t)(t26));
  t28 = ((int64_t)(((int64_t)UINT64_C(5))));
  t29 = fg161_constant(t28);
  t30 = ((uint64_t)(t29));
  t31 = fg161_gg_function_47(t27, t30, base + 0);
  fg161_zero(t17, t31);
  }
  }
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t32 = ((int64_t)(v4));
  t33 = ((int64_t)(v1));
  t34 = ((int64_t)(((int64_t)UINT64_C(8))));
  t35 = ((int64_t)(((int64_t)UINT64_C(1))));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = fg161_get(t33, t34, t35, t36);
  t38 = ((uint64_t)(t37));
  t39 = fg161_gg_function_3(t38, base + 0);
  t40 = ((int64_t)(t39));
  if (!(((t32) < (t40)))) goto label2;
  {
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(v1));
  t43 = ((int64_t)(((int64_t)UINT64_C(8))));
  t44 = ((int64_t)(((int64_t)UINT64_C(3))));
  t45 = ((int64_t)(v4));
  t46 = fg161_get(t42, t43, t44, t45);
  t47 = ((uint64_t)(t46));
  t48 = fg161_gg_function_3(t47, base + 0);
  t49 = ((int64_t)(v3));
  t50 = ((int64_t)(v4));
  t51 = fg161_gg_function_67(t49, t50, base + 0);
  fg161_gg_function_62(t41, t48, t51, base + 0);
  t52 = v4;
  t53 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t52) + (uint64_t)(t53)));
  }
  goto label1;
label2:;
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t54 = ((int64_t)(v4));
  t55 = ((int64_t)(v1));
  t56 = ((int64_t)(((int64_t)UINT64_C(8))));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = fg161_get(t55, t56, t57, t58);
  t60 = ((uint64_t)(t59));
  t61 = fg161_gg_function_3(t60, base + 0);
  t62 = ((int64_t)(t61));
  if (!(((t54) < (t62)))) goto label5;
  {
  t63 = ((int64_t)(v1));
  t64 = ((int64_t)(((int64_t)UINT64_C(8))));
  t65 = ((int64_t)(((int64_t)UINT64_C(2))));
  t66 = ((int64_t)(v4));
  t67 = fg161_get(t63, t64, t65, t66);
  t68 = ((uint64_t)(t67));
  v5 = fg161_gg_function_3(t68, base + 0);
  t69 = ((int64_t)(v5));
  t70 = ((int64_t)(((int64_t)UINT64_C(7))));
  t71 = ((int64_t)(((int64_t)UINT64_C(0))));
  t72 = fg161_gg_function_59(t69, t70, t71, base + 0);
  v6 = fg161_gg_function_3(t72, base + 0);
  t73 = ((int64_t)(v6));
  t74 = ((int64_t)(((int64_t)UINT64_C(0))));
  t75 = ((t73) != (t74));
  t76 = t75;
  if (t76) {
  t77 = ((int64_t)(v5));
  t78 = ((int64_t)(((int64_t)UINT64_C(3))));
  t79 = ((int64_t)(((int64_t)UINT64_C(0))));
  t80 = fg161_gg_function_59(t77, t78, t79, base + 0);
  t81 = ((uint64_t)(t80));
  t82 = ((int64_t)(((int64_t)UINT64_C(16))));
  t83 = fg161_constant(t82);
  t84 = ((uint64_t)(t83));
  t76 = ((t81) != (t84));
  }
  t85 = t76;
  if (t85) {
  {
  t86 = ((int64_t)(v0));
  t87 = ((int64_t)(v6));
  t88 = fg161_gg_function_53(t86, t87, base + 0);
  t89 = ((int64_t)(((int64_t)UINT64_C(1))));
  t90 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg161_gg_function_72(t88, t89, t90, base + 0);
  }
  }
  t91 = ((int64_t)(v5));
  t92 = ((int64_t)(((int64_t)UINT64_C(4))));
  t93 = ((int64_t)(((int64_t)UINT64_C(0))));
  t94 = fg161_gg_function_59(t91, t92, t93, base + 0);
  t95 = ((uint64_t)(t94));
  t96 = ((int64_t)(((int64_t)UINT64_C(13))));
  t97 = fg161_constant(t96);
  t98 = ((uint64_t)(t97));
  t99 = ((uint64_t)(((t95) & (t98))));
  t100 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t101 = ((t99) != (t100));
  if (t101) {
  {
  t102 = ((int64_t)(v0));
  t103 = ((int64_t)(v5));
  t104 = ((int64_t)(v2));
  t105 = ((int64_t)(v4));
  t106 = fg161_gg_function_67(t104, t105, base + 0);
  fg161_gg_function_62(t102, t103, t106, base + 0);
  }
  }
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t107 = ((uint64_t)(((uint64_t)(v7))));
  t108 = ((int64_t)(((int64_t)UINT64_C(12))));
  t109 = fg161_constant(t108);
  t110 = ((uint64_t)(t109));
  if (!(((t107) < (t110)))) goto label8;
  {
  t111 = ((int64_t)(v5));
  t112 = ((int64_t)(((int64_t)UINT64_C(9))));
  t113 = ((int64_t)(v7));
  t114 = fg161_gg_function_59(t111, t112, t113, base + 0);
  v8 = fg161_gg_function_3(t114, base + 0);
  t115 = ((int64_t)(v8));
  t116 = ((int64_t)(((int64_t)UINT64_C(0))));
  t117 = ((t115) != (t116));
  if (t117) {
  {
  t118 = ((int64_t)(v0));
  t119 = ((int64_t)(v8));
  t120 = fg161_gg_function_53(t118, t119, base + 0);
  t121 = ((int64_t)(((int64_t)UINT64_C(0))));
  t122 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg161_gg_function_72(t120, t121, t122, base + 0);
  t123 = ((int64_t)(v8));
  t124 = ((int64_t)(((int64_t)UINT64_C(4))));
  t125 = ((int64_t)(((int64_t)UINT64_C(0))));
  t126 = fg161_gg_function_59(t123, t124, t125, base + 0);
  t127 = ((uint64_t)(t126));
  t128 = ((int64_t)(((int64_t)UINT64_C(13))));
  t129 = fg161_constant(t128);
  t130 = ((uint64_t)(t129));
  t131 = ((uint64_t)(((t127) & (t130))));
  t132 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t133 = ((t131) != (t132));
  if (t133) {
  {
  t134 = ((int64_t)(v0));
  t135 = ((int64_t)(v8));
  t136 = ((int64_t)(v2));
  t137 = ((int64_t)(v4));
  t138 = fg161_gg_function_67(t136, t137, base + 0);
  fg161_gg_function_62(t134, t135, t138, base + 0);
  }
  }
  }
  }
  t139 = v7;
  t140 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)((uint64_t)(t139) + (uint64_t)(t140)));
  }
  goto label7;
label8:;
  t141 = v4;
  t142 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t141) + (uint64_t)(t142)));
  }
  goto label4;
label5:;
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t143 = ((int64_t)(v4));
  t144 = ((int64_t)(v1));
  t145 = ((int64_t)(((int64_t)UINT64_C(8))));
  t146 = ((int64_t)(((int64_t)UINT64_C(0))));
  t147 = ((int64_t)(((int64_t)UINT64_C(0))));
  t148 = fg161_get(t144, t145, t146, t147);
  t149 = ((uint64_t)(t148));
  t150 = fg161_gg_function_3(t149, base + 0);
  t151 = ((int64_t)(t150));
  if (!(((t143) < (t151)))) goto label11;
  {
  t152 = ((int64_t)(v1));
  t153 = ((int64_t)(((int64_t)UINT64_C(8))));
  t154 = ((int64_t)(((int64_t)UINT64_C(2))));
  t155 = ((int64_t)(v4));
  t156 = fg161_get(t152, t153, t154, t155);
  t157 = ((uint64_t)(t156));
  v9 = fg161_gg_function_3(t157, base + 0);
  t158 = ((int64_t)(v2));
  t159 = ((int64_t)(v4));
  v10 = fg161_gg_function_67(t158, t159, base + 0);
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t160 = ((uint64_t)(((uint64_t)(v11))));
  t161 = ((int64_t)(((int64_t)UINT64_C(12))));
  t162 = fg161_constant(t161);
  t163 = ((uint64_t)(t162));
  if (!(((t160) < (t163)))) goto label14;
  {
  t164 = ((int64_t)(v9));
  t165 = ((int64_t)(((int64_t)UINT64_C(9))));
  t166 = ((int64_t)(v11));
  t167 = fg161_gg_function_59(t164, t165, t166, base + 0);
  v12 = fg161_gg_function_3(t167, base + 0);
  t168 = ((int64_t)(v12));
  t169 = ((int64_t)(((int64_t)UINT64_C(0))));
  t170 = ((t168) != (t169));
  if (t170) {
  {
  t171 = ((int64_t)(v0));
  t172 = ((int64_t)(v12));
  t173 = ((int64_t)(v10));
  fg161_gg_function_62(t171, t172, t173, base + 0);
  }
  }
  t174 = v11;
  t175 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t174) + (uint64_t)(t175)));
  }
  goto label13;
label14:;
  t176 = ((int64_t)(v0));
  t177 = ((int64_t)(v9));
  t178 = ((int64_t)(v10));
  fg161_gg_function_62(t176, t177, t178, base + 0);
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label16:;
  t179 = ((uint64_t)(((uint64_t)(v11))));
  t180 = ((int64_t)(((int64_t)UINT64_C(12))));
  t181 = fg161_constant(t180);
  t182 = ((uint64_t)(t181));
  if (!(((t179) < (t182)))) goto label17;
  {
  t183 = ((int64_t)(v9));
  t184 = ((int64_t)(((int64_t)UINT64_C(9))));
  t185 = ((int64_t)(v11));
  t186 = fg161_gg_function_59(t183, t184, t185, base + 0);
  v13 = fg161_gg_function_3(t186, base + 0);
  t187 = ((int64_t)(v13));
  t188 = ((int64_t)(((int64_t)UINT64_C(0))));
  t189 = ((t187) != (t188));
  if (t189) {
  {
  t190 = ((int64_t)(v0));
  t191 = ((int64_t)(v13));
  v14 = fg161_gg_function_53(t190, t191, base + 0);
  t192 = ((int64_t)(v14));
  t193 = ((int64_t)(((int64_t)UINT64_C(0))));
  t194 = ((int64_t)UINT64_C(1));
  t195 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t194)))));
  fg161_gg_function_72(t192, t193, t195, base + 0);
  t196 = ((int64_t)(v14));
  t197 = ((int64_t)(((int64_t)UINT64_C(0))));
  t198 = fg161_gg_function_57(t196, t197, base + 0);
  t199 = ((uint64_t)(t198));
  t200 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t201 = ((t199) == (t200));
  t202 = t201;
  if (t202) {
  t203 = ((int64_t)(v14));
  t204 = ((int64_t)(((int64_t)UINT64_C(1))));
  t205 = fg161_gg_function_57(t203, t204, base + 0);
  t206 = ((uint64_t)(t205));
  t207 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t202 = ((t206) == (t207));
  }
  t208 = t202;
  if (t208) {
  {
  t209 = ((int64_t)(v13));
  t210 = ((int64_t)(((int64_t)UINT64_C(7))));
  t211 = ((int64_t)(((int64_t)UINT64_C(0))));
  t212 = fg161_gg_function_59(t209, t210, t211, base + 0);
  v15 = fg161_gg_function_3(t212, base + 0);
  t213 = ((int64_t)(v15));
  t214 = ((int64_t)(((int64_t)UINT64_C(0))));
  t215 = ((t213) != (t214));
  if (t215) {
  {
  t216 = ((int64_t)(v0));
  t217 = ((int64_t)(v15));
  v16 = fg161_gg_function_53(t216, t217, base + 0);
  t218 = ((int64_t)(v16));
  t219 = ((int64_t)(((int64_t)UINT64_C(1))));
  t220 = ((int64_t)UINT64_C(1));
  t221 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t220)))));
  fg161_gg_function_72(t218, t219, t221, base + 0);
  t222 = ((int64_t)(v16));
  t223 = ((int64_t)(((int64_t)UINT64_C(1))));
  t224 = fg161_gg_function_57(t222, t223, base + 0);
  t225 = ((uint64_t)(t224));
  t226 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t227 = ((t225) == (t226));
  t228 = t227;
  if (t228) {
  t229 = ((int64_t)(v16));
  t230 = ((int64_t)(((int64_t)UINT64_C(0))));
  t231 = fg161_gg_function_57(t229, t230, base + 0);
  t232 = ((uint64_t)(t231));
  t233 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t228 = ((t232) == (t233));
  }
  t234 = t228;
  t235 = t234;
  if (t235) {
  t236 = ((int64_t)(v16));
  t237 = ((int64_t)(((int64_t)UINT64_C(5))));
  t238 = fg161_gg_function_57(t236, t237, base + 0);
  t239 = ((uint64_t)(t238));
  t240 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t235 = ((t239) != (t240));
  }
  t241 = t235;
  if (t241) {
  {
  t242 = ((int64_t)(v0));
  t243 = ((int64_t)(v15));
  fg161_gg_function_66(t242, t243, base + 0);
  }
  }
  }
  } else {
  {
  t244 = ((int64_t)(v14));
  t245 = ((int64_t)(((int64_t)UINT64_C(5))));
  t246 = fg161_gg_function_57(t244, t245, base + 0);
  t247 = ((uint64_t)(t246));
  t248 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t249 = ((t247) != (t248));
  if (t249) {
  {
  t250 = ((int64_t)(v0));
  t251 = ((int64_t)(v13));
  fg161_gg_function_66(t250, t251, base + 0);
  }
  }
  }
  }
  }
  }
  }
  }
  t252 = v11;
  t253 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t252) + (uint64_t)(t253)));
  }
  goto label16;
label17:;
  t254 = v4;
  t255 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t254) + (uint64_t)(t255)));
  }
  goto label10;
label11:;
  }
fg161_gg_done:;
}
static void fg161_gg_function_27(int64_t p0, int64_t base) {
  uint64_t t0;
  int64_t t1;
  uint64_t t2;
  uint64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  uint64_t t18;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  {
  v1 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t0 = ((uint64_t)(((uint64_t)(v1))));
  t1 = ((int64_t)(((int64_t)UINT64_C(10))));
  t2 = fg161_constant(t1);
  t3 = ((uint64_t)(t2));
  if (!(((t0) < (t3)))) goto label2;
  {
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(3))));
  t6 = ((int64_t)(v1));
  t7 = fg161_gg_function_21(t4, t5, t6, base + 0);
  t8 = fg161_gg_function_3(t7, base + 0);
  fg161_release(t8);
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(3))));
  t11 = ((int64_t)(v1));
  t12 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg161_gg_function_23(t9, t10, t11, t12, base + 0);
  t13 = v1;
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  v1 = ((int64_t)((uint64_t)(t13) + (uint64_t)(t14)));
  }
  goto label1;
label2:;
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(2))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg161_gg_function_23(t15, t16, t17, t18, base + 0);
  }
fg161_gg_done:;
}
static uint8_t fg161_gg_function_73(int64_t p0, int64_t p1, int64_t p2, int64_t p3, uint8_t p4, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  uint64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  uint64_t t25;
  uint64_t t26;
  uint64_t t27;
  uint8_t t28;
  uint64_t t29;
  uint64_t t30;
  uint64_t t31;
  uint64_t t32;
  uint64_t t33;
  uint64_t t34;
  uint64_t t35;
  uint64_t t36;
  uint64_t t37;
  uint64_t t38;
  uint64_t t39;
  uint64_t t40;
  uint8_t t41;
  uint64_t t42;
  uint64_t t43;
  uint64_t t44;
  uint64_t t45;
  int64_t t46;
  uint64_t t47;
  uint64_t t48;
  uint64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  uint8_t t54;
  int64_t t55;
  uint64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  uint8_t t61;
  int64_t t62;
  int64_t t63;
  uint64_t t64;
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
  uint64_t t77;
  uint64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  uint64_t t84;
  uint64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  uint64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  uint64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  uint64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  uint64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  uint64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  uint64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  uint64_t t121;
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
  uint64_t t132;
  uint64_t t133;
  uint64_t t134;
  uint8_t t135;
  uint64_t t136;
  int64_t t137;
  uint64_t t138;
  uint64_t t139;
  uint64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  uint8_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  uint64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  uint64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  uint64_t t159;
  uint64_t t160;
  uint64_t t161;
  int64_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  uint64_t t166;
  uint64_t t167;
  int64_t t168;
  int64_t t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  uint64_t t177;
  int64_t t178;
  uint64_t t179;
  uint64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  uint64_t t185;
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
  uint64_t t200;
  uint64_t t201;
  uint64_t t202;
  uint8_t t203;
  uint64_t t204;
  int64_t t205;
  uint64_t t206;
  uint64_t t207;
  uint64_t t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  int64_t t212;
  uint8_t t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  int64_t t217;
  uint64_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  uint64_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  uint64_t t227;
  uint64_t t228;
  uint64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  uint64_t t234;
  uint64_t t235;
  int64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  int64_t t251;
  int64_t t252;
  uint64_t t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  int64_t t257;
  int64_t t258;
  int64_t t259;
  int64_t t260;
  uint64_t t261;
  uint64_t t262;
  int64_t t263;
  int64_t t264;
  int64_t t265;
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
  int64_t t278;
  int64_t t279;
  int64_t t280;
  uint64_t t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  uint64_t t285;
  int64_t t286;
  int64_t t287;
  int64_t t288;
  int64_t t289;
  int64_t t290;
  int64_t t291;
  uint64_t t292;
  int64_t t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  uint8_t t297;
  int64_t t298;
  int64_t t299;
  int64_t t300;
  int64_t t301;
  uint64_t t302;
  uint64_t t303;
  uint64_t t304;
  uint64_t t305;
  uint8_t t306;
  int64_t t307;
  int64_t t308;
  uint8_t t309;
  int64_t t310;
  int64_t t311;
  int64_t t312;
  int64_t t313;
  uint64_t t314;
  uint8_t t315;
  uint8_t t316;
  int64_t t317;
  int64_t t318;
  int64_t t319;
  uint64_t t320;
  int64_t t321;
  int64_t t322;
  int64_t t323;
  uint64_t t324;
  int64_t t325;
  int64_t t326;
  int64_t t327;
  int64_t t328;
  uint64_t t329;
  int64_t t330;
  int64_t t331;
  int64_t t332;
  uint64_t t333;
  int64_t t334;
  int64_t t335;
  uint8_t t336;
  int64_t t337;
  int64_t t338;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  uint8_t v4;
  uint64_t v5;
  uint64_t v6;
  uint64_t v7;
  uint64_t v8;
  int64_t v9;
  uint64_t v10;
  uint64_t v11;
  uint64_t v12;
  uint64_t v13;
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
  uint8_t v26;
  int64_t v27;
  uint64_t v28;
  uint64_t v29;
  uint64_t v30;
  int64_t v31;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(8))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = fg161_get(t1, t2, t3, t4);
  v5 = ((uint64_t)(t5));
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(8))));
  t8 = ((int64_t)(((int64_t)UINT64_C(1))));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = fg161_get(t6, t7, t8, t9);
  v6 = ((uint64_t)(t10));
  t11 = ((uint64_t)(v5));
  t12 = ((uint64_t)(v6));
  v7 = fg161_gg_function_8(t11, t12, base + 0);
  t13 = ((uint64_t)(v7));
  t14 = ((uint64_t)(v7));
  t15 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t16 = ((uint64_t)(((uint64_t)fg161_gg_udiv(t14, t15))));
  v8 = fg161_gg_function_8(t13, t16, base + 0);
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(5))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = fg161_gg_function_50(t17, t18, t19, base + 0);
  v9 = fg161_gg_function_3(t20, base + 0);
  t21 = ((int64_t)(v9));
  t22 = ((int64_t)(((int64_t)UINT64_C(9))));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = fg161_get(t21, t22, t23, t24);
  t26 = ((uint64_t)(t25));
  t27 = ((uint64_t)(v8));
  t28 = ((t26) < (t27));
  if (t28) {
  {
  t29 = ((uint64_t)(v8));
  t30 = fg161_hash_size(t29);
  v10 = ((uint64_t)(t30));
  t31 = ((uint64_t)(v10));
  t32 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  v11 = fg161_gg_function_47(t31, t32, base + 0);
  t33 = ((uint64_t)(v10));
  t34 = ((uint64_t)(((int64_t)UINT64_C(32))));
  t35 = ((uint64_t)(((uint64_t)fg161_gg_udiv(t33, t34))));
  t37 = ((uint64_t)(v10));
  t38 = ((uint64_t)(((int64_t)UINT64_C(32))));
  t39 = ((uint64_t)(((uint64_t)fg161_gg_umod(t37, t38))));
  t40 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t41 = ((t39) != (t40));
  if (t41) {
  {
  t36 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t36 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t42 = ((uint64_t)(t36));
  t43 = ((uint64_t)(((uint64_t)((uint64_t)(t35) + (uint64_t)(t42)))));
  t44 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  v12 = fg161_gg_function_47(t43, t44, base + 0);
  t45 = ((uint64_t)(v10));
  t46 = ((int64_t)(((int64_t)UINT64_C(5))));
  t47 = fg161_constant(t46);
  t48 = ((uint64_t)(t47));
  v13 = fg161_gg_function_47(t45, t48, base + 0);
  t49 = ((uint64_t)(v11));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = fg161_heap(t49, t50);
  v14 = ((int64_t)(t51));
  t52 = ((int64_t)(v14));
  t53 = ((int64_t)(((int64_t)UINT64_C(0))));
  t54 = ((t52) != (t53));
  t55 = ((int64_t)(((int64_t)UINT64_C(26))));
  fg161_gg_function_6(t54, t55, base + 0);
  t56 = ((uint64_t)(v12));
  t57 = ((int64_t)(((int64_t)UINT64_C(1))));
  t58 = fg161_heap(t56, t57);
  v15 = ((int64_t)(t58));
  t59 = ((int64_t)(v15));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = ((t59) == (t60));
  if (t61) {
  {
  t62 = ((int64_t)(v14));
  fg161_release(t62);
  t63 = ((int64_t)(((int64_t)UINT64_C(27))));
  fg161_fail(t63);
  t0 = ((uint8_t)UINT64_C(0));
  goto fg161_gg_done;
  }
  }
  t64 = ((uint64_t)(v13));
  t65 = ((int64_t)(((int64_t)UINT64_C(0))));
  t66 = fg161_heap(t64, t65);
  v16 = ((int64_t)(t66));
  t67 = ((int64_t)(v16));
  t68 = ((int64_t)(((int64_t)UINT64_C(0))));
  t69 = ((t67) == (t68));
  if (t69) {
  {
  t70 = ((int64_t)(v14));
  fg161_release(t70);
  t71 = ((int64_t)(v15));
  fg161_release(t71);
  t72 = ((int64_t)(((int64_t)UINT64_C(28))));
  fg161_fail(t72);
  t0 = ((uint8_t)UINT64_C(0));
  goto fg161_gg_done;
  }
  }
  t73 = ((int64_t)(v9));
  t74 = ((int64_t)(((int64_t)UINT64_C(9))));
  t75 = ((int64_t)(((int64_t)UINT64_C(1))));
  t76 = ((int64_t)(((int64_t)UINT64_C(0))));
  t77 = fg161_get(t73, t74, t75, t76);
  t78 = ((uint64_t)(t77));
  t79 = fg161_gg_function_3(t78, base + 0);
  fg161_release(t79);
  t80 = ((int64_t)(v9));
  t81 = ((int64_t)(((int64_t)UINT64_C(9))));
  t82 = ((int64_t)(((int64_t)UINT64_C(2))));
  t83 = ((int64_t)(((int64_t)UINT64_C(0))));
  t84 = fg161_get(t80, t81, t82, t83);
  t85 = ((uint64_t)(t84));
  t86 = fg161_gg_function_3(t85, base + 0);
  fg161_release(t86);
  t87 = ((int64_t)(v0));
  t88 = ((int64_t)(((int64_t)UINT64_C(6))));
  t89 = ((int64_t)(((int64_t)UINT64_C(0))));
  t90 = fg161_gg_function_50(t87, t88, t89, base + 0);
  t91 = fg161_gg_function_3(t90, base + 0);
  fg161_release(t91);
  t92 = ((int64_t)(v9));
  t93 = ((int64_t)(((int64_t)UINT64_C(9))));
  t94 = ((int64_t)(((int64_t)UINT64_C(0))));
  t95 = ((int64_t)(((int64_t)UINT64_C(0))));
  t96 = ((uint64_t)(v10));
  fg161_put(t92, t93, t94, t95, t96);
  t97 = ((int64_t)(v9));
  t98 = ((int64_t)(((int64_t)UINT64_C(9))));
  t99 = ((int64_t)(((int64_t)UINT64_C(1))));
  t100 = ((int64_t)(((int64_t)UINT64_C(0))));
  t101 = ((uint64_t)(((uint64_t)(v15))));
  fg161_put(t97, t98, t99, t100, t101);
  t102 = ((int64_t)(v9));
  t103 = ((int64_t)(((int64_t)UINT64_C(9))));
  t104 = ((int64_t)(((int64_t)UINT64_C(2))));
  t105 = ((int64_t)(((int64_t)UINT64_C(0))));
  t106 = ((uint64_t)(((uint64_t)(v14))));
  fg161_put(t102, t103, t104, t105, t106);
  t107 = ((int64_t)(v0));
  t108 = ((int64_t)(((int64_t)UINT64_C(6))));
  t109 = ((int64_t)(((int64_t)UINT64_C(0))));
  t110 = ((uint64_t)(((uint64_t)(v16))));
  fg161_gg_function_48(t107, t108, t109, t110, base + 0);
  }
  }
  v17 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t111 = ((int64_t)(v17));
  t112 = ((int64_t)(v0));
  t113 = ((int64_t)(((int64_t)UINT64_C(0))));
  t114 = ((int64_t)(((int64_t)UINT64_C(0))));
  t115 = fg161_gg_function_50(t112, t113, t114, base + 0);
  t116 = fg161_gg_function_3(t115, base + 0);
  t117 = ((int64_t)(t116));
  if (!(((t111) < (t117)))) goto label2;
  {
  t118 = ((int64_t)(v0));
  t119 = ((int64_t)(((int64_t)UINT64_C(3))));
  t120 = ((int64_t)(v17));
  t121 = fg161_gg_function_50(t118, t119, t120, base + 0);
  t122 = fg161_gg_function_3(t121, base + 0);
  fg161_gg_function_27(t122, base + 0);
  t123 = v17;
  t124 = ((int64_t)(((int64_t)UINT64_C(1))));
  v17 = ((int64_t)((uint64_t)(t123) + (uint64_t)(t124)));
  }
  goto label1;
label2:;
  t125 = ((int64_t)(v0));
  t126 = ((int64_t)(v1));
  t127 = ((int64_t)(v2));
  t128 = ((int64_t)(v3));
  fg161_gg_function_69(t125, t126, t127, t128, base + 0);
  t129 = ((int64_t)(v0));
  t130 = ((int64_t)(((int64_t)UINT64_C(7))));
  t131 = ((int64_t)(((int64_t)UINT64_C(0))));
  t132 = fg161_gg_function_50(t129, t130, t131, base + 0);
  t133 = ((uint64_t)(t132));
  t134 = ((uint64_t)(v5));
  t135 = ((t133) < (t134));
  if (t135) {
  {
  t136 = ((uint64_t)(v5));
  t137 = ((int64_t)(((int64_t)UINT64_C(7))));
  t138 = fg161_constant(t137);
  t139 = ((uint64_t)(t138));
  t140 = fg161_gg_function_47(t136, t139, base + 0);
  t141 = ((int64_t)(((int64_t)UINT64_C(1))));
  t142 = fg161_heap(t140, t141);
  v18 = ((int64_t)(t142));
  t143 = ((int64_t)(v18));
  t144 = ((int64_t)(((int64_t)UINT64_C(0))));
  t145 = ((t143) != (t144));
  t146 = ((int64_t)(((int64_t)UINT64_C(29))));
  fg161_gg_function_6(t145, t146, base + 0);
  t147 = ((int64_t)(v0));
  t148 = ((int64_t)(((int64_t)UINT64_C(8))));
  t149 = ((int64_t)(((int64_t)UINT64_C(0))));
  t150 = fg161_gg_function_50(t147, t148, t149, base + 0);
  t151 = fg161_gg_function_3(t150, base + 0);
  fg161_release(t151);
  t152 = ((int64_t)(v0));
  t153 = ((int64_t)(((int64_t)UINT64_C(8))));
  t154 = ((int64_t)(((int64_t)UINT64_C(0))));
  t155 = ((uint64_t)(((uint64_t)(v18))));
  fg161_gg_function_48(t152, t153, t154, t155, base + 0);
  }
  }
  t156 = ((int64_t)(v0));
  t157 = ((int64_t)(((int64_t)UINT64_C(7))));
  t158 = ((int64_t)(((int64_t)UINT64_C(0))));
  t159 = ((uint64_t)(v5));
  fg161_gg_function_48(t156, t157, t158, t159, base + 0);
  v17 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t160 = ((uint64_t)(((uint64_t)(v17))));
  t161 = ((uint64_t)(v5));
  if (!(((t160) < (t161)))) goto label5;
  {
  t162 = ((int64_t)(v1));
  t163 = ((int64_t)(((int64_t)UINT64_C(8))));
  t164 = ((int64_t)(((int64_t)UINT64_C(2))));
  t165 = ((int64_t)(v17));
  t166 = fg161_get(t162, t163, t164, t165);
  t167 = ((uint64_t)(t166));
  v19 = fg161_gg_function_3(t167, base + 0);
  t168 = ((int64_t)(v0));
  t169 = ((int64_t)(v19));
  t170 = ((int64_t)(v0));
  t171 = ((int64_t)(((int64_t)UINT64_C(1))));
  t172 = ((int64_t)(v17));
  t173 = ((int64_t)UINT64_C(1));
  t174 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t173)))));
  t175 = fg161_record(t170, t171, t172, t174);
  t176 = ((int64_t)(t175));
  fg161_gg_function_75(t168, t169, t176, base + 0);
  v20 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t177 = ((uint64_t)(((uint64_t)(v20))));
  t178 = ((int64_t)(((int64_t)UINT64_C(12))));
  t179 = fg161_constant(t178);
  t180 = ((uint64_t)(t179));
  if (!(((t177) < (t180)))) goto label8;
  {
  t181 = ((int64_t)(v0));
  t182 = ((int64_t)(v19));
  t183 = ((int64_t)(((int64_t)UINT64_C(9))));
  t184 = ((int64_t)(v20));
  t185 = fg161_gg_function_59(t182, t183, t184, base + 0);
  t186 = fg161_gg_function_3(t185, base + 0);
  t187 = ((int64_t)(v0));
  t188 = ((int64_t)(((int64_t)UINT64_C(1))));
  t189 = ((int64_t)(v17));
  t190 = ((int64_t)(v20));
  t191 = fg161_record(t187, t188, t189, t190);
  t192 = ((int64_t)(t191));
  fg161_gg_function_75(t181, t186, t192, base + 0);
  t193 = v20;
  t194 = ((int64_t)(((int64_t)UINT64_C(1))));
  v20 = ((int64_t)((uint64_t)(t193) + (uint64_t)(t194)));
  }
  goto label7;
label8:;
  t195 = v17;
  t196 = ((int64_t)(((int64_t)UINT64_C(1))));
  v17 = ((int64_t)((uint64_t)(t195) + (uint64_t)(t196)));
  }
  goto label4;
label5:;
  t197 = ((int64_t)(v0));
  t198 = ((int64_t)(((int64_t)UINT64_C(9))));
  t199 = ((int64_t)(((int64_t)UINT64_C(0))));
  t200 = fg161_gg_function_50(t197, t198, t199, base + 0);
  t201 = ((uint64_t)(t200));
  t202 = ((uint64_t)(v6));
  t203 = ((t201) < (t202));
  if (t203) {
  {
  t204 = ((uint64_t)(v6));
  t205 = ((int64_t)(((int64_t)UINT64_C(8))));
  t206 = fg161_constant(t205);
  t207 = ((uint64_t)(t206));
  t208 = fg161_gg_function_47(t204, t207, base + 0);
  t209 = ((int64_t)(((int64_t)UINT64_C(1))));
  t210 = fg161_heap(t208, t209);
  v21 = ((int64_t)(t210));
  t211 = ((int64_t)(v21));
  t212 = ((int64_t)(((int64_t)UINT64_C(0))));
  t213 = ((t211) != (t212));
  t214 = ((int64_t)(((int64_t)UINT64_C(30))));
  fg161_gg_function_6(t213, t214, base + 0);
  t215 = ((int64_t)(v0));
  t216 = ((int64_t)(((int64_t)UINT64_C(10))));
  t217 = ((int64_t)(((int64_t)UINT64_C(0))));
  t218 = fg161_gg_function_50(t215, t216, t217, base + 0);
  t219 = fg161_gg_function_3(t218, base + 0);
  fg161_release(t219);
  t220 = ((int64_t)(v0));
  t221 = ((int64_t)(((int64_t)UINT64_C(10))));
  t222 = ((int64_t)(((int64_t)UINT64_C(0))));
  t223 = ((uint64_t)(((uint64_t)(v21))));
  fg161_gg_function_48(t220, t221, t222, t223, base + 0);
  }
  }
  t224 = ((int64_t)(v0));
  t225 = ((int64_t)(((int64_t)UINT64_C(9))));
  t226 = ((int64_t)(((int64_t)UINT64_C(0))));
  t227 = ((uint64_t)(v6));
  fg161_gg_function_48(t224, t225, t226, t227, base + 0);
  v17 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t228 = ((uint64_t)(((uint64_t)(v17))));
  t229 = ((uint64_t)(v6));
  if (!(((t228) < (t229)))) goto label11;
  {
  t230 = ((int64_t)(v1));
  t231 = ((int64_t)(((int64_t)UINT64_C(8))));
  t232 = ((int64_t)(((int64_t)UINT64_C(3))));
  t233 = ((int64_t)(v17));
  t234 = fg161_get(t230, t231, t232, t233);
  t235 = ((uint64_t)(t234));
  v22 = fg161_gg_function_3(t235, base + 0);
  t236 = ((int64_t)(v0));
  t237 = ((int64_t)(v22));
  t238 = fg161_gg_function_53(t236, t237, base + 0);
  (void)(t238);
  t239 = ((int64_t)(v0));
  t240 = ((int64_t)(v22));
  t241 = ((int64_t)(v0));
  t242 = ((int64_t)(((int64_t)UINT64_C(2))));
  t243 = ((int64_t)(v17));
  t244 = ((int64_t)(((int64_t)UINT64_C(0))));
  t245 = fg161_record(t241, t242, t243, t244);
  t246 = ((int64_t)(t245));
  fg161_gg_function_75(t239, t240, t246, base + 0);
  t247 = v17;
  t248 = ((int64_t)(((int64_t)UINT64_C(1))));
  v17 = ((int64_t)((uint64_t)(t247) + (uint64_t)(t248)));
  }
  goto label10;
label11:;
  v17 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t249 = ((int64_t)(v17));
  t250 = ((int64_t)(v0));
  t251 = ((int64_t)(((int64_t)UINT64_C(0))));
  t252 = ((int64_t)(((int64_t)UINT64_C(0))));
  t253 = fg161_gg_function_50(t250, t251, t252, base + 0);
  t254 = fg161_gg_function_3(t253, base + 0);
  t255 = ((int64_t)(t254));
  if (!(((t249) < (t255)))) goto label14;
  {
  v23 = ((int64_t)(((int64_t)UINT64_C(0))));
label16:;
  t256 = ((int64_t)(v23));
  t257 = ((int64_t)(v17));
  if (!(((t256) < (t257)))) goto label17;
  {
  t258 = ((int64_t)(v0));
  t259 = ((int64_t)(((int64_t)UINT64_C(3))));
  t260 = ((int64_t)(v23));
  t261 = fg161_gg_function_50(t258, t259, t260, base + 0);
  t262 = ((uint64_t)(t261));
  t263 = ((int64_t)(v0));
  t264 = ((int64_t)(((int64_t)UINT64_C(3))));
  t265 = ((int64_t)(v17));
  t266 = fg161_gg_function_50(t263, t264, t265, base + 0);
  t267 = ((uint64_t)(t266));
  t268 = ((t262) == (t267));
  if (t268) {
  {
  t269 = ((int64_t)(v0));
  t270 = ((int64_t)(((int64_t)UINT64_C(2))));
  t271 = ((int64_t)(v17));
  t272 = ((int64_t)(v0));
  t273 = ((int64_t)(((int64_t)UINT64_C(2))));
  t274 = ((int64_t)(v23));
  t275 = fg161_gg_function_50(t272, t273, t274, base + 0);
  fg161_gg_function_48(t269, t270, t271, t275, base + 0);
  goto label17;
  }
  }
  t276 = v23;
  t277 = ((int64_t)(((int64_t)UINT64_C(1))));
  v23 = ((int64_t)((uint64_t)(t276) + (uint64_t)(t277)));
  }
  goto label16;
label17:;
  t278 = ((int64_t)(v0));
  t279 = ((int64_t)(((int64_t)UINT64_C(3))));
  t280 = ((int64_t)(v17));
  t281 = fg161_gg_function_50(t278, t279, t280, base + 0);
  v24 = fg161_gg_function_3(t281, base + 0);
  t282 = ((int64_t)(v0));
  t283 = ((int64_t)(((int64_t)UINT64_C(2))));
  t284 = ((int64_t)(v17));
  t285 = fg161_gg_function_50(t282, t283, t284, base + 0);
  v25 = fg161_gg_function_3(t285, base + 0);
  t286 = ((int64_t)(v25));
  t287 = ((int64_t)(((int64_t)UINT64_C(0))));
  v26 = ((t286) == (t287));
  v27 = ((int64_t)(((int64_t)UINT64_C(0))));
  v28 = ((uint64_t)(((int64_t)UINT64_C(0))));
label19:;
  t288 = ((int64_t)(v27));
  t289 = ((int64_t)(v24));
  t290 = ((int64_t)(((int64_t)UINT64_C(2))));
  t291 = ((int64_t)(((int64_t)UINT64_C(0))));
  t292 = fg161_gg_function_21(t289, t290, t291, base + 0);
  t293 = fg161_gg_function_3(t292, base + 0);
  t294 = ((int64_t)(t293));
  if (!(((t288) < (t294)))) goto label20;
  {
  t295 = ((int64_t)(v25));
  t296 = ((int64_t)(((int64_t)UINT64_C(0))));
  t297 = ((t295) != (t296));
  if (t297) {
  {
  t298 = ((int64_t)(v25));
  t299 = ((int64_t)(v27));
  v29 = fg161_gg_function_36(t298, t299, base + 0);
  }
  } else {
  {
  v29 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t300 = ((int64_t)(v24));
  t301 = ((int64_t)(v27));
  v30 = fg161_gg_function_32(t300, t301, base + 0);
  t302 = ((uint64_t)(v28));
  t303 = ((uint64_t)(v30));
  v28 = fg161_gg_function_8(t302, t303, base + 0);
  t304 = ((uint64_t)(v30));
  t305 = ((uint64_t)(v29));
  t306 = ((t304) > (t305));
  if (t306) {
  {
  v26 = ((uint8_t)UINT64_C(1));
  }
  }
  t307 = v27;
  t308 = ((int64_t)(((int64_t)UINT64_C(1))));
  v27 = ((int64_t)((uint64_t)(t307) + (uint64_t)(t308)));
  }
  goto label19;
label20:;
  t309 = v26;
  if (t309) {
  {
  t310 = ((int64_t)(v25));
  fg161_gg_function_33(t310, base + 0);
  t311 = ((int64_t)(v0));
  t312 = ((int64_t)(((int64_t)UINT64_C(2))));
  t313 = ((int64_t)(v17));
  t314 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg161_gg_function_48(t311, t312, t313, t314, base + 0);
  t315 = v4;
  t316 = (!(t315));
  if (t316) {
  {
  t317 = ((int64_t)(v0));
  t318 = ((int64_t)(((int64_t)UINT64_C(1))));
  t319 = ((int64_t)(v17));
  t320 = fg161_gg_function_50(t317, t318, t319, base + 0);
  t321 = fg161_gg_function_3(t320, base + 0);
  t322 = ((int64_t)(v24));
  t323 = ((int64_t)(((int64_t)UINT64_C(15))));
  t324 = fg161_constant(t323);
  t325 = ((int64_t)(((int64_t)(t324))));
  t326 = ((int64_t)(v0));
  t327 = ((int64_t)(((int64_t)UINT64_C(4))));
  t328 = ((int64_t)(v17));
  t329 = fg161_gg_function_50(t326, t327, t328, base + 0);
  v31 = fg161_gg_function_40(t321, t322, t325, t329, base + 0);
  t330 = ((int64_t)(v0));
  t331 = ((int64_t)(((int64_t)UINT64_C(2))));
  t332 = ((int64_t)(v17));
  t333 = ((uint64_t)(((uint64_t)(v31))));
  fg161_gg_function_48(t330, t331, t332, t333, base + 0);
  t334 = ((int64_t)(v31));
  t335 = ((int64_t)(((int64_t)UINT64_C(0))));
  t336 = ((t334) == (t335));
  if (t336) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg161_gg_done;
  }
  }
  }
  }
  }
  }
  t337 = v17;
  t338 = ((int64_t)(((int64_t)UINT64_C(1))));
  v17 = ((int64_t)((uint64_t)(t337) + (uint64_t)(t338)));
  }
  goto label13;
label14:;
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static uint8_t fg161_gg_function_79(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
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
  uint64_t t20;
  uint64_t t21;
  int64_t t22;
  int64_t t23;
  uint8_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  uint64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  uint64_t t39;
  uint64_t t40;
  uint64_t t41;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  v3 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(6))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg161_gg_function_59(t1, t2, t3, base + 0);
  t5 = ((uint64_t)(t4));
  t6 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) == (t6));
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(7))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fg161_gg_function_59(t9, t10, t11, base + 0);
  t13 = ((uint64_t)(t12));
  t14 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t13) == (t14));
  }
  t15 = t8;
  if (t15) {
  {
  t16 = ((int64_t)(v2));
  t17 = ((int64_t)(((int64_t)UINT64_C(6))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = fg161_get(t16, t17, t18, t19);
  t21 = ((uint64_t)(t20));
  v4 = fg161_gg_function_3(t21, base + 0);
  t22 = ((int64_t)(v4));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((t22) < (t23));
  if (t24) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg161_gg_done;
  }
  }
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(v4));
  fg161_gg_function_61(t25, t26, base + 0);
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t29 = ((int64_t)(v4));
  t30 = fg161_gg_function_50(t27, t28, t29, base + 0);
  t31 = fg161_gg_function_3(t30, base + 0);
  t32 = ((int64_t)(((int64_t)UINT64_C(2))));
  t33 = ((int64_t)(v1));
  t34 = fg161_buft(t31, t32, t33);
  v3 = ((uint64_t)(t34));
  }
  }
  t35 = ((int64_t)(v2));
  t36 = ((int64_t)(((int64_t)UINT64_C(6))));
  t37 = ((int64_t)(((int64_t)UINT64_C(3))));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = fg161_get(t35, t36, t37, t38);
  t40 = ((uint64_t)(t39));
  t41 = ((uint64_t)(v3));
  t0 = ((t40) >= (t41));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static uint8_t fg161_gg_function_80(int64_t p0, int64_t p1, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  uint64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint64_t t10;
  uint64_t t11;
  uint8_t t12;
  uint8_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  uint64_t t17;
  uint64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint64_t t23;
  uint64_t t24;
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  uint64_t t31;
  uint64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  uint64_t t39;
  uint64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  uint8_t t50;
  uint8_t t51;
  uint8_t t52;
  uint64_t t53;
  int64_t t54;
  uint64_t t55;
  uint64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  uint64_t t60;
  int64_t t61;
  int64_t t62;
  uint8_t t63;
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
  uint8_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(7))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg161_gg_function_50(t1, t2, t3, base + 0);
  t5 = ((uint64_t)(t4));
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(8))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = fg161_get(t6, t7, t8, t9);
  t11 = ((uint64_t)(t10));
  t12 = ((t5) != (t11));
  t13 = t12;
  if (!t13) {
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(9))));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = fg161_gg_function_50(t14, t15, t16, base + 0);
  t18 = ((uint64_t)(t17));
  t19 = ((int64_t)(v1));
  t20 = ((int64_t)(((int64_t)UINT64_C(8))));
  t21 = ((int64_t)(((int64_t)UINT64_C(1))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = fg161_get(t19, t20, t21, t22);
  t24 = ((uint64_t)(t23));
  t13 = ((t18) != (t24));
  }
  t25 = t13;
  if (t25) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t26 = ((int64_t)(v2));
  t27 = ((int64_t)(v1));
  t28 = ((int64_t)(((int64_t)UINT64_C(8))));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = fg161_get(t27, t28, t29, t30);
  t32 = ((uint64_t)(t31));
  t33 = fg161_gg_function_3(t32, base + 0);
  t34 = ((int64_t)(t33));
  if (!(((t26) < (t34)))) goto label2;
  {
  t35 = ((int64_t)(v1));
  t36 = ((int64_t)(((int64_t)UINT64_C(8))));
  t37 = ((int64_t)(((int64_t)UINT64_C(2))));
  t38 = ((int64_t)(v2));
  t39 = fg161_get(t35, t36, t37, t38);
  t40 = ((uint64_t)(t39));
  v3 = fg161_gg_function_3(t40, base + 0);
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(v3));
  t43 = ((int64_t)(v0));
  t44 = ((int64_t)(((int64_t)UINT64_C(1))));
  t45 = ((int64_t)(v2));
  t46 = ((int64_t)UINT64_C(1));
  t47 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t46)))));
  t48 = fg161_record(t43, t44, t45, t47);
  t49 = ((int64_t)(t48));
  t50 = fg161_gg_function_79(t41, t42, t49, base + 0);
  t51 = t50;
  t52 = (!(t51));
  if (t52) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t53 = ((uint64_t)(((uint64_t)(v4))));
  t54 = ((int64_t)(((int64_t)UINT64_C(12))));
  t55 = fg161_constant(t54);
  t56 = ((uint64_t)(t55));
  if (!(((t53) < (t56)))) goto label5;
  {
  t57 = ((int64_t)(v3));
  t58 = ((int64_t)(((int64_t)UINT64_C(9))));
  t59 = ((int64_t)(v4));
  t60 = fg161_gg_function_59(t57, t58, t59, base + 0);
  v5 = fg161_gg_function_3(t60, base + 0);
  t61 = ((int64_t)(v5));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t63 = ((t61) != (t62));
  t64 = t63;
  if (t64) {
  t65 = ((int64_t)(v0));
  t66 = ((int64_t)(v5));
  t67 = ((int64_t)(v0));
  t68 = ((int64_t)(((int64_t)UINT64_C(1))));
  t69 = ((int64_t)(v2));
  t70 = ((int64_t)(v4));
  t71 = fg161_record(t67, t68, t69, t70);
  t72 = ((int64_t)(t71));
  t73 = fg161_gg_function_79(t65, t66, t72, base + 0);
  t74 = t73;
  t64 = (!(t74));
  }
  t75 = t64;
  if (t75) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
  }
  t76 = v4;
  t77 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t76) + (uint64_t)(t77)));
  }
  goto label4;
label5:;
  t78 = v2;
  t79 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t78) + (uint64_t)(t79)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(0));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static uint8_t fg161_gg_function_81(int64_t p0, int64_t p1, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  uint8_t t17;
  uint8_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint64_t t30;
  int64_t t31;
  int64_t t32;
  uint8_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  uint64_t t42;
  uint64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  uint64_t t51;
  uint64_t t52;
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
  uint64_t t67;
  uint64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  uint64_t t75;
  uint64_t t76;
  uint64_t t77;
  int64_t t78;
  uint64_t t79;
  uint64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  uint64_t t84;
  int64_t t85;
  int64_t t86;
  uint8_t t87;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = fg161_gg_function_80(t1, t2, base + 0);
  t4 = t3;
  if (t4) {
  {
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg161_gg_function_50(t5, t6, t7, base + 0);
  t9 = ((uint64_t)(t8));
  t10 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t11 = ((t9) != (t10));
  if (t11) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg161_gg_done;
  }
  }
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((uint8_t)UINT64_C(0));
  t17 = fg161_gg_function_73(t12, t13, t14, t15, t16, base + 0);
  t18 = t17;
  t19 = (!(t18));
  if (t19) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg161_gg_done;
  }
  }
  }
  }
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t20 = ((int64_t)(v2));
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = fg161_gg_function_50(t21, t22, t23, base + 0);
  t25 = fg161_gg_function_3(t24, base + 0);
  t26 = ((int64_t)(t25));
  if (!(((t20) < (t26)))) goto label2;
  {
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(2))));
  t29 = ((int64_t)(v2));
  t30 = fg161_gg_function_50(t27, t28, t29, base + 0);
  v3 = fg161_gg_function_3(t30, base + 0);
  t31 = ((int64_t)(v3));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = ((t31) != (t32));
  if (t33) {
  {
  t34 = ((int64_t)(v3));
  fg161_gg_function_44(t34, base + 0);
  }
  }
  t35 = v2;
  t36 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t35) + (uint64_t)(t36)));
  }
  goto label1;
label2:;
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t37 = ((int64_t)(v2));
  t38 = ((int64_t)(v1));
  t39 = ((int64_t)(((int64_t)UINT64_C(8))));
  t40 = ((int64_t)(((int64_t)UINT64_C(1))));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = fg161_get(t38, t39, t40, t41);
  t43 = ((uint64_t)(t42));
  t44 = fg161_gg_function_3(t43, base + 0);
  t45 = ((int64_t)(t44));
  if (!(((t37) < (t45)))) goto label5;
  {
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(v1));
  t48 = ((int64_t)(((int64_t)UINT64_C(8))));
  t49 = ((int64_t)(((int64_t)UINT64_C(3))));
  t50 = ((int64_t)(v2));
  t51 = fg161_get(t47, t48, t49, t50);
  t52 = ((uint64_t)(t51));
  t53 = fg161_gg_function_3(t52, base + 0);
  t54 = ((int64_t)(v0));
  t55 = ((int64_t)(((int64_t)UINT64_C(2))));
  t56 = ((int64_t)(v2));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = fg161_record(t54, t55, t56, t57);
  t59 = ((int64_t)(t58));
  fg161_gg_function_77(t46, t53, t59, base + 0);
  t60 = v2;
  t61 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t60) + (uint64_t)(t61)));
  }
  goto label4;
label5:;
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t62 = ((int64_t)(v2));
  t63 = ((int64_t)(v1));
  t64 = ((int64_t)(((int64_t)UINT64_C(8))));
  t65 = ((int64_t)(((int64_t)UINT64_C(0))));
  t66 = ((int64_t)(((int64_t)UINT64_C(0))));
  t67 = fg161_get(t63, t64, t65, t66);
  t68 = ((uint64_t)(t67));
  t69 = fg161_gg_function_3(t68, base + 0);
  t70 = ((int64_t)(t69));
  if (!(((t62) < (t70)))) goto label8;
  {
  t71 = ((int64_t)(v1));
  t72 = ((int64_t)(((int64_t)UINT64_C(8))));
  t73 = ((int64_t)(((int64_t)UINT64_C(2))));
  t74 = ((int64_t)(v2));
  t75 = fg161_get(t71, t72, t73, t74);
  t76 = ((uint64_t)(t75));
  v4 = fg161_gg_function_3(t76, base + 0);
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t77 = ((uint64_t)(((uint64_t)(v5))));
  t78 = ((int64_t)(((int64_t)UINT64_C(12))));
  t79 = fg161_constant(t78);
  t80 = ((uint64_t)(t79));
  if (!(((t77) < (t80)))) goto label11;
  {
  t81 = ((int64_t)(v4));
  t82 = ((int64_t)(((int64_t)UINT64_C(9))));
  t83 = ((int64_t)(v5));
  t84 = fg161_gg_function_59(t81, t82, t83, base + 0);
  v6 = fg161_gg_function_3(t84, base + 0);
  t85 = ((int64_t)(v6));
  t86 = ((int64_t)(((int64_t)UINT64_C(0))));
  t87 = ((t85) != (t86));
  if (t87) {
  {
  t88 = ((int64_t)(v0));
  t89 = ((int64_t)(v6));
  t90 = ((int64_t)(v0));
  t91 = ((int64_t)(((int64_t)UINT64_C(1))));
  t92 = ((int64_t)(v2));
  t93 = ((int64_t)(v5));
  t94 = fg161_record(t90, t91, t92, t93);
  t95 = ((int64_t)(t94));
  fg161_gg_function_77(t88, t89, t95, base + 0);
  }
  }
  t96 = v5;
  t97 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t96) + (uint64_t)(t97)));
  }
  goto label10;
label11:;
  t98 = ((int64_t)(v0));
  t99 = ((int64_t)(v4));
  t100 = ((int64_t)(v0));
  t101 = ((int64_t)(((int64_t)UINT64_C(1))));
  t102 = ((int64_t)(v2));
  t103 = ((int64_t)UINT64_C(1));
  t104 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t103)))));
  t105 = fg161_record(t100, t101, t102, t104);
  t106 = ((int64_t)(t105));
  fg161_gg_function_77(t98, t99, t106, base + 0);
  t107 = v2;
  t108 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t107) + (uint64_t)(t108)));
  }
  goto label7;
label8:;
  t0 = ((uint8_t)UINT64_C(1));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static void fg161_gg_function_31(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  uint64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  goto fg161_gg_done;
  }
  }
  v1 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(2))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = fg161_gg_function_21(t4, t5, t6, base + 0);
  t8 = fg161_gg_function_3(t7, base + 0);
  t9 = ((int64_t)(t8));
  if (!(((t3) < (t9)))) goto label2;
  {
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(3))));
  t12 = ((int64_t)(v1));
  t13 = fg161_gg_function_21(t10, t11, t12, base + 0);
  t14 = fg161_gg_function_3(t13, base + 0);
  fg161_release(t14);
  t15 = v1;
  t16 = ((int64_t)(((int64_t)UINT64_C(1))));
  v1 = ((int64_t)((uint64_t)(t15) + (uint64_t)(t16)));
  }
  goto label1;
label2:;
  t17 = ((int64_t)(v0));
  fg161_release(t17);
  }
fg161_gg_done:;
}
static void fg161_gg_function_49(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint64_t t16;
  uint8_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  uint64_t t26;
  uint64_t t27;
  uint64_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  uint8_t t32;
  uint8_t t33;
  uint64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint64_t t40;
  uint64_t t41;
  uint64_t t42;
  uint8_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  uint64_t t52;
  uint64_t t53;
  uint64_t t54;
  uint8_t t55;
  int64_t t56;
  int64_t t57;
  uint8_t t58;
  uint8_t t59;
  uint64_t t60;
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
  uint64_t t72;
  uint64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  uint64_t t79;
  uint64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  uint64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  uint64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  uint64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  uint64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  uint64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  uint64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  uint64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  int64_t v3;
  uint8_t v4;
  uint64_t v5;
  int64_t v6;
  uint8_t v7;
  int64_t v8;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  goto fg161_gg_done;
  }
  }
  v1 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = fg161_gg_function_50(t4, t5, t6, base + 0);
  t8 = fg161_gg_function_3(t7, base + 0);
  t9 = ((int64_t)(t8));
  if (!(((t3) < (t9)))) goto label2;
  {
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
  t12 = ((int64_t)UINT64_C(1));
  t13 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t12)))));
  t14 = fg161_gg_function_50(t10, t11, t13, base + 0);
  t15 = ((uint64_t)(t14));
  t16 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t15) != (t16));
  if (t17) {
  {
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(2))));
  t20 = ((int64_t)(v1));
  v2 = fg161_gg_function_50(t18, t19, t20, base + 0);
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
  v4 = ((uint8_t)UINT64_C(0));
label4:;
  t21 = ((int64_t)(v3));
  t22 = ((int64_t)(v1));
  if (!(((t21) < (t22)))) goto label5;
  {
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(((int64_t)UINT64_C(2))));
  t25 = ((int64_t)(v3));
  t26 = fg161_gg_function_50(t23, t24, t25, base + 0);
  t27 = ((uint64_t)(t26));
  t28 = ((uint64_t)(v2));
  t29 = ((t27) == (t28));
  if (t29) {
  {
  v4 = ((uint8_t)UINT64_C(1));
  goto label5;
  }
  }
  t30 = v3;
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t30) + (uint64_t)(t31)));
  }
  goto label4;
label5:;
  t32 = v4;
  t33 = (!(t32));
  if (t33) {
  {
  t34 = ((uint64_t)(v2));
  t35 = fg161_gg_function_3(t34, base + 0);
  fg161_gg_function_33(t35, base + 0);
  }
  }
  }
  }
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(((int64_t)UINT64_C(3))));
  t38 = ((int64_t)UINT64_C(1));
  t39 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t38)))));
  t40 = fg161_gg_function_50(t36, t37, t39, base + 0);
  t41 = ((uint64_t)(t40));
  t42 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t43 = ((t41) != (t42));
  if (t43) {
  {
  t44 = ((int64_t)(v0));
  t45 = ((int64_t)(((int64_t)UINT64_C(3))));
  t46 = ((int64_t)(v1));
  v5 = fg161_gg_function_50(t44, t45, t46, base + 0);
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
  v7 = ((uint8_t)UINT64_C(0));
label7:;
  t47 = ((int64_t)(v6));
  t48 = ((int64_t)(v1));
  if (!(((t47) < (t48)))) goto label8;
  {
  t49 = ((int64_t)(v0));
  t50 = ((int64_t)(((int64_t)UINT64_C(3))));
  t51 = ((int64_t)(v6));
  t52 = fg161_gg_function_50(t49, t50, t51, base + 0);
  t53 = ((uint64_t)(t52));
  t54 = ((uint64_t)(v5));
  t55 = ((t53) == (t54));
  if (t55) {
  {
  v7 = ((uint8_t)UINT64_C(1));
  goto label8;
  }
  }
  t56 = v6;
  t57 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t56) + (uint64_t)(t57)));
  }
  goto label7;
label8:;
  t58 = v7;
  t59 = (!(t58));
  if (t59) {
  {
  t60 = ((uint64_t)(v5));
  t61 = fg161_gg_function_3(t60, base + 0);
  fg161_gg_function_31(t61, base + 0);
  }
  }
  }
  }
  t62 = v1;
  t63 = ((int64_t)(((int64_t)UINT64_C(1))));
  v1 = ((int64_t)((uint64_t)(t62) + (uint64_t)(t63)));
  }
  goto label1;
label2:;
  t64 = ((int64_t)(v0));
  t65 = ((int64_t)(((int64_t)UINT64_C(5))));
  t66 = ((int64_t)(((int64_t)UINT64_C(0))));
  t67 = fg161_gg_function_50(t64, t65, t66, base + 0);
  v8 = fg161_gg_function_3(t67, base + 0);
  t68 = ((int64_t)(v8));
  t69 = ((int64_t)(((int64_t)UINT64_C(9))));
  t70 = ((int64_t)(((int64_t)UINT64_C(1))));
  t71 = ((int64_t)(((int64_t)UINT64_C(0))));
  t72 = fg161_get(t68, t69, t70, t71);
  t73 = ((uint64_t)(t72));
  t74 = fg161_gg_function_3(t73, base + 0);
  fg161_release(t74);
  t75 = ((int64_t)(v8));
  t76 = ((int64_t)(((int64_t)UINT64_C(9))));
  t77 = ((int64_t)(((int64_t)UINT64_C(2))));
  t78 = ((int64_t)(((int64_t)UINT64_C(0))));
  t79 = fg161_get(t75, t76, t77, t78);
  t80 = ((uint64_t)(t79));
  t81 = fg161_gg_function_3(t80, base + 0);
  fg161_release(t81);
  t82 = ((int64_t)(v0));
  t83 = ((int64_t)(((int64_t)UINT64_C(6))));
  t84 = ((int64_t)(((int64_t)UINT64_C(0))));
  t85 = fg161_gg_function_50(t82, t83, t84, base + 0);
  t86 = fg161_gg_function_3(t85, base + 0);
  fg161_release(t86);
  t87 = ((int64_t)(v0));
  t88 = ((int64_t)(((int64_t)UINT64_C(1))));
  t89 = ((int64_t)UINT64_C(1));
  t90 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t89)))));
  t91 = fg161_gg_function_50(t87, t88, t90, base + 0);
  t92 = fg161_gg_function_3(t91, base + 0);
  fg161_release(t92);
  t93 = ((int64_t)(v0));
  t94 = ((int64_t)(((int64_t)UINT64_C(2))));
  t95 = ((int64_t)UINT64_C(1));
  t96 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t95)))));
  t97 = fg161_gg_function_50(t93, t94, t96, base + 0);
  t98 = fg161_gg_function_3(t97, base + 0);
  fg161_release(t98);
  t99 = ((int64_t)(v0));
  t100 = ((int64_t)(((int64_t)UINT64_C(3))));
  t101 = ((int64_t)UINT64_C(1));
  t102 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t101)))));
  t103 = fg161_gg_function_50(t99, t100, t102, base + 0);
  t104 = fg161_gg_function_3(t103, base + 0);
  fg161_release(t104);
  t105 = ((int64_t)(v0));
  t106 = ((int64_t)(((int64_t)UINT64_C(4))));
  t107 = ((int64_t)UINT64_C(1));
  t108 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t107)))));
  t109 = fg161_gg_function_50(t105, t106, t108, base + 0);
  t110 = fg161_gg_function_3(t109, base + 0);
  fg161_release(t110);
  t111 = ((int64_t)(v0));
  t112 = ((int64_t)(((int64_t)UINT64_C(8))));
  t113 = ((int64_t)(((int64_t)UINT64_C(0))));
  t114 = fg161_gg_function_50(t111, t112, t113, base + 0);
  t115 = fg161_gg_function_3(t114, base + 0);
  fg161_release(t115);
  t116 = ((int64_t)(v0));
  t117 = ((int64_t)(((int64_t)UINT64_C(10))));
  t118 = ((int64_t)(((int64_t)UINT64_C(0))));
  t119 = fg161_gg_function_50(t116, t117, t118, base + 0);
  t120 = fg161_gg_function_3(t119, base + 0);
  fg161_release(t120);
  t121 = ((int64_t)(v0));
  fg161_release(t121);
  }
fg161_gg_done:;
}
static void fg161_gg_function_52(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint64_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint64_t t23;
  int64_t t24;
  int64_t t25;
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
  uint8_t t37;
  int64_t t38;
  int64_t t39;
  uint64_t t40;
  uint64_t t41;
  uint64_t t42;
  uint8_t t43;
  int64_t t44;
  int64_t t45;
  uint64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  uint64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  goto fg161_gg_done;
  }
  }
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = fg161_gg_function_50(t4, t5, t6, base + 0);
  t8 = fg161_gg_function_3(t7, base + 0);
  t9 = ((int64_t)(t8));
  if (!(((t3) < (t9)))) goto label2;
  {
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t10) != (t11));
  if (t12) {
  {
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(v2));
  t15 = fg161_ptr(t13, t14);
  v3 = ((uint64_t)(t15));
  }
  } else {
  {
  v3 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t16 = ((int64_t)(v1));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) != (t17));
  if (t18) {
  {
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t19 = ((int64_t)(v4));
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = fg161_gg_function_50(t20, t21, t22, base + 0);
  t24 = fg161_gg_function_3(t23, base + 0);
  t25 = ((int64_t)(t24));
  if (!(((t19) < (t25)))) goto label5;
  {
  t26 = ((int64_t)(v0));
  t27 = ((int64_t)(((int64_t)UINT64_C(3))));
  t28 = ((int64_t)(v4));
  t29 = fg161_gg_function_50(t26, t27, t28, base + 0);
  t30 = ((uint64_t)(t29));
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(3))));
  t33 = ((int64_t)(v2));
  t34 = fg161_gg_function_50(t31, t32, t33, base + 0);
  t35 = ((uint64_t)(t34));
  t36 = ((t30) == (t35));
  t37 = t36;
  if (t37) {
  t38 = ((int64_t)(v1));
  t39 = ((int64_t)(v4));
  t40 = fg161_ptr(t38, t39);
  t41 = ((uint64_t)(t40));
  t42 = ((uint64_t)(v3));
  t37 = ((t41) < (t42));
  }
  t43 = t37;
  if (t43) {
  {
  t44 = ((int64_t)(v1));
  t45 = ((int64_t)(v4));
  t46 = fg161_ptr(t44, t45);
  v3 = ((uint64_t)(t46));
  }
  }
  t47 = v4;
  t48 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t47) + (uint64_t)(t48)));
  }
  goto label4;
label5:;
  }
  }
  t49 = ((int64_t)(v0));
  t50 = ((int64_t)(((int64_t)UINT64_C(4))));
  t51 = ((int64_t)(v2));
  t52 = ((uint64_t)(v3));
  fg161_gg_function_48(t49, t50, t51, t52, base + 0);
  t53 = v2;
  t54 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t53) + (uint64_t)(t54)));
  }
  goto label1;
label2:;
  }
fg161_gg_done:;
}
static int64_t fg161_gg_function_30(uint64_t p0, uint64_t p1, int64_t base) {
  int64_t t0;
  uint64_t t1;
  int64_t t2;
  uint64_t t3;
  uint64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  uint64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  uint64_t t17;
  uint64_t t18;
  uint64_t t19;
  uint64_t t20;
  uint64_t t21;
  uint64_t t22;
  uint8_t t23;
  uint64_t t24;
  uint64_t t25;
  uint64_t t26;
  uint64_t t27;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  fg161_gg_function_5(t1, base + 0);
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = fg161_constant(t2);
  t4 = ((uint64_t)(t3));
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  t6 = fg161_heap(t4, t5);
  v2 = ((int64_t)(t6));
  t7 = ((int64_t)(v2));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg161_gg_done;
  }
  }
  t10 = ((int64_t)(v2));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((uint64_t)(v0));
  fg161_gg_function_23(t10, t11, t12, t13, base + 0);
  t14 = ((int64_t)(v2));
  t15 = ((int64_t)(((int64_t)UINT64_C(1))));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((uint64_t)(v1));
  t19 = fg161_gg_function_9(base + 0);
  t20 = ((uint64_t)(t19));
  t21 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t22 = ((uint64_t)(((uint64_t)fg161_gg_udiv(t20, t21))));
  t23 = ((t18) < (t22));
  if (t23) {
  {
  t17 = ((uint64_t)(v1));
  }
  } else {
  {
  t24 = fg161_gg_function_9(base + 0);
  t25 = ((uint64_t)(t24));
  t26 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t17 = ((uint64_t)(((uint64_t)fg161_gg_udiv(t25, t26))));
  }
  }
  t27 = ((uint64_t)(t17));
  fg161_gg_function_23(t14, t15, t16, t27, base + 0);
  t0 = ((int64_t)(v2));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static int64_t fg161_gg_function_46(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  uint8_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  int64_t t12;
  uint64_t t13;
  uint64_t t14;
  int64_t t15;
  uint64_t t16;
  uint64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  uint64_t t27;
  int64_t t28;
  int64_t t29;
  uint64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  uint8_t t35;
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
  uint64_t t49;
  uint64_t t50;
  int64_t t51;
  int64_t t52;
  uint8_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  uint64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  uint64_t t64;
  uint64_t t65;
  uint64_t t66;
  uint8_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  uint64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  uint64_t t80;
  uint64_t t81;
  uint64_t t82;
  uint8_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  uint64_t t87;
  uint64_t t88;
  uint64_t t89;
  uint8_t t90;
  uint8_t t91;
  uint64_t t92;
  uint64_t t93;
  uint64_t t94;
  uint64_t t95;
  uint64_t t96;
  uint64_t t97;
  uint8_t t98;
  int64_t t99;
  int64_t t100;
  uint64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  uint64_t t105;
  uint64_t t106;
  int64_t t107;
  int64_t t108;
  uint8_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  uint64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  uint64_t v9;
  int64_t v10;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) == (t6));
  t8 = t7;
  if (!t8) {
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t9) != (t10));
  }
  t4 = t8;
  }
  t11 = t4;
  t12 = ((int64_t)(((int64_t)UINT64_C(18))));
  fg161_gg_function_6(t11, t12, base + 0);
  t13 = ((uint64_t)(((uint64_t)(v1))));
  t14 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  v2 = fg161_gg_function_47(t13, t14, base + 0);
  t15 = ((int64_t)(((int64_t)UINT64_C(4))));
  t16 = fg161_constant(t15);
  t17 = ((uint64_t)(t16));
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  t19 = fg161_heap(t17, t18);
  v3 = ((int64_t)(t19));
  t20 = ((int64_t)(v3));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t20) != (t21));
  t23 = ((int64_t)(((int64_t)UINT64_C(19))));
  fg161_gg_function_6(t22, t23, base + 0);
  t24 = ((int64_t)(v3));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((uint64_t)(((uint64_t)(v1))));
  fg161_gg_function_48(t24, t25, t26, t27, base + 0);
  v4 = ((int64_t)(((int64_t)UINT64_C(1))));
label1:;
  t28 = ((int64_t)(v4));
  t29 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t28) <= (t29)))) goto label2;
  {
  t30 = ((uint64_t)(v2));
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  t32 = fg161_heap(t30, t31);
  v5 = ((int64_t)(t32));
  t33 = ((int64_t)(v5));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((t33) == (t34));
  if (t35) {
  {
  t36 = ((int64_t)(v3));
  fg161_gg_function_49(t36, base + 0);
  t37 = ((int64_t)(((int64_t)UINT64_C(20))));
  fg161_fail(t37);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg161_gg_done;
  }
  }
  t38 = ((int64_t)(v3));
  t39 = ((int64_t)(v4));
  t40 = ((int64_t)UINT64_C(1));
  t41 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t40)))));
  t42 = ((uint64_t)(((uint64_t)(v5))));
  fg161_gg_function_48(t38, t39, t41, t42, base + 0);
  t43 = v4;
  t44 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t43) + (uint64_t)(t44)));
  }
  goto label1;
label2:;
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t45 = ((int64_t)(v6));
  t46 = ((int64_t)(v1));
  if (!(((t45) < (t46)))) goto label5;
  {
  t47 = ((int64_t)(v0));
  t48 = ((int64_t)(v6));
  t49 = fg161_ptr(t47, t48);
  t50 = ((uint64_t)(t49));
  v7 = fg161_gg_function_3(t50, base + 0);
  t51 = ((int64_t)(v7));
  t52 = ((int64_t)(((int64_t)UINT64_C(0))));
  t53 = ((t51) == (t52));
  if (t53) {
  {
  t54 = ((int64_t)(v3));
  fg161_gg_function_49(t54, base + 0);
  t55 = ((int64_t)(((int64_t)UINT64_C(21))));
  fg161_fail(t55);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg161_gg_done;
  }
  }
  t56 = ((int64_t)(v3));
  t57 = ((int64_t)(((int64_t)UINT64_C(1))));
  t58 = ((int64_t)(v6));
  t59 = ((uint64_t)(((uint64_t)(v7))));
  fg161_gg_function_48(t56, t57, t58, t59, base + 0);
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t60 = ((int64_t)(v8));
  t61 = ((int64_t)(v6));
  if (!(((t60) < (t61)))) goto label8;
  {
  t62 = ((int64_t)(v0));
  t63 = ((int64_t)(v8));
  t64 = fg161_ptr(t62, t63);
  t65 = ((uint64_t)(t64));
  t66 = ((uint64_t)(((uint64_t)(v7))));
  t67 = ((t65) == (t66));
  if (t67) {
  {
  t68 = ((int64_t)(v3));
  t69 = ((int64_t)(((int64_t)UINT64_C(3))));
  t70 = ((int64_t)(v6));
  t71 = ((int64_t)(v3));
  t72 = ((int64_t)(((int64_t)UINT64_C(3))));
  t73 = ((int64_t)(v8));
  t74 = fg161_gg_function_50(t71, t72, t73, base + 0);
  fg161_gg_function_48(t68, t69, t70, t74, base + 0);
  goto label8;
  }
  }
  t75 = v8;
  t76 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = ((int64_t)((uint64_t)(t75) + (uint64_t)(t76)));
  }
  goto label7;
label8:;
  t77 = ((int64_t)(v3));
  t78 = ((int64_t)(((int64_t)UINT64_C(3))));
  t79 = ((int64_t)(v6));
  t80 = fg161_gg_function_50(t77, t78, t79, base + 0);
  t81 = ((uint64_t)(t80));
  t82 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t83 = ((t81) == (t82));
  if (t83) {
  {
  t84 = ((int64_t)(v7));
  t85 = ((int64_t)(((int64_t)UINT64_C(0))));
  t86 = ((int64_t)(((int64_t)UINT64_C(0))));
  t87 = fg161_buft(t84, t85, t86);
  v9 = ((uint64_t)(t87));
  t88 = ((uint64_t)(v9));
  t89 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t90 = ((t88) == (t89));
  t91 = t90;
  if (!t91) {
  t92 = ((uint64_t)(v9));
  t93 = ((uint64_t)(v9));
  t94 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t95 = ((uint64_t)(((uint64_t)((uint64_t)(t93) - (uint64_t)(t94)))));
  t96 = ((uint64_t)(((t92) & (t95))));
  t97 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t91 = ((t96) != (t97));
  }
  t98 = t91;
  if (t98) {
  {
  t99 = ((int64_t)(v3));
  fg161_gg_function_49(t99, base + 0);
  t100 = ((int64_t)(((int64_t)UINT64_C(22))));
  fg161_fail(t100);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg161_gg_done;
  }
  }
  t101 = ((uint64_t)(v9));
  t102 = ((int64_t)(v7));
  t103 = ((int64_t)(((int64_t)UINT64_C(1))));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  t105 = fg161_buft(t102, t103, t104);
  t106 = ((uint64_t)(t105));
  v10 = fg161_gg_function_30(t101, t106, base + 0);
  t107 = ((int64_t)(v10));
  t108 = ((int64_t)(((int64_t)UINT64_C(0))));
  t109 = ((t107) == (t108));
  if (t109) {
  {
  t110 = ((int64_t)(v3));
  fg161_gg_function_49(t110, base + 0);
  t111 = ((int64_t)(((int64_t)UINT64_C(38))));
  fg161_fail(t111);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg161_gg_done;
  }
  }
  t112 = ((int64_t)(v3));
  t113 = ((int64_t)(((int64_t)UINT64_C(3))));
  t114 = ((int64_t)(v6));
  t115 = ((uint64_t)(((uint64_t)(v10))));
  fg161_gg_function_48(t112, t113, t114, t115, base + 0);
  }
  }
  t116 = v6;
  t117 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t116) + (uint64_t)(t117)));
  }
  goto label4;
label5:;
  t0 = ((int64_t)(v3));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static int64_t fg161_gg_function_29(uint64_t p0, uint64_t p1, int64_t base) {
  int64_t t0;
  uint64_t t1;
  uint64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  int64_t t6;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(v1));
  v2 = fg161_gg_function_30(t1, t2, base + 0);
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) != (t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(8))));
  fg161_gg_function_6(t5, t6, base + 0);
  t0 = ((int64_t)(v2));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static uint64_t fg161_gg_function_1(int64_t p0, int64_t base) {
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
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  uint64_t t28;
  uint64_t t29;
  int64_t t30;
  int64_t t31;
  uint64_t t32;
  uint64_t t33;
  int64_t t34;
  int64_t t35;
  uint64_t t36;
  uint64_t t37;
  int64_t t38;
  int64_t t39;
  uint8_t t40;
  int64_t t41;
  uint8_t t42;
  uint8_t t43;
  int64_t t44;
  int64_t t45;
  uint8_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  uint64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  uint64_t t54;
  uint64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  uint64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  uint64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  uint64_t t75;
  uint64_t t76;
  uint64_t t77;
  uint64_t t78;
  int64_t t79;
  int64_t t80;
  uint8_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  uint64_t t86;
  uint64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  uint64_t t92;
  uint64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  uint64_t t97;
  uint64_t t98;
  uint64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  uint64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  uint64_t t108;
  uint64_t t109;
  uint64_t t110;
  uint8_t t111;
  uint8_t t112;
  uint64_t t113;
  uint64_t t114;
  uint64_t t115;
  uint64_t t116;
  uint8_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  uint64_t t122;
  uint64_t t123;
  uint64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  uint64_t t129;
  uint64_t t130;
  uint64_t t131;
  uint64_t t132;
  uint64_t t133;
  uint64_t t134;
  uint64_t t135;
  uint8_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  uint64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  uint8_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  uint64_t t148;
  uint64_t t149;
  int64_t t150;
  int64_t t151;
  uint64_t t152;
  uint64_t t153;
  int64_t t154;
  int64_t t155;
  uint8_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  uint8_t t161;
  int64_t t162;
  int64_t t163;
  int64_t t164;
  uint64_t t165;
  uint64_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  uint8_t t170;
  int64_t t171;
  int64_t t172;
  int64_t t173;
  uint64_t t174;
  uint64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  uint8_t t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  uint64_t t184;
  uint64_t t185;
  uint64_t t186;
  int64_t t187;
  int64_t t188;
  uint8_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  uint8_t t193;
  int64_t t194;
  int64_t t195;
  uint64_t t196;
  uint64_t t197;
  int64_t t198;
  int64_t t199;
  uint64_t t200;
  uint64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t t204;
  uint8_t t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  uint8_t t209;
  int64_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  uint8_t t214;
  int64_t t215;
  int64_t t216;
  int64_t t217;
  uint8_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  uint8_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  uint8_t t227;
  int64_t t228;
  int64_t t229;
  int64_t t230;
  uint64_t t231;
  uint64_t t232;
  int64_t t233;
  int64_t t234;
  uint64_t t235;
  uint64_t t236;
  int64_t t237;
  int64_t t238;
  uint8_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  uint64_t t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  uint8_t t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  uint64_t t251;
  int64_t t252;
  int64_t t253;
  uint8_t t254;
  int64_t t255;
  int64_t t256;
  int64_t t257;
  uint8_t t258;
  int64_t t259;
  int64_t t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  uint8_t t264;
  int64_t t265;
  int64_t t266;
  int64_t t267;
  int64_t t268;
  uint8_t t269;
  int64_t t270;
  int64_t t271;
  int64_t t272;
  uint8_t t273;
  int64_t t274;
  int64_t t275;
  int64_t t276;
  int64_t t277;
  int64_t t278;
  uint8_t t279;
  int64_t t280;
  int64_t t281;
  uint8_t t282;
  uint8_t t283;
  int64_t t284;
  int64_t t285;
  uint8_t t286;
  int64_t t287;
  int64_t t288;
  uint8_t t289;
  uint8_t t290;
  int64_t t291;
  int64_t t292;
  uint8_t t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  int64_t t297;
  int64_t t298;
  uint8_t t299;
  int64_t t300;
  int64_t t301;
  int64_t t302;
  int64_t t303;
  int64_t t304;
  uint8_t t305;
  int64_t t306;
  int64_t t307;
  int64_t t308;
  int64_t t309;
  uint8_t t310;
  int64_t t311;
  int64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t t315;
  uint8_t t316;
  int64_t t317;
  int64_t t318;
  int64_t t319;
  uint64_t t320;
  int64_t t321;
  int64_t t322;
  int64_t t323;
  uint8_t t324;
  int64_t t325;
  int64_t t326;
  int64_t t327;
  uint64_t t328;
  int64_t t329;
  uint64_t t330;
  uint64_t t331;
  uint8_t t332;
  uint8_t t333;
  uint8_t t334;
  int64_t t335;
  int64_t t336;
  uint8_t t337;
  int64_t t338;
  int64_t t339;
  int64_t t340;
  uint64_t t341;
  int64_t t342;
  uint8_t t343;
  uint8_t t344;
  int64_t t345;
  int64_t t346;
  int64_t t347;
  int64_t t348;
  int64_t t349;
  uint64_t t350;
  int64_t t351;
  int64_t t352;
  int64_t t353;
  int64_t t354;
  int64_t t355;
  uint64_t t356;
  int64_t t357;
  int64_t t358;
  int64_t t359;
  int64_t t360;
  uint64_t t361;
  int64_t t362;
  int64_t t363;
  uint64_t t364;
  int64_t t365;
  int64_t t366;
  uint64_t t367;
  int64_t t368;
  int64_t t369;
  uint64_t t370;
  int64_t t371;
  int64_t t372;
  uint64_t t373;
  int64_t t374;
  int64_t t375;
  int64_t t376;
  int64_t t377;
  uint8_t t378;
  int64_t t379;
  int64_t t380;
  int64_t t381;
  int64_t t382;
  int64_t t383;
  uint8_t t384;
  int64_t t385;
  int64_t t386;
  int64_t t387;
  uint8_t t388;
  uint8_t t389;
  int64_t t390;
  int64_t t391;
  uint8_t t392;
  int64_t t393;
  int64_t t394;
  uint8_t t395;
  uint8_t t396;
  int64_t t397;
  int64_t t398;
  uint8_t t399;
  int64_t t400;
  int64_t t401;
  uint8_t t402;
  uint8_t t403;
  int64_t t404;
  int64_t t405;
  uint8_t t406;
  int64_t t407;
  int64_t t408;
  int64_t t409;
  int64_t t410;
  int64_t t411;
  uint64_t t412;
  int64_t t413;
  int64_t t414;
  uint8_t t415;
  int64_t t416;
  int64_t t417;
  int64_t t418;
  int64_t t419;
  int64_t t420;
  uint64_t t421;
  uint64_t t422;
  uint64_t t423;
  uint8_t t424;
  int64_t t425;
  int64_t t426;
  int64_t t427;
  int64_t t428;
  int64_t t429;
  uint8_t t430;
  int64_t t431;
  int64_t t432;
  int64_t t433;
  int64_t t434;
  int64_t t435;
  uint8_t t436;
  uint8_t t437;
  int64_t t438;
  int64_t t439;
  uint8_t t440;
  int64_t t441;
  int64_t t442;
  int64_t t443;
  int64_t t444;
  uint8_t t445;
  int64_t t446;
  int64_t t447;
  int64_t t448;
  uint8_t t449;
  uint8_t t450;
  int64_t t451;
  int64_t t452;
  uint8_t t453;
  int64_t t454;
  int64_t t455;
  int64_t t456;
  int64_t t457;
  uint64_t t458;
  uint64_t t459;
  uint64_t t460;
  uint8_t t461;
  int64_t t462;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  uint64_t v5;
  uint64_t v6;
  uint64_t v7;
  uint64_t v8;
  int64_t v9;
  uint64_t v10;
  uint64_t v11;
  uint64_t v12;
  uint64_t v13;
  uint64_t v14;
  int64_t v15;
  uint64_t v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  int64_t v20;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg161_arg(t1, t2);
  t4 = ((uint64_t)(t3));
  v1 = fg161_gg_function_3(t4, base + 0);
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = fg161_arg(t5, t6);
  t8 = ((uint64_t)(t7));
  v2 = fg161_gg_function_3(t8, base + 0);
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = fg161_arg(t9, t10);
  t12 = ((uint64_t)(t11));
  v3 = fg161_gg_function_3(t12, base + 0);
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(3))));
  t15 = fg161_arg(t13, t14);
  t16 = ((uint64_t)(t15));
  v4 = fg161_gg_function_3(t16, base + 0);
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(4))));
  t19 = fg161_arg(t17, t18);
  v5 = ((uint64_t)(t19));
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(5))));
  t22 = fg161_arg(t20, t21);
  v6 = ((uint64_t)(t22));
  t23 = ((int64_t)(v1));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((t23) == (t24));
  if (t25) {
  {
  t26 = ((int64_t)(v0));
  t27 = ((int64_t)(((int64_t)UINT64_C(1))));
  t28 = fg161_arg(t26, t27);
  t29 = ((uint64_t)(t28));
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(((int64_t)UINT64_C(2))));
  t32 = fg161_arg(t30, t31);
  t33 = ((uint64_t)(t32));
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(((int64_t)UINT64_C(3))));
  t36 = fg161_arg(t34, t35);
  t37 = ((uint64_t)(t36));
  t0 = fg161_gg_function_4(t29, t33, t37, base + 0);
  goto fg161_gg_done;
  }
  }
  t38 = ((int64_t)(v1));
  t39 = ((int64_t)(((int64_t)UINT64_C(1))));
  t40 = ((t38) == (t39));
  if (t40) {
  {
  t41 = ((int64_t)(v2));
  t42 = fg161_gg_function_10(t41, base + 0);
  t43 = t42;
  if (t43) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  goto fg161_gg_done;
  }
  }
  t44 = ((int64_t)(v1));
  t45 = ((int64_t)(((int64_t)UINT64_C(2))));
  t46 = ((t44) == (t45));
  if (t46) {
  {
  t47 = ((int64_t)(v3));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = fg161_buffer(t47, t48, t49);
  v7 = ((uint64_t)(t50));
  t51 = ((int64_t)(v3));
  t52 = ((int64_t)(((int64_t)UINT64_C(1))));
  t53 = ((int64_t)(((int64_t)UINT64_C(0))));
  t54 = fg161_buffer(t51, t52, t53);
  v8 = ((uint64_t)(t54));
  t55 = ((uint64_t)(v8));
  fg161_gg_function_5(t55, base + 0);
  t56 = ((int64_t)(v2));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
  t60 = ((uint64_t)(((uint64_t)(v3))));
  fg161_put(t56, t57, t58, t59, t60);
  t61 = ((int64_t)(v2));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t63 = ((int64_t)(((int64_t)UINT64_C(1))));
  t64 = ((int64_t)(((int64_t)UINT64_C(0))));
  t65 = ((uint64_t)(v7));
  fg161_put(t61, t62, t63, t64, t65);
  t66 = ((int64_t)(v2));
  t67 = ((int64_t)(((int64_t)UINT64_C(0))));
  t68 = ((int64_t)(((int64_t)UINT64_C(2))));
  t69 = ((int64_t)(((int64_t)UINT64_C(0))));
  t70 = ((uint64_t)(v8));
  fg161_put(t66, t67, t68, t69, t70);
  t71 = ((int64_t)(v2));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  t73 = ((int64_t)(((int64_t)UINT64_C(3))));
  t74 = ((int64_t)(((int64_t)UINT64_C(0))));
  t75 = ((uint64_t)(v7));
  t76 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t77 = ((uint64_t)(v8));
  t78 = fg161_gg_function_4(t75, t76, t77, base + 0);
  fg161_put(t71, t72, t73, t74, t78);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg161_gg_done;
  }
  }
  t79 = ((int64_t)(v1));
  t80 = ((int64_t)(((int64_t)UINT64_C(3))));
  t81 = ((t79) == (t80));
  if (t81) {
  {
  t82 = ((int64_t)(v2));
  t83 = ((int64_t)(((int64_t)UINT64_C(0))));
  t84 = ((int64_t)(((int64_t)UINT64_C(0))));
  t85 = ((int64_t)(((int64_t)UINT64_C(0))));
  t86 = fg161_get(t82, t83, t84, t85);
  t87 = ((uint64_t)(t86));
  v9 = fg161_gg_function_3(t87, base + 0);
  t88 = ((int64_t)(v2));
  t89 = ((int64_t)(((int64_t)UINT64_C(0))));
  t90 = ((int64_t)(((int64_t)UINT64_C(2))));
  t91 = ((int64_t)(((int64_t)UINT64_C(0))));
  t92 = fg161_get(t88, t89, t90, t91);
  v10 = ((uint64_t)(t92));
  t93 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t94 = ((int64_t)(v9));
  t95 = ((int64_t)(((int64_t)UINT64_C(3))));
  t96 = ((int64_t)(v3));
  t97 = fg161_buffer(t94, t95, t96);
  t98 = ((uint64_t)(t97));
  t99 = ((uint64_t)(v10));
  v11 = fg161_gg_function_4(t93, t98, t99, base + 0);
  t100 = ((int64_t)(v2));
  t101 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = ((int64_t)(((int64_t)UINT64_C(3))));
  t103 = ((int64_t)(((int64_t)UINT64_C(0))));
  t104 = fg161_get(t100, t101, t102, t103);
  v12 = ((uint64_t)(t104));
  t105 = ((int64_t)(v9));
  t106 = ((int64_t)(((int64_t)UINT64_C(2))));
  t107 = ((int64_t)(((int64_t)UINT64_C(0))));
  t108 = fg161_buffer(t105, t106, t107);
  v13 = ((uint64_t)(t108));
  t109 = ((uint64_t)(v12));
  t110 = ((uint64_t)(v13));
  t111 = ((t109) <= (t110));
  t112 = t111;
  if (t112) {
  t113 = ((uint64_t)(v11));
  t114 = ((uint64_t)(v13));
  t115 = ((uint64_t)(v12));
  t116 = ((uint64_t)(((uint64_t)((uint64_t)(t114) - (uint64_t)(t115)))));
  t112 = ((t113) <= (t116));
  }
  t117 = t112;
  t118 = ((int64_t)(((int64_t)UINT64_C(35))));
  fg161_gg_function_6(t117, t118, base + 0);
  t119 = ((int64_t)(v9));
  t120 = ((int64_t)(((int64_t)UINT64_C(0))));
  t121 = ((int64_t)(((int64_t)UINT64_C(0))));
  t122 = fg161_buffer(t119, t120, t121);
  t123 = ((uint64_t)(t122));
  t124 = ((uint64_t)(v12));
  v14 = fg161_gg_function_8(t123, t124, base + 0);
  t125 = ((int64_t)(v2));
  t126 = ((int64_t)(((int64_t)UINT64_C(0))));
  t127 = ((int64_t)(((int64_t)UINT64_C(3))));
  t128 = ((int64_t)(((int64_t)UINT64_C(0))));
  t129 = ((uint64_t)(v12));
  t130 = ((uint64_t)(v11));
  t131 = ((uint64_t)(((uint64_t)((uint64_t)(t129) + (uint64_t)(t130)))));
  fg161_put(t125, t126, t127, t128, t131);
  t132 = ((uint64_t)(v14));
  t133 = ((uint64_t)(v10));
  t134 = ((uint64_t)(((uint64_t)fg161_gg_umod(t132, t133))));
  t135 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t136 = ((t134) == (t135));
  t137 = ((int64_t)(((int64_t)UINT64_C(36))));
  fg161_gg_function_6(t136, t137, base + 0);
  t138 = ((int64_t)(v9));
  t139 = ((int64_t)(v3));
  t140 = ((uint64_t)(v14));
  t141 = fg161_tensor_alloc(t138, t139, t140);
  t0 = ((uint64_t)(((uint64_t)(t141))));
  goto fg161_gg_done;
  }
  }
  t142 = ((int64_t)(v1));
  t143 = ((int64_t)(((int64_t)UINT64_C(4))));
  t144 = ((t142) == (t143));
  if (t144) {
  {
  t145 = ((int64_t)(v2));
  t146 = ((int64_t)(v0));
  t147 = ((int64_t)(((int64_t)UINT64_C(2))));
  t148 = fg161_arg(t146, t147);
  t149 = ((uint64_t)(t148));
  t150 = ((int64_t)(v0));
  t151 = ((int64_t)(((int64_t)UINT64_C(3))));
  t152 = fg161_arg(t150, t151);
  t153 = ((uint64_t)(t152));
  fg161_gg_function_15(t145, t149, t153, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg161_gg_done;
  }
  }
  t154 = ((int64_t)(v1));
  t155 = ((int64_t)(((int64_t)UINT64_C(5))));
  t156 = ((t154) == (t155));
  if (t156) {
  {
  t157 = ((int64_t)(v2));
  t158 = ((int64_t)(v3));
  fg161_gg_function_19(t157, t158, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg161_gg_done;
  }
  }
  t159 = ((int64_t)(v1));
  t160 = ((int64_t)(((int64_t)UINT64_C(6))));
  t161 = ((t159) == (t160));
  if (t161) {
  {
  t162 = ((int64_t)(v2));
  t163 = ((int64_t)(v0));
  t164 = ((int64_t)(((int64_t)UINT64_C(2))));
  t165 = fg161_arg(t163, t164);
  t166 = ((uint64_t)(t165));
  t167 = fg161_gg_function_20(t162, t166, base + 0);
  t0 = ((uint64_t)(((uint64_t)(t167))));
  goto fg161_gg_done;
  }
  }
  t168 = ((int64_t)(v1));
  t169 = ((int64_t)(((int64_t)UINT64_C(7))));
  t170 = ((t168) == (t169));
  if (t170) {
  {
  t171 = ((int64_t)(v2));
  t172 = ((int64_t)(v0));
  t173 = ((int64_t)(((int64_t)UINT64_C(2))));
  t174 = fg161_arg(t172, t173);
  t175 = ((uint64_t)(t174));
  t176 = ((int64_t)(v4));
  fg161_gg_function_24(t171, t175, t176, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg161_gg_done;
  }
  }
  t177 = ((int64_t)(v1));
  t178 = ((int64_t)(((int64_t)UINT64_C(8))));
  t179 = ((t177) == (t178));
  if (t179) {
  {
  t180 = ((int64_t)(v2));
  t181 = ((int64_t)(v3));
  t182 = ((int64_t)(v0));
  t183 = ((int64_t)(((int64_t)UINT64_C(3))));
  t184 = fg161_arg(t182, t183);
  t185 = ((uint64_t)(t184));
  t186 = ((uint64_t)(v5));
  fg161_gg_function_26(t180, t181, t185, t186, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg161_gg_done;
  }
  }
  t187 = ((int64_t)(v1));
  t188 = ((int64_t)(((int64_t)UINT64_C(9))));
  t189 = ((t187) == (t188));
  if (t189) {
  {
  t190 = ((int64_t)(v2));
  fg161_gg_function_27(t190, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg161_gg_done;
  }
  }
  t191 = ((int64_t)(v1));
  t192 = ((int64_t)(((int64_t)UINT64_C(10))));
  t193 = ((t191) == (t192));
  if (t193) {
  {
  t194 = ((int64_t)(v0));
  t195 = ((int64_t)(((int64_t)UINT64_C(1))));
  t196 = fg161_arg(t194, t195);
  t197 = ((uint64_t)(t196));
  t198 = ((int64_t)(v0));
  t199 = ((int64_t)(((int64_t)UINT64_C(2))));
  t200 = fg161_arg(t198, t199);
  t201 = ((uint64_t)(t200));
  t202 = fg161_gg_function_29(t197, t201, base + 0);
  t0 = ((uint64_t)(((uint64_t)(t202))));
  goto fg161_gg_done;
  }
  }
  t203 = ((int64_t)(v1));
  t204 = ((int64_t)(((int64_t)UINT64_C(11))));
  t205 = ((t203) == (t204));
  if (t205) {
  {
  t206 = ((int64_t)(v2));
  fg161_gg_function_31(t206, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg161_gg_done;
  }
  }
  t207 = ((int64_t)(v1));
  t208 = ((int64_t)(((int64_t)UINT64_C(12))));
  t209 = ((t207) == (t208));
  if (t209) {
  {
  t210 = ((int64_t)(v2));
  t211 = ((int64_t)(v3));
  t0 = fg161_gg_function_32(t210, t211, base + 0);
  goto fg161_gg_done;
  }
  }
  t212 = ((int64_t)(v1));
  t213 = ((int64_t)(((int64_t)UINT64_C(13))));
  t214 = ((t212) == (t213));
  if (t214) {
  {
  t215 = ((int64_t)(v2));
  fg161_gg_function_33(t215, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg161_gg_done;
  }
  }
  t216 = ((int64_t)(v1));
  t217 = ((int64_t)(((int64_t)UINT64_C(14))));
  t218 = ((t216) == (t217));
  if (t218) {
  {
  t219 = ((int64_t)(v2));
  t220 = ((int64_t)(v3));
  t0 = fg161_gg_function_36(t219, t220, base + 0);
  goto fg161_gg_done;
  }
  }
  t221 = ((int64_t)(v1));
  t222 = ((int64_t)(((int64_t)UINT64_C(15))));
  t223 = ((t221) == (t222));
  if (t223) {
  {
  t224 = ((int64_t)(v2));
  t0 = fg161_gg_function_37(t224, base + 0);
  goto fg161_gg_done;
  }
  }
  t225 = ((int64_t)(v1));
  t226 = ((int64_t)(((int64_t)UINT64_C(16))));
  t227 = ((t225) == (t226));
  if (t227) {
  {
  t228 = ((int64_t)(v2));
  t229 = ((int64_t)(v0));
  t230 = ((int64_t)(((int64_t)UINT64_C(2))));
  t231 = fg161_arg(t229, t230);
  t232 = ((uint64_t)(t231));
  t233 = ((int64_t)(v0));
  t234 = ((int64_t)(((int64_t)UINT64_C(3))));
  t235 = fg161_arg(t233, t234);
  t236 = ((uint64_t)(t235));
  t0 = fg161_gg_function_38(t228, t232, t236, base + 0);
  goto fg161_gg_done;
  }
  }
  t237 = ((int64_t)(v1));
  t238 = ((int64_t)(((int64_t)UINT64_C(17))));
  t239 = ((t237) == (t238));
  if (t239) {
  {
  t240 = ((int64_t)(v2));
  t241 = ((int64_t)(v3));
  t242 = ((int64_t)(v4));
  t243 = ((uint64_t)(v5));
  t244 = fg161_gg_function_40(t240, t241, t242, t243, base + 0);
  t0 = ((uint64_t)(((uint64_t)(t244))));
  goto fg161_gg_done;
  }
  }
  t245 = ((int64_t)(v1));
  t246 = ((int64_t)(((int64_t)UINT64_C(18))));
  t247 = ((t245) == (t246));
  if (t247) {
  {
  t248 = ((int64_t)(v2));
  t249 = ((int64_t)(v3));
  t250 = ((int64_t)(v4));
  t251 = ((uint64_t)(v5));
  fg161_gg_function_43(t248, t249, t250, t251, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg161_gg_done;
  }
  }
  t252 = ((int64_t)(v1));
  t253 = ((int64_t)(((int64_t)UINT64_C(19))));
  t254 = ((t252) == (t253));
  if (t254) {
  {
  t255 = ((int64_t)(v2));
  fg161_gg_function_44(t255, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg161_gg_done;
  }
  }
  t256 = ((int64_t)(v1));
  t257 = ((int64_t)(((int64_t)UINT64_C(20))));
  t258 = ((t256) == (t257));
  if (t258) {
  {
  t259 = ((int64_t)(v2));
  t260 = ((int64_t)(v3));
  t261 = fg161_gg_function_46(t259, t260, base + 0);
  t0 = ((uint64_t)(((uint64_t)(t261))));
  goto fg161_gg_done;
  }
  }
  t262 = ((int64_t)(v1));
  t263 = ((int64_t)(((int64_t)UINT64_C(21))));
  t264 = ((t262) == (t263));
  if (t264) {
  {
  t265 = ((int64_t)(v2));
  t266 = ((int64_t)(v3));
  fg161_gg_function_52(t265, t266, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg161_gg_done;
  }
  }
  t267 = ((int64_t)(v1));
  t268 = ((int64_t)(((int64_t)UINT64_C(22))));
  t269 = ((t267) == (t268));
  if (t269) {
  {
  t270 = ((int64_t)(v2));
  fg161_gg_function_49(t270, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg161_gg_done;
  }
  }
  t271 = ((int64_t)(v1));
  t272 = ((int64_t)(((int64_t)UINT64_C(23))));
  t273 = ((t271) == (t272));
  if (t273) {
  {
  t274 = ((int64_t)(v2));
  t275 = ((int64_t)(v3));
  t276 = fg161_gg_function_53(t274, t275, base + 0);
  t0 = ((uint64_t)(((uint64_t)(t276))));
  goto fg161_gg_done;
  }
  }
  t277 = ((int64_t)(v1));
  t278 = ((int64_t)(((int64_t)UINT64_C(24))));
  t279 = ((t277) == (t278));
  if (t279) {
  {
  t280 = ((int64_t)(v2));
  t281 = ((int64_t)(v3));
  t282 = fg161_gg_function_56(t280, t281, base + 0);
  t283 = t282;
  if (t283) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  goto fg161_gg_done;
  }
  }
  t284 = ((int64_t)(v1));
  t285 = ((int64_t)(((int64_t)UINT64_C(25))));
  t286 = ((t284) == (t285));
  if (t286) {
  {
  t287 = ((int64_t)(v2));
  t288 = ((int64_t)(v3));
  t289 = fg161_gg_function_58(t287, t288, base + 0);
  t290 = t289;
  if (t290) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  goto fg161_gg_done;
  }
  }
  t291 = ((int64_t)(v1));
  t292 = ((int64_t)(((int64_t)UINT64_C(26))));
  t293 = ((t291) == (t292));
  if (t293) {
  {
  t294 = ((int64_t)(v2));
  t295 = ((int64_t)(v3));
  t296 = ((int64_t)(v4));
  fg161_gg_function_60(t294, t295, t296, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg161_gg_done;
  }
  }
  t297 = ((int64_t)(v1));
  t298 = ((int64_t)(((int64_t)UINT64_C(27))));
  t299 = ((t297) == (t298));
  if (t299) {
  {
  t300 = ((int64_t)(v2));
  t301 = ((int64_t)(v3));
  t302 = ((int64_t)(v4));
  fg161_gg_function_62(t300, t301, t302, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg161_gg_done;
  }
  }
  t303 = ((int64_t)(v1));
  t304 = ((int64_t)(((int64_t)UINT64_C(28))));
  t305 = ((t303) == (t304));
  if (t305) {
  {
  t306 = ((int64_t)(v2));
  t307 = ((int64_t)(v3));
  fg161_gg_function_66(t306, t307, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg161_gg_done;
  }
  }
  t308 = ((int64_t)(v1));
  t309 = ((int64_t)(((int64_t)UINT64_C(29))));
  t310 = ((t308) == (t309));
  if (t310) {
  {
  t311 = ((int64_t)(v2));
  t312 = ((int64_t)(v3));
  t313 = fg161_gg_function_67(t311, t312, base + 0);
  t0 = ((uint64_t)(((uint64_t)(t313))));
  goto fg161_gg_done;
  }
  }
  t314 = ((int64_t)(v1));
  t315 = ((int64_t)(((int64_t)UINT64_C(30))));
  t316 = ((t314) == (t315));
  if (t316) {
  {
  t317 = ((int64_t)(v2));
  t318 = ((int64_t)(v3));
  t319 = ((int64_t)(v4));
  t320 = ((uint64_t)(v5));
  t321 = fg161_gg_function_3(t320, base + 0);
  fg161_gg_function_69(t317, t318, t319, t321, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg161_gg_done;
  }
  }
  t322 = ((int64_t)(v1));
  t323 = ((int64_t)(((int64_t)UINT64_C(31))));
  t324 = ((t322) == (t323));
  if (t324) {
  {
  t325 = ((int64_t)(v2));
  t326 = ((int64_t)(v3));
  t327 = ((int64_t)(v4));
  t328 = ((uint64_t)(v5));
  t329 = fg161_gg_function_3(t328, base + 0);
  t330 = ((uint64_t)(v6));
  t331 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t332 = ((t330) != (t331));
  t333 = fg161_gg_function_73(t325, t326, t327, t329, t332, base + 0);
  t334 = t333;
  if (t334) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  goto fg161_gg_done;
  }
  }
  t335 = ((int64_t)(v1));
  t336 = ((int64_t)(((int64_t)UINT64_C(32))));
  t337 = ((t335) == (t336));
  if (t337) {
  {
  t338 = ((int64_t)(v2));
  t339 = ((int64_t)(v3));
  t340 = ((int64_t)(v4));
  t341 = ((uint64_t)(v5));
  t342 = fg161_gg_function_3(t341, base + 0);
  t343 = ((uint8_t)UINT64_C(1));
  t344 = fg161_gg_function_73(t338, t339, t340, t342, t343, base + 0);
  t345 = ((int64_t)(((int64_t)UINT64_C(37))));
  fg161_gg_function_6(t344, t345, base + 0);
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t346 = ((int64_t)(v15));
  t347 = ((int64_t)(v2));
  t348 = ((int64_t)(((int64_t)UINT64_C(0))));
  t349 = ((int64_t)(((int64_t)UINT64_C(0))));
  t350 = fg161_gg_function_50(t347, t348, t349, base + 0);
  t351 = fg161_gg_function_3(t350, base + 0);
  t352 = ((int64_t)(t351));
  if (!(((t346) < (t352)))) goto label2;
  {
  v16 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t353 = ((int64_t)(v2));
  t354 = ((int64_t)(((int64_t)UINT64_C(3))));
  t355 = ((int64_t)(v15));
  t356 = fg161_gg_function_50(t353, t354, t355, base + 0);
  v17 = fg161_gg_function_3(t356, base + 0);
  v18 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t357 = ((int64_t)(v18));
  t358 = ((int64_t)(v17));
  t359 = ((int64_t)(((int64_t)UINT64_C(2))));
  t360 = ((int64_t)(((int64_t)UINT64_C(0))));
  t361 = fg161_gg_function_21(t358, t359, t360, base + 0);
  t362 = fg161_gg_function_3(t361, base + 0);
  t363 = ((int64_t)(t362));
  if (!(((t357) < (t363)))) goto label5;
  {
  t364 = ((uint64_t)(v16));
  t365 = ((int64_t)(v17));
  t366 = ((int64_t)(v18));
  t367 = fg161_gg_function_32(t365, t366, base + 0);
  v16 = fg161_gg_function_8(t364, t367, base + 0);
  t368 = v18;
  t369 = ((int64_t)(((int64_t)UINT64_C(1))));
  v18 = ((int64_t)((uint64_t)(t368) + (uint64_t)(t369)));
  }
  goto label4;
label5:;
  t370 = ((uint64_t)(v6));
  t371 = fg161_gg_function_3(t370, base + 0);
  t372 = ((int64_t)(v15));
  t373 = ((uint64_t)(v16));
  fg161_ptr_put(t371, t372, t373);
  t374 = v15;
  t375 = ((int64_t)(((int64_t)UINT64_C(1))));
  v15 = ((int64_t)((uint64_t)(t374) + (uint64_t)(t375)));
  }
  goto label1;
label2:;
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg161_gg_done;
  }
  }
  t376 = ((int64_t)(v1));
  t377 = ((int64_t)(((int64_t)UINT64_C(33))));
  t378 = ((t376) == (t377));
  if (t378) {
  {
  t379 = ((int64_t)(v2));
  t380 = ((int64_t)(v3));
  t381 = ((int64_t)(v4));
  fg161_gg_function_77(t379, t380, t381, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg161_gg_done;
  }
  }
  t382 = ((int64_t)(v1));
  t383 = ((int64_t)(((int64_t)UINT64_C(34))));
  t384 = ((t382) == (t383));
  if (t384) {
  {
  t385 = ((int64_t)(v2));
  t386 = ((int64_t)(v3));
  t387 = ((int64_t)(v4));
  t388 = fg161_gg_function_79(t385, t386, t387, base + 0);
  t389 = t388;
  if (t389) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  goto fg161_gg_done;
  }
  }
  t390 = ((int64_t)(v1));
  t391 = ((int64_t)(((int64_t)UINT64_C(35))));
  t392 = ((t390) == (t391));
  if (t392) {
  {
  t393 = ((int64_t)(v2));
  t394 = ((int64_t)(v3));
  t395 = fg161_gg_function_80(t393, t394, base + 0);
  t396 = t395;
  if (t396) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  goto fg161_gg_done;
  }
  }
  t397 = ((int64_t)(v1));
  t398 = ((int64_t)(((int64_t)UINT64_C(36))));
  t399 = ((t397) == (t398));
  if (t399) {
  {
  t400 = ((int64_t)(v2));
  t401 = ((int64_t)(v3));
  t402 = fg161_gg_function_81(t400, t401, base + 0);
  t403 = t402;
  if (t403) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  goto fg161_gg_done;
  }
  }
  t404 = ((int64_t)(v1));
  t405 = ((int64_t)(((int64_t)UINT64_C(37))));
  t406 = ((t404) == (t405));
  if (t406) {
  {
  t407 = ((int64_t)(v2));
  t408 = ((int64_t)(v3));
  fg161_gg_function_61(t407, t408, base + 0);
  t409 = ((int64_t)(v2));
  t410 = ((int64_t)(((int64_t)UINT64_C(2))));
  t411 = ((int64_t)(v3));
  t412 = fg161_gg_function_50(t409, t410, t411, base + 0);
  v19 = fg161_gg_function_3(t412, base + 0);
  t413 = ((int64_t)(v19));
  t414 = ((int64_t)(((int64_t)UINT64_C(0))));
  t415 = ((t413) == (t414));
  if (t415) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg161_gg_done;
  }
  }
  v20 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t416 = ((int64_t)(v20));
  t417 = ((int64_t)(v3));
  if (!(((t416) < (t417)))) goto label8;
  {
  t418 = ((int64_t)(v2));
  t419 = ((int64_t)(((int64_t)UINT64_C(2))));
  t420 = ((int64_t)(v20));
  t421 = fg161_gg_function_50(t418, t419, t420, base + 0);
  t422 = ((uint64_t)(t421));
  t423 = ((uint64_t)(((uint64_t)(v19))));
  t424 = ((t422) == (t423));
  if (t424) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg161_gg_done;
  }
  }
  t425 = v20;
  t426 = ((int64_t)(((int64_t)UINT64_C(1))));
  v20 = ((int64_t)((uint64_t)(t425) + (uint64_t)(t426)));
  }
  goto label7;
label8:;
  t427 = ((int64_t)(v19));
  t0 = fg161_gg_function_37(t427, base + 0);
  goto fg161_gg_done;
  }
  }
  t428 = ((int64_t)(v1));
  t429 = ((int64_t)(((int64_t)UINT64_C(38))));
  t430 = ((t428) == (t429));
  if (t430) {
  {
  t431 = ((int64_t)(v2));
  t432 = ((int64_t)(v3));
  t433 = fg161_gg_function_82(t431, t432, base + 0);
  t0 = ((uint64_t)(((uint64_t)(t433))));
  goto fg161_gg_done;
  }
  }
  t434 = ((int64_t)(v1));
  t435 = ((int64_t)(((int64_t)UINT64_C(39))));
  t436 = ((t434) == (t435));
  t437 = t436;
  if (!t437) {
  t438 = ((int64_t)(v1));
  t439 = ((int64_t)(((int64_t)UINT64_C(40))));
  t437 = ((t438) == (t439));
  }
  t440 = t437;
  if (t440) {
  {
  t441 = ((int64_t)(v2));
  t442 = ((int64_t)(v3));
  t443 = ((int64_t)(v1));
  t444 = ((int64_t)(((int64_t)UINT64_C(40))));
  t445 = ((t443) == (t444));
  t446 = ((int64_t)(v4));
  t0 = fg161_gg_function_86(t441, t442, t445, t446, base + 0);
  goto fg161_gg_done;
  }
  }
  t447 = ((int64_t)(v1));
  t448 = ((int64_t)(((int64_t)UINT64_C(41))));
  t449 = ((t447) == (t448));
  if (t449) {
  {
  t451 = ((int64_t)(v2));
  t452 = ((int64_t)(v4));
  t453 = ((t451) != (t452));
  if (t453) {
  {
  t454 = ((int64_t)(v2));
  t455 = ((int64_t)(v4));
  t450 = ((t454) < (t455));
  }
  } else {
  {
  t456 = ((int64_t)(v0));
  t457 = ((int64_t)(((int64_t)UINT64_C(2))));
  t458 = fg161_arg(t456, t457);
  t459 = ((uint64_t)(t458));
  t460 = ((uint64_t)(v5));
  t450 = ((t459) < (t460));
  }
  }
  t461 = t450;
  if (t461) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  goto fg161_gg_done;
  }
  }
  t462 = ((int64_t)(((int64_t)UINT64_C(99))));
  fg161_fail(t462);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg161_gg_done;
  }
fg161_gg_done:;
  return t0;
}
static void fg161_gg_function_0(int64_t base) {
  fg161_gg_header t0;
  int64_t t1;
  uint64_t t2;
  int64_t t3;
  {
  t0.base = fg161_gg_get_stack(base + 0)->base + fg161_gg_get_stack(base + 0)->top; t0.len = 0;
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = fg161_gg_function_1(t1, base + 1);
  fg161_gg_unsigned(fg161_gg_ref(&t0, fg161_gg_get_stack(base + 0)), t2);
  fg161_gg_append(fg161_gg_ref(&t0, fg161_gg_get_stack(base + 0)), "\n", 1, 1);
  fg161_gg_write(stdout, fg161_gg_view((t0).base, (t0).len));
  }
fg161_gg_done:;
  for (t3 = 0; t3 < 1; t3++) fg161_gg_stacks[base + t3].top = 0;
}
static int fg161_gg_program(int argc, char **argv) {
  struct { int64_t base; } frame;
  frame.base = 0;
  fg161_gg_argc = argc; fg161_gg_argv = argv;
  fg161_gg_function_0(frame.base + 0);
  return 0;
}
int main(int argc, char **argv) {
#ifdef _MSC_VER
  __try { return fg161_gg_program(argc, argv); }
  __except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    _resetstkoflw(); fg161_gg_fail("native call stack overflow"); return 1;
  }
#else
  return fg161_gg_program(argc, argv);
#endif
}
