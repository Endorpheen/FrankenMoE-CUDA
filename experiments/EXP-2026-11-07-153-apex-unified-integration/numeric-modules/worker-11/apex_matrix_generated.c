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
typedef struct { void *data; int64_t len; } gg_slice;
typedef struct { unsigned char *base; int64_t len; } gg_header;
typedef struct { unsigned char *base; size_t top, committed; } gg_stack;
typedef struct { gg_header *header; gg_stack *stack; } gg_reference;
#define GG_RESERVATION ((size_t)268435456)
#define GG_STACK_COUNT 8192
static gg_stack gg_stacks[GG_STACK_COUNT];
static int gg_argc;
static char **gg_argv;
static void gg_fail(const char *message) {
    fprintf(stderr, "goose runtime error: %s\n", message); exit(1);
}
static gg_stack *gg_get_stack(int64_t id) {
    gg_stack *s;
    if (id < 0 || id >= GG_STACK_COUNT) gg_fail("data stack limit exceeded");
    s = &gg_stacks[id];
    if (!s->base) {
#ifdef _WIN32
        s->base = VirtualAlloc(NULL, GG_RESERVATION + 65536, MEM_RESERVE, PAGE_NOACCESS);
#else
        s->base = mmap(NULL, GG_RESERVATION + 65536, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (s->base == MAP_FAILED) s->base = NULL;
#endif
        if (!s->base) gg_fail("cannot reserve data stack");
    }
    return s;
}
static void *gg_allocate(gg_stack *s, size_t size) {
    size_t end, committed;
    void *result;
    if (size > GG_RESERVATION - s->top) gg_fail("data stack reservation exceeded");
    result = s->base + s->top; end = s->top + size;
    if (end > s->committed) {
        committed = (end + 65535) & ~(size_t)65535;
#ifdef _WIN32
        if (!VirtualAlloc(s->base + s->committed, committed - s->committed, MEM_COMMIT, PAGE_READWRITE)) gg_fail("cannot commit data stack");
#else
        if (mprotect(s->base + s->committed, committed - s->committed, PROT_READ | PROT_WRITE)) gg_fail("cannot commit data stack");
#endif
        s->committed = committed;
    }
    s->top = end; return result;
}
static gg_reference gg_ref(gg_header *h, gg_stack *s) { gg_reference r; r.header=h; r.stack=s; return r; }
static gg_slice gg_view(void *p, int64_t n) { gg_slice v; v.data=p; v.len=n; return v; }
static gg_slice gg_byte_view(gg_slice a, size_t width) {
    if (a.len < 0 || (width && (uint64_t)a.len > (uint64_t)INT64_MAX / width)) gg_fail("byte view size overflow");
    return gg_view(a.data, (int64_t)((uint64_t)a.len * width));
}
static gg_slice gg_sub(gg_slice a, int64_t lo, int64_t hi, size_t element) {
    if (lo < 0 || hi < lo || hi > a.len) gg_fail("slice out of bounds");
    return gg_view((unsigned char *)a.data + (size_t)lo * element, hi - lo);
}
static int64_t gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) gg_fail("array index out of bounds"); return i; }
static void gg_append(gg_reference a, const void *p, int64_t n, size_t element) {
    void *to;
    if (n < 0 || (uint64_t)n > GG_RESERVATION / (element ? element : 1)) gg_fail("invalid array size");
    to=gg_allocate(a.stack, (size_t)n * element);
    if (n && element) memmove(to, p, (size_t)n * element);
    a.header->len += n;
}
static void gg_resize(gg_reference a, int64_t n, size_t element, const void *fill) {
    int64_t old=a.header->len, i;
    if (n < 0) gg_fail("negative array length");
    if (n > old) {
        unsigned char *p;
        if (!fill) gg_fail("resize without fill may only shrink");
        if ((uint64_t)(n-old) > GG_RESERVATION / (element ? element : 1)) gg_fail("invalid array size");
        p=gg_allocate(a.stack, (size_t)(n-old)*element);
        for (i=0; i<n-old; i++) if(element) memcpy(p+(size_t)i*element, fill, element);
    } else { a.stack->top = (size_t)(a.header->base-a.stack->base) + (size_t)n*element; }
    a.header->len=n;
}
static int64_t gg_div(int64_t a, int64_t b) { if (!b) gg_fail("division by zero"); if (a==INT64_MIN && b==-1) gg_fail("division overflow"); return a/b; }
static int64_t gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static uint64_t gg_udiv(uint64_t a,uint64_t b) { if(!b)gg_fail("division by zero");return a/b; }
static uint64_t gg_umod(uint64_t a,uint64_t b) { if(!b)gg_fail("division by zero");return a%b; }
static int gg_equal(gg_slice a, gg_slice b, size_t element) { return a.len==b.len && (!a.len || !memcmp(a.data,b.data,(size_t)a.len*element)); }
static void gg_text(gg_reference out, gg_slice v) { gg_append(out,v.data,v.len,1); }
static void gg_integer(gg_reference out,int64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%lld",(long long)n);gg_append(out,b,k,1); }
static void gg_unsigned(gg_reference out,uint64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%llu",(unsigned long long)n);gg_append(out,b,k,1); }
static double gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static int64_t ggf_fmt_f64(uint8_t *out,double n);
static int64_t ggf_fmt_f32(uint8_t *out,float n);
static void gg_float(gg_reference out,double n) { uint8_t b[64];int64_t k=ggf_fmt_f64(b,n);gg_append(out,b,k,1); }
static void gg_float32(gg_reference out,float n) { uint8_t b[64];int64_t k=ggf_fmt_f32(b,n);gg_append(out,b,k,1); }
static void gg_bool(gg_reference out,uint8_t n) { gg_append(out,n?"true":"false",n?4:5,1); }
static void gg_write(FILE *file,gg_slice text) { if(text.len && fwrite(text.data,1,(size_t)text.len,file)!=(size_t)text.len)gg_fail("write failed"); }
static char *gg_cstring(gg_slice path) {
    char *s=malloc((size_t)path.len+1); if(!s)gg_fail("out of memory in file shim");
    if(path.len)memcpy(s,path.data,(size_t)path.len);s[path.len]=0;return s;
}
static uint8_t gs_os_read_file(gg_slice path,gg_reference out) {
    char *p=gg_cstring(path);FILE *f=fopen(p,"rb");unsigned char b[16384];size_t n;int ok;free(p);
    if(!f)return 0;while((n=fread(b,1,sizeof b,f))!=0)gg_append(out,b,(int64_t)n,1);ok=!ferror(f);fclose(f);return (uint8_t)ok;
}
static uint8_t gs_os_write_file(gg_slice path,gg_slice data) {
    char *p=gg_cstring(path);FILE *f=fopen(p,"wb");int ok;free(p);if(!f)return 0;
    ok=!data.len || fwrite(data.data,1,(size_t)data.len,f)==(size_t)data.len;if(fclose(f))ok=0;return (uint8_t)ok;
}
static uint8_t gs_os_file_exists(gg_slice path) { char*p=gg_cstring(path);FILE*f=fopen(p,"rb");free(p);if(!f)return 0;fclose(f);return 1; }
static void gs_os_write_stdout(gg_slice s) { gg_write(stdout,s); }
static void gs_os_write_stderr(gg_slice s) { gg_write(stderr,s); }
static void gs_os_flush_stdout(void) { fflush(stdout); }
static int64_t gs_os_arg_count(void) { return gg_argc; }
static void gs_os_arg(int64_t i,gg_reference out) { if(i>=0 && i<gg_argc)gg_append(out,gg_argv[i],(int64_t)strlen(gg_argv[i]),1); }
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


#pragma pack(push, 1)
typedef struct { uint8_t data[4096]; } gg_type_2;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[1024]; } gg_type_4;
#pragma pack(pop)
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, uint8_t value);
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, gg_type_2 value);
static uint8_t gg_eq_3(float a, float b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, float value);
static uint8_t gg_eq_4(gg_type_4 a, gg_type_4 b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, gg_type_4 value);
static uint8_t gg_eq_5(int64_t a, int64_t b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, int64_t value);
static uint8_t gg_eq_6(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, uint8_t value);
static uint8_t gg_eq_7(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, gg_slice value);
static uint8_t gg_eq_8(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, gg_slice value);
static uint8_t gg_eq_10(uint16_t a, uint16_t b);
static uint8_t gg_equal_elements_10(gg_slice a, gg_slice b);
static void gg_render_10(gg_reference out, uint16_t value);
static uint8_t gg_eq_11(int8_t a, int8_t b);
static uint8_t gg_equal_elements_11(gg_slice a, gg_slice b);
static void gg_render_11(gg_reference out, int8_t value);
static uint8_t gg_eq_12(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_12(gg_slice a, gg_slice b);
static void gg_render_12(gg_reference out, uint32_t value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_1(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((gg_type_2*)a.data)[i],((gg_type_2*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b) {
return gg_equal_elements_1(gg_view((a).data, 4096),gg_view((b).data, 4096));
}
static void gg_render_2(gg_reference out, gg_type_2 value) {
gg_text(out,gg_view((value).data, 4096));
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(float a, float b) {
return a == b;
}
static void gg_render_3(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((gg_type_4*)a.data)[i],((gg_type_4*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(gg_type_4 a, gg_type_4 b) {
return gg_equal_elements_3(gg_view((a).data, 1024),gg_view((b).data, 1024));
}
static void gg_render_4(gg_reference out, gg_type_4 value) {
int64_t i; gg_slice view=gg_view((value).data, 1024);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_5(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_6(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(gg_slice a, gg_slice b) {
return gg_equal_elements_1(a,b);
}
static void gg_render_7(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(gg_slice a, gg_slice b) {
return gg_equal_elements_3(a,b);
}
static void gg_render_8(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((uint16_t*)a.data)[i],((uint16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(uint16_t a, uint16_t b) {
return a == b;
}
static void gg_render_10(gg_reference out, uint16_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((int8_t*)a.data)[i],((int8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(int8_t a, int8_t b) {
return a == b;
}
static void gg_render_11(gg_reference out, int8_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_12(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_12(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_12(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_12(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static int64_t gg_function_1(int64_t p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t base);
static float gg_function_3(gg_slice p0, int64_t p1, int64_t p2, int64_t base);
static float gg_function_4(gg_slice p0, int64_t p1, int64_t base);
static uint16_t gg_function_5(gg_slice p0, int64_t p1, int64_t base);
static int64_t gg_function_7(gg_slice p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_8(gg_slice p0, int64_t p1, int64_t p2, int64_t base);
static float gg_function_9(gg_slice p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_10(gg_slice p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_11(gg_slice p0, int64_t p1, int64_t base);
static int64_t gg_function_12(gg_slice p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_13(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t p4, int64_t base);
static float gg_function_14(gg_slice p0, int64_t p1, int64_t base);
static uint32_t gg_function_15(gg_slice p0, int64_t p1, int64_t base);
static float gg_function_17(gg_slice p0, int64_t p1, int64_t base);
static int64_t gg_function_18(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_19(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base);
static int64_t gg_function_20(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base);
static float gg_function_21(gg_slice p0, int64_t p1, gg_slice p2, int64_t base);
static float gg_function_22(gg_slice p0, int64_t p1, int64_t base);
static float gg_function_23(gg_slice p0, int64_t p1, gg_slice p2, int64_t base);
static int64_t gg_function_24(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_25(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_26(int64_t p0, int64_t p1, int64_t p2, gg_slice p3, int64_t base);
static int64_t gg_function_27(int64_t p0, int64_t base);
static int64_t gg_function_28(int64_t p0, int64_t base);
static int64_t gg_function_29(int64_t p0, int64_t p1, gg_slice p2, int64_t base);
static int64_t gg_function_30(int64_t p0, int64_t base);
static int64_t gg_function_31(int64_t p0, int64_t p1, int64_t p2, gg_slice p3, int64_t base);
static int64_t gg_function_32(gg_slice p0, gg_slice p1, int64_t p2, int64_t base);
static int64_t gg_function_12(gg_slice p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t13;
  gg_slice t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint8_t * t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  gg_slice t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  uint8_t * t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  gg_slice v0;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)UINT64_C(128))));
  v3 = ((int64_t)(((int64_t)gg_div(t1, t2))));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(128))));
  v4 = ((int64_t)(((int64_t)gg_mod(t3, t4))));
  t5 = ((int64_t)(v4));
  t6 = ((int64_t)(((int64_t)UINT64_C(32))));
  v5 = ((int64_t)(((int64_t)gg_mod(t5, t6))));
  t7 = ((int64_t)(v4));
  t8 = ((int64_t)(((int64_t)UINT64_C(32))));
  v6 = ((int64_t)(((int64_t)gg_div(t7, t8))));
  t9 = ((int64_t)(v6));
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = ((int64_t)(((int64_t)gg_mod(t9, t10))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t11) == (t12));
  if (t13) {
  {
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  v7 = ((int64_t)(((int64_t)UINT64_C(32))));
  }
  }
  t14 = v0;
  t15 = ((int64_t)(v1));
  t16 = ((int64_t)(((int64_t)UINT64_C(64))));
  t17 = ((int64_t)(v3));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t15) + (uint64_t)(t18)))));
  t20 = ((int64_t)(v5));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) + (uint64_t)(t20)))));
  t22 = ((int64_t)(v7));
  t23 = ((int64_t)((uint64_t)(t21) + (uint64_t)(t22)));
  t24 = ((uint8_t *)t14.data) + gg_index(t23, t14.len);
  v8 = ((int64_t)(((int64_t)((*t24)))));
  t25 = ((int64_t)(v6));
  t26 = ((int64_t)(((int64_t)UINT64_C(2))));
  t27 = ((t25) < (t26));
  if (t27) {
  {
  t28 = ((int64_t)(v8));
  t29 = ((int64_t)(((int64_t)UINT64_C(15))));
  v9 = ((int64_t)(((t28) & (t29))));
  }
  } else {
  {
  t30 = ((int64_t)(v8));
  t31 = ((int64_t)(((int64_t)UINT64_C(4))));
  v9 = ((int64_t)(((t30) >> ((uint64_t)(t31) & 63))));
  }
  }
  t32 = v0;
  t33 = ((int64_t)(v1));
  t34 = ((int64_t)(((int64_t)UINT64_C(128))));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t33) + (uint64_t)(t34)))));
  t36 = ((int64_t)(((int64_t)UINT64_C(32))));
  t37 = ((int64_t)(v3));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t37)))));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t38)))));
  t40 = ((int64_t)(v5));
  t41 = ((int64_t)((uint64_t)(t39) + (uint64_t)(t40)));
  t42 = ((uint8_t *)t32.data) + gg_index(t41, t32.len);
  v10 = ((int64_t)(((int64_t)((*t42)))));
  t43 = ((int64_t)(((int64_t)UINT64_C(2))));
  t44 = ((int64_t)(v6));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t43) * (uint64_t)(t44)))));
  t45 = ((int64_t)(v10));
  t46 = ((int64_t)(v11));
  t47 = ((int64_t)(((t45) >> ((uint64_t)(t46) & 63))));
  t48 = ((int64_t)(((int64_t)UINT64_C(3))));
  t49 = ((int64_t)(((t47) & (t48))));
  t50 = ((int64_t)(((int64_t)UINT64_C(4))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t49) << ((uint64_t)(t50) & 63)))));
  t51 = ((int64_t)(v9));
  t52 = ((int64_t)(v12));
  t0 = ((int64_t)(((t51) | (t52))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_11(gg_slice p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t * t8;
  gg_slice v0;
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
  t6 = ((int64_t)((v0).len));
  t4 = ((t5) < (t6));
  }
  if (!(t4)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t7 = v1;
  t8 = ((uint8_t *)(v0).data) + gg_index(t7, (v0).len);
  t0 = ((int64_t)(((int64_t)(((int8_t)((*t8)))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_10(gg_slice p0, int64_t p1, int64_t p2, int64_t base) {
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
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  gg_slice v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)UINT64_C(128))));
  v3 = ((int64_t)(((int64_t)gg_div(t1, t2))));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(128))));
  v4 = ((int64_t)(((int64_t)gg_mod(t3, t4))));
  t5 = ((int64_t)(v4));
  t6 = ((int64_t)(((int64_t)UINT64_C(32))));
  v5 = ((int64_t)(((int64_t)gg_mod(t5, t6))));
  t7 = ((int64_t)(v4));
  t8 = ((int64_t)(((int64_t)UINT64_C(32))));
  v6 = ((int64_t)(((int64_t)gg_div(t7, t8))));
  t9 = ((int64_t)(((int64_t)UINT64_C(8))));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))));
  t12 = ((int64_t)(v5));
  t13 = ((int64_t)(((int64_t)UINT64_C(16))));
  t14 = ((int64_t)(((int64_t)gg_div(t12, t13))));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t11) + (uint64_t)(t14)))));
  t16 = ((int64_t)(((int64_t)UINT64_C(2))));
  t17 = ((int64_t)(v6));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t15) + (uint64_t)(t18)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint16_t gg_function_5(gg_slice p0, int64_t p1, int64_t base) {
  uint16_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t * t10;
  uint16_t t11;
  gg_slice t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t * t16;
  uint16_t t17;
  int64_t t18;
  uint16_t t19;
  gg_slice v0;
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
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t5) + (uint64_t)(t6)))));
  t8 = ((int64_t)((v0).len));
  t4 = ((t7) < (t8));
  }
  if (!(t4)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t9 = v1;
  t10 = ((uint8_t *)(v0).data) + gg_index(t9, (v0).len);
  t11 = ((uint16_t)(((uint16_t)((*t10)))));
  t12 = v0;
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = ((int64_t)((uint64_t)(t13) + (uint64_t)(t14)));
  t16 = ((uint8_t *)t12.data) + gg_index(t15, t12.len);
  t17 = ((uint16_t)(((uint16_t)((*t16)))));
  t18 = ((int64_t)(((int64_t)UINT64_C(8))));
  t19 = ((uint16_t)(((uint16_t)((uint64_t)(t17) << ((uint64_t)(t18) & 15)))));
  t0 = ((uint16_t)(((t11) | (t19))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_4(gg_slice p0, int64_t p1, int64_t base) {
  float t0;
  gg_slice t1;
  int64_t t2;
  uint16_t t3;
  float t4;
  gg_slice v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = v0;
  t2 = ((int64_t)(v1));
  t3 = gg_function_5(t1, t2, base + 0);
  t4 = fgm_half(t3);
  t0 = ((float)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_9(gg_slice p0, int64_t p1, int64_t p2, int64_t base) {
  float t0;
  gg_slice t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  gg_slice t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  gg_slice t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  gg_slice t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  float t22;
  float t23;
  float t24;
  float t25;
  gg_slice v0;
  int64_t v1;
  int64_t v2;
  float v3;
  float v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = v0;
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)UINT64_C(208))));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) + (uint64_t)(t3)))));
  v3 = gg_function_4(t1, t4, base + 0);
  t5 = v0;
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(192))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) + (uint64_t)(t7)))));
  t9 = v0;
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(v2));
  t12 = gg_function_10(t9, t10, t11, base + 0);
  t13 = ((int64_t)(t12));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t13)))));
  t15 = gg_function_11(t5, t14, base + 0);
  v4 = ((float)(((float)(t15))));
  t16 = v0;
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(v2));
  t19 = gg_function_12(t16, t17, t18, base + 0);
  t20 = ((int64_t)(t19));
  t21 = ((int64_t)(((int64_t)UINT64_C(32))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t20) - (uint64_t)(t21)))));
  t22 = ((float)(v3));
  t23 = ((float)(v4));
  t24 = ((float)(((t22) * (t23))));
  t25 = ((float)(((float)(v5))));
  t0 = ((float)(((t24) * (t25))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_8(gg_slice p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  gg_slice t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint8_t * t12;
  int64_t t13;
  int64_t t14;
  gg_slice t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t * t21;
  int64_t t22;
  int64_t t23;
  gg_slice t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t * t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  gg_slice v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(4))));
  v3 = ((int64_t)(((int64_t)((uint64_t)(t1) + (uint64_t)(t2)))));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(4))));
  t5 = ((t3) < (t4));
  if (t5) {
  {
  t6 = v0;
  t7 = ((int64_t)(v3));
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) + (uint64_t)(t8)))));
  t10 = ((int64_t)(((int64_t)UINT64_C(4))));
  t11 = ((int64_t)((uint64_t)(t9) + (uint64_t)(t10)));
  t12 = ((uint8_t *)t6.data) + gg_index(t11, t6.len);
  t13 = ((int64_t)(((int64_t)((*t12)))));
  t14 = ((int64_t)(((int64_t)UINT64_C(63))));
  t0 = ((int64_t)(((t13) & (t14))));
  goto gg_done;
  }
  }
  t15 = v0;
  t16 = ((int64_t)(v3));
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) + (uint64_t)(t17)))));
  t19 = ((int64_t)(((int64_t)UINT64_C(4))));
  t20 = ((int64_t)((uint64_t)(t18) + (uint64_t)(t19)));
  t21 = ((uint8_t *)t15.data) + gg_index(t20, t15.len);
  t22 = ((int64_t)(((int64_t)((*t21)))));
  t23 = ((int64_t)(((int64_t)UINT64_C(4))));
  v4 = ((int64_t)(((t22) >> ((uint64_t)(t23) & 63))));
  t24 = v0;
  t25 = ((int64_t)(v3));
  t26 = ((int64_t)(v2));
  t27 = ((int64_t)((uint64_t)(t25) + (uint64_t)(t26)));
  t28 = ((uint8_t *)t24.data) + gg_index(t27, t24.len);
  t29 = ((int64_t)(((int64_t)((*t28)))));
  t30 = ((int64_t)(((int64_t)UINT64_C(6))));
  t31 = ((int64_t)(((t29) >> ((uint64_t)(t30) & 63))));
  t32 = ((int64_t)(((int64_t)UINT64_C(4))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t31) << ((uint64_t)(t32) & 63)))));
  t33 = ((int64_t)(v4));
  t34 = ((int64_t)(v5));
  t0 = ((int64_t)(((t33) | (t34))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_7(gg_slice p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  gg_slice t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t * t10;
  int64_t t11;
  int64_t t12;
  gg_slice t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t * t19;
  int64_t t20;
  int64_t t21;
  gg_slice t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t * t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  gg_slice v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(4))));
  v3 = ((int64_t)(((int64_t)((uint64_t)(t1) + (uint64_t)(t2)))));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(4))));
  t5 = ((t3) < (t4));
  if (t5) {
  {
  t6 = v0;
  t7 = ((int64_t)(v3));
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)((uint64_t)(t7) + (uint64_t)(t8)));
  t10 = ((uint8_t *)t6.data) + gg_index(t9, t6.len);
  t11 = ((int64_t)(((int64_t)((*t10)))));
  t12 = ((int64_t)(((int64_t)UINT64_C(63))));
  t0 = ((int64_t)(((t11) & (t12))));
  goto gg_done;
  }
  }
  t13 = v0;
  t14 = ((int64_t)(v3));
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t15)))));
  t17 = ((int64_t)(((int64_t)UINT64_C(4))));
  t18 = ((int64_t)((uint64_t)(t16) + (uint64_t)(t17)));
  t19 = ((uint8_t *)t13.data) + gg_index(t18, t13.len);
  t20 = ((int64_t)(((int64_t)((*t19)))));
  t21 = ((int64_t)(((int64_t)UINT64_C(15))));
  v4 = ((int64_t)(((t20) & (t21))));
  t22 = v0;
  t23 = ((int64_t)(v3));
  t24 = ((int64_t)(v2));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) + (uint64_t)(t24)))));
  t26 = ((int64_t)(((int64_t)UINT64_C(4))));
  t27 = ((int64_t)((uint64_t)(t25) - (uint64_t)(t26)));
  t28 = ((uint8_t *)t22.data) + gg_index(t27, t22.len);
  t29 = ((int64_t)(((int64_t)((*t28)))));
  t30 = ((int64_t)(((int64_t)UINT64_C(6))));
  t31 = ((int64_t)(((t29) >> ((uint64_t)(t30) & 63))));
  t32 = ((int64_t)(((int64_t)UINT64_C(4))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t31) << ((uint64_t)(t32) & 63)))));
  t33 = ((int64_t)(v4));
  t34 = ((int64_t)(v5));
  t0 = ((int64_t)(((t33) | (t34))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_3(gg_slice p0, int64_t p1, int64_t p2, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  gg_slice t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t * t19;
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
  gg_slice t31;
  int64_t t32;
  gg_slice t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  gg_slice t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  gg_slice t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  float t45;
  float t46;
  float t47;
  float t48;
  float t49;
  float t50;
  float t51;
  float t52;
  gg_slice v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  float v10;
  float v11;
  float v12;
  float v13;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)UINT64_C(64))));
  v3 = ((int64_t)(((int64_t)gg_div(t1, t2))));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(64))));
  v4 = ((int64_t)(((int64_t)gg_mod(t3, t4))));
  t5 = ((int64_t)(v4));
  t6 = ((int64_t)(((int64_t)UINT64_C(32))));
  v5 = ((int64_t)(((int64_t)gg_mod(t5, t6))));
  t7 = ((int64_t)(v4));
  t8 = ((int64_t)(((int64_t)UINT64_C(32))));
  v6 = ((int64_t)(((int64_t)gg_div(t7, t8))));
  t9 = v0;
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(((int64_t)UINT64_C(16))));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))));
  t13 = ((int64_t)(((int64_t)UINT64_C(32))));
  t14 = ((int64_t)(v3));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t12) + (uint64_t)(t15)))));
  t17 = ((int64_t)(v5));
  t18 = ((int64_t)((uint64_t)(t16) + (uint64_t)(t17)));
  t19 = ((uint8_t *)t9.data) + gg_index(t18, t9.len);
  v7 = ((int64_t)(((int64_t)((*t19)))));
  t20 = ((int64_t)(v6));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t20) == (t21));
  if (t22) {
  {
  t23 = ((int64_t)(v7));
  t24 = ((int64_t)(((int64_t)UINT64_C(15))));
  v8 = ((int64_t)(((t23) & (t24))));
  }
  } else {
  {
  t25 = ((int64_t)(v7));
  t26 = ((int64_t)(((int64_t)UINT64_C(4))));
  v8 = ((int64_t)(((t25) >> ((uint64_t)(t26) & 63))));
  }
  }
  t27 = ((int64_t)(((int64_t)UINT64_C(2))));
  t28 = ((int64_t)(v3));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) * (uint64_t)(t28)))));
  t30 = ((int64_t)(v6));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t30)))));
  t31 = v0;
  t32 = ((int64_t)(v1));
  v10 = gg_function_4(t31, t32, base + 0);
  t33 = v0;
  t34 = ((int64_t)(v1));
  t35 = ((int64_t)(((int64_t)UINT64_C(2))));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t35)))));
  v11 = gg_function_4(t33, t36, base + 0);
  t37 = v0;
  t38 = ((int64_t)(v1));
  t39 = ((int64_t)(v9));
  t40 = gg_function_7(t37, t38, t39, base + 0);
  v12 = ((float)(((float)(t40))));
  t41 = v0;
  t42 = ((int64_t)(v1));
  t43 = ((int64_t)(v9));
  t44 = gg_function_8(t41, t42, t43, base + 0);
  v13 = ((float)(((float)(t44))));
  t45 = ((float)(v10));
  t46 = ((float)(v12));
  t47 = ((float)(((t45) * (t46))));
  t48 = ((float)(((float)(v8))));
  t49 = ((float)(((t47) * (t48))));
  t50 = ((float)(v11));
  t51 = ((float)(v13));
  t52 = ((float)(((t50) * (t51))));
  t0 = ((float)(((t49) - (t52))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_32(gg_slice p0, gg_slice p1, int64_t p2, int64_t base) {
  int64_t t0;
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
  uint8_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
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
  uint8_t t34;
  gg_slice t35;
  int64_t t36;
  int64_t t37;
  gg_slice t38;
  int64_t t39;
  int64_t t40;
  gg_slice t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  float * t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  gg_slice v0;
  gg_slice v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  float v8;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)UINT64_C(12))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(144))));
  }
  } else {
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(((int64_t)UINT64_C(14))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(210))));
  }
  } else {
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  }
  t7 = ((int64_t)(v3));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t10 = ((int64_t)((v1).len));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t10) <= (t11));
  t13 = t12;
  if (!t13) {
  t14 = ((int64_t)((v1).len));
  t15 = ((int64_t)(((int64_t)UINT64_C(256))));
  t16 = ((int64_t)(((int64_t)gg_mod(t14, t15))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t16) != (t17));
  }
  t18 = t13;
  if (t18) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t19 = ((int64_t)((v1).len));
  t20 = ((int64_t)(((int64_t)UINT64_C(256))));
  v4 = ((int64_t)(((int64_t)gg_div(t19, t20))));
  t21 = ((int64_t)((v0).len));
  t22 = ((int64_t)(v4));
  t23 = ((int64_t)(v3));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t22) * (uint64_t)(t23)))));
  t25 = ((t21) < (t24));
  if (t25) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t26 = ((int64_t)(v5));
  t27 = ((int64_t)(v4));
  if (!(((t26) < (t27)))) goto label2;
  {
  t28 = ((int64_t)(v5));
  t29 = ((int64_t)(v3));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t30 = ((int64_t)(v7));
  t31 = ((int64_t)(((int64_t)UINT64_C(256))));
  if (!(((t30) < (t31)))) goto label5;
  {
  t32 = ((int64_t)(v2));
  t33 = ((int64_t)(((int64_t)UINT64_C(12))));
  t34 = ((t32) == (t33));
  if (t34) {
  {
  t35 = v0;
  t36 = ((int64_t)(v6));
  t37 = ((int64_t)(v7));
  v8 = gg_function_3(t35, t36, t37, base + 0);
  }
  } else {
  {
  t38 = v0;
  t39 = ((int64_t)(v6));
  t40 = ((int64_t)(v7));
  v8 = gg_function_9(t38, t39, t40, base + 0);
  }
  }
  t41 = v1;
  t42 = ((int64_t)(v5));
  t43 = ((int64_t)(((int64_t)UINT64_C(256))));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) * (uint64_t)(t43)))));
  t45 = ((int64_t)(v7));
  t46 = ((int64_t)((uint64_t)(t44) + (uint64_t)(t45)));
  t47 = ((float *)t41.data) + gg_index(t46, t41.len);
  (*t47) = ((float)(v8));
  t48 = v7;
  t49 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)((uint64_t)(t48) + (uint64_t)(t49)));
  }
  goto label4;
