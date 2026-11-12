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
typedef struct { void *data; int64_t len; } fg160_gg_slice;
typedef struct { unsigned char *base; int64_t len; } fg160_gg_header;
typedef struct { unsigned char *base; size_t top, committed; } fg160_gg_stack;
typedef struct { fg160_gg_header *header; fg160_gg_stack *stack; } fg160_gg_reference;
#define GG_RESERVATION ((size_t)268435456)
#define GG_STACK_COUNT 8192
static fg160_gg_stack fg160_gg_stacks[GG_STACK_COUNT];
static int fg160_gg_argc;
static char **fg160_gg_argv;
static void fg160_gg_fail(const char *message) {
    fprintf(stderr, "goose runtime error: %s\n", message); exit(1);
}
static fg160_gg_stack *fg160_gg_get_stack(int64_t id) {
    fg160_gg_stack *s;
    if (id < 0 || id >= GG_STACK_COUNT) fg160_gg_fail("data stack limit exceeded");
    s = &fg160_gg_stacks[id];
    if (!s->base) {
#ifdef _WIN32
        s->base = VirtualAlloc(NULL, GG_RESERVATION + 65536, MEM_RESERVE, PAGE_NOACCESS);
#else
        s->base = mmap(NULL, GG_RESERVATION + 65536, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (s->base == MAP_FAILED) s->base = NULL;
#endif
        if (!s->base) fg160_gg_fail("cannot reserve data stack");
    }
    return s;
}
static void *fg160_gg_allocate(fg160_gg_stack *s, size_t size) {
    size_t end, committed;
    void *result;
    if (size > GG_RESERVATION - s->top) fg160_gg_fail("data stack reservation exceeded");
    result = s->base + s->top; end = s->top + size;
    if (end > s->committed) {
        committed = (end + 65535) & ~(size_t)65535;
#ifdef _WIN32
        if (!VirtualAlloc(s->base + s->committed, committed - s->committed, MEM_COMMIT, PAGE_READWRITE)) fg160_gg_fail("cannot commit data stack");
#else
        if (mprotect(s->base + s->committed, committed - s->committed, PROT_READ | PROT_WRITE)) fg160_gg_fail("cannot commit data stack");
#endif
        s->committed = committed;
    }
    s->top = end; return result;
}
static fg160_gg_reference fg160_gg_ref(fg160_gg_header *h, fg160_gg_stack *s) { fg160_gg_reference r; r.header=h; r.stack=s; return r; }
static fg160_gg_slice fg160_gg_view(void *p, int64_t n) { fg160_gg_slice v; v.data=p; v.len=n; return v; }
static fg160_gg_slice fg160_gg_byte_view(fg160_gg_slice a, size_t width) {
    if (a.len < 0 || (width && (uint64_t)a.len > (uint64_t)INT64_MAX / width)) fg160_gg_fail("byte view size overflow");
    return fg160_gg_view(a.data, (int64_t)((uint64_t)a.len * width));
}
static fg160_gg_slice fg160_gg_sub(fg160_gg_slice a, int64_t lo, int64_t hi, size_t element) {
    if (lo < 0 || hi < lo || hi > a.len) fg160_gg_fail("slice out of bounds");
    return fg160_gg_view((unsigned char *)a.data + (size_t)lo * element, hi - lo);
}
static int64_t fg160_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg160_gg_fail("array index out of bounds"); return i; }
static void fg160_gg_append(fg160_gg_reference a, const void *p, int64_t n, size_t element) {
    void *to;
    if (n < 0 || (uint64_t)n > GG_RESERVATION / (element ? element : 1)) fg160_gg_fail("invalid array size");
    to=fg160_gg_allocate(a.stack, (size_t)n * element);
    if (n && element) memmove(to, p, (size_t)n * element);
    a.header->len += n;
}
static void fg160_gg_resize(fg160_gg_reference a, int64_t n, size_t element, const void *fill) {
    int64_t old=a.header->len, i;
    if (n < 0) fg160_gg_fail("negative array length");
    if (n > old) {
        unsigned char *p;
        if (!fill) fg160_gg_fail("resize without fill may only shrink");
        if ((uint64_t)(n-old) > GG_RESERVATION / (element ? element : 1)) fg160_gg_fail("invalid array size");
        p=fg160_gg_allocate(a.stack, (size_t)(n-old)*element);
        for (i=0; i<n-old; i++) if(element) memcpy(p+(size_t)i*element, fill, element);
    } else { a.stack->top = (size_t)(a.header->base-a.stack->base) + (size_t)n*element; }
    a.header->len=n;
}
static int64_t fg160_gg_div(int64_t a, int64_t b) { if (!b) fg160_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg160_gg_fail("division overflow"); return a/b; }
static int64_t fg160_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg160_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static uint64_t fg160_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg160_gg_fail("division by zero");return a/b; }
static uint64_t fg160_gg_umod(uint64_t a,uint64_t b) { if(!b)fg160_gg_fail("division by zero");return a%b; }
static int fg160_gg_equal(fg160_gg_slice a, fg160_gg_slice b, size_t element) { return a.len==b.len && (!a.len || !memcmp(a.data,b.data,(size_t)a.len*element)); }
static void fg160_gg_text(fg160_gg_reference out, fg160_gg_slice v) { fg160_gg_append(out,v.data,v.len,1); }
static void fg160_gg_integer(fg160_gg_reference out,int64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%lld",(long long)n);fg160_gg_append(out,b,k,1); }
static void fg160_gg_unsigned(fg160_gg_reference out,uint64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%llu",(unsigned long long)n);fg160_gg_append(out,b,k,1); }
static double fg160_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static int64_t fg160_ggf_fmt_f64(uint8_t *out,double n);
static int64_t fg160_ggf_fmt_f32(uint8_t *out,float n);
static void fg160_gg_float(fg160_gg_reference out,double n) { uint8_t b[64];int64_t k=fg160_ggf_fmt_f64(b,n);fg160_gg_append(out,b,k,1); }
static void fg160_gg_float32(fg160_gg_reference out,float n) { uint8_t b[64];int64_t k=fg160_ggf_fmt_f32(b,n);fg160_gg_append(out,b,k,1); }
static void fg160_gg_bool(fg160_gg_reference out,uint8_t n) { fg160_gg_append(out,n?"true":"false",n?4:5,1); }
static void fg160_gg_write(FILE *file,fg160_gg_slice text) { if(text.len && fwrite(text.data,1,(size_t)text.len,file)!=(size_t)text.len)fg160_gg_fail("write failed"); }
static char *fg160_gg_cstring(fg160_gg_slice path) {
    char *s=malloc((size_t)path.len+1); if(!s)fg160_gg_fail("out of memory in file shim");
    if(path.len)memcpy(s,path.data,(size_t)path.len);s[path.len]=0;return s;
}
static uint8_t fg160_gs_os_read_file(fg160_gg_slice path,fg160_gg_reference out) {
    char *p=fg160_gg_cstring(path);FILE *f=fopen(p,"rb");unsigned char b[16384];size_t n;int ok;free(p);
    if(!f)return 0;while((n=fread(b,1,sizeof b,f))!=0)fg160_gg_append(out,b,(int64_t)n,1);ok=!ferror(f);fclose(f);return (uint8_t)ok;
}
static uint8_t fg160_gs_os_write_file(fg160_gg_slice path,fg160_gg_slice data) {
    char *p=fg160_gg_cstring(path);FILE *f=fopen(p,"wb");int ok;free(p);if(!f)return 0;
    ok=!data.len || fwrite(data.data,1,(size_t)data.len,f)==(size_t)data.len;if(fclose(f))ok=0;return (uint8_t)ok;
}
static uint8_t fg160_gs_os_file_exists(fg160_gg_slice path) { char*p=fg160_gg_cstring(path);FILE*f=fopen(p,"rb");free(p);if(!f)return 0;fclose(f);return 1; }
static void fg160_gs_os_write_stdout(fg160_gg_slice s) { fg160_gg_write(stdout,s); }
static void fg160_gs_os_write_stderr(fg160_gg_slice s) { fg160_gg_write(stderr,s); }
static void fg160_gs_os_flush_stdout(void) { fflush(stdout); }
static int64_t fg160_gs_os_arg_count(void) { return fg160_gg_argc; }
static void fg160_gs_os_arg(int64_t i,fg160_gg_reference out) { if(i>=0 && i<fg160_gg_argc)fg160_gg_append(out,fg160_gg_argv[i],(int64_t)strlen(fg160_gg_argv[i]),1); }
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
typedef struct { int n; uint32_t d[36]; } fg160_ggf_big;

/* a = v << sh, for v > 0. */
static void fg160_ggf_big_set(fg160_ggf_big *a, uint64_t v, int sh) {
    int w = sh >> 5, b = sh & 31, i;
    uint64_t lo = v << b, hi = b ? v >> (64 - b) : 0;
    for (i = 0; i < w; i++) a->d[i] = 0;
    a->d[w] = (uint32_t)lo;
    a->d[w + 1] = (uint32_t)(lo >> 32);
    a->d[w + 2] = (uint32_t)hi;
    a->n = w + 3;
    while (!a->d[a->n - 1]) a->n--;
}

static void fg160_ggf_big_mul(fg160_ggf_big *a, uint32_t m) {
    uint64_t c = 0;
    for (int i = 0; i < a->n; i++) {
        c += (uint64_t)a->d[i] * m;
        a->d[i] = (uint32_t)c;
        c >>= 32;
    }
    if (c) a->d[a->n++] = (uint32_t)c;
}

static void fg160_ggf_big_mul_pow10(fg160_ggf_big *a, int k) {
    for (; k >= 9; k -= 9) fg160_ggf_big_mul(a, 1000000000u);
    for (; k > 0; k--) fg160_ggf_big_mul(a, 10);
}

static int fg160_ggf_big_cmp(const fg160_ggf_big *a, const fg160_ggf_big *b) {
    if (a->n != b->n) return a->n < b->n ? -1 : 1;
    for (int i = a->n - 1; i >= 0; i--)
        if (a->d[i] != b->d[i]) return a->d[i] < b->d[i] ? -1 : 1;
    return 0;
}

/* t = a + b */
static void fg160_ggf_big_add(fg160_ggf_big *t, const fg160_ggf_big *a, const fg160_ggf_big *b) {
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
static void fg160_ggf_big_sub(fg160_ggf_big *a, const fg160_ggf_big *b) {
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
static int fg160_ggf_float_digits(uint64_t f, int e, int bits, int emin, char *dig, int *k) {
    fg160_ggf_big r, s, mp, mlo, t;
    /* Reading rounds a tie to the even significand, so the interval that
       reads back as f * 2^e includes its ends when f is even. */
    int even = !(f & 1);
    /* Below a power of two, the gap to the next float down is half the gap up. */
    int asym = f == (uint64_t)1 << (bits - 1) && e > emin;
    int up = e > 0 ? e : 0, lg = e - 1, n = 0, c;
    /* The value is r / s, and what reads back as it reaches from (r - *mm) / s
       to (r + mp) / s: half the gap to each neighbour. */
    fg160_ggf_big *mm = asym ? &mlo : &mp;
    fg160_ggf_big_set(&r, f, up + 1 + asym);
    fg160_ggf_big_set(&s, 1, up - e + 1 + asym);
    fg160_ggf_big_set(&mp, 1, up + asym);
    fg160_ggf_big_set(&mlo, 1, up);
    /* Scale to 10^*k, the least power of ten above the interval: with
       lg = floor(log2 value), ceil(lg log10 2) is it or one short, and
       78913 / 2^18 is log10 2 closely enough to compute that for |lg| < 1650. */
    for (uint64_t g = f; g; g >>= 1) lg++;
    *k = lg > 0 ? ((lg * 78913) >> 18) + 1 : -((-lg * 78913) >> 18);
    if (*k >= 0) fg160_ggf_big_mul_pow10(&s, *k);
    else {
        fg160_ggf_big_mul_pow10(&r, -*k);
        fg160_ggf_big_mul_pow10(&mp, -*k);
        if (asym) fg160_ggf_big_mul_pow10(&mlo, -*k);
    }
    fg160_ggf_big_add(&t, &r, &mp);
    c = fg160_ggf_big_cmp(&t, &s);
    if (even ? c >= 0 : c > 0) {
        fg160_ggf_big_mul(&s, 10);
        ++*k;
    }
    /* Each digit d leaves the remainder r: stop where the digits so far
       followed by d (r within *mm) or by d + 1 (r + mp past s) read back. */
    for (;;) {
        int d = 0, low, high;
        fg160_ggf_big_mul(&r, 10);
        fg160_ggf_big_mul(&mp, 10);
        if (asym) fg160_ggf_big_mul(&mlo, 10);
        while (fg160_ggf_big_cmp(&r, &s) >= 0) {
            fg160_ggf_big_sub(&r, &s);
            d++;
        }
        c = fg160_ggf_big_cmp(&r, mm);
        low = even ? c <= 0 : c < 0;
        fg160_ggf_big_add(&t, &r, &mp);
        c = fg160_ggf_big_cmp(&t, &s);
        high = even ? c >= 0 : c > 0;
        if (!low && !high) {
            dig[n++] = (char)('0' + d);
            continue;
        }
        if (low && high) {
            /* Both read back: the nearer, d + 1 when r is past half of s,
               and of two as near, the even one. */
            fg160_ggf_big_add(&t, &r, &r);
            c = fg160_ggf_big_cmp(&t, &s);
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
static int64_t fg160_ggf_fmt_float(uint8_t *dst, int neg, uint64_t frac, int bexp, int bits,
                            int emin) {
    char dig[20], *p = (char *)dst;
    int k, n, x, i;
    if (neg) *p++ = '-';
    if (!frac && !bexp) {
        memcpy(p, "0.0", 3);
        return p + 3 - (char *)dst;
    }
    n = fg160_ggf_float_digits(bexp ? frac | (uint64_t)1 << (bits - 1) : frac,
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

static int64_t fg160_ggf_fmt_f64(uint8_t *dst, double v) {
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
    return fg160_ggf_fmt_float(dst, (int)(b >> 63), b & (((uint64_t)1 << 52) - 1),
                        (int)(b >> 52) & 0x7FF, 53, -1074);
}

/* The digits are the f32's own, laid out as an f64's are, so 0.1 as an f32
   prints as 0.1, not as the digits of the f64 it widens to. */
static int64_t fg160_ggf_fmt_f32(uint8_t *dst, float v) {
    double d = v;
    uint32_t b;
    if (d != d || isinf(d)) return fg160_ggf_fmt_f64(dst, d);
    memcpy(&b, &v, sizeof b);
    return fg160_ggf_fmt_float(dst, (int)(b >> 31), b & 0x7FFFFF, (int)(b >> 23) & 0xFF, 24, -149);
}


static void fg160_gg_quote(fg160_gg_reference out,fg160_gg_slice text) { int64_t i; unsigned char c; fg160_gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){fg160_gg_append(out,"\\",1,1);fg160_gg_append(out,&c,1,1);} else if(c=='\n')fg160_gg_append(out,"\\n",2,1);else if(c=='\r')fg160_gg_append(out,"\\r",2,1);else if(c=='\t')fg160_gg_append(out,"\\t",2,1);else fg160_gg_append(out,&c,1,1);}fg160_gg_append(out,"\"",1,1);}
static uint8_t fg160_gg_eq_1(int64_t a, int64_t b);
static uint8_t fg160_gg_equal_elements_1(fg160_gg_slice a, fg160_gg_slice b);
static void fg160_gg_render_1(fg160_gg_reference out, int64_t value);
static uint8_t fg160_gg_eq_2(uint64_t a, uint64_t b);
static uint8_t fg160_gg_equal_elements_2(fg160_gg_slice a, fg160_gg_slice b);
static void fg160_gg_render_2(fg160_gg_reference out, uint64_t value);
static uint8_t fg160_gg_eq_4(uint8_t a, uint8_t b);
static uint8_t fg160_gg_equal_elements_4(fg160_gg_slice a, fg160_gg_slice b);
static void fg160_gg_render_4(fg160_gg_reference out, uint8_t value);
static uint8_t fg160_gg_eq_5(uint8_t a, uint8_t b);
static uint8_t fg160_gg_equal_elements_5(fg160_gg_slice a, fg160_gg_slice b);
static void fg160_gg_render_5(fg160_gg_reference out, uint8_t value);
static uint8_t fg160_gg_equal_elements_1(fg160_gg_slice a,fg160_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg160_gg_eq_1(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg160_gg_eq_1(int64_t a, int64_t b) {
return a == b;
}
static void fg160_gg_render_1(fg160_gg_reference out, int64_t value) {
fg160_gg_integer(out,value);
}
static uint8_t fg160_gg_equal_elements_2(fg160_gg_slice a,fg160_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg160_gg_eq_2(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg160_gg_eq_2(uint64_t a, uint64_t b) {
return a == b;
}
static void fg160_gg_render_2(fg160_gg_reference out, uint64_t value) {
fg160_gg_unsigned(out,value);
}
static uint8_t fg160_gg_equal_elements_4(fg160_gg_slice a,fg160_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg160_gg_eq_4(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg160_gg_eq_4(uint8_t a, uint8_t b) {
return a == b;
}
static void fg160_gg_render_4(fg160_gg_reference out, uint8_t value) {
fg160_gg_bool(out,value);
}
static uint8_t fg160_gg_equal_elements_5(fg160_gg_slice a,fg160_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg160_gg_eq_5(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg160_gg_eq_5(uint8_t a, uint8_t b) {
return a == b;
}
static void fg160_gg_render_5(fg160_gg_reference out, uint8_t value) {
fg160_gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} fg160_gg_globals;
static void fg160_gg_function_0(int64_t base);
static uint64_t fg160_gg_function_1(int64_t p0, int64_t p1, int64_t base);
static int64_t fg160_gg_function_3(uint64_t p0, int64_t base);
static int64_t fg160_gg_function_4(int64_t p0, int64_t p1, int64_t base);
static uint8_t fg160_gg_function_5(int64_t p0, int64_t p1, int64_t base);
static void fg160_gg_function_7(uint8_t p0, int64_t p1, int64_t base);
static uint8_t fg160_gg_function_13(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static uint64_t fg160_gg_function_15(int64_t base);
static uint8_t fg160_gg_function_16(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t fg160_gg_function_17(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static uint64_t fg160_gg_function_19(int64_t base);
static uint8_t fg160_gg_function_20(int64_t p0, int64_t p1, int64_t base);
static uint8_t fg160_gg_function_21(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base);
static int64_t fg160_gg_function_22(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static uint64_t fg160_gg_function_23(int64_t base);
static uint8_t fg160_gg_function_24(int64_t p0, int64_t base);
static uint64_t fg160_gg_function_26(int64_t base);
static uint64_t fg160_gg_function_27(int64_t base);
static uint64_t fg160_gg_function_28(int64_t base);
static uint64_t fg160_gg_function_32(int64_t base);
static uint64_t fg160_gg_function_33(int64_t base);
static uint64_t fg160_gg_function_34(int64_t base);
static uint64_t fg160_gg_function_35(int64_t base);
static uint64_t fg160_gg_function_36(int64_t base);
static int64_t fg160_gg_function_40(int64_t p0, int64_t p1, int64_t base);
static uint64_t fg160_gg_function_42(int64_t base);
static uint64_t fg160_gg_function_42(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(64))))));
  goto fg160_gg_done;
  }
fg160_gg_done:;
  return t0;
}
static int64_t fg160_gg_function_3(uint64_t p0, int64_t base) {
  int64_t t0;
  uint64_t v0;
  v0 = p0;
  {
  t0 = ((int64_t)(((int64_t)(v0))));
  goto fg160_gg_done;
  }
fg160_gg_done:;
  return t0;
}
static void fg160_gg_function_7(uint8_t p0, int64_t p1, int64_t base) {
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
  fg160_fail(t2);
  }
  }
  }
fg160_gg_done:;
}
static int64_t fg160_gg_function_40(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) > (t2));
  if (t3) {
  {
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t5) - (uint64_t)(t6)))));
  t8 = ((t4) <= (t7));
  t9 = ((int64_t)(((int64_t)UINT64_C(3))));
  fg160_gg_function_7(t8, t9, base + 0);
  }
  }
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t10) < (t11));
  if (t12) {
  {
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)UINT64_C(9223372036854775807));
  t15 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t14)))));
  t16 = ((int64_t)(((int64_t)UINT64_C(1))));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) - (uint64_t)(t16)))));
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) - (uint64_t)(t18)))));
  t20 = ((t13) >= (t19));
  t21 = ((int64_t)(((int64_t)UINT64_C(3))));
  fg160_gg_function_7(t20, t21, base + 0);
  }
  }
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(v1));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t22) + (uint64_t)(t23)))));
  goto fg160_gg_done;
  }
