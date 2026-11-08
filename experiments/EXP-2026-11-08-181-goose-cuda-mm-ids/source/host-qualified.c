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
typedef struct { void *data; int64_t len; } fg181ms_gg_slice;
typedef struct { unsigned char *base; int64_t len; } fg181ms_gg_header;
typedef struct { unsigned char *base; size_t top, committed; } fg181ms_gg_stack;
typedef struct { fg181ms_gg_header *header; fg181ms_gg_stack *stack; } fg181ms_gg_reference;
#define GG_RESERVATION ((size_t)268435456)
#define GG_STACK_COUNT 8192
static fg181ms_gg_stack fg181ms_gg_stacks[GG_STACK_COUNT];
static int fg181ms_gg_argc;
static char **fg181ms_gg_argv;
static void fg181ms_gg_fail(const char *message) {
    fprintf(stderr, "goose runtime error: %s\n", message); exit(1);
}
static fg181ms_gg_stack *fg181ms_gg_get_stack(int64_t id) {
    fg181ms_gg_stack *s;
    if (id < 0 || id >= GG_STACK_COUNT) fg181ms_gg_fail("data stack limit exceeded");
    s = &fg181ms_gg_stacks[id];
    if (!s->base) {
#ifdef _WIN32
        s->base = VirtualAlloc(NULL, GG_RESERVATION + 65536, MEM_RESERVE, PAGE_NOACCESS);
#else
        s->base = mmap(NULL, GG_RESERVATION + 65536, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (s->base == MAP_FAILED) s->base = NULL;
#endif
        if (!s->base) fg181ms_gg_fail("cannot reserve data stack");
    }
    return s;
}
static void *fg181ms_gg_allocate(fg181ms_gg_stack *s, size_t size) {
    size_t end, committed;
    void *result;
    if (size > GG_RESERVATION - s->top) fg181ms_gg_fail("data stack reservation exceeded");
    result = s->base + s->top; end = s->top + size;
    if (end > s->committed) {
        committed = (end + 65535) & ~(size_t)65535;
#ifdef _WIN32
        if (!VirtualAlloc(s->base + s->committed, committed - s->committed, MEM_COMMIT, PAGE_READWRITE)) fg181ms_gg_fail("cannot commit data stack");
#else
        if (mprotect(s->base + s->committed, committed - s->committed, PROT_READ | PROT_WRITE)) fg181ms_gg_fail("cannot commit data stack");
#endif
        s->committed = committed;
    }
    s->top = end; return result;
}
static fg181ms_gg_reference fg181ms_gg_ref(fg181ms_gg_header *h, fg181ms_gg_stack *s) { fg181ms_gg_reference r; r.header=h; r.stack=s; return r; }
static fg181ms_gg_slice fg181ms_gg_view(void *p, int64_t n) { fg181ms_gg_slice v; v.data=p; v.len=n; return v; }
static fg181ms_gg_slice fg181ms_gg_byte_view(fg181ms_gg_slice a, size_t width) {
    if (a.len < 0 || (width && (uint64_t)a.len > (uint64_t)INT64_MAX / width)) fg181ms_gg_fail("byte view size overflow");
    return fg181ms_gg_view(a.data, (int64_t)((uint64_t)a.len * width));
}
static fg181ms_gg_slice fg181ms_gg_sub(fg181ms_gg_slice a, int64_t lo, int64_t hi, size_t element) {
    if (lo < 0 || hi < lo || hi > a.len) fg181ms_gg_fail("slice out of bounds");
    return fg181ms_gg_view((unsigned char *)a.data + (size_t)lo * element, hi - lo);
}
static int64_t fg181ms_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg181ms_gg_fail("array index out of bounds"); return i; }
static void fg181ms_gg_append(fg181ms_gg_reference a, const void *p, int64_t n, size_t element) {
    void *to;
    if (n < 0 || (uint64_t)n > GG_RESERVATION / (element ? element : 1)) fg181ms_gg_fail("invalid array size");
    to=fg181ms_gg_allocate(a.stack, (size_t)n * element);
    if (n && element) memmove(to, p, (size_t)n * element);
    a.header->len += n;
}
static void fg181ms_gg_resize(fg181ms_gg_reference a, int64_t n, size_t element, const void *fill) {
    int64_t old=a.header->len, i;
    if (n < 0) fg181ms_gg_fail("negative array length");
    if (n > old) {
        unsigned char *p;
        if (!fill) fg181ms_gg_fail("resize without fill may only shrink");
        if ((uint64_t)(n-old) > GG_RESERVATION / (element ? element : 1)) fg181ms_gg_fail("invalid array size");
        p=fg181ms_gg_allocate(a.stack, (size_t)(n-old)*element);
        for (i=0; i<n-old; i++) if(element) memcpy(p+(size_t)i*element, fill, element);
    } else { a.stack->top = (size_t)(a.header->base-a.stack->base) + (size_t)n*element; }
    a.header->len=n;
}
static int64_t fg181ms_gg_div(int64_t a, int64_t b) { if (!b) fg181ms_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg181ms_gg_fail("division overflow"); return a/b; }
static int64_t fg181ms_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg181ms_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static uint64_t fg181ms_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg181ms_gg_fail("division by zero");return a/b; }
static uint64_t fg181ms_gg_umod(uint64_t a,uint64_t b) { if(!b)fg181ms_gg_fail("division by zero");return a%b; }
static int fg181ms_gg_equal(fg181ms_gg_slice a, fg181ms_gg_slice b, size_t element) { return a.len==b.len && (!a.len || !memcmp(a.data,b.data,(size_t)a.len*element)); }
static void fg181ms_gg_text(fg181ms_gg_reference out, fg181ms_gg_slice v) { fg181ms_gg_append(out,v.data,v.len,1); }
static void fg181ms_gg_integer(fg181ms_gg_reference out,int64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%lld",(long long)n);fg181ms_gg_append(out,b,k,1); }
static void fg181ms_gg_unsigned(fg181ms_gg_reference out,uint64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%llu",(unsigned long long)n);fg181ms_gg_append(out,b,k,1); }
static double fg181ms_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static int64_t ggf_fmt_f64(uint8_t *out,double n);
static int64_t ggf_fmt_f32(uint8_t *out,float n);
static void fg181ms_gg_float(fg181ms_gg_reference out,double n) { uint8_t b[64];int64_t k=ggf_fmt_f64(b,n);fg181ms_gg_append(out,b,k,1); }
static void fg181ms_gg_float32(fg181ms_gg_reference out,float n) { uint8_t b[64];int64_t k=ggf_fmt_f32(b,n);fg181ms_gg_append(out,b,k,1); }
static void fg181ms_gg_bool(fg181ms_gg_reference out,uint8_t n) { fg181ms_gg_append(out,n?"true":"false",n?4:5,1); }
static void fg181ms_gg_write(FILE *file,fg181ms_gg_slice text) { if(text.len && fwrite(text.data,1,(size_t)text.len,file)!=(size_t)text.len)fg181ms_gg_fail("write failed"); }
static char *fg181ms_gg_cstring(fg181ms_gg_slice path) {
    char *s=malloc((size_t)path.len+1); if(!s)fg181ms_gg_fail("out of memory in file shim");
    if(path.len)memcpy(s,path.data,(size_t)path.len);s[path.len]=0;return s;
}
static uint8_t gs_os_read_file(fg181ms_gg_slice path,fg181ms_gg_reference out) {
    char *p=fg181ms_gg_cstring(path);FILE *f=fopen(p,"rb");unsigned char b[16384];size_t n;int ok;free(p);
    if(!f)return 0;while((n=fread(b,1,sizeof b,f))!=0)fg181ms_gg_append(out,b,(int64_t)n,1);ok=!ferror(f);fclose(f);return (uint8_t)ok;
}
static uint8_t gs_os_write_file(fg181ms_gg_slice path,fg181ms_gg_slice data) {
    char *p=fg181ms_gg_cstring(path);FILE *f=fopen(p,"wb");int ok;free(p);if(!f)return 0;
    ok=!data.len || fwrite(data.data,1,(size_t)data.len,f)==(size_t)data.len;if(fclose(f))ok=0;return (uint8_t)ok;
}
static uint8_t gs_os_file_exists(fg181ms_gg_slice path) { char*p=fg181ms_gg_cstring(path);FILE*f=fopen(p,"rb");free(p);if(!f)return 0;fclose(f);return 1; }
static void gs_os_write_stdout(fg181ms_gg_slice s) { fg181ms_gg_write(stdout,s); }
static void gs_os_write_stderr(fg181ms_gg_slice s) { fg181ms_gg_write(stderr,s); }
static void gs_os_flush_stdout(void) { fflush(stdout); }
static int64_t gs_os_arg_count(void) { return fg181ms_gg_argc; }
static void gs_os_arg(int64_t i,fg181ms_gg_reference out) { if(i>=0 && i<fg181ms_gg_argc)fg181ms_gg_append(out,fg181ms_gg_argv[i],(int64_t)strlen(fg181ms_gg_argv[i]),1); }
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


static void fg181ms_gg_quote(fg181ms_gg_reference out,fg181ms_gg_slice text) { int64_t i; unsigned char c; fg181ms_gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){fg181ms_gg_append(out,"\\",1,1);fg181ms_gg_append(out,&c,1,1);} else if(c=='\n')fg181ms_gg_append(out,"\\n",2,1);else if(c=='\r')fg181ms_gg_append(out,"\\r",2,1);else if(c=='\t')fg181ms_gg_append(out,"\\t",2,1);else fg181ms_gg_append(out,&c,1,1);}fg181ms_gg_append(out,"\"",1,1);}
static uint8_t fg181ms_gg_eq_1(int64_t a, int64_t b);
static uint8_t fg181ms_gg_equal_elements_1(fg181ms_gg_slice a, fg181ms_gg_slice b);
static void fg181ms_gg_render_1(fg181ms_gg_reference out, int64_t value);
static uint8_t fg181ms_gg_eq_2(uint8_t a, uint8_t b);
static uint8_t fg181ms_gg_equal_elements_2(fg181ms_gg_slice a, fg181ms_gg_slice b);
static void fg181ms_gg_render_2(fg181ms_gg_reference out, uint8_t value);
static uint8_t fg181ms_gg_eq_3(uint64_t a, uint64_t b);
static uint8_t fg181ms_gg_equal_elements_3(fg181ms_gg_slice a, fg181ms_gg_slice b);
static void fg181ms_gg_render_3(fg181ms_gg_reference out, uint64_t value);
static uint8_t fg181ms_gg_eq_5(uint8_t a, uint8_t b);
static uint8_t fg181ms_gg_equal_elements_5(fg181ms_gg_slice a, fg181ms_gg_slice b);
static void fg181ms_gg_render_5(fg181ms_gg_reference out, uint8_t value);
static uint8_t fg181ms_gg_equal_elements_1(fg181ms_gg_slice a,fg181ms_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg181ms_gg_eq_1(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg181ms_gg_eq_1(int64_t a, int64_t b) {
return a == b;
}
static void fg181ms_gg_render_1(fg181ms_gg_reference out, int64_t value) {
fg181ms_gg_integer(out,value);
}
static uint8_t fg181ms_gg_equal_elements_2(fg181ms_gg_slice a,fg181ms_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg181ms_gg_eq_2(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg181ms_gg_eq_2(uint8_t a, uint8_t b) {
return a == b;
}
static void fg181ms_gg_render_2(fg181ms_gg_reference out, uint8_t value) {
fg181ms_gg_bool(out,value);
}
static uint8_t fg181ms_gg_equal_elements_3(fg181ms_gg_slice a,fg181ms_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg181ms_gg_eq_3(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg181ms_gg_eq_3(uint64_t a, uint64_t b) {
return a == b;
}
static void fg181ms_gg_render_3(fg181ms_gg_reference out, uint64_t value) {
fg181ms_gg_unsigned(out,value);
}
static uint8_t fg181ms_gg_equal_elements_5(fg181ms_gg_slice a,fg181ms_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg181ms_gg_eq_5(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg181ms_gg_eq_5(uint8_t a, uint8_t b) {
return a == b;
}
static void fg181ms_gg_render_5(fg181ms_gg_reference out, uint8_t value) {
fg181ms_gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} fg181ms_gg_globals;
static void fg181ms_gg_function_0(int64_t base);
static uint8_t fg181ms_gg_function_1(int64_t p0, int64_t base);
static void fg181ms_gg_function_3(uint8_t p0, int64_t p1, int64_t base);
static uint64_t fg181ms_gg_function_5(uint64_t p0, uint64_t p1, int64_t base);
static uint64_t fg181ms_gg_function_6(int64_t base);
static void fg181ms_gg_function_3(uint8_t p0, int64_t p1, int64_t base) {
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
  fg181h_fail(t2);
  }
  }
  }
fg181ms_gg_done:;
}
static uint64_t fg181ms_gg_function_6(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((uint64_t)UINT64_C(18446744073709551615))))));
  goto fg181ms_gg_done;
  }
fg181ms_gg_done:;
  return t0;
}
static uint64_t fg181ms_gg_function_5(uint64_t p0, uint64_t p1, int64_t base) {
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
  t6 = fg181ms_gg_function_6(base + 0);
  t7 = ((uint64_t)(t6));
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(((uint64_t)fg181ms_gg_udiv(t7, t8))));
  t4 = ((t5) <= (t9));
  }
  t10 = t4;
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg181ms_gg_function_3(t10, t11, base + 0);
  t12 = ((uint64_t)(v0));
  t13 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  goto fg181ms_gg_done;
  }