label5:;
  t50 = v5;
  t51 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t50) + (uint64_t)(t51)));
  }
  goto label1;
label2:;
  t52 = ((int64_t)(((int64_t)UINT64_C(40))));
  t53 = ((int64_t)(v4));
  fgm_witness(t52, t53);
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_31(int64_t p0, int64_t p1, int64_t p2, gg_slice p3, int64_t base) {
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
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  int64_t t18;
  int64_t t19;
  uint8_t t20;
  int64_t t21;
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
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  float * t55;
  int64_t t56;
  float * t57;
  int64_t t58;
  float * t59;
  int64_t t60;
  float * t61;
  int64_t t62;
  float * t63;
  int64_t t64;
  float * t65;
  int64_t t66;
  int64_t t67;
  uint8_t t68;
  int64_t t69;
  int64_t t70;
  uint8_t t71;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  gg_slice v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)((v3).len));
  t2 = ((int64_t)(((int64_t)UINT64_C(6))));
  t3 = ((t1) < (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) <= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (!t8) {
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(32))));
  t11 = ((int64_t)(((int64_t)gg_mod(t9, t10))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t11) != (t12));
  }
  t13 = t8;
  t14 = t13;
  if (!t14) {
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t15) <= (t16));
  }
  t17 = t14;
  if (t17) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(12))));
  t20 = ((t18) == (t19));
  if (t20) {
  {
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(((int64_t)UINT64_C(32))));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) * (uint64_t)(t22)))));
  t24 = ((int64_t)(v1));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t23) + (uint64_t)(t24)))));
  v5 = ((int64_t)(v1));
  }
  } else {
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(14))));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(((int64_t)UINT64_C(64))));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t31 = ((int64_t)(v1));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t31)))));
  t32 = ((int64_t)(v1));
  t33 = ((int64_t)(v1));
  t34 = ((int64_t)(((int64_t)UINT64_C(32))));
  t35 = ((int64_t)(((int64_t)gg_div(t33, t34))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t32) + (uint64_t)(t35)))));
  }
  } else {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  }
  t36 = ((int64_t)(v1));
  t37 = ((int64_t)(((int64_t)UINT64_C(32))));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t37)))));
  t39 = ((int64_t)(((int64_t)UINT64_C(8))));
  t40 = ((int64_t)(((int64_t)gg_div(t38, t39))));
  t41 = ((int64_t)(v1));
  t42 = ((int64_t)(((int64_t)UINT64_C(8))));
  t43 = ((int64_t)(((int64_t)gg_div(t41, t42))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t43)))));
  t44 = ((int64_t)(v4));
  t45 = ((int64_t)(v5));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) + (uint64_t)(t45)))));
  t47 = ((int64_t)(v6));
  t48 = ((int64_t)(((int64_t)UINT64_C(3))));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) + (uint64_t)(t48)))));
  t50 = ((int64_t)(((int64_t)UINT64_C(4))));
  t51 = ((int64_t)(((int64_t)gg_div(t49, t50))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t46) + (uint64_t)(t51)))));
  t52 = ((int64_t)(v1));
  t53 = ((int64_t)(v2));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t52) * (uint64_t)(t53)))));
  /* FG_DIRECT_SLICE_INDEX */
  t54 = ((int64_t)UINT64_C(0));
  t55 = ((float *)(v3).data) + gg_index(t54, (v3).len);
  (*t55) = ((float)(((float)(v4))));
  /* FG_DIRECT_SLICE_INDEX */
  t56 = ((int64_t)UINT64_C(1));
  t57 = ((float *)(v3).data) + gg_index(t56, (v3).len);
  (*t57) = ((float)(((float)(v5))));
  /* FG_DIRECT_SLICE_INDEX */
  t58 = ((int64_t)UINT64_C(2));
  t59 = ((float *)(v3).data) + gg_index(t58, (v3).len);
  (*t59) = ((float)(((float)(v6))));
  /* FG_DIRECT_SLICE_INDEX */
  t60 = ((int64_t)UINT64_C(3));
  t61 = ((float *)(v3).data) + gg_index(t60, (v3).len);
  (*t61) = ((float)(((float)(v7))));
  /* FG_DIRECT_SLICE_INDEX */
  t62 = ((int64_t)UINT64_C(4));
  t63 = ((float *)(v3).data) + gg_index(t62, (v3).len);
  (*t63) = ((float)(((float)(v8))));
  /* FG_DIRECT_SLICE_INDEX */
  t64 = ((int64_t)UINT64_C(5));
  t65 = ((float *)(v3).data) + gg_index(t64, (v3).len);
  t66 = ((int64_t)(v7));
  t67 = ((int64_t)(v8));
  t68 = ((t66) <= (t67));
  if (t68) {
  {
  (*t65) = ((float)(((float)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  (*t65) = ((float)(((float)(((int64_t)UINT64_C(0))))));
  }
  }
  t69 = ((int64_t)(v7));
  t70 = ((int64_t)(v8));
  t71 = ((t69) <= (t70));
  if (t71) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_30(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(128))));
  t3 = ((int64_t)(((int64_t)gg_mod(t1, t2))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) != (t4));
  if (t5) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_29(int64_t p0, int64_t p1, gg_slice p2, int64_t base) {
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
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  int64_t t17;
  float * t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  float * t24;
  int64_t t25;
  float * t26;
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
  uint8_t t37;
  int64_t t38;
  float * t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  float * t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  float * t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t v0;
  int64_t v1;
  gg_slice v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)((v2).len));
  t2 = ((int64_t)(((int64_t)UINT64_C(3))));
  t3 = ((t1) < (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) <= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (!t8) {
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(32))));
  t11 = ((int64_t)(((int64_t)gg_mod(t9, t10))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t11) != (t12));
  }
  t13 = t8;
  if (t13) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(12))));
  t16 = ((t14) == (t15));
  if (t16) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t17 = ((int64_t)UINT64_C(0));
  t18 = ((float *)(v2).data) + gg_index(t17, (v2).len);
  t19 = ((int64_t)(v1));
  t20 = ((int64_t)(((int64_t)UINT64_C(32))));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t22 = ((int64_t)(v1));
  (*t18) = ((float)(((float)(((int64_t)((uint64_t)(t21) + (uint64_t)(t22)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t23 = ((int64_t)UINT64_C(1));
  t24 = ((float *)(v2).data) + gg_index(t23, (v2).len);
  (*t24) = ((float)(((float)(v1))));
  /* FG_DIRECT_SLICE_INDEX */
  t25 = ((int64_t)UINT64_C(2));
  t26 = ((float *)(v2).data) + gg_index(t25, (v2).len);
  t27 = ((int64_t)(v1));
  t28 = ((int64_t)(((int64_t)UINT64_C(32))));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) * (uint64_t)(t28)))));
  t30 = ((int64_t)(((int64_t)UINT64_C(8))));
  t31 = ((int64_t)(((int64_t)gg_div(t29, t30))));
  t32 = ((int64_t)(v1));
  t33 = ((int64_t)(((int64_t)UINT64_C(8))));
  t34 = ((int64_t)(((int64_t)gg_div(t32, t33))));
  (*t26) = ((float)(((float)(((int64_t)((uint64_t)(t31) + (uint64_t)(t34)))))));
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t35 = ((int64_t)(v0));
  t36 = ((int64_t)(((int64_t)UINT64_C(14))));
  t37 = ((t35) == (t36));
  if (t37) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t38 = ((int64_t)UINT64_C(0));
  t39 = ((float *)(v2).data) + gg_index(t38, (v2).len);
  t40 = ((int64_t)(v1));
  t41 = ((int64_t)(((int64_t)UINT64_C(64))));
  t42 = ((int64_t)(((int64_t)((uint64_t)(t40) * (uint64_t)(t41)))));
  t43 = ((int64_t)(v1));
  (*t39) = ((float)(((float)(((int64_t)((uint64_t)(t42) + (uint64_t)(t43)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t44 = ((int64_t)UINT64_C(1));
  t45 = ((float *)(v2).data) + gg_index(t44, (v2).len);
  t46 = ((int64_t)(v1));
  t47 = ((int64_t)(v1));
  t48 = ((int64_t)(((int64_t)UINT64_C(32))));
  t49 = ((int64_t)(((int64_t)gg_div(t47, t48))));
  (*t45) = ((float)(((float)(((int64_t)((uint64_t)(t46) + (uint64_t)(t49)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t50 = ((int64_t)UINT64_C(2));
  t51 = ((float *)(v2).data) + gg_index(t50, (v2).len);
  t52 = ((int64_t)(v1));
  t53 = ((int64_t)(((int64_t)UINT64_C(32))));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) * (uint64_t)(t53)))));
  t55 = ((int64_t)(((int64_t)UINT64_C(8))));
  t56 = ((int64_t)(((int64_t)gg_div(t54, t55))));
  t57 = ((int64_t)(v1));
  t58 = ((int64_t)(((int64_t)UINT64_C(8))));
  t59 = ((int64_t)(((int64_t)gg_div(t57, t58))));
  (*t51) = ((float)(((float)(((int64_t)((uint64_t)(t56) + (uint64_t)(t59)))))));
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_28(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(84))));
  goto gg_done;
  }
  }
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(4))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(76))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_27(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(12))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(14))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(4))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_26(int64_t p0, int64_t p1, int64_t p2, gg_slice p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
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
  uint8_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint8_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  uint8_t t33;
  int64_t t34;
  float * t35;
  int64_t t36;
  float * t37;
  int64_t t38;
  float * t39;
  int64_t t40;
  float * t41;
  int64_t t42;
  float * t43;
  int64_t t44;
  float * t45;
  int64_t t46;
  float * t47;
  int64_t t48;
  float * t49;
  int64_t t50;
  float * t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  float * t55;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  gg_slice v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)((v3).len));
  t2 = ((int64_t)(((int64_t)UINT64_C(10))));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(12))));
  t6 = ((t4) != (t5));
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(14))));
  t7 = ((t8) != (t9));
  }
  t10 = t7;
  if (t10) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)UINT64_C(8))));
  t13 = ((t11) < (t12));
  t14 = t13;
  if (!t14) {
  t15 = ((int64_t)(v1));
  t16 = ((int64_t)(((int64_t)UINT64_C(128))));
  t14 = ((t15) > (t16));
  }
  t17 = t14;
  t18 = t17;
  if (!t18) {
  t19 = ((int64_t)(v1));
  t20 = ((int64_t)(((int64_t)UINT64_C(8))));
  t21 = ((int64_t)(((int64_t)gg_mod(t19, t20))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t21) != (t22));
  }
  t23 = t18;
  if (t23) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t24 = ((int64_t)(v0));
  v4 = gg_function_27(t24, base + 0);
  t25 = ((int64_t)(v1));
  t26 = ((int64_t)(((int64_t)UINT64_C(48))));
  t27 = ((t25) >= (t26));
  t28 = t27;
  if (t28) {
  t29 = ((int64_t)(v1));
  t30 = ((int64_t)(((int64_t)UINT64_C(16))));
  t31 = ((int64_t)(((int64_t)gg_mod(t29, t30))));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = ((t31) == (t32));
  }
  t33 = t28;
  if (t33) {
  {
  v5 = ((int64_t)(((int64_t)UINT64_C(32))));
  }
  } else {
  {
  v5 = ((int64_t)(((int64_t)UINT64_C(16))));
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t34 = ((int64_t)UINT64_C(0));
  t35 = ((float *)(v3).data) + gg_index(t34, (v3).len);
  (*t35) = ((float)(((float)(((int64_t)UINT64_C(256))))));
  /* FG_DIRECT_SLICE_INDEX */
  t36 = ((int64_t)UINT64_C(1));
  t37 = ((float *)(v3).data) + gg_index(t36, (v3).len);
  (*t37) = ((float)(((float)(((int64_t)UINT64_C(1))))));
  /* FG_DIRECT_SLICE_INDEX */
  t38 = ((int64_t)UINT64_C(2));
  t39 = ((float *)(v3).data) + gg_index(t38, (v3).len);
  (*t39) = ((float)(((float)(((int64_t)UINT64_C(128))))));
  /* FG_DIRECT_SLICE_INDEX */
  t40 = ((int64_t)UINT64_C(3));
  t41 = ((float *)(v3).data) + gg_index(t40, (v3).len);
  (*t41) = ((float)(((float)(v1))));
  /* FG_DIRECT_SLICE_INDEX */
  t42 = ((int64_t)UINT64_C(4));
  t43 = ((float *)(v3).data) + gg_index(t42, (v3).len);
  (*t43) = ((float)(((float)(v4))));
  /* FG_DIRECT_SLICE_INDEX */
  t44 = ((int64_t)UINT64_C(5));
  t45 = ((float *)(v3).data) + gg_index(t44, (v3).len);
  (*t45) = ((float)(((float)(((int64_t)UINT64_C(256))))));
  /* FG_DIRECT_SLICE_INDEX */
  t46 = ((int64_t)UINT64_C(6));
  t47 = ((float *)(v3).data) + gg_index(t46, (v3).len);
  (*t47) = ((float)(((float)(((int64_t)UINT64_C(1))))));
  /* FG_DIRECT_SLICE_INDEX */
  t48 = ((int64_t)UINT64_C(7));
  t49 = ((float *)(v3).data) + gg_index(t48, (v3).len);
  (*t49) = ((float)(((float)(v5))));
  /* FG_DIRECT_SLICE_INDEX */
  t50 = ((int64_t)UINT64_C(8));
  t51 = ((float *)(v3).data) + gg_index(t50, (v3).len);
  t52 = ((int64_t)(v4));
  t53 = gg_function_28(t52, base + 0);
  (*t51) = ((float)(((float)(t53))));
  /* FG_DIRECT_SLICE_INDEX */
  t54 = ((int64_t)UINT64_C(9));
  t55 = ((float *)(v3).data) + gg_index(t54, (v3).len);
  (*t55) = ((float)(((float)(v2))));
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_25(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) <= (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(((int64_t)UINT64_C(4))));
  t9 = ((t7) <= (t8));
  if (t9) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(4))));
  goto gg_done;
  }
  }
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(((int64_t)UINT64_C(8))));
  t12 = ((t10) <= (t11));
  if (t12) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(2))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_24(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  uint8_t t34;
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
  uint8_t t46;
  int64_t t47;
  int64_t t48;
  uint8_t t49;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(12))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(14))));
  t4 = ((t5) != (t6));
  }
  t7 = t4;
  if (t7) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t8) <= (t9));
  if (t10) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)UINT64_C(890))));
  t13 = ((t11) == (t12));
  if (t13) {
  {
  t14 = ((int64_t)(v2));
  t15 = ((int64_t)(((int64_t)UINT64_C(8))));
  t16 = ((t14) <= (t15));
  if (t16) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  goto gg_done;
  }
  }
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(((int64_t)UINT64_C(1200))));
  t19 = ((t17) == (t18));
  if (t19) {
  {
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(12))));
  t22 = ((t20) == (t21));
  if (t22) {
  {
  t23 = ((int64_t)(v2));
  t24 = ((int64_t)(((int64_t)UINT64_C(5))));
  t25 = ((t23) <= (t24));
  if (t25) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  goto gg_done;
  }
  }
  t26 = ((int64_t)(v2));
  t27 = ((int64_t)(((int64_t)UINT64_C(7))));
  t28 = ((t26) <= (t27));
  if (t28) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  goto gg_done;
  }
  }
  t29 = ((int64_t)(v1));
  t30 = ((int64_t)(((int64_t)UINT64_C(870))));
  t31 = ((t29) == (t30));
  if (t31) {
  {
  t32 = ((int64_t)(v2));
  t33 = ((int64_t)(((int64_t)UINT64_C(1))));
  t34 = ((t32) <= (t33));
  if (t34) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  goto gg_done;
  }
  }
  t35 = ((int64_t)(v1));
  t36 = ((int64_t)(((int64_t)UINT64_C(700))));
  t37 = ((t35) == (t36));
  if (t37) {
  {
  t38 = ((int64_t)(v0));
  t39 = ((int64_t)(((int64_t)UINT64_C(12))));
  t40 = ((t38) == (t39));
  if (t40) {
  {
  t41 = ((int64_t)(v2));
  t42 = ((int64_t)(((int64_t)UINT64_C(5))));
  t43 = ((t41) <= (t42));
  if (t43) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  goto gg_done;
  }
  }
  t44 = ((int64_t)(v2));
  t45 = ((int64_t)(((int64_t)UINT64_C(7))));
  t46 = ((t44) <= (t45));
  if (t46) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  goto gg_done;
  }
  }
  t47 = ((int64_t)(v2));
  t48 = ((int64_t)(((int64_t)UINT64_C(8))));
  t49 = ((t47) <= (t48));
  if (t49) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_22(gg_slice p0, int64_t p1, int64_t base) {
  /* FG_SHARED_BODY:4 */
  return gg_function_4(p0, p1, base);
}
static float gg_function_23(gg_slice p0, int64_t p1, gg_slice p2, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  gg_slice t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  gg_slice t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  gg_slice t21;
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
  int64_t t32;
  int64_t t33;
  gg_slice t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  gg_slice t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
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
  float t64;
  float t65;
  float t66;
  float t67;
  float t68;
  float t69;
  int64_t t70;
  int64_t t71;
  float t72;
  float t73;
  gg_slice v0;
  int64_t v1;
  gg_slice v2;
  int64_t v3;
  int64_t v4;
  float v5;
  float v6;
  int64_t v7;
  float v8;
  float v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(210))));
  v3 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(((int64_t)UINT64_C(8))));
  t5 = ((int64_t)(((int64_t)((uint64_t)(t3) * (uint64_t)(t4)))));
  t6 = ((int64_t)(((int64_t)UINT64_C(36))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  t7 = v0;
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(208))));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  v5 = gg_function_4(t7, t10, base + 0);
  v6 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t11 = ((int64_t)(v7));
  t12 = ((int64_t)(((int64_t)UINT64_C(16))));
  if (!(((t11) < (t12)))) goto label2;
  {
  t13 = v2;
  t14 = ((int64_t)(v4));
  t15 = ((int64_t)(v7));
  t16 = ((int64_t)(((int64_t)UINT64_C(2))));
  t17 = ((int64_t)(((int64_t)gg_div(t15, t16))));
  t18 = ((int64_t)(((int64_t)UINT64_C(36))));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t19)))));
  v8 = gg_function_4(t13, t20, base + 0);
  t21 = v0;
  t22 = ((int64_t)(v3));
  t23 = ((int64_t)(((int64_t)UINT64_C(192))));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t22) + (uint64_t)(t23)))));
  t25 = ((int64_t)(v7));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) + (uint64_t)(t25)))));
  t27 = gg_function_11(t21, t26, base + 0);
  v9 = ((float)(((float)(t27))));
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t28 = ((int64_t)(v11));
  t29 = ((int64_t)(((int64_t)UINT64_C(16))));
  if (!(((t28) < (t29)))) goto label5;
  {
  t30 = ((int64_t)(v7));
  t31 = ((int64_t)(((int64_t)UINT64_C(16))));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) * (uint64_t)(t31)))));
  t33 = ((int64_t)(v11));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t32) + (uint64_t)(t33)))));
  t34 = v0;
  t35 = ((int64_t)(v3));
  t36 = ((int64_t)(v12));
  t37 = gg_function_12(t34, t35, t36, base + 0);
  t38 = ((int64_t)(t37));
  t39 = ((int64_t)(((int64_t)UINT64_C(32))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t38) - (uint64_t)(t39)))));
  t40 = v2;
  t41 = ((int64_t)(v4));
  t42 = ((int64_t)(v7));
  t43 = ((int64_t)(((int64_t)UINT64_C(2))));
  t44 = ((int64_t)(((int64_t)gg_div(t42, t43))));
  t45 = ((int64_t)(((int64_t)UINT64_C(36))));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) * (uint64_t)(t45)))));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t41) + (uint64_t)(t46)))));
  t48 = ((int64_t)(((int64_t)UINT64_C(4))));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) + (uint64_t)(t48)))));
  t50 = ((int64_t)(v7));
  t51 = ((int64_t)(((int64_t)UINT64_C(2))));
  t52 = ((int64_t)(((int64_t)gg_mod(t50, t51))));
  t53 = ((int64_t)(((int64_t)UINT64_C(16))));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) * (uint64_t)(t53)))));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t49) + (uint64_t)(t54)))));
  t56 = ((int64_t)(v11));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) + (uint64_t)(t56)))));
  v14 = gg_function_11(t40, t57, base + 0);
  t58 = v10;
  t59 = ((int64_t)(v13));
  t60 = ((int64_t)(v14));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t59) * (uint64_t)(t60)))));
  v10 = ((int64_t)((uint64_t)(t58) + (uint64_t)(t61)));
  t62 = v11;
  t63 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t62) + (uint64_t)(t63)));
  }
  goto label4;