fg160_gg_done:;
  return t0;
}
static uint64_t fg160_gg_function_36(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(6))))));
  goto fg160_gg_done;
  }
fg160_gg_done:;
  return t0;
}
static uint64_t fg160_gg_function_35(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(3))))));
  goto fg160_gg_done;
  }
fg160_gg_done:;
  return t0;
}
static uint64_t fg160_gg_function_34(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(24))))));
  goto fg160_gg_done;
  }
fg160_gg_done:;
  return t0;
}
static uint64_t fg160_gg_function_33(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(49))))));
  goto fg160_gg_done;
  }
fg160_gg_done:;
  return t0;
}
static uint64_t fg160_gg_function_32(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(48))))));
  goto fg160_gg_done;
  }
fg160_gg_done:;
  return t0;
}
static uint64_t fg160_gg_function_28(int64_t base) {
  /* FG_SHARED_BODY:42 */
  return fg160_gg_function_42(base);
}
static uint64_t fg160_gg_function_27(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  goto fg160_gg_done;
  }
fg160_gg_done:;
  return t0;
}
static uint64_t fg160_gg_function_26(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  goto fg160_gg_done;
  }
fg160_gg_done:;
  return t0;
}
static uint8_t fg160_gg_function_24(int64_t p0, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  uint64_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  uint64_t t13;
  int64_t t14;
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  uint64_t t20;
  uint64_t t21;
  uint64_t t22;
  uint64_t t23;
  uint64_t t24;
  uint64_t t25;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(4))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg160_tensor(t1, t2, t3);
  t5 = ((uint64_t)(t4));
  v1 = fg160_gg_function_3(t5, base + 0);
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t6) != (t7));
  t9 = t8;
  if (t9) {
  t10 = ((int64_t)(v1));
  t11 = fg160_usage(t10);
  t12 = ((int64_t)(t11));
  t13 = fg160_gg_function_26(base + 0);
  t14 = ((int64_t)(((int64_t)(t13))));
  t9 = ((t12) == (t14));
  }
  t15 = t9;
  t16 = t15;
  if (t16) {
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = fg160_tensor(t17, t18, t19);
  t21 = ((uint64_t)(t20));
  t22 = fg160_gg_function_27(base + 0);
  t23 = ((uint64_t)(t22));
  t24 = ((uint64_t)(((t21) & (t23))));
  t25 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t24) == (t25));
  }
  t0 = t16;
  goto fg160_gg_done;
  }