fg181ms_gg_done:;
  return t0;
}
static uint8_t fg181ms_gg_function_1(int64_t p0, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint64_t t18;
  uint8_t t19;
  uint8_t t20;
  uint64_t t21;
  uint64_t t22;
  uint8_t t23;
  uint8_t t24;
  uint64_t t25;
  uint64_t t26;
  uint8_t t27;
  uint8_t t28;
  uint64_t t29;
  uint64_t t30;
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  uint64_t t36;
  int64_t t37;
  int64_t t38;
  uint8_t t39;
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
  uint8_t t54;
  uint8_t t55;
  int64_t t56;
  int64_t t57;
  uint8_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  uint8_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  uint8_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  uint8_t t70;
  int64_t t71;
  uint64_t t72;
  uint64_t t73;
  uint64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  uint64_t t78;
  uint64_t t79;
  uint8_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
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
  int64_t t107;
  int64_t t108;
  int64_t t109;
  uint64_t t110;
  uint64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  uint64_t t118;
  uint64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  uint64_t t126;
  uint64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  uint64_t t134;
  uint64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  uint8_t t140;
  int64_t t141;
  int64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  uint64_t v9;
  int64_t v10;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg181h_get(t1, t2, t3);
  v1 = ((uint64_t)(t4));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  t8 = fg181h_get(t5, t6, t7);
  v2 = ((uint64_t)(t8));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
  t12 = fg181h_get(t9, t10, t11);
  v3 = ((uint64_t)(t12));
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((int64_t)(((int64_t)UINT64_C(3))));
  t16 = fg181h_get(t13, t14, t15);
  v4 = ((uint64_t)(t16));
  t17 = ((uint64_t)(v1));
  t18 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t17) != (t18));
  t20 = t19;
  if (t20) {
  t21 = ((uint64_t)(v2));
  t22 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t20 = ((t21) != (t22));
  }
  t23 = t20;
  t24 = t23;
  if (t24) {
  t25 = ((uint64_t)(v3));
  t26 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t24 = ((t25) != (t26));
  }
  t27 = t24;
  t28 = t27;
  if (t28) {
  t29 = ((uint64_t)(v4));
  t30 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t28 = ((t29) != (t30));
  }
  t31 = t28;
  t32 = ((int64_t)(((int64_t)UINT64_C(20))));
  fg181ms_gg_function_3(t31, t32, base + 0);
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(1))));
  t35 = ((int64_t)(((int64_t)UINT64_C(2))));
  t36 = fg181h_get(t33, t34, t35);
  v5 = ((int64_t)(((int64_t)(t36))));
  t37 = ((int64_t)(v5));
  t38 = ((int64_t)(((int64_t)UINT64_C(8))));
  t39 = ((t37) != (t38));
  if (t39) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg181ms_gg_done;
  }
  }
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(1))));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = fg181h_get(t40, t41, t42);
  v6 = ((int64_t)(((int64_t)(t43))));
  t44 = ((int64_t)(v0));
  t45 = ((int64_t)(((int64_t)UINT64_C(1))));
  t46 = ((int64_t)(((int64_t)UINT64_C(1))));
  t47 = fg181h_get(t44, t45, t46);
  v7 = ((int64_t)(((int64_t)(t47))));
  t48 = ((int64_t)(v0));
  t49 = ((int64_t)(((int64_t)UINT64_C(1))));
  t50 = ((int64_t)(((int64_t)UINT64_C(7))));
  t51 = fg181h_get(t48, t49, t50);
  v8 = ((int64_t)(((int64_t)(t51))));
  t52 = ((int64_t)(v6));
  t53 = ((int64_t)(((int64_t)UINT64_C(0))));
  t54 = ((t52) > (t53));
  t55 = t54;
  if (t55) {
  t56 = ((int64_t)(v7));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t55 = ((t56) > (t57));
  }
  t58 = t55;
  t59 = ((int64_t)(((int64_t)UINT64_C(21))));
  fg181ms_gg_function_3(t58, t59, base + 0);
  t60 = ((int64_t)(v8));
  t61 = ((int64_t)(((int64_t)UINT64_C(32))));
  t62 = ((t60) == (t61));
  t63 = ((int64_t)(((int64_t)UINT64_C(22))));
  fg181ms_gg_function_3(t62, t63, base + 0);
  t64 = ((int64_t)(v7));
  t65 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4194304))))));
  t66 = ((t64) < (t65));
  t67 = ((int64_t)(((int64_t)UINT64_C(23))));
  fg181ms_gg_function_3(t66, t67, base + 0);
  t68 = ((int64_t)(v5));
  t69 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1024))))));
  t70 = ((t68) < (t69));
  t71 = ((int64_t)(((int64_t)UINT64_C(24))));
  fg181ms_gg_function_3(t70, t71, base + 0);
  t72 = ((uint64_t)(((uint64_t)(v7))));
  t73 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  v9 = fg181ms_gg_function_5(t72, t73, base + 0);
  t74 = ((uint64_t)(v9));
  t75 = ((int64_t)(v0));
  t76 = ((int64_t)(((int64_t)UINT64_C(1))));
  t77 = ((int64_t)(((int64_t)UINT64_C(8))));
  t78 = fg181h_get(t75, t76, t77);
  t79 = ((uint64_t)(t78));
  t80 = ((t74) <= (t79));
  t81 = ((int64_t)(((int64_t)UINT64_C(25))));
  fg181ms_gg_function_3(t80, t81, base + 0);
  t82 = ((int64_t)(v0));
  t83 = fg181h_plan(t82);
  v10 = ((int64_t)(t83));
  t84 = ((int64_t)(v10));
  t85 = ((int64_t)(((int64_t)UINT64_C(1))));
  t86 = ((int64_t)(((int64_t)UINT64_C(0))));
  t87 = ((uint64_t)(((uint64_t)(v6))));
  fg181h_put(t84, t85, t86, t87);
  t88 = ((int64_t)(v10));
  t89 = ((int64_t)(((int64_t)UINT64_C(1))));
  t90 = ((int64_t)(((int64_t)UINT64_C(1))));
  t91 = ((uint64_t)(((uint64_t)(v8))));
  fg181h_put(t88, t89, t90, t91);
  t92 = ((int64_t)(v10));
  t93 = ((int64_t)(((int64_t)UINT64_C(1))));
  t94 = ((int64_t)(((int64_t)UINT64_C(2))));
  t95 = ((uint64_t)(v9));
  fg181h_put(t92, t93, t94, t95);
  t96 = ((int64_t)(v10));
  t97 = ((int64_t)(((int64_t)UINT64_C(2))));
  t98 = ((int64_t)(((int64_t)UINT64_C(0))));
  t99 = ((uint64_t)(((uint64_t)(v7))));
  fg181h_put(t96, t97, t98, t99);
  t100 = ((int64_t)(v10));
  t101 = ((int64_t)(((int64_t)UINT64_C(2))));
  t102 = ((int64_t)(((int64_t)UINT64_C(1))));
  t103 = ((uint64_t)(((uint64_t)(v5))));
  fg181h_put(t100, t101, t102, t103);
  t104 = ((int64_t)(v10));
  t105 = ((int64_t)(((int64_t)UINT64_C(2))));
  t106 = ((int64_t)(((int64_t)UINT64_C(2))));
  t107 = ((int64_t)(v0));
  t108 = ((int64_t)(((int64_t)UINT64_C(1))));
  t109 = ((int64_t)(((int64_t)UINT64_C(3))));
  t110 = fg181h_get(t107, t108, t109);
  t111 = ((uint64_t)(t110));
  fg181h_put(t104, t105, t106, t111);
  t112 = ((int64_t)(v10));
  t113 = ((int64_t)(((int64_t)UINT64_C(2))));
  t114 = ((int64_t)(((int64_t)UINT64_C(3))));
  t115 = ((int64_t)(v0));
  t116 = ((int64_t)(((int64_t)UINT64_C(1))));
  t117 = ((int64_t)(((int64_t)UINT64_C(4))));
  t118 = fg181h_get(t115, t116, t117);
  t119 = ((uint64_t)(t118));
  fg181h_put(t112, t113, t114, t119);
  t120 = ((int64_t)(v10));
  t121 = ((int64_t)(((int64_t)UINT64_C(2))));
  t122 = ((int64_t)(((int64_t)UINT64_C(4))));
  t123 = ((int64_t)(v0));
  t124 = ((int64_t)(((int64_t)UINT64_C(1))));
  t125 = ((int64_t)(((int64_t)UINT64_C(5))));
  t126 = fg181h_get(t123, t124, t125);
  t127 = ((uint64_t)(t126));
  fg181h_put(t120, t121, t122, t127);
  t128 = ((int64_t)(v10));
  t129 = ((int64_t)(((int64_t)UINT64_C(2))));
  t130 = ((int64_t)(((int64_t)UINT64_C(5))));
  t131 = ((int64_t)(v0));
  t132 = ((int64_t)(((int64_t)UINT64_C(1))));
  t133 = ((int64_t)(((int64_t)UINT64_C(6))));
  t134 = fg181h_get(t131, t132, t133);
  t135 = ((uint64_t)(t134));
  fg181h_put(t128, t129, t130, t135);
  t136 = ((int64_t)(v0));
  t137 = fg181h_launch(t136);
  t138 = ((int64_t)(t137));
  t139 = ((int64_t)(((int64_t)UINT64_C(0))));
  t140 = ((t138) == (t139));
  t141 = ((int64_t)(((int64_t)UINT64_C(26))));
  fg181ms_gg_function_3(t140, t141, base + 0);
  t0 = ((uint8_t)UINT64_C(1));
  goto fg181ms_gg_done;
  }
fg181ms_gg_done:;
  return t0;
}
static void fg181ms_gg_function_0(int64_t base) {
  int64_t t0;
  uint8_t t1;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = fg181ms_gg_function_1(t0, base + 0);
  (void)(t1);
  }
fg181ms_gg_done:;
}
static int fg181ms_gg_program(int argc, char **argv) {
  struct { int64_t base; } frame;
  frame.base = 0;
  fg181ms_gg_argc = argc; fg181ms_gg_argv = argv;
  fg181ms_gg_function_0(frame.base + 0);
  return 0;
}
int main(int argc, char **argv) {
#ifdef _MSC_VER
  __try { return fg181ms_gg_program(argc, argv); }
  __except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    _resetstkoflw(); fg181ms_gg_fail("native call stack overflow"); return 1;
  }
#else
  return fg181ms_gg_program(argc, argv);
#endif
}