label5:;
  t64 = ((float)(v6));
  t65 = ((float)(v8));
  t66 = ((float)(((float)(v10))));
  t67 = ((float)(v9));
  t68 = ((float)(((t66) * (t67))));
  t69 = ((float)(((t65) * (t68))));
  v6 = ((float)(((t64) + (t69))));
  t70 = v7;
  t71 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)((uint64_t)(t70) + (uint64_t)(t71)));
  }
  goto label1;
label2:;
  t72 = ((float)(v5));
  t73 = ((float)(v6));
  t0 = ((float)(((t72) * (t73))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_21(gg_slice p0, int64_t p1, gg_slice p2, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  gg_slice t7;
  int64_t t8;
  gg_slice t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  gg_slice t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  gg_slice t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  uint8_t * t37;
  int64_t t38;
  int64_t t39;
  uint8_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  gg_slice t45;
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
  int64_t t56;
  int64_t t57;
  int64_t t58;
  gg_slice t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  gg_slice t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  float t67;
  float t68;
  float t69;
  float t70;
  float t71;
  float t72;
  float t73;
  float t74;
  float t75;
  float t76;
  float t77;
  float t78;
  int64_t t79;
  int64_t t80;
  float t81;
  float t82;
  float t83;
  float t84;
  float t85;
  float t86;
  gg_slice v0;
  int64_t v1;
  gg_slice v2;
  int64_t v3;
  int64_t v4;
  float v5;
  float v6;
  float v7;
  float v8;
  int64_t v9;
  int64_t v10;
  float v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  float v20;
  float v21;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(144))));
  v3 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(((int64_t)UINT64_C(8))));
  t5 = ((int64_t)(((int64_t)((uint64_t)(t3) * (uint64_t)(t4)))));
  t6 = ((int64_t)(((int64_t)UINT64_C(36))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  t7 = v0;
  t8 = ((int64_t)(v3));
  v5 = gg_function_4(t7, t8, base + 0);
  t9 = v0;
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))));
  v6 = gg_function_4(t9, t12, base + 0);
  v7 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  v8 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t13 = ((int64_t)(v9));
  t14 = ((int64_t)(((int64_t)UINT64_C(8))));
  if (!(((t13) < (t14)))) goto label2;
  {
  t15 = ((int64_t)(v4));
  t16 = ((int64_t)(v9));
  t17 = ((int64_t)(((int64_t)UINT64_C(36))));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t15) + (uint64_t)(t18)))));
  t19 = v2;
  t20 = ((int64_t)(v10));
  v11 = gg_function_4(t19, t20, base + 0);
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
  v13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((int64_t)(v9));
  t22 = ((int64_t)(((int64_t)UINT64_C(2))));
  v14 = ((int64_t)(((int64_t)gg_div(t21, t22))));
  t23 = ((int64_t)(v9));
  t24 = ((int64_t)(((int64_t)UINT64_C(2))));
  v15 = ((int64_t)(((int64_t)gg_mod(t23, t24))));
  v16 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t25 = ((int64_t)(v16));
  t26 = ((int64_t)(((int64_t)UINT64_C(32))));
  if (!(((t25) < (t26)))) goto label5;
  {
  t27 = v0;
  t28 = ((int64_t)(v3));
  t29 = ((int64_t)(((int64_t)UINT64_C(16))));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) + (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)UINT64_C(32))));
  t32 = ((int64_t)(v14));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) * (uint64_t)(t32)))));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t33)))));
  t35 = ((int64_t)(v16));
  t36 = ((int64_t)((uint64_t)(t34) + (uint64_t)(t35)));
  t37 = ((uint8_t *)t27.data) + gg_index(t36, t27.len);
  v17 = ((int64_t)(((int64_t)((*t37)))));
  t38 = ((int64_t)(v15));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = ((t38) == (t39));
  if (t40) {
  {
  t41 = ((int64_t)(v17));
  t42 = ((int64_t)(((int64_t)UINT64_C(15))));
  v18 = ((int64_t)(((t41) & (t42))));
  }
  } else {
  {
  t43 = ((int64_t)(v17));
  t44 = ((int64_t)(((int64_t)UINT64_C(4))));
  v18 = ((int64_t)(((t43) >> ((uint64_t)(t44) & 63))));
  }
  }
  t45 = v2;
  t46 = ((int64_t)(v10));
  t47 = ((int64_t)(((int64_t)UINT64_C(4))));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) + (uint64_t)(t47)))));
  t49 = ((int64_t)(v16));
  t50 = ((int64_t)(((int64_t)((uint64_t)(t48) + (uint64_t)(t49)))));
  v19 = gg_function_11(t45, t50, base + 0);
  t51 = v12;
  t52 = ((int64_t)(v18));
  t53 = ((int64_t)(v19));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) * (uint64_t)(t53)))));
  v12 = ((int64_t)((uint64_t)(t51) + (uint64_t)(t54)));
  t55 = v13;
  t56 = ((int64_t)(v19));
  v13 = ((int64_t)((uint64_t)(t55) + (uint64_t)(t56)));
  t57 = v16;
  t58 = ((int64_t)(((int64_t)UINT64_C(1))));
  v16 = ((int64_t)((uint64_t)(t57) + (uint64_t)(t58)));
  }
  goto label4;