fg160_gg_done:;
  return t0;
}
static uint8_t fg160_gg_function_5(int64_t p0, int64_t p1, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg160_graph(t6, t7);
  t9 = ((int64_t)(t8));
  t4 = ((t5) < (t9));
  }
  t0 = t4;
  goto fg160_gg_done;
  }
fg160_gg_done:;
  return t0;
}
static int64_t fg160_gg_function_17(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(v2));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  }
  } else {
  {
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(v2));
  t8 = fg160_i32(t6, t7);
  t0 = ((int64_t)(t8));
  }
  }
  goto fg160_gg_done;
  }
fg160_gg_done:;
  return t0;
}
static int64_t fg160_gg_function_22(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t1 = ((int64_t)(v5));
  t2 = ((int64_t)(v2));
  if (!(((t1) < (t2)))) goto label2;
  {
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(v5));
  v6 = fg160_gg_function_17(t3, t4, t5, base + 0);
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(v6));
  t8 = fg160_gg_function_5(t6, t7, base + 0);
  t9 = t8;
  t10 = (!(t9));
  if (t10) {
  {
  t11 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t11)))));
  goto fg160_gg_done;
  }
  }
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(v6));
  t14 = fg160_node(t12, t13);
  t15 = ((int64_t)(t14));
  t16 = ((int64_t)(v3));
  t17 = ((t15) == (t16));
  if (t17) {
  {
  t0 = ((int64_t)(v5));
  goto fg160_gg_done;
  }
  }
  t18 = v5;
  t19 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t18) + (uint64_t)(t19)));
  }
  goto label1;
label2:;
  t20 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t20)))));
  goto fg160_gg_done;
  }
fg160_gg_done:;
  return t0;
}
static uint64_t fg160_gg_function_23(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(10))))));
  goto fg160_gg_done;
  }
fg160_gg_done:;
  return t0;
}
static int64_t fg160_gg_function_4(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint8_t t13;
  uint8_t t14;
  int64_t t15;
  uint64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  uint8_t t20;
  int64_t t21;
  uint64_t t22;
  int64_t t23;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = fg160_gg_function_5(t1, t2, base + 0);
  t4 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg160_gg_function_7(t3, t4, base + 0);
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(v1));
  t8 = fg160_node(t6, t7);
  t9 = ((int64_t)(t8));
  t10 = fg160_hash_find(t5, t9);
  v2 = ((uint64_t)(t10));
  t11 = ((uint64_t)(v2));
  t12 = ((uint64_t)(((uint64_t)(((uint64_t)UINT64_C(18446744073709551615))))));
  t13 = ((t11) == (t12));
  t14 = t13;
  if (!t14) {
  t15 = ((int64_t)(v0));
  t16 = ((uint64_t)(v2));
  t17 = fg160_hash_used(t15, t16);
  t18 = ((int64_t)(t17));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t18) == (t19));
  }
  t20 = t14;
  if (t20) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg160_gg_done;
  }
  }
  t21 = ((int64_t)(v0));
  t22 = ((uint64_t)(v2));
  t23 = fg160_count(t21, t22);
  t0 = ((int64_t)(t23));
  goto fg160_gg_done;
  }
fg160_gg_done:;
  return t0;
}
static uint64_t fg160_gg_function_15(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2))))));
  goto fg160_gg_done;
  }
fg160_gg_done:;
  return t0;
}
static uint64_t fg160_gg_function_19(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(16))))));
  goto fg160_gg_done;
  }
fg160_gg_done:;
  return t0;
}
static uint8_t fg160_gg_function_21(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
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
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  uint8_t t20;
  uint8_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint64_t t28;
  uint64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  uint64_t t33;
  uint8_t t34;
  uint8_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  uint64_t t39;
  uint64_t t40;
  uint64_t t41;
  uint64_t t42;
  uint64_t t43;
  uint64_t t44;
  uint8_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  uint8_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  uint64_t t61;
  uint64_t t62;
  uint64_t t63;
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
  int64_t t74;
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
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  uint64_t t89;
  uint64_t t90;
  uint64_t t91;
  uint8_t t92;
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
  uint8_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  uint64_t t108;
  uint64_t t109;
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
  uint8_t t121;
  uint8_t t122;
  int64_t t123;
  uint8_t t124;
  uint8_t t125;
  uint8_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  uint64_t t130;
  uint64_t t131;
  int64_t t132;
  int64_t t133;
  uint8_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  uint64_t t138;
  uint64_t t139;
  int64_t t140;
  int64_t t141;
  uint8_t t142;
  int64_t t143;
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
  uint64_t t154;
  uint64_t t155;
  int64_t t156;
  int64_t t157;
  uint8_t t158;
  uint8_t t159;
  int64_t t160;
  int64_t t161;
  uint8_t t162;
  int64_t t163;
  int64_t t164;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t1 = ((int64_t)(v5));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v6));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) > (t6));
  }
  t7 = t4;
  t8 = ((int64_t)(((int64_t)UINT64_C(2))));
  fg160_gg_function_7(t7, t8, base + 0);
  t9 = ((int64_t)(v4));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) < (t10));
  if (t11) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg160_gg_done;
  }
  }
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t12 = ((int64_t)(v7));
  t13 = ((int64_t)(v4));
  if (!(((t12) < (t13)))) goto label2;
  {
  t14 = ((int64_t)(v1));
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(v7));
  v8 = fg160_gg_function_17(t14, t15, t16, base + 0);
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(v8));
  t19 = fg160_gg_function_5(t17, t18, base + 0);
  t20 = t19;
  t21 = (!(t20));
  if (t21) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg160_gg_done;
  }
  }
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(v8));
  t24 = fg160_node(t22, t23);
  v9 = ((int64_t)(t24));
  t25 = ((int64_t)(v9));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = fg160_tensor(t25, t26, t27);
  t29 = ((uint64_t)(t28));
  t30 = ((int64_t)(v3));
  t31 = ((int64_t)(v7));
  t32 = fg160_i32(t30, t31);
  t33 = ((uint64_t)(((uint64_t)(t32))));
  t34 = ((t29) != (t33));
  t35 = t34;
  if (!t35) {
  t36 = ((int64_t)(v9));
  t37 = ((int64_t)(((int64_t)UINT64_C(1))));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = fg160_tensor(t36, t37, t38);
  t40 = ((uint64_t)(t39));
  t41 = fg160_gg_function_19(base + 0);
  t42 = ((uint64_t)(t41));
  t43 = ((uint64_t)(((t40) & (t42))));
  t44 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t35 = ((t43) == (t44));
  }
  t45 = t35;
  if (t45) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg160_gg_done;
  }
  }
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(v5));
  t48 = ((int64_t)(v6));
  t49 = ((int64_t)(v9));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = fg160_gg_function_22(t46, t47, t48, t49, t50, base + 0);
  t52 = ((int64_t)(t51));
  t53 = ((int64_t)UINT64_C(1));
  t54 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t53)))));
  t55 = ((t52) != (t54));
  if (t55) {
  {
  t56 = v7;
  t57 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)((uint64_t)(t56) + (uint64_t)(t57)));
  goto label1;
  }
  }
  t58 = ((int64_t)(v9));
  t59 = ((int64_t)(((int64_t)UINT64_C(1))));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = fg160_tensor(t58, t59, t60);
  t62 = ((uint64_t)(t61));
  t63 = fg160_gg_function_15(base + 0);
  t64 = ((uint64_t)(t63));
  t65 = ((uint64_t)(((t62) & (t64))));
  t66 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t67 = ((t65) != (t66));
  if (t67) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg160_gg_done;
  }
  }
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t68 = ((int64_t)(v7));
  t69 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t68) + (uint64_t)(t69)))));
label4:;
  t70 = ((int64_t)(v11));
  t71 = ((int64_t)(v4));
  if (!(((t70) < (t71)))) goto label5;
  {
  t72 = ((int64_t)(v1));
  t73 = ((int64_t)(v2));
  t74 = ((int64_t)(v11));
  v12 = fg160_gg_function_17(t72, t73, t74, base + 0);
  t75 = ((int64_t)(v0));
  t76 = ((int64_t)(v12));
  t77 = fg160_gg_function_5(t75, t76, base + 0);
  t78 = t77;
  t79 = (!(t78));
  if (t79) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg160_gg_done;
  }
  }
  t80 = ((int64_t)(v0));
  t81 = ((int64_t)(v12));
  t82 = fg160_node(t80, t81);
  v13 = ((int64_t)(t82));
  v14 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t83 = ((int64_t)(v14));
  t84 = fg160_gg_function_23(base + 0);
  t85 = ((int64_t)(((int64_t)(t84))));
  if (!(((t83) < (t85)))) goto label8;
  {
  t86 = ((int64_t)(v13));
  t87 = ((int64_t)(((int64_t)UINT64_C(5))));
  t88 = ((int64_t)(v14));
  t89 = fg160_tensor(t86, t87, t88);
  t90 = ((uint64_t)(t89));
  t91 = ((uint64_t)(((uint64_t)(v9))));
  t92 = ((t90) == (t91));
  if (t92) {
  {
  t93 = v10;
  t94 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = ((int64_t)((uint64_t)(t93) + (uint64_t)(t94)));
  }
  }
  t95 = v14;
  t96 = ((int64_t)(((int64_t)UINT64_C(1))));
  v14 = ((int64_t)((uint64_t)(t95) + (uint64_t)(t96)));
  }
  goto label7;
label8:;
  t97 = v11;
  t98 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t97) + (uint64_t)(t98)));
  }
  goto label4;