label5:;
  t59 = v0;
  t60 = ((int64_t)(v3));
  t61 = ((int64_t)(v9));
  t62 = gg_function_7(t59, t60, t61, base + 0);
  v20 = ((float)(((float)(t62))));
  t63 = v0;
  t64 = ((int64_t)(v3));
  t65 = ((int64_t)(v9));
  t66 = gg_function_8(t63, t64, t65, base + 0);
  v21 = ((float)(((float)(t66))));
  t67 = ((float)(v7));
  t68 = ((float)(v11));
  t69 = ((float)(((float)(v12))));
  t70 = ((float)(v20));
  t71 = ((float)(((t69) * (t70))));
  t72 = ((float)(((t68) * (t71))));
  v7 = ((float)(((t67) + (t72))));
  t73 = ((float)(v8));
  t74 = ((float)(v11));
  t75 = ((float)(((float)(v13))));
  t76 = ((float)(v21));
  t77 = ((float)(((t75) * (t76))));
  t78 = ((float)(((t74) * (t77))));
  v8 = ((float)(((t73) + (t78))));
  t79 = v9;
  t80 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)((uint64_t)(t79) + (uint64_t)(t80)));
  }
  goto label1;
label2:;
  t81 = ((float)(v5));
  t82 = ((float)(v7));
  t83 = ((float)(((t81) * (t82))));
  t84 = ((float)(v6));
  t85 = ((float)(v8));
  t86 = ((float)(((t84) * (t85))));
  t0 = ((float)(((t83) - (t86))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint32_t gg_function_15(gg_slice p0, int64_t p1, int64_t base) {
  uint32_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  gg_slice t9;
  int64_t t10;
  uint16_t t11;
  uint32_t t12;
  gg_slice t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  uint16_t t17;
  uint32_t t18;
  int64_t t19;
  uint32_t t20;
  gg_slice v0;
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
  t6 = ((int64_t)(((int64_t)UINT64_C(3))));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t5) + (uint64_t)(t6)))));
  t8 = ((int64_t)((v0).len));
  t4 = ((t7) < (t8));
  }
  if (!(t4)) gg_fail("assertion failed");
  t9 = v0;
  t10 = ((int64_t)(v1));
  t11 = gg_function_5(t9, t10, base + 0);
  t12 = ((uint32_t)(((uint32_t)(t11))));
  t13 = v0;
  t14 = ((int64_t)(v1));
  t15 = ((int64_t)(((int64_t)UINT64_C(2))));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t15)))));
  t17 = gg_function_5(t13, t16, base + 0);
  t18 = ((uint32_t)(((uint32_t)(t17))));
  t19 = ((int64_t)(((int64_t)UINT64_C(16))));
  t20 = ((uint32_t)(((uint32_t)((uint64_t)(t18) << ((uint64_t)(t19) & 31)))));
  t0 = ((uint32_t)(((t12) | (t20))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_17(gg_slice p0, int64_t p1, int64_t base) {
  float t0;
  gg_slice t1;
  int64_t t2;
  uint32_t t3;
  float t4;
  gg_slice v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = v0;
  t2 = ((int64_t)(v1));
  t3 = gg_function_15(t1, t2, base + 0);
  t4 = fgm_from_bits(t3);
  t0 = ((float)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_20(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base) {
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
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  uint8_t t34;
  uint8_t t35;
  int64_t t36;
  int64_t t37;
  uint8_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  float t43;
  gg_slice t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  float t52;
  float t53;
  gg_slice t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  float t62;
  float t63;
  float t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  float * t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  gg_slice v0;
  gg_slice v1;
  gg_slice v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  float v8;
  int64_t v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) <= (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v4));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) <= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (!t8) {
  t9 = ((int64_t)(v5));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t9) <= (t10));
  }
  t11 = t8;
  t12 = t11;
  if (!t12) {
  t13 = ((int64_t)(v6));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t13) < (t14));
  }
  t15 = t12;
  t16 = t15;
  if (!t16) {
  t17 = ((int64_t)(v6));
  t18 = ((int64_t)(v3));
  t16 = ((t17) >= (t18));
  }
  t19 = t16;
  if (t19) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t20 = ((int64_t)((v0).len));
  t21 = ((int64_t)(v3));
  t22 = ((int64_t)(v5));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) * (uint64_t)(t22)))));
  t24 = ((int64_t)(((int64_t)UINT64_C(2))));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) * (uint64_t)(t24)))));
  t26 = ((t20) < (t25));
  t27 = t26;
  if (!t27) {
  t28 = ((int64_t)((v1).len));
  t29 = ((int64_t)(v5));
  t30 = ((int64_t)(v4));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t29) * (uint64_t)(t30)))));
  t32 = ((int64_t)(((int64_t)UINT64_C(4))));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) * (uint64_t)(t32)))));
  t27 = ((t28) < (t33));
  }
  t34 = t27;
  t35 = t34;
  if (!t35) {
  t36 = ((int64_t)((v2).len));
  t37 = ((int64_t)(v4));
  t35 = ((t36) < (t37));
  }
  t38 = t35;
  if (t38) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t39 = ((int64_t)(v7));
  t40 = ((int64_t)(v4));
  if (!(((t39) < (t40)))) goto label2;
  {
  v8 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t41 = ((int64_t)(v9));
  t42 = ((int64_t)(v5));
  if (!(((t41) < (t42)))) goto label5;
  {
  t43 = ((float)(v8));
  t44 = v0;
  t45 = ((int64_t)(v6));
  t46 = ((int64_t)(v5));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t45) * (uint64_t)(t46)))));
  t48 = ((int64_t)(v9));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) + (uint64_t)(t48)))));
  t50 = ((int64_t)(((int64_t)UINT64_C(2))));
  t51 = ((int64_t)(((int64_t)((uint64_t)(t49) * (uint64_t)(t50)))));
  t52 = gg_function_4(t44, t51, base + 0);
  t53 = ((float)(t52));
  t54 = v1;
  t55 = ((int64_t)(v9));
  t56 = ((int64_t)(v4));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) * (uint64_t)(t56)))));
  t58 = ((int64_t)(v7));
  t59 = ((int64_t)(((int64_t)((uint64_t)(t57) + (uint64_t)(t58)))));
  t60 = ((int64_t)(((int64_t)UINT64_C(4))));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t59) * (uint64_t)(t60)))));
  t62 = gg_function_17(t54, t61, base + 0);
  t63 = ((float)(t62));
  t64 = ((float)(((t53) * (t63))));
  v8 = ((float)(((t43) + (t64))));
  t65 = v9;
  t66 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)((uint64_t)(t65) + (uint64_t)(t66)));
  }
  goto label4;
label5:;
  /* FG_DIRECT_SLICE_INDEX */
  t67 = v7;
  t68 = ((float *)(v2).data) + gg_index(t67, (v2).len);
  (*t68) = ((float)(v8));
  t69 = v7;
  t70 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)((uint64_t)(t69) + (uint64_t)(t70)));
  }
  goto label1;
label2:;
  t71 = ((int64_t)(((int64_t)UINT64_C(44))));
  t72 = ((int64_t)(v6));
  fgm_witness(t71, t72);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_14(gg_slice p0, int64_t p1, int64_t base) {
  /* FG_SHARED_BODY:17 */
  return gg_function_17(p0, p1, base);
}
static int64_t gg_function_19(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base) {
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
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  uint8_t t34;
  uint8_t t35;
  int64_t t36;
  int64_t t37;
  uint8_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  float t43;
  gg_slice t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  float t52;
  float t53;
  gg_slice t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  float t62;
  float t63;
  float t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  float * t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  gg_slice v0;
  gg_slice v1;
  gg_slice v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  float v8;
  int64_t v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) <= (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v4));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) <= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (!t8) {
  t9 = ((int64_t)(v5));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t9) <= (t10));
  }
  t11 = t8;
  t12 = t11;
  if (!t12) {
  t13 = ((int64_t)(v6));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t13) < (t14));
  }
  t15 = t12;
  t16 = t15;
  if (!t16) {
  t17 = ((int64_t)(v6));
  t18 = ((int64_t)(v3));
  t16 = ((t17) >= (t18));
  }
  t19 = t16;
  if (t19) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t20 = ((int64_t)((v0).len));
  t21 = ((int64_t)(v3));
  t22 = ((int64_t)(v5));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) * (uint64_t)(t22)))));
  t24 = ((int64_t)(((int64_t)UINT64_C(4))));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) * (uint64_t)(t24)))));
  t26 = ((t20) < (t25));
  t27 = t26;
  if (!t27) {
  t28 = ((int64_t)((v1).len));
  t29 = ((int64_t)(v5));
  t30 = ((int64_t)(v4));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t29) * (uint64_t)(t30)))));
  t32 = ((int64_t)(((int64_t)UINT64_C(4))));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) * (uint64_t)(t32)))));
  t27 = ((t28) < (t33));
  }
  t34 = t27;
  t35 = t34;
  if (!t35) {
  t36 = ((int64_t)((v2).len));
  t37 = ((int64_t)(v4));
  t35 = ((t36) < (t37));
  }
  t38 = t35;
  if (t38) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t39 = ((int64_t)(v7));
  t40 = ((int64_t)(v4));
  if (!(((t39) < (t40)))) goto label2;
  {
  v8 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t41 = ((int64_t)(v9));
  t42 = ((int64_t)(v5));
  if (!(((t41) < (t42)))) goto label5;
  {
  t43 = ((float)(v8));
  t44 = v0;
  t45 = ((int64_t)(v6));
  t46 = ((int64_t)(v5));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t45) * (uint64_t)(t46)))));
  t48 = ((int64_t)(v9));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) + (uint64_t)(t48)))));
  t50 = ((int64_t)(((int64_t)UINT64_C(4))));
  t51 = ((int64_t)(((int64_t)((uint64_t)(t49) * (uint64_t)(t50)))));
  t52 = gg_function_17(t44, t51, base + 0);
  t53 = ((float)(t52));
  t54 = v1;
  t55 = ((int64_t)(v9));
  t56 = ((int64_t)(v4));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) * (uint64_t)(t56)))));
  t58 = ((int64_t)(v7));
  t59 = ((int64_t)(((int64_t)((uint64_t)(t57) + (uint64_t)(t58)))));
  t60 = ((int64_t)(((int64_t)UINT64_C(4))));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t59) * (uint64_t)(t60)))));
  t62 = gg_function_17(t54, t61, base + 0);
  t63 = ((float)(t62));
  t64 = ((float)(((t53) * (t63))));
  v8 = ((float)(((t43) + (t64))));
  t65 = v9;
  t66 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)((uint64_t)(t65) + (uint64_t)(t66)));
  }
  goto label4;