label5:;
  t99 = ((int64_t)(v10));
  t100 = ((int64_t)(v0));
  t101 = ((int64_t)(v8));
  t102 = fg160_gg_function_4(t100, t101, base + 0);
  t103 = ((int64_t)(t102));
  t104 = ((t99) != (t103));
  if (t104) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg160_gg_done;
  }
  }
  t105 = ((int64_t)(v9));
  t106 = ((int64_t)(((int64_t)UINT64_C(2))));
  t107 = ((int64_t)(((int64_t)UINT64_C(0))));
  t108 = fg160_tensor(t105, t106, t107);
  t109 = ((uint64_t)(t108));
  v15 = fg160_gg_function_3(t109, base + 0);
  v16 = ((int64_t)(v15));
  v17 = ((int64_t)(v15));
label10:;
  t110 = ((int64_t)(v15));
  t111 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t110) != (t111)))) goto label11;
  {
  t112 = ((int64_t)(v0));
  t113 = ((int64_t)(v1));
  t114 = ((int64_t)(v4));
  t115 = ((int64_t)(v15));
  t116 = ((int64_t)(v2));
  t117 = fg160_gg_function_22(t112, t113, t114, t115, t116, base + 0);
  t118 = ((int64_t)(t117));
  t119 = ((int64_t)UINT64_C(1));
  t120 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t119)))));
  t121 = ((t118) == (t120));
  t122 = t121;
  if (t122) {
  t123 = ((int64_t)(v15));
  t124 = fg160_gg_function_24(t123, base + 0);
  t125 = t124;
  t122 = (!(t125));
  }
  t126 = t122;
  if (t126) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg160_gg_done;
  }
  }
  t127 = ((int64_t)(v15));
  t128 = ((int64_t)(((int64_t)UINT64_C(2))));
  t129 = ((int64_t)(((int64_t)UINT64_C(0))));
  t130 = fg160_tensor(t127, t128, t129);
  t131 = ((uint64_t)(t130));
  v15 = fg160_gg_function_3(t131, base + 0);
  t132 = ((int64_t)(v16));
  t133 = ((int64_t)(((int64_t)UINT64_C(0))));
  t134 = ((t132) != (t133));
  if (t134) {
  {
  t135 = ((int64_t)(v16));
  t136 = ((int64_t)(((int64_t)UINT64_C(2))));
  t137 = ((int64_t)(((int64_t)UINT64_C(0))));
  t138 = fg160_tensor(t135, t136, t137);
  t139 = ((uint64_t)(t138));
  v16 = fg160_gg_function_3(t139, base + 0);
  }
  }
  t140 = ((int64_t)(v17));
  t141 = ((int64_t)(((int64_t)UINT64_C(0))));
  t142 = ((t140) != (t141));
  if (t142) {
  {
  t143 = ((int64_t)(v17));
  t144 = ((int64_t)(((int64_t)UINT64_C(2))));
  t145 = ((int64_t)(((int64_t)UINT64_C(0))));
  t146 = fg160_tensor(t143, t144, t145);
  t147 = ((uint64_t)(t146));
  v17 = fg160_gg_function_3(t147, base + 0);
  }
  }
  t148 = ((int64_t)(v17));
  t149 = ((int64_t)(((int64_t)UINT64_C(0))));
  t150 = ((t148) != (t149));
  if (t150) {
  {
  t151 = ((int64_t)(v17));
  t152 = ((int64_t)(((int64_t)UINT64_C(2))));
  t153 = ((int64_t)(((int64_t)UINT64_C(0))));
  t154 = fg160_tensor(t151, t152, t153);
  t155 = ((uint64_t)(t154));
  v17 = fg160_gg_function_3(t155, base + 0);
  }
  }
  t156 = ((int64_t)(v16));
  t157 = ((int64_t)(((int64_t)UINT64_C(0))));
  t158 = ((t156) != (t157));
  t159 = t158;
  if (t159) {
  t160 = ((int64_t)(v16));
  t161 = ((int64_t)(v17));
  t159 = ((t160) == (t161));
  }
  t162 = t159;
  if (t162) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg160_gg_done;
  }
  }
  }
  goto label10;
label11:;
  t163 = v7;
  t164 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)((uint64_t)(t163) + (uint64_t)(t164)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto fg160_gg_done;
  }
fg160_gg_done:;
  return t0;
}
static uint8_t fg160_gg_function_20(int64_t p0, int64_t p1, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint64_t t6;
  uint64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint8_t t13;
  int64_t t14;
  int64_t t15;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t1) < (t2)))) goto label2;
  {
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(3))));
  t5 = ((int64_t)(v2));
  t6 = fg160_tensor(t3, t4, t5);
  t7 = ((uint64_t)(t6));
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(((int64_t)UINT64_C(3))));
  t10 = ((int64_t)(v2));
  t11 = fg160_tensor(t8, t9, t10);
  t12 = ((uint64_t)(t11));
  t13 = ((t7) != (t12));
  if (t13) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg160_gg_done;
  }
  }
  t14 = v2;
  t15 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t14) + (uint64_t)(t15)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto fg160_gg_done;
  }
fg160_gg_done:;
  return t0;
}
static uint8_t fg160_gg_function_13(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
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
  uint8_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint64_t t19;
  uint64_t t20;
  uint64_t t21;
  uint8_t t22;
  uint8_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  uint64_t t27;
  uint64_t t28;
  uint64_t t29;
  uint64_t t30;
  uint64_t t31;
  uint64_t t32;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = fg160_gg_function_5(t1, t2, base + 0);
  t4 = t3;
  t5 = (!(t4));
  if (t5) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg160_gg_done;
  }
  }
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(v1));
  t8 = fg160_node(t6, t7);
  v3 = ((int64_t)(t8));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(v1));
  t11 = fg160_gg_function_4(t9, t10, base + 0);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(v2));
  t14 = ((t12) == (t13));
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(v3));
  t17 = ((int64_t)(((int64_t)UINT64_C(2))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = fg160_tensor(t16, t17, t18);
  t20 = ((uint64_t)(t19));
  t21 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t20) == (t21));
  }
  t22 = t15;
  t23 = t22;
  if (t23) {
  t24 = ((int64_t)(v3));
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = fg160_tensor(t24, t25, t26);
  t28 = ((uint64_t)(t27));
  t29 = fg160_gg_function_15(base + 0);
  t30 = ((uint64_t)(t29));
  t31 = ((uint64_t)(((t28) & (t30))));
  t32 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t23 = ((t31) == (t32));
  }
  t0 = t23;
  goto fg160_gg_done;
  }
fg160_gg_done:;
  return t0;
}
static uint8_t fg160_gg_function_16(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
  uint8_t t0;
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
  uint8_t t11;
  uint8_t t12;
  uint8_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  uint64_t t20;
  uint64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  uint64_t t25;
  uint8_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  uint64_t t31;
  uint64_t t32;
  uint64_t t33;
  uint64_t t34;
  uint64_t t35;
  uint64_t t36;
  uint8_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  uint8_t t42;
  uint8_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint8_t t47;
  uint8_t t48;
  uint8_t t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
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
  uint64_t t64;
  uint64_t t65;
  uint64_t t66;
  uint8_t t67;
  uint8_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  uint64_t t72;
  uint64_t t73;
  uint64_t t74;
  uint8_t t75;
  uint8_t t76;
  int64_t t77;
  int64_t t78;
  uint8_t t79;
  uint8_t t80;
  uint8_t t81;
  int64_t t82;
  int64_t t83;
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
  v4 = p4;
  {
  t1 = ((int64_t)(v4));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg160_gg_done;
  }
  }
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t4 = ((int64_t)(v5));
  t5 = ((int64_t)(v4));
  if (!(((t4) < (t5)))) goto label2;
  {
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(v2));
  t8 = ((int64_t)(v5));
  v6 = fg160_gg_function_17(t6, t7, t8, base + 0);
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(v6));
  t11 = fg160_gg_function_5(t9, t10, base + 0);
  t12 = t11;
  t13 = (!(t12));
  if (t13) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg160_gg_done;
  }
  }
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(v6));
  t16 = fg160_node(t14, t15);
  v7 = ((int64_t)(t16));
  t17 = ((int64_t)(v7));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = fg160_tensor(t17, t18, t19);
  t21 = ((uint64_t)(t20));
  t22 = ((int64_t)(v3));
  t23 = ((int64_t)(v5));
  t24 = fg160_i32(t22, t23);
  t25 = ((uint64_t)(((uint64_t)(t24))));
  t26 = ((t21) != (t25));
  t27 = t26;
  if (!t27) {
  t28 = ((int64_t)(v7));
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = fg160_tensor(t28, t29, t30);
  t32 = ((uint64_t)(t31));
  t33 = fg160_gg_function_19(base + 0);
  t34 = ((uint64_t)(t33));
  t35 = ((uint64_t)(((t32) & (t34))));
  t36 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t27 = ((t35) == (t36));
  }
  t37 = t27;
  if (t37) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg160_gg_done;
  }
  }
  t38 = ((int64_t)(v5));
  t39 = ((int64_t)(v4));
  t40 = ((int64_t)(((int64_t)UINT64_C(1))));
  t41 = ((int64_t)(((int64_t)((uint64_t)(t39) - (uint64_t)(t40)))));
  t42 = ((t38) < (t41));
  t43 = t42;
  if (t43) {
  t44 = ((int64_t)(v0));
  t45 = ((int64_t)(v6));
  t46 = ((int64_t)(((int64_t)UINT64_C(1))));
  t47 = fg160_gg_function_13(t44, t45, t46, base + 0);
  t48 = t47;
  t43 = (!(t48));
  }
  t49 = t43;
  if (t49) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg160_gg_done;
  }
  }
  t50 = ((int64_t)(v5));
  t51 = ((int64_t)(((int64_t)UINT64_C(0))));
  t52 = ((t50) > (t51));
  if (t52) {
  {
  t53 = ((int64_t)(v0));
  t54 = ((int64_t)(v1));
  t55 = ((int64_t)(v2));
  t56 = ((int64_t)(v5));
  t57 = ((int64_t)(((int64_t)UINT64_C(1))));
  t58 = ((int64_t)(((int64_t)((uint64_t)(t56) - (uint64_t)(t57)))));
  t59 = fg160_gg_function_17(t54, t55, t58, base + 0);
  t60 = fg160_node(t53, t59);
  v8 = ((int64_t)(t60));
  t61 = ((int64_t)(v7));
  t62 = ((int64_t)(((int64_t)UINT64_C(5))));
  t63 = ((int64_t)(((int64_t)UINT64_C(0))));
  t64 = fg160_tensor(t61, t62, t63);
  t65 = ((uint64_t)(t64));
  t66 = ((uint64_t)(((uint64_t)(v8))));
  t67 = ((t65) != (t66));
  t68 = t67;
  if (t68) {
  t69 = ((int64_t)(v7));
  t70 = ((int64_t)(((int64_t)UINT64_C(5))));
  t71 = ((int64_t)(((int64_t)UINT64_C(1))));
  t72 = fg160_tensor(t69, t70, t71);
  t73 = ((uint64_t)(t72));
  t74 = ((uint64_t)(((uint64_t)(v8))));
  t68 = ((t73) != (t74));
  }
  t75 = t68;
  t76 = t75;
  if (!t76) {
  t77 = ((int64_t)(v7));
  t78 = ((int64_t)(v8));
  t79 = fg160_gg_function_20(t77, t78, base + 0);
  t80 = t79;
  t76 = (!(t80));
  }
  t81 = t76;
  if (t81) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg160_gg_done;
  }
  }
  }
  }
  t82 = v5;
  t83 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t82) + (uint64_t)(t83)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto fg160_gg_done;
  }