label5:;
  /* FG_DIRECT_SLICE_INDEX */
  t67 = v7;
  t68 = ((float *)(v2).data) + gg_index(t67, (v2).len);
  (*t68) = ((float)(v8));
  t69 = v7;
  t70 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)((uint64_t)(t69) + (uint64_t)(t70)));
  }
  goto label1;
label2:;
  t71 = ((int64_t)(((int64_t)UINT64_C(43))));
  t72 = ((int64_t)(v6));
  fgm_witness(t71, t72);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_18(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t p4, int64_t base) {
  int64_t t0;
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
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  uint8_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  float t25;
  gg_slice t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  float t34;
  float t35;
  gg_slice t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  float t40;
  float t41;
  float t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  float * t46;
  int64_t t47;
  int64_t t48;
  gg_slice v0;
  gg_slice v1;
  gg_slice v2;
  int64_t v3;
  int64_t v4;
  float v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) <= (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v4));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) < (t6));
  }
  t7 = t4;
  if (t7) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t8 = ((int64_t)((v0).len));
  t9 = ((int64_t)(v4));
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t10)))));
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((int64_t)(((int64_t)UINT64_C(2))));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))));
  t16 = ((t8) < (t15));
  t17 = t16;
  if (!t17) {
  t18 = ((int64_t)((v1).len));
  t19 = ((int64_t)(v3));
  t20 = ((int64_t)(((int64_t)UINT64_C(4))));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t17 = ((t18) < (t21));
  }
  t22 = t17;
  if (t22) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v5 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t23 = ((int64_t)(v6));
  t24 = ((int64_t)(v3));
  if (!(((t23) < (t24)))) goto label2;
  {
  t25 = ((float)(v5));
  t26 = v0;
  t27 = ((int64_t)(v4));
  t28 = ((int64_t)(v3));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) * (uint64_t)(t28)))));
  t30 = ((int64_t)(v6));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t30)))));
  t32 = ((int64_t)(((int64_t)UINT64_C(2))));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) * (uint64_t)(t32)))));
  t34 = gg_function_4(t26, t33, base + 0);
  t35 = ((float)(t34));
  t36 = v1;
  t37 = ((int64_t)(v6));
  t38 = ((int64_t)(((int64_t)UINT64_C(4))));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) * (uint64_t)(t38)))));
  t40 = gg_function_17(t36, t39, base + 0);
  t41 = ((float)(t40));
  t42 = ((float)(((t35) * (t41))));
  v5 = ((float)(((t25) + (t42))));
  t43 = v6;
  t44 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t43) + (uint64_t)(t44)));
  }
  goto label1;