fg160_gg_done:;
  return t0;
}
static uint64_t fg160_gg_function_1(int64_t p0, int64_t p1, int64_t base) {
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
  uint64_t t20;
  int64_t t21;
  int64_t t22;
  uint64_t t23;
  uint64_t t24;
  int64_t t25;
  int64_t t26;
  uint64_t t27;
  uint64_t t28;
  int64_t t29;
  int64_t t30;
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  uint8_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  uint8_t t41;
  uint8_t t42;
  int64_t t43;
  int64_t t44;
  uint8_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  uint8_t t51;
  uint8_t t52;
  int64_t t53;
  int64_t t54;
  uint8_t t55;
  int64_t t56;
  int64_t t57;
  uint8_t t58;
  int64_t t59;
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
  uint8_t t81;
  uint8_t t82;
  int64_t t83;
  int64_t t84;
  uint8_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  uint8_t t93;
  uint8_t t94;
  int64_t t95;
  int64_t t96;
  uint8_t t97;
  int64_t t98;
  int64_t t99;
  uint8_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  uint8_t t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  uint8_t t108;
  uint8_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
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
  uint8_t t125;
  uint8_t t126;
  int64_t t127;
  int64_t t128;
  uint8_t t129;
  int64_t t130;
  int64_t t131;
  uint8_t t132;
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
  uint8_t t160;
  uint8_t t161;
  uint8_t t162;
  uint8_t t163;
  int64_t t164;
  int64_t t165;
  uint8_t t166;
  uint8_t t167;
  uint8_t t168;
  uint8_t t169;
  int64_t t170;
  int64_t t171;
  uint8_t t172;
  uint8_t t173;
  int64_t t174;
  uint64_t t175;
  int64_t t176;
  uint8_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  uint64_t t184;
  uint64_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  uint64_t t189;
  uint8_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  uint8_t t195;
  int64_t t196;
  int64_t t197;
  uint8_t t198;
  uint8_t t199;
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
  uint8_t t212;
  int64_t t213;
  uint8_t t214;
  uint8_t t215;
  int64_t t216;
  int64_t t217;
  uint8_t t218;
  int64_t t219;
  int64_t t220;
  uint8_t t221;
  uint8_t t222;
  int64_t t223;
  int64_t t224;
  uint64_t t225;
  uint64_t t226;
  uint64_t t227;
  uint64_t t228;
  uint8_t t229;
  uint8_t t230;
  int64_t t231;
  int64_t t232;
  uint64_t t233;
  uint64_t t234;
  uint64_t t235;
  uint8_t t236;
  uint8_t t237;
  int64_t t238;
  int64_t t239;
  uint8_t t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  uint64_t t244;
  uint64_t t245;
  uint64_t t246;
  uint8_t t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  int64_t t251;
  uint64_t t252;
  uint64_t t253;
  int64_t t254;
  int64_t t255;
  uint8_t t256;
  uint8_t t257;
  int64_t t258;
  int64_t t259;
  uint8_t t260;
  int64_t t261;
  int64_t t262;
  uint8_t t263;
  uint8_t t264;
  int64_t t265;
  int64_t t266;
  uint64_t t267;
  uint64_t t268;
  uint64_t t269;
  uint64_t t270;
  uint64_t t271;
  uint64_t t272;
  uint8_t t273;
  int64_t t274;
  int64_t t275;
  int64_t t276;
  int64_t t277;
  uint64_t t278;
  uint64_t t279;
  uint64_t t280;
  uint64_t t281;
  uint64_t t282;
  int64_t t283;
  int64_t t284;
  uint8_t t285;
  uint8_t t286;
  int64_t t287;
  int64_t t288;
  uint8_t t289;
  int64_t t290;
  int64_t t291;
  uint8_t t292;
  uint8_t t293;
  int64_t t294;
  int64_t t295;
  uint64_t t296;
  uint64_t t297;
  uint64_t t298;
  uint64_t t299;
  uint64_t t300;
  uint64_t t301;
  uint8_t t302;
  int64_t t303;
  int64_t t304;
  int64_t t305;
  int64_t t306;
  uint64_t t307;
  uint64_t t308;
  uint64_t t309;
  uint64_t t310;
  int64_t t311;
  int64_t t312;
  uint64_t t313;
  uint64_t t314;
  int64_t t315;
  int64_t t316;
  uint8_t t317;
  int64_t t318;
  int64_t t319;
  int64_t t320;
  uint64_t t321;
  uint64_t t322;
  uint64_t t323;
  uint64_t t324;
  uint8_t t325;
  uint8_t t326;
  uint64_t t327;
  uint64_t t328;
  uint64_t t329;
  uint8_t t330;
  uint8_t t331;
  int64_t t332;
  int64_t t333;
  uint8_t t334;
  uint8_t t335;
  int64_t t336;
  uint64_t t337;
  uint64_t t338;
  uint64_t t339;
  uint64_t t340;
  uint64_t t341;
  uint8_t t342;
  int64_t t343;
  int64_t t344;
  uint64_t t345;
  int64_t t346;
  int64_t t347;
  uint64_t t348;
  uint64_t t349;
  int64_t t350;
  int64_t t351;
  uint8_t t352;
  int64_t t353;
  int64_t t354;
  int64_t t355;
  int64_t t356;
  int64_t t357;
  int64_t t358;
  uint8_t t359;
  uint64_t t360;
  uint64_t t361;
  int64_t t362;
  int64_t t363;
  int64_t t364;
  int64_t t365;
  uint8_t t366;
  int64_t t367;
  uint64_t t368;
  int64_t t369;
  int64_t t370;
  uint64_t t371;
  uint64_t t372;
  int64_t t373;
  uint64_t t374;
  int64_t t375;
  int64_t t376;
  uint64_t t377;
  uint64_t t378;
  int64_t t379;
  int64_t t380;
  uint8_t t381;
  int64_t t382;
  int64_t t383;
  uint8_t t384;
  uint8_t t385;
  uint64_t t386;
  int64_t t387;
  int64_t t388;
  int64_t t389;
  uint64_t t390;
  uint64_t t391;
  uint8_t t392;
  int64_t t393;
  int64_t t394;
  int64_t t395;
  int64_t t396;
  int64_t t397;
  int64_t t398;
  int64_t t399;
  int64_t t400;
  int64_t t401;
  int64_t t402;
  uint64_t t403;
  uint64_t t404;
  int64_t t405;
  int64_t t406;
  int64_t t407;
  int64_t t408;
  uint64_t t409;
  uint64_t t410;
  int64_t t411;
  int64_t t412;
  int64_t t413;
  int64_t t414;
  uint64_t t415;
  uint64_t t416;
  int64_t t417;
  int64_t t418;
  int64_t t419;
  int64_t t420;
  uint64_t t421;
  uint64_t t422;
  int64_t t423;
  int64_t t424;
  int64_t t425;
  uint64_t t426;
  uint64_t t427;
  int64_t t428;
  int64_t t429;
  int64_t t430;
  uint64_t t431;
  uint64_t t432;
  int64_t t433;
  int64_t t434;
  int64_t t435;
  uint8_t t436;
  int64_t t437;
  int64_t t438;
  int64_t t439;
  uint64_t t440;
  uint64_t t441;
  int64_t t442;
  int64_t t443;
  int64_t t444;
  int64_t t445;
  int64_t t446;
  int64_t t447;
  int64_t t448;
  int64_t t449;
  int64_t t450;
  int64_t t451;
  int64_t t452;
  int64_t t453;
  int64_t t454;
  uint64_t t455;
  uint64_t t456;
  int64_t t457;
  int64_t t458;
  int64_t t459;
  int64_t t460;
  int64_t t461;
  int64_t t462;
  int64_t t463;
  int64_t t464;
  int64_t t465;
  int64_t t466;
  int64_t t467;
  int64_t t468;
  int64_t t469;
  uint64_t t470;
  uint64_t t471;
  int64_t t472;
  int64_t t473;
  int64_t t474;
  int64_t t475;
  int64_t t476;
  int64_t t477;
  int64_t t478;
  int64_t t479;
  int64_t t480;
  int64_t t481;
  int64_t t482;
  int64_t t483;
  int64_t t484;
  uint64_t t485;
  uint64_t t486;
  int64_t t487;
  int64_t t488;
  int64_t t489;
  int64_t t490;
  int64_t t491;
  int64_t t492;
  int64_t t493;
  int64_t t494;
  int64_t t495;
  int64_t t496;
  int64_t t497;
  int64_t t498;
  uint8_t t499;
  uint8_t t500;
  int64_t t501;
  int64_t t502;
  uint8_t t503;
  uint8_t t504;
  int64_t t505;
  int64_t t506;
  uint8_t t507;
  uint8_t t508;
  int64_t t509;
  int64_t t510;
  uint8_t t511;
  int64_t t512;
  int64_t t513;
  int64_t t514;
  int64_t t515;
  int64_t t516;
  uint64_t t517;
  uint64_t t518;
  int64_t t519;
  int64_t t520;
  int64_t t521;
  int64_t t522;
  int64_t t523;
  int64_t t524;
  int64_t t525;
  int64_t t526;
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
  uint64_t t539;
  int64_t t540;
  int64_t t541;
  uint8_t t542;
  uint64_t t543;
  int64_t t544;
  uint64_t t545;
  int64_t t546;
  int64_t t547;
  int64_t t548;
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
  uint64_t v15;
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
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg160_arg(t1, t2);
  t4 = ((uint64_t)(t3));
  v2 = fg160_gg_function_3(t4, base + 0);
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = fg160_arg(t5, t6);
  t8 = ((uint64_t)(t7));
  v3 = fg160_gg_function_3(t8, base + 0);
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = fg160_arg(t9, t10);
  t12 = ((uint64_t)(t11));
  v4 = fg160_gg_function_3(t12, base + 0);
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(3))));
  t15 = fg160_arg(t13, t14);
  t16 = ((uint64_t)(t15));
  v5 = fg160_gg_function_3(t16, base + 0);
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(((int64_t)UINT64_C(4))));
  t19 = fg160_arg(t17, t18);
  t20 = ((uint64_t)(t19));
  v6 = fg160_gg_function_3(t20, base + 0);
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(((int64_t)UINT64_C(5))));
  t23 = fg160_arg(t21, t22);
  t24 = ((uint64_t)(t23));
  v7 = fg160_gg_function_3(t24, base + 0);
  t25 = ((int64_t)(v1));
  t26 = ((int64_t)(((int64_t)UINT64_C(6))));
  t27 = fg160_arg(t25, t26);
  t28 = ((uint64_t)(t27));
  v8 = fg160_gg_function_3(t28, base + 0);
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((t29) == (t30));
  if (t31) {
  {
  t32 = ((int64_t)(v2));
  t33 = ((int64_t)(v3));
  t34 = fg160_gg_function_4(t32, t33, base + 0);
  t0 = ((uint64_t)(((uint64_t)(t34))));
  goto fg160_gg_done;
  }
  }
  t35 = ((int64_t)(v0));
  t36 = ((int64_t)(((int64_t)UINT64_C(1))));
  t37 = ((t35) == (t36));
  if (t37) {
  {
  t38 = ((int64_t)(v2));
  t39 = ((int64_t)(v3));
  t40 = ((int64_t)(v4));
  t41 = fg160_gg_function_13(t38, t39, t40, base + 0);
  t42 = t41;
  if (t42) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  goto fg160_gg_done;
  }
  }
  t43 = ((int64_t)(v0));
  t44 = ((int64_t)(((int64_t)UINT64_C(2))));
  t45 = ((t43) == (t44));
  if (t45) {
  {
  t46 = ((int64_t)(v2));
  t47 = ((int64_t)(v3));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = ((int64_t)(v4));
  t50 = ((int64_t)(v5));
  t51 = fg160_gg_function_16(t46, t47, t48, t49, t50, base + 0);
  t52 = t51;
  if (t52) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  goto fg160_gg_done;
  }
  }
  t53 = ((int64_t)(v0));
  t54 = ((int64_t)(((int64_t)UINT64_C(3))));
  t55 = ((t53) == (t54));
  if (t55) {
  {
  t56 = ((int64_t)(v5));
  t57 = ((int64_t)(((int64_t)UINT64_C(32))));
  t58 = ((t56) < (t57));
  t59 = ((int64_t)(((int64_t)UINT64_C(4))));
  fg160_gg_function_7(t58, t59, base + 0);
  t60 = ((int64_t)(v3));
  t61 = ((int64_t)(((int64_t)UINT64_C(0))));
  t62 = ((t60) < (t61));
  t63 = t62;
  if (!t63) {
  t64 = ((int64_t)(v5));
  t65 = ((int64_t)(((int64_t)UINT64_C(0))));
  t63 = ((t64) < (t65));
  }
  t66 = t63;
  t67 = t66;
  if (!t67) {
  t68 = ((int64_t)(v3));
  t69 = ((int64_t)(v5));
  t70 = ((int64_t)(((int64_t)((uint64_t)(t68) + (uint64_t)(t69)))));
  t71 = ((int64_t)(v2));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  t73 = fg160_graph(t71, t72);
  t74 = ((int64_t)(t73));
  t67 = ((t70) > (t74));
  }
  t75 = t67;
  if (t75) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg160_gg_done;
  }
  }
  t76 = ((int64_t)(v2));
  t77 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = ((int64_t)(v3));
  t79 = ((int64_t)(v4));
  t80 = ((int64_t)(v5));
  t81 = fg160_gg_function_16(t76, t77, t78, t79, t80, base + 0);
  t82 = t81;
  if (t82) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  goto fg160_gg_done;
  }
  }
  t83 = ((int64_t)(v0));
  t84 = ((int64_t)(((int64_t)UINT64_C(4))));
  t85 = ((t83) == (t84));
  if (t85) {
  {
  t86 = ((int64_t)(v2));
  t87 = ((int64_t)(v3));
  t88 = ((int64_t)(((int64_t)UINT64_C(0))));
  t89 = ((int64_t)(v5));
  t90 = ((int64_t)(v4));
  t91 = ((int64_t)(v6));
  t92 = ((int64_t)(v7));
  t93 = fg160_gg_function_21(t86, t87, t88, t89, t90, t91, t92, base + 0);
  t94 = t93;
  if (t94) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  goto fg160_gg_done;
  }
  }
  t95 = ((int64_t)(v0));
  t96 = ((int64_t)(((int64_t)UINT64_C(5))));
  t97 = ((t95) == (t96));
  if (t97) {
  {
  t98 = ((int64_t)(v4));
  t99 = ((int64_t)(((int64_t)UINT64_C(32))));
  t100 = ((t98) < (t99));
  t101 = ((int64_t)(((int64_t)UINT64_C(5))));
  fg160_gg_function_7(t100, t101, base + 0);
  t102 = ((int64_t)(v3));
  t103 = ((int64_t)(((int64_t)UINT64_C(0))));
  t104 = ((t102) < (t103));
  t105 = t104;
  if (!t105) {
  t106 = ((int64_t)(v4));
  t107 = ((int64_t)(((int64_t)UINT64_C(0))));
  t105 = ((t106) < (t107));
  }
  t108 = t105;
  t109 = t108;
  if (!t109) {
  t110 = ((int64_t)(v3));
  t111 = ((int64_t)(v4));
  t112 = ((int64_t)(((int64_t)((uint64_t)(t110) + (uint64_t)(t111)))));
  t113 = ((int64_t)(v2));
  t114 = ((int64_t)(((int64_t)UINT64_C(0))));
  t115 = fg160_graph(t113, t114);
  t116 = ((int64_t)(t115));
  t109 = ((t112) > (t116));
  }
  t117 = t109;
  if (t117) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg160_gg_done;
  }
  }
  t118 = ((int64_t)(v2));
  t119 = ((int64_t)(((int64_t)UINT64_C(0))));
  t120 = ((int64_t)(v3));
  t121 = ((int64_t)(v5));
  t122 = ((int64_t)(v4));
  t123 = ((int64_t)(v6));
  t124 = ((int64_t)(v7));
  t125 = fg160_gg_function_21(t118, t119, t120, t121, t122, t123, t124, base + 0);
  t126 = t125;
  if (t126) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  goto fg160_gg_done;
  }
  }
  t127 = ((int64_t)(v0));
  t128 = ((int64_t)(((int64_t)UINT64_C(6))));
  t129 = ((t127) == (t128));
  if (t129) {
  {
  t130 = ((int64_t)(v5));
  t131 = ((int64_t)(((int64_t)UINT64_C(0))));
  t132 = ((t130) < (t131));
  if (t132) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg160_gg_done;
  }
  }
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t133 = ((int64_t)(v9));
  t134 = ((int64_t)(v5));
  if (!(((t133) < (t134)))) goto label2;
  {
  t135 = ((int64_t)(v4));
  t136 = ((int64_t)(v9));
  t137 = ((int64_t)(((int64_t)UINT64_C(3))));
  t138 = ((int64_t)(((int64_t)((uint64_t)(t136) * (uint64_t)(t137)))));
  t139 = fg160_i32(t135, t138);
  v10 = ((int64_t)(t139));
  t140 = ((int64_t)(v4));
  t141 = ((int64_t)(v9));
  t142 = ((int64_t)(((int64_t)UINT64_C(3))));
  t143 = ((int64_t)(((int64_t)((uint64_t)(t141) * (uint64_t)(t142)))));
  t144 = ((int64_t)(((int64_t)UINT64_C(1))));
  t145 = ((int64_t)(((int64_t)((uint64_t)(t143) + (uint64_t)(t144)))));
  t146 = fg160_i32(t140, t145);
  v11 = ((int64_t)(t146));
  t147 = ((int64_t)(v4));
  t148 = ((int64_t)(v9));
  t149 = ((int64_t)(((int64_t)UINT64_C(3))));
  t150 = ((int64_t)(((int64_t)((uint64_t)(t148) * (uint64_t)(t149)))));
  t151 = ((int64_t)(((int64_t)UINT64_C(2))));
  t152 = ((int64_t)(((int64_t)((uint64_t)(t150) + (uint64_t)(t151)))));
  t153 = fg160_i32(t147, t152);
  v12 = ((int64_t)(t153));
  t154 = ((int64_t)(v3));
  t155 = ((int64_t)(v10));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t154) + (uint64_t)(t155)))));
  t156 = ((int64_t)(v3));
  t157 = ((int64_t)(v12));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t156) + (uint64_t)(t157)))));
  t158 = ((int64_t)(v2));
  t159 = ((int64_t)(v13));
  t160 = fg160_gg_function_5(t158, t159, base + 0);
  t161 = t160;
  t162 = (!(t161));
  t163 = t162;
  if (!t163) {
  t164 = ((int64_t)(v2));
  t165 = ((int64_t)(v14));
  t166 = fg160_gg_function_5(t164, t165, base + 0);
  t167 = t166;
  t163 = (!(t167));
  }
  t168 = t163;
  t169 = t168;
  if (!t169) {
  t170 = ((int64_t)(v11));
  t171 = ((int64_t)(((int64_t)UINT64_C(0))));
  t169 = ((t170) < (t171));
  }
  t172 = t169;
  t173 = t172;
  if (!t173) {
  t174 = ((int64_t)(v11));
  t175 = fg160_gg_function_23(base + 0);
  t176 = ((int64_t)(((int64_t)(t175))));
  t173 = ((t174) >= (t176));
  }
  t177 = t173;
  if (t177) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg160_gg_done;
  }
  }
  t178 = ((int64_t)(v2));
  t179 = ((int64_t)(v13));
  t180 = fg160_node(t178, t179);
  t181 = ((int64_t)(t180));
  t182 = ((int64_t)(((int64_t)UINT64_C(5))));
  t183 = ((int64_t)(v11));
  t184 = fg160_tensor(t181, t182, t183);
  t185 = ((uint64_t)(t184));
  t186 = ((int64_t)(v2));
  t187 = ((int64_t)(v14));
  t188 = fg160_node(t186, t187);
  t189 = ((uint64_t)(((uint64_t)(t188))));
  t190 = ((t185) != (t189));
  if (t190) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg160_gg_done;
  }
  }
  t191 = v9;
  t192 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)((uint64_t)(t191) + (uint64_t)(t192)));
  }
  goto label1;
label2:;
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  goto fg160_gg_done;
  }
  }
  t193 = ((int64_t)(v0));
  t194 = ((int64_t)(((int64_t)UINT64_C(7))));
  t195 = ((t193) == (t194));
  if (t195) {
  {
  t196 = ((int64_t)(v2));
  t197 = ((int64_t)(((int64_t)UINT64_C(0))));
  t198 = ((t196) != (t197));
  t199 = t198;
  if (t199) {
  t200 = ((int64_t)(v3));
  t201 = ((int64_t)(((int64_t)UINT64_C(0))));
  t199 = ((t200) != (t201));
  }
  t202 = t199;
  t203 = ((int64_t)(((int64_t)UINT64_C(6))));
  fg160_gg_function_7(t202, t203, base + 0);
  t204 = ((int64_t)(v2));
  t205 = ((int64_t)(v3));
  t206 = ((int64_t)(v4));
  t207 = ((int64_t)(v5));
  t208 = ((int64_t)(((int64_t)UINT64_C(0))));
  t209 = fg160_gg_function_22(t204, t205, t206, t207, t208, base + 0);
  t0 = ((uint64_t)(((uint64_t)(t209))));
  goto fg160_gg_done;
  }
  }
  t210 = ((int64_t)(v0));
  t211 = ((int64_t)(((int64_t)UINT64_C(8))));
  t212 = ((t210) == (t211));
  if (t212) {
  {
  t213 = ((int64_t)(v2));
  t214 = fg160_gg_function_24(t213, base + 0);
  t215 = t214;
  if (t215) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  goto fg160_gg_done;
  }
  }
  t216 = ((int64_t)(v0));
  t217 = ((int64_t)(((int64_t)UINT64_C(9))));
  t218 = ((t216) == (t217));
  if (t218) {
  {
  t219 = ((int64_t)(v2));
  t220 = ((int64_t)(((int64_t)UINT64_C(0))));
  t221 = ((t219) != (t220));
  t222 = t221;
  if (t222) {
  t223 = ((int64_t)(v1));
  t224 = ((int64_t)(((int64_t)UINT64_C(2))));
  t225 = fg160_arg(t223, t224);
  t226 = ((uint64_t)(t225));
  t227 = fg160_gg_function_42(base + 0);
  t228 = ((uint64_t)(t227));
  t222 = ((t226) <= (t228));
  }
  t229 = t222;
  t230 = t229;
  if (t230) {
  t231 = ((int64_t)(v1));
  t232 = ((int64_t)(((int64_t)UINT64_C(2))));
  t233 = fg160_arg(t231, t232);
  t234 = ((uint64_t)(t233));
  t235 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t236 = ((t234) == (t235));
  t237 = t236;
  if (!t237) {
  t238 = ((int64_t)(v3));
  t239 = ((int64_t)(((int64_t)UINT64_C(0))));
  t237 = ((t238) != (t239));
  }
  t230 = t237;
  }
  t240 = t230;
  t241 = ((int64_t)(((int64_t)UINT64_C(7))));
  fg160_gg_function_7(t240, t241, base + 0);
  t242 = ((int64_t)(v1));
  t243 = ((int64_t)(((int64_t)UINT64_C(2))));
  t244 = fg160_arg(t242, t243);
  t245 = ((uint64_t)(t244));
  t246 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t247 = ((t245) > (t246));
  if (t247) {
  {
  t248 = ((int64_t)(v2));
  t249 = ((int64_t)(v3));
  t250 = ((int64_t)(v1));
  t251 = ((int64_t)(((int64_t)UINT64_C(2))));
  t252 = fg160_arg(t250, t251);
  t253 = ((uint64_t)(t252));
  fg160_param_copy(t248, t249, t253);
  }
  }
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg160_gg_done;
  }
  }
  t254 = ((int64_t)(v0));
  t255 = ((int64_t)(((int64_t)UINT64_C(10))));
  t256 = ((t254) == (t255));
  t257 = t256;
  if (!t257) {
  t258 = ((int64_t)(v0));
  t259 = ((int64_t)(((int64_t)UINT64_C(11))));
  t257 = ((t258) == (t259));
  }
  t260 = t257;
  if (t260) {
  {
  t261 = ((int64_t)(v2));
  t262 = ((int64_t)(((int64_t)UINT64_C(0))));
  t263 = ((t261) != (t262));
  t264 = t263;
  if (t264) {
  t265 = ((int64_t)(v1));
  t266 = ((int64_t)(((int64_t)UINT64_C(1))));
  t267 = fg160_arg(t265, t266);
  t268 = ((uint64_t)(t267));
  t269 = fg160_gg_function_42(base + 0);
  t270 = ((uint64_t)(t269));
  t271 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t272 = ((uint64_t)(((uint64_t)fg160_gg_udiv(t270, t271))));
  t264 = ((t268) < (t272));
  }
  t273 = t264;
  t274 = ((int64_t)(((int64_t)UINT64_C(8))));
  fg160_gg_function_7(t273, t274, base + 0);
  t275 = ((int64_t)(v2));
  t276 = ((int64_t)(v1));
  t277 = ((int64_t)(((int64_t)UINT64_C(1))));
  t278 = fg160_arg(t276, t277);
  t279 = ((uint64_t)(t278));
  t280 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t281 = ((uint64_t)(((uint64_t)((uint64_t)(t279) * (uint64_t)(t280)))));
  t282 = fg160_param_read(t275, t281);
  t0 = ((uint64_t)(t282));
  goto fg160_gg_done;
  }
  }
  t283 = ((int64_t)(v0));
  t284 = ((int64_t)(((int64_t)UINT64_C(12))));
  t285 = ((t283) == (t284));
  t286 = t285;
  if (!t286) {
  t287 = ((int64_t)(v0));
  t288 = ((int64_t)(((int64_t)UINT64_C(13))));
  t286 = ((t287) == (t288));
  }
  t289 = t286;
  if (t289) {
  {
  t290 = ((int64_t)(v2));
  t291 = ((int64_t)(((int64_t)UINT64_C(0))));
  t292 = ((t290) != (t291));
  t293 = t292;
  if (t293) {
  t294 = ((int64_t)(v1));
  t295 = ((int64_t)(((int64_t)UINT64_C(1))));
  t296 = fg160_arg(t294, t295);
  t297 = ((uint64_t)(t296));
  t298 = fg160_gg_function_42(base + 0);
  t299 = ((uint64_t)(t298));
  t300 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t301 = ((uint64_t)(((uint64_t)fg160_gg_udiv(t299, t300))));
  t293 = ((t297) < (t301));
  }
  t302 = t293;
  t303 = ((int64_t)(((int64_t)UINT64_C(9))));
  fg160_gg_function_7(t302, t303, base + 0);
  t304 = ((int64_t)(v2));
  t305 = ((int64_t)(v1));
  t306 = ((int64_t)(((int64_t)UINT64_C(1))));
  t307 = fg160_arg(t305, t306);
  t308 = ((uint64_t)(t307));
  t309 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t310 = ((uint64_t)(((uint64_t)((uint64_t)(t308) * (uint64_t)(t309)))));
  t311 = ((int64_t)(v1));
  t312 = ((int64_t)(((int64_t)UINT64_C(2))));
  t313 = fg160_arg(t311, t312);
  t314 = ((uint64_t)(t313));
  fg160_param_write(t304, t310, t314);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg160_gg_done;
  }
  }
  t315 = ((int64_t)(v0));
  t316 = ((int64_t)(((int64_t)UINT64_C(14))));
  t317 = ((t315) == (t316));
  if (t317) {
  {
  t318 = ((int64_t)(v2));
  t319 = ((int64_t)(((int64_t)UINT64_C(0))));
  t320 = ((int64_t)(((int64_t)UINT64_C(0))));
  t321 = fg160_tensor(t318, t319, t320);
  v15 = ((uint64_t)(t321));
  t322 = ((uint64_t)(v15));
  t323 = fg160_gg_function_32(base + 0);
  t324 = ((uint64_t)(t323));
  t325 = ((t322) == (t324));
  t326 = t325;
  if (!t326) {
  t327 = ((uint64_t)(v15));
  t328 = fg160_gg_function_33(base + 0);
  t329 = ((uint64_t)(t328));
  t326 = ((t327) == (t329));
  }
  t330 = t326;
  t331 = t330;
  if (t331) {
  t332 = ((int64_t)(v3));
  t333 = ((int64_t)(((int64_t)UINT64_C(0))));
  t331 = ((t332) >= (t333));
  }
  t334 = t331;
  t335 = t334;
  if (t335) {
  t336 = ((int64_t)(v2));
  t337 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  t338 = fg160_param_read(t336, t337);
  t339 = ((uint64_t)(t338));
  t340 = fg160_gg_function_34(base + 0);
  t341 = ((uint64_t)(t340));
  t335 = ((t339) != (t341));
  }
  t342 = t335;
  t343 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg160_gg_function_7(t342, t343, base + 0);
  t344 = ((int64_t)(v2));
  t345 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(60))))));
  t346 = ((int64_t)(v1));
  t347 = ((int64_t)(((int64_t)UINT64_C(1))));
  t348 = fg160_arg(t346, t347);
  t349 = ((uint64_t)(t348));
  fg160_param_write(t344, t345, t349);
  t0 = ((uint64_t)(((uint64_t)(v2))));
  goto fg160_gg_done;
  }
  }
  t350 = ((int64_t)(v0));
  t351 = ((int64_t)(((int64_t)UINT64_C(15))));
  t352 = ((t350) == (t351));
  if (t352) {
  {
  t353 = ((int64_t)(v2));
  t354 = ((int64_t)(v3));
  t355 = ((int64_t)(v4));
  t357 = ((int64_t)(v8));
  t358 = ((int64_t)(((int64_t)UINT64_C(0))));
  t359 = ((t357) == (t358));
  if (t359) {
  {
  t360 = fg160_gg_function_35(base + 0);
  t356 = ((int64_t)(((int64_t)(t360))));
  }
  } else {
  {
  t361 = fg160_gg_function_36(base + 0);
  t356 = ((int64_t)(((int64_t)(t361))));
  }
  }
  t362 = ((int64_t)(t356));
  t363 = fg160_glu(t353, t354, t355, t362);
  v16 = ((int64_t)(t363));
  t364 = ((int64_t)(v8));
  t365 = ((int64_t)(((int64_t)UINT64_C(0))));
  t366 = ((t364) == (t365));
  if (t366) {
  {
  t367 = ((int64_t)(v16));
  t368 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  t369 = ((int64_t)(v1));
  t370 = ((int64_t)(((int64_t)UINT64_C(3))));
  t371 = fg160_arg(t369, t370);
  t372 = ((uint64_t)(t371));
  fg160_param_write(t367, t368, t372);
  }
  }
  t373 = ((int64_t)(v16));
  t374 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(12))))));
  t375 = ((int64_t)(v1));
  t376 = ((int64_t)(((int64_t)UINT64_C(4))));
  t377 = fg160_arg(t375, t376);
  t378 = ((uint64_t)(t377));
  fg160_param_write(t373, t374, t378);
  t0 = ((uint64_t)(((uint64_t)(v16))));
  goto fg160_gg_done;
  }
  }
  t379 = ((int64_t)(v0));
  t380 = ((int64_t)(((int64_t)UINT64_C(16))));
  t381 = ((t379) == (t380));
  if (t381) {
  {
  t382 = ((int64_t)(v4));
  t383 = ((int64_t)(((int64_t)UINT64_C(0))));
  t384 = ((t382) >= (t383));
  t385 = t384;
  if (t385) {
  t386 = ((uint64_t)(((uint64_t)(v4))));
  t387 = ((int64_t)(v3));
  t388 = ((int64_t)(((int64_t)UINT64_C(3))));
  t389 = ((int64_t)(((int64_t)UINT64_C(0))));
  t390 = fg160_tensor(t387, t388, t389);
  t391 = ((uint64_t)(t390));
  t385 = ((t386) <= (t391));
  }
  t392 = t385;
  t393 = ((int64_t)(((int64_t)UINT64_C(11))));
  fg160_gg_function_7(t392, t393, base + 0);
  t394 = ((int64_t)(v2));
  t395 = ((int64_t)(v3));
  t396 = fg160_argsort(t394, t395);
  v17 = ((int64_t)(t396));
  t397 = ((int64_t)(v2));
  t398 = ((int64_t)(v17));
  t399 = ((int64_t)(v4));
  t400 = ((int64_t)(v17));
  t401 = ((int64_t)(((int64_t)UINT64_C(3))));
  t402 = ((int64_t)(((int64_t)UINT64_C(1))));
  t403 = fg160_tensor(t400, t401, t402);
  t404 = ((uint64_t)(t403));
  t405 = fg160_gg_function_3(t404, base + 0);
  t406 = ((int64_t)(v17));
  t407 = ((int64_t)(((int64_t)UINT64_C(3))));
  t408 = ((int64_t)(((int64_t)UINT64_C(2))));
  t409 = fg160_tensor(t406, t407, t408);
  t410 = ((uint64_t)(t409));
  t411 = fg160_gg_function_3(t410, base + 0);
  t412 = ((int64_t)(v17));
  t413 = ((int64_t)(((int64_t)UINT64_C(3))));
  t414 = ((int64_t)(((int64_t)UINT64_C(3))));
  t415 = fg160_tensor(t412, t413, t414);
  t416 = ((uint64_t)(t415));
  t417 = fg160_gg_function_3(t416, base + 0);
  t418 = ((int64_t)(v17));
  t419 = ((int64_t)(((int64_t)UINT64_C(6))));
  t420 = ((int64_t)(((int64_t)UINT64_C(1))));
  t421 = fg160_tensor(t418, t419, t420);
  t422 = ((uint64_t)(t421));
  t423 = ((int64_t)(v17));
  t424 = ((int64_t)(((int64_t)UINT64_C(6))));
  t425 = ((int64_t)(((int64_t)UINT64_C(2))));
  t426 = fg160_tensor(t423, t424, t425);
  t427 = ((uint64_t)(t426));
  t428 = ((int64_t)(v17));
  t429 = ((int64_t)(((int64_t)UINT64_C(6))));
  t430 = ((int64_t)(((int64_t)UINT64_C(3))));
  t431 = fg160_tensor(t428, t429, t430);
  t432 = ((uint64_t)(t431));
  t433 = fg160_view(t397, t398, t399, t405, t411, t417, t422, t427, t432);
  t0 = ((uint64_t)(((uint64_t)(t433))));
  goto fg160_gg_done;
  }
  }
  t434 = ((int64_t)(v0));
  t435 = ((int64_t)(((int64_t)UINT64_C(17))));
  t436 = ((t434) == (t435));
  if (t436) {
  {
  t437 = ((int64_t)(v3));
  t438 = ((int64_t)(((int64_t)UINT64_C(3))));
  t439 = ((int64_t)(((int64_t)UINT64_C(0))));
  t440 = fg160_tensor(t437, t438, t439);
  t441 = ((uint64_t)(t440));
  t442 = fg160_gg_function_3(t441, base + 0);
  t443 = ((int64_t)(v4));
  t444 = ((int64_t)(((int64_t)UINT64_C(0))));
  t445 = fg160_i32(t443, t444);
  t446 = ((int64_t)(t445));
  t447 = fg160_gg_function_40(t442, t446, base + 0);
  t448 = ((int64_t)(v4));
  t449 = ((int64_t)(((int64_t)UINT64_C(1))));
  t450 = fg160_i32(t448, t449);
  t451 = ((int64_t)(t450));
  v18 = fg160_gg_function_40(t447, t451, base + 0);
  t452 = ((int64_t)(v3));
  t453 = ((int64_t)(((int64_t)UINT64_C(3))));
  t454 = ((int64_t)(((int64_t)UINT64_C(1))));
  t455 = fg160_tensor(t452, t453, t454);
  t456 = ((uint64_t)(t455));
  t457 = fg160_gg_function_3(t456, base + 0);
  t458 = ((int64_t)(v4));
  t459 = ((int64_t)(((int64_t)UINT64_C(2))));
  t460 = fg160_i32(t458, t459);
  t461 = ((int64_t)(t460));
  t462 = fg160_gg_function_40(t457, t461, base + 0);
  t463 = ((int64_t)(v4));
  t464 = ((int64_t)(((int64_t)UINT64_C(3))));
  t465 = fg160_i32(t463, t464);
  t466 = ((int64_t)(t465));
  v19 = fg160_gg_function_40(t462, t466, base + 0);
  t467 = ((int64_t)(v3));
  t468 = ((int64_t)(((int64_t)UINT64_C(3))));
  t469 = ((int64_t)(((int64_t)UINT64_C(2))));
  t470 = fg160_tensor(t467, t468, t469);
  t471 = ((uint64_t)(t470));
  t472 = fg160_gg_function_3(t471, base + 0);
  t473 = ((int64_t)(v4));
  t474 = ((int64_t)(((int64_t)UINT64_C(4))));
  t475 = fg160_i32(t473, t474);
  t476 = ((int64_t)(t475));
  t477 = fg160_gg_function_40(t472, t476, base + 0);
  t478 = ((int64_t)(v4));
  t479 = ((int64_t)(((int64_t)UINT64_C(5))));
  t480 = fg160_i32(t478, t479);
  t481 = ((int64_t)(t480));
  v20 = fg160_gg_function_40(t477, t481, base + 0);
  t482 = ((int64_t)(v3));
  t483 = ((int64_t)(((int64_t)UINT64_C(3))));
  t484 = ((int64_t)(((int64_t)UINT64_C(3))));
  t485 = fg160_tensor(t482, t483, t484);
  t486 = ((uint64_t)(t485));
  t487 = fg160_gg_function_3(t486, base + 0);
  t488 = ((int64_t)(v4));
  t489 = ((int64_t)(((int64_t)UINT64_C(6))));
  t490 = fg160_i32(t488, t489);
  t491 = ((int64_t)(t490));
  t492 = fg160_gg_function_40(t487, t491, base + 0);
  t493 = ((int64_t)(v4));
  t494 = ((int64_t)(((int64_t)UINT64_C(7))));
  t495 = fg160_i32(t493, t494);
  t496 = ((int64_t)(t495));
  v21 = fg160_gg_function_40(t492, t496, base + 0);
  t497 = ((int64_t)(v18));
  t498 = ((int64_t)(((int64_t)UINT64_C(0))));
  t499 = ((t497) >= (t498));
  t500 = t499;
  if (t500) {
  t501 = ((int64_t)(v19));
  t502 = ((int64_t)(((int64_t)UINT64_C(0))));
  t500 = ((t501) >= (t502));
  }
  t503 = t500;
  t504 = t503;
  if (t504) {
  t505 = ((int64_t)(v20));
  t506 = ((int64_t)(((int64_t)UINT64_C(0))));
  t504 = ((t505) >= (t506));
  }
  t507 = t504;
  t508 = t507;
  if (t508) {
  t509 = ((int64_t)(v21));
  t510 = ((int64_t)(((int64_t)UINT64_C(0))));
  t508 = ((t509) >= (t510));
  }
  t511 = t508;
  t512 = ((int64_t)(((int64_t)UINT64_C(12))));
  fg160_gg_function_7(t511, t512, base + 0);
  t513 = ((int64_t)(v2));
  t514 = ((int64_t)(v3));
  t515 = ((int64_t)(((int64_t)UINT64_C(7))));
  t516 = ((int64_t)(((int64_t)UINT64_C(0))));
  t517 = fg160_tensor(t514, t515, t516);
  t518 = ((uint64_t)(t517));
  t519 = fg160_gg_function_3(t518, base + 0);
  t520 = ((int64_t)(v18));
  t521 = ((int64_t)(v19));
  t522 = ((int64_t)(v20));
  t523 = ((int64_t)(v21));
  t524 = fg160_new(t513, t519, t520, t521, t522, t523);
  v22 = ((int64_t)(t524));
  v23 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t525 = ((int64_t)(v23));
  t526 = ((int64_t)(((int64_t)UINT64_C(8))));
  if (!(((t525) < (t526)))) goto label5;
  {
  t527 = ((int64_t)(v22));
  t528 = ((int64_t)(v23));
  t529 = ((int64_t)(((int64_t)UINT64_C(4))));
  t530 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t528) * (uint64_t)(t529)))))));
  t531 = ((int64_t)(v4));
  t532 = ((int64_t)(v23));
  t533 = fg160_i32(t531, t532);
  t534 = ((uint64_t)(((uint64_t)(t533))));
  fg160_param_write(t527, t530, t534);
  t535 = v23;
  t536 = ((int64_t)(((int64_t)UINT64_C(1))));
  v23 = ((int64_t)((uint64_t)(t535) + (uint64_t)(t536)));
  }
  goto label4;