label2:;
  /* FG_DIRECT_SLICE_INDEX */
  t45 = ((int64_t)UINT64_C(0));
  t46 = ((float *)(v2).data) + gg_index(t45, (v2).len);
  (*t46) = ((float)(v5));
  t47 = ((int64_t)(((int64_t)UINT64_C(42))));
  t48 = ((int64_t)(v4));
  fgm_witness(t47, t48);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_13(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t p4, int64_t base) {
  int64_t t0;
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
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  uint8_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  float t25;
  gg_slice t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  float t34;
  float t35;
  gg_slice t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  float t40;
  float t41;
  float t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  float * t46;
  int64_t t47;
  int64_t t48;
  gg_slice v0;
  gg_slice v1;
  gg_slice v2;
  int64_t v3;
  int64_t v4;
  float v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) <= (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v4));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) < (t6));
  }
  t7 = t4;
  if (t7) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t8 = ((int64_t)((v0).len));
  t9 = ((int64_t)(v4));
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t10)))));
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((int64_t)(((int64_t)UINT64_C(4))));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))));
  t16 = ((t8) < (t15));
  t17 = t16;
  if (!t17) {
  t18 = ((int64_t)((v1).len));
  t19 = ((int64_t)(v3));
  t20 = ((int64_t)(((int64_t)UINT64_C(4))));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t17 = ((t18) < (t21));
  }
  t22 = t17;
  if (t22) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v5 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t23 = ((int64_t)(v6));
  t24 = ((int64_t)(v3));
  if (!(((t23) < (t24)))) goto label2;
  {
  t25 = ((float)(v5));
  t26 = v0;
  t27 = ((int64_t)(v4));
  t28 = ((int64_t)(v3));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) * (uint64_t)(t28)))));
  t30 = ((int64_t)(v6));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t30)))));
  t32 = ((int64_t)(((int64_t)UINT64_C(4))));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) * (uint64_t)(t32)))));
  t34 = gg_function_17(t26, t33, base + 0);
  t35 = ((float)(t34));
  t36 = v1;
  t37 = ((int64_t)(v6));
  t38 = ((int64_t)(((int64_t)UINT64_C(4))));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) * (uint64_t)(t38)))));
  t40 = gg_function_17(t36, t39, base + 0);
  t41 = ((float)(t40));
  t42 = ((float)(((t35) * (t41))));
  v5 = ((float)(((t25) + (t42))));
  t43 = v6;
  t44 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t43) + (uint64_t)(t44)));
  }
  goto label1;