label5:;
  t537 = ((int64_t)(v22));
  t538 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(32))))));
  t540 = ((int64_t)(v5));
  t541 = ((int64_t)(((int64_t)UINT64_C(0))));
  t542 = ((t540) != (t541));
  if (t542) {
  {
  t539 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t539 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t543 = ((uint64_t)(t539));
  fg160_param_write(t537, t538, t543);
  t544 = ((int64_t)(v22));
  t545 = fg160_gg_function_42(base + 0);
  t546 = ((int64_t)(((int64_t)(t545))));
  t547 = ((int64_t)(v3));
  fg160_op_src(t544, t546, t547);
  t0 = ((uint64_t)(((uint64_t)(v22))));
  goto fg160_gg_done;
  }
  }
  t548 = ((int64_t)(((int64_t)UINT64_C(99))));
  fg160_fail(t548);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto fg160_gg_done;
  }
fg160_gg_done:;
  return t0;
}
static void fg160_gg_function_0(int64_t base) {
  fg160_gg_header t0;
  int64_t t1;
  int64_t t2;
  uint64_t t3;
  int64_t t4;
  {
  t0.base = fg160_gg_get_stack(base + 0)->base + fg160_gg_get_stack(base + 0)->top; t0.len = 0;
  t1 = ((int64_t)(((int64_t)UINT64_C(99))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg160_gg_function_1(t1, t2, base + 1);
  fg160_gg_unsigned(fg160_gg_ref(&t0, fg160_gg_get_stack(base + 0)), t3);
  fg160_gg_append(fg160_gg_ref(&t0, fg160_gg_get_stack(base + 0)), "\n", 1, 1);
  fg160_gg_write(stdout, fg160_gg_view((t0).base, (t0).len));
  }
fg160_gg_done:;
  for (t4 = 0; t4 < 1; t4++) fg160_gg_stacks[base + t4].top = 0;
}
static int fg160_gg_program(int argc, char **argv) {
  struct { int64_t base; } frame;
  frame.base = 0;
  fg160_gg_argc = argc; fg160_gg_argv = argv;
  fg160_gg_function_0(frame.base + 0);
  return 0;
}
int main(int argc, char **argv) {
#ifdef _MSC_VER
  __try { return fg160_gg_program(argc, argv); }
  __except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    _resetstkoflw(); fg160_gg_fail("native call stack overflow"); return 1;
  }
#else
  return fg160_gg_program(argc, argv);
#endif
}