label2:;
  /* FG_DIRECT_SLICE_INDEX */
  t45 = ((int64_t)UINT64_C(0));
  t46 = ((float *)(v2).data) + gg_index(t45, (v2).len);
  (*t46) = ((float)(v5));
  t47 = ((int64_t)(((int64_t)UINT64_C(41))));
  t48 = ((int64_t)(v4));
  fgm_witness(t47, t48);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_1(int64_t p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t base) {
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
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  uint8_t t20;
  int64_t t21;
  float * t22;
  gg_slice t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
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
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  uint8_t t45;
  int64_t t46;
  float * t47;
  gg_slice t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  uint8_t t55;
  gg_slice t56;
  gg_slice t57;
  gg_slice t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  uint8_t t63;
  gg_slice t64;
  gg_slice t65;
  gg_slice t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  uint8_t t71;
  gg_slice t72;
  gg_slice t73;
  gg_slice t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  uint8_t t81;
  gg_slice t82;
  gg_slice t83;
  gg_slice t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  uint8_t t91;
  int64_t t92;
  int64_t t93;
  uint8_t t94;
  uint8_t t95;
  int64_t t96;
  int64_t t97;
  uint8_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  uint8_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  uint8_t t115;
  int64_t t116;
  float * t117;
  gg_slice t118;
  int64_t t119;
  gg_slice t120;
  int64_t t121;
  int64_t t122;
  uint8_t t123;
  int64_t t124;
  int64_t t125;
  uint8_t t126;
  uint8_t t127;
  int64_t t128;
  int64_t t129;
  uint8_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  uint8_t t137;
  uint8_t t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  uint8_t t147;
  int64_t t148;
  float * t149;
  gg_slice t150;
  int64_t t151;
  gg_slice t152;
  int64_t t153;
  int64_t t154;
  uint8_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  uint8_t t161;
  int64_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  uint8_t t167;
  int64_t t168;
  int64_t t169;
  int64_t t170;
  gg_slice t171;
  int64_t t172;
  int64_t t173;
  uint8_t t174;
  int64_t t175;
  int64_t t176;
  gg_slice t177;
  int64_t t178;
  int64_t t179;
  uint8_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  uint8_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  gg_slice t188;
  int64_t t189;
  int64_t t190;
  uint8_t t191;
  gg_slice t192;
  gg_slice t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  uint8_t t197;
  gg_slice t198;
  gg_slice t199;
  int64_t t200;
  int64_t v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  fgm_witness(t1, t2);
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) == (t4));
  if (t5) {
  {
  t6 = ((int64_t)((v3).len));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  t8 = ((t6) < (t7));
  if (t8) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t9 = ((int64_t)(v4));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) < (t10));
  if (t11) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)UINT64_C(256))));
  t14 = ((int64_t)(((int64_t)gg_div(t12, t13))));
  t15 = ((int64_t)(((int64_t)UINT64_C(144))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t16 = ((int64_t)((v1).len));
  t17 = ((int64_t)(v8));
  t18 = ((int64_t)(((int64_t)UINT64_C(144))));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t18)))));
  t20 = ((t16) < (t19));
  if (t20) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t21 = ((int64_t)UINT64_C(0));
  t22 = ((float *)(v3).data) + gg_index(t21, (v3).len);
  t23 = v1;
  t24 = ((int64_t)(v8));
  t25 = ((int64_t)(v4));
  t26 = ((int64_t)(((int64_t)UINT64_C(256))));
  t27 = ((int64_t)(((int64_t)gg_mod(t25, t26))));
  (*t22) = gg_function_3(t23, t24, t27, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  t30 = ((t28) == (t29));
  if (t30) {
  {
  t31 = ((int64_t)((v3).len));
  t32 = ((int64_t)(((int64_t)UINT64_C(1))));
  t33 = ((t31) < (t32));
  if (t33) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t34 = ((int64_t)(v4));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((t34) < (t35));
  if (t36) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t37 = ((int64_t)(v4));
  t38 = ((int64_t)(((int64_t)UINT64_C(256))));
  t39 = ((int64_t)(((int64_t)gg_div(t37, t38))));
  t40 = ((int64_t)(((int64_t)UINT64_C(210))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t39) * (uint64_t)(t40)))));
  t41 = ((int64_t)((v1).len));
  t42 = ((int64_t)(v9));
  t43 = ((int64_t)(((int64_t)UINT64_C(210))));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) + (uint64_t)(t43)))));
  t45 = ((t41) < (t44));
  if (t45) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t46 = ((int64_t)UINT64_C(0));
  t47 = ((float *)(v3).data) + gg_index(t46, (v3).len);
  t48 = v1;
  t49 = ((int64_t)(v9));
  t50 = ((int64_t)(v4));
  t51 = ((int64_t)(((int64_t)UINT64_C(256))));
  t52 = ((int64_t)(((int64_t)gg_mod(t50, t51))));
  (*t47) = gg_function_9(t48, t49, t52, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t53 = ((int64_t)(v0));
  t54 = ((int64_t)(((int64_t)UINT64_C(2))));
  t55 = ((t53) == (t54));
  if (t55) {
  {
  t56 = v1;
  t57 = v2;
  t58 = v3;
  t59 = ((int64_t)(v5));
  t60 = ((int64_t)(v6));
  t0 = gg_function_13(t56, t57, t58, t59, t60, base + 0);
  goto gg_done;
  }
  }
  t61 = ((int64_t)(v0));
  t62 = ((int64_t)(((int64_t)UINT64_C(3))));
  t63 = ((t61) == (t62));
  if (t63) {
  {
  t64 = v1;
  t65 = v2;
  t66 = v3;
  t67 = ((int64_t)(v5));
  t68 = ((int64_t)(v6));
  t0 = gg_function_18(t64, t65, t66, t67, t68, base + 0);
  goto gg_done;
  }
  }
  t69 = ((int64_t)(v0));
  t70 = ((int64_t)(((int64_t)UINT64_C(4))));
  t71 = ((t69) == (t70));
  if (t71) {
  {
  t72 = v1;
  t73 = v2;
  t74 = v3;
  t75 = ((int64_t)(v4));
  t76 = ((int64_t)(v5));
  t77 = ((int64_t)(v6));
  t78 = ((int64_t)(v7));
  t0 = gg_function_19(t72, t73, t74, t75, t76, t77, t78, base + 0);
  goto gg_done;
  }
  }
  t79 = ((int64_t)(v0));
  t80 = ((int64_t)(((int64_t)UINT64_C(5))));
  t81 = ((t79) == (t80));
  if (t81) {
  {
  t82 = v1;
  t83 = v2;
  t84 = v3;
  t85 = ((int64_t)(v4));
  t86 = ((int64_t)(v5));
  t87 = ((int64_t)(v6));
  t88 = ((int64_t)(v7));
  t0 = gg_function_20(t82, t83, t84, t85, t86, t87, t88, base + 0);
  goto gg_done;
  }
  }
  t89 = ((int64_t)(v0));
  t90 = ((int64_t)(((int64_t)UINT64_C(6))));
  t91 = ((t89) == (t90));
  if (t91) {
  {
  t92 = ((int64_t)((v3).len));
  t93 = ((int64_t)(((int64_t)UINT64_C(1))));
  t94 = ((t92) < (t93));
  t95 = t94;
  if (!t95) {
  t96 = ((int64_t)(v4));
  t97 = ((int64_t)(((int64_t)UINT64_C(0))));
  t95 = ((t96) < (t97));
  }
  t98 = t95;
  if (t98) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t99 = ((int64_t)((v1).len));
  t100 = ((int64_t)(v4));
  t101 = ((int64_t)(((int64_t)UINT64_C(1))));
  t102 = ((int64_t)(((int64_t)((uint64_t)(t100) + (uint64_t)(t101)))));
  t103 = ((int64_t)(((int64_t)UINT64_C(144))));
  t104 = ((int64_t)(((int64_t)((uint64_t)(t102) * (uint64_t)(t103)))));
  t105 = ((t99) < (t104));
  t106 = t105;
  if (!t106) {
  t107 = ((int64_t)((v2).len));
  t108 = ((int64_t)(v4));
  t109 = ((int64_t)(((int64_t)UINT64_C(1))));
  t110 = ((int64_t)(((int64_t)((uint64_t)(t108) + (uint64_t)(t109)))));
  t111 = ((int64_t)(((int64_t)UINT64_C(8))));
  t112 = ((int64_t)(((int64_t)((uint64_t)(t110) * (uint64_t)(t111)))));
  t113 = ((int64_t)(((int64_t)UINT64_C(36))));
  t114 = ((int64_t)(((int64_t)((uint64_t)(t112) * (uint64_t)(t113)))));
  t106 = ((t107) < (t114));
  }
  t115 = t106;
  if (t115) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t116 = ((int64_t)UINT64_C(0));
  t117 = ((float *)(v3).data) + gg_index(t116, (v3).len);
  t118 = v1;
  t119 = ((int64_t)(v4));
  t120 = v2;
  (*t117) = gg_function_21(t118, t119, t120, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t121 = ((int64_t)(v0));
  t122 = ((int64_t)(((int64_t)UINT64_C(7))));
  t123 = ((t121) == (t122));
  if (t123) {
  {
  t124 = ((int64_t)((v3).len));
  t125 = ((int64_t)(((int64_t)UINT64_C(1))));
  t126 = ((t124) < (t125));
  t127 = t126;
  if (!t127) {
  t128 = ((int64_t)(v4));
  t129 = ((int64_t)(((int64_t)UINT64_C(0))));
  t127 = ((t128) < (t129));
  }
  t130 = t127;
  if (t130) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t131 = ((int64_t)((v1).len));
  t132 = ((int64_t)(v4));
  t133 = ((int64_t)(((int64_t)UINT64_C(1))));
  t134 = ((int64_t)(((int64_t)((uint64_t)(t132) + (uint64_t)(t133)))));
  t135 = ((int64_t)(((int64_t)UINT64_C(210))));
  t136 = ((int64_t)(((int64_t)((uint64_t)(t134) * (uint64_t)(t135)))));
  t137 = ((t131) < (t136));
  t138 = t137;
  if (!t138) {
  t139 = ((int64_t)((v2).len));
  t140 = ((int64_t)(v4));
  t141 = ((int64_t)(((int64_t)UINT64_C(1))));
  t142 = ((int64_t)(((int64_t)((uint64_t)(t140) + (uint64_t)(t141)))));
  t143 = ((int64_t)(((int64_t)UINT64_C(8))));
  t144 = ((int64_t)(((int64_t)((uint64_t)(t142) * (uint64_t)(t143)))));
  t145 = ((int64_t)(((int64_t)UINT64_C(36))));
  t146 = ((int64_t)(((int64_t)((uint64_t)(t144) * (uint64_t)(t145)))));
  t138 = ((t139) < (t146));
  }
  t147 = t138;
  if (t147) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t148 = ((int64_t)UINT64_C(0));
  t149 = ((float *)(v3).data) + gg_index(t148, (v3).len);
  t150 = v1;
  t151 = ((int64_t)(v4));
  t152 = v2;
  (*t149) = gg_function_23(t150, t151, t152, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t153 = ((int64_t)(v0));
  t154 = ((int64_t)(((int64_t)UINT64_C(8))));
  t155 = ((t153) == (t154));
  if (t155) {
  {
  t156 = ((int64_t)(v4));
  t157 = ((int64_t)(v5));
  t158 = ((int64_t)(v6));
  t0 = gg_function_24(t156, t157, t158, base + 0);
  goto gg_done;
  }
  }
  t159 = ((int64_t)(v0));
  t160 = ((int64_t)(((int64_t)UINT64_C(9))));
  t161 = ((t159) == (t160));
  if (t161) {
  {
  t162 = ((int64_t)(v4));
  t163 = ((int64_t)(v5));
  t164 = ((int64_t)(v6));
  t0 = gg_function_25(t162, t163, t164, base + 0);
  goto gg_done;
  }
  }
  t165 = ((int64_t)(v0));
  t166 = ((int64_t)(((int64_t)UINT64_C(10))));
  t167 = ((t165) == (t166));
  if (t167) {
  {
  t168 = ((int64_t)(v4));
  t169 = ((int64_t)(v5));
  t170 = ((int64_t)(v6));
  t171 = v3;
  t0 = gg_function_26(t168, t169, t170, t171, base + 0);
  goto gg_done;
  }
  }
  t172 = ((int64_t)(v0));
  t173 = ((int64_t)(((int64_t)UINT64_C(11))));
  t174 = ((t172) == (t173));
  if (t174) {
  {
  t175 = ((int64_t)(v4));
  t176 = ((int64_t)(v5));
  t177 = v3;
  t0 = gg_function_29(t175, t176, t177, base + 0);
  goto gg_done;
  }
  }
  t178 = ((int64_t)(v0));
  t179 = ((int64_t)(((int64_t)UINT64_C(12))));
  t180 = ((t178) == (t179));
  if (t180) {
  {
  t181 = ((int64_t)(v4));
  t0 = gg_function_30(t181, base + 0);
  goto gg_done;
  }
  }
  t182 = ((int64_t)(v0));
  t183 = ((int64_t)(((int64_t)UINT64_C(15))));
  t184 = ((t182) == (t183));
  if (t184) {
  {
  t185 = ((int64_t)(v4));
  t186 = ((int64_t)(v5));
  t187 = ((int64_t)(v6));
  t188 = v3;
  t0 = gg_function_31(t185, t186, t187, t188, base + 0);
  goto gg_done;
  }
  }
  t189 = ((int64_t)(v0));
  t190 = ((int64_t)(((int64_t)UINT64_C(13))));
  t191 = ((t189) == (t190));
  if (t191) {
  {
  t192 = v1;
  t193 = v3;
  t194 = ((int64_t)(((int64_t)UINT64_C(12))));
  t0 = gg_function_32(t192, t193, t194, base + 0);
  goto gg_done;
  }
  }
  t195 = ((int64_t)(v0));
  t196 = ((int64_t)(((int64_t)UINT64_C(14))));
  t197 = ((t195) == (t196));
  if (t197) {
  {
  t198 = v1;
  t199 = v3;
  t200 = ((int64_t)(((int64_t)UINT64_C(14))));
  t0 = gg_function_32(t198, t199, t200, base + 0);
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_0(int64_t base) {
  uint8_t t0;
  uint8_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  float t6;
  float t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  gg_slice t13;
  gg_slice t14;
  gg_slice t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  gg_header t24;
  int64_t t25;
  gg_type_2 v0;
  gg_type_2 v1;
  gg_type_4 v2;
  int64_t v3;
  int64_t v4;
  {
  memset(&t0, 0, sizeof(t0));
  t1 = ((uint8_t)(t0));
  for (t2=0; t2<4096; t2++) (v0).data[t2] = t1;
  memset(&t3, 0, sizeof(t3));
  t4 = ((uint8_t)(t3));
  for (t5=0; t5<4096; t5++) (v1).data[t5] = t4;
  memset(&t6, 0, sizeof(t6));
  t7 = ((float)(t6));
  for (t8=0; t8<1024; t8++) (v2).data[t8] = t7;
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t9 = ((int64_t)(v4));
  t10 = ((int64_t)(((int64_t)UINT64_C(16))));
  if (!(((t9) < (t10)))) goto label2;
  {
  t11 = v3;
  t12 = ((int64_t)(v4));
  t13 = gg_view((v0).data, 4096);
  t14 = gg_view((v1).data, 4096);
  t15 = gg_view((v2).data, 1024);
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((int64_t)(((int64_t)UINT64_C(8))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = gg_function_1(t12, t13, t14, t15, t16, t17, t18, t19, base + 0);
  t21 = ((int64_t)(t20));
  v3 = ((int64_t)((uint64_t)(t11) + (uint64_t)(t21)));
  t22 = v4;
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t22) + (uint64_t)(t23)));
  }
  goto label1;
label2:;
  t24.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t24.len = 0;
  gg_integer(gg_ref(&t24, gg_get_stack(base + 0)), v3);
  gg_append(gg_ref(&t24, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t24).base, (t24).len));
  }
gg_done:;
  for (t25 = 0; t25 < 1; t25++) gg_stacks[base + t25].top = 0;
}
static int gg_program(int argc, char **argv) {
  struct { int64_t base; } frame;
  frame.base = 0;
  gg_argc = argc; gg_argv = argv;
  gg_function_0(frame.base + 0);
  return 0;
}
int main(int argc, char **argv) {
#ifdef _MSC_VER
  __try { return gg_program(argc, argv); }
  __except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    _resetstkoflw(); gg_fail("native call stack overflow"); return 1;
  }
#else
  return gg_program(argc, argv);
#endif
}
