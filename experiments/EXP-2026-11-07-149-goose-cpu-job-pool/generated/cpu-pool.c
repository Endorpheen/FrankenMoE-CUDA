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
typedef struct { uint8_t data[1]; } gg_type_2;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint8_t data[512]; } gg_type_8;
#pragma pack(pop)
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, uint8_t value);
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, gg_type_2 value);
static uint8_t gg_eq_3(int64_t a, int64_t b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, int64_t value);
static uint8_t gg_eq_4(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, gg_slice value);
static uint8_t gg_eq_5(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, gg_slice value);
static uint8_t gg_eq_6(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, uint8_t value);
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, gg_type_8 value);
static uint8_t gg_eq_9(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, uint32_t value);
static uint8_t gg_eq_10(int32_t a, int32_t b);
static uint8_t gg_equal_elements_10(gg_slice a, gg_slice b);
static void gg_render_10(gg_reference out, int32_t value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_1(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((gg_type_2*)a.data)[i],((gg_type_2*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b) {
return gg_equal_elements_1(gg_view((a).data, 0),gg_view((b).data, 0));
}
static void gg_render_2(gg_reference out, gg_type_2 value) {
gg_text(out,gg_view((value).data, 0));
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_3(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(gg_slice a, gg_slice b) {
return gg_equal_elements_1(a,b);
}
static void gg_render_4(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(gg_slice a, gg_slice b) {
return gg_equal_elements_1(a,b);
}
static void gg_render_5(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_6(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((gg_type_8*)a.data)[i],((gg_type_8*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b) {
return gg_equal_elements_1(gg_view((a).data, 512),gg_view((b).data, 512));
}
static void gg_render_8(gg_reference out, gg_type_8 value) {
gg_text(out,gg_view((value).data, 512));
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_9(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_10(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static int64_t gg_function_1(int64_t p0, int64_t p1, gg_slice p2, gg_slice p3, int64_t base);
static int64_t gg_function_3(int64_t p0, gg_slice p1, int64_t base);
static int64_t gg_function_5(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_8(gg_slice p0, gg_slice p1, uint8_t p2, int64_t p3, int64_t base);
static void gg_function_9(int64_t p0, int64_t p1, gg_slice p2, int64_t base);
static void gg_function_10(int64_t p0, int64_t p1, uint8_t p2, uint8_t p3, int64_t base);
static void gg_function_11(int64_t p0, gg_slice p1, int64_t base);
static int64_t gg_function_12(int64_t p0, int64_t p1, int64_t base);
static uint8_t gg_function_13(gg_slice p0, int64_t base);
static void gg_function_14(int64_t p0, int64_t base);
static uint8_t gg_function_15(gg_slice p0, int64_t base);
static int64_t gg_function_16(gg_slice p0, gg_slice p1, uint8_t p2, int64_t p3, int64_t base);
static void gg_function_17(int64_t p0, int64_t p1, int64_t base);
static void gg_function_18(int64_t p0, int64_t p1, int64_t base);
static void gg_function_19(int64_t p0, int64_t base);
static uint8_t gg_function_20(int64_t p0, int64_t base);
static uint8_t gg_function_21(int64_t p0, int64_t base);
static uint8_t gg_function_22(int64_t p0, int64_t base);
static void gg_function_23(int64_t p0, gg_slice p1, int64_t base);
static void gg_function_24(int64_t p0, gg_slice p1, int64_t base);
static void gg_function_25(int64_t p0, int64_t p1, gg_slice p2, int64_t base);
static void gg_function_26(int64_t p0, uint8_t p1, int64_t base);
static int64_t gg_function_12(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  uint8_t t17;
  int64_t t18;
  int64_t t19;
  uint8_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(80))));
  t6 = fg149_meta(t4, t5);
  v2 = ((int64_t)(t6));
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)UINT64_C(1));
  t9 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t8)))));
  t10 = ((t7) == (t9));
  if (t10) {
  {
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(((int64_t)UINT64_C(81))));
  t13 = fg149_meta(t11, t12);
  v2 = ((int64_t)(t13));
  }
  } else {
  t14 = ((int64_t)(v1));
  t15 = ((int64_t)(((int64_t)UINT64_C(1))));
  t16 = ((t14) >= (t15));
  t17 = t16;
  if (t17) {
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(((int64_t)UINT64_C(3))));
  t17 = ((t18) <= (t19));
  }
  t20 = t17;
  if (t20) {
  {
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(82))));
  t23 = fg149_meta(t21, t22);
  v2 = ((int64_t)(t23));
  t24 = ((int64_t)(v1));
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((t24) == (t25));
  if (t26) {
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(40))));
  }
  } else {
  t27 = ((int64_t)(v1));
  t28 = ((int64_t)(((int64_t)UINT64_C(2))));
  t29 = ((t27) == (t28));
  if (t29) {
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(80))));
  }
  } else {
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(90))));
  }
  }
  }
  }
  }
  }
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(((int64_t)UINT64_C(26))));
  t32 = ((int64_t)(v2));
  t33 = ((int64_t)(v3));
  t34 = fg149_action(t30, t31, t32, t33);
  t0 = ((int64_t)(t34));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_26(int64_t p0, uint8_t p1, int64_t base) {
  int64_t t0;
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
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
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
  uint8_t t30;
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
  uint8_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint8_t t47;
  uint8_t t48;
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
  uint8_t t61;
  uint8_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
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
  int64_t t83;
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
  int64_t v0;
  uint8_t v1;
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
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(70))));
  t2 = fg149_meta(t0, t1);
  t3 = ((int64_t)(t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(1))));
  t5 = ((t3) <= (t4));
  if (t5) {
  {
  goto gg_done;
  }
  }
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(73))));
  t8 = fg149_meta(t6, t7);
  v2 = ((int64_t)(t8));
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) > (t10));
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(((int64_t)UINT64_C(512))));
  t12 = ((t13) <= (t14));
  }
  if (!(t12)) gg_fail("assertion failed");
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = v1;
  t16 = (!(t15));
  if (t16) {
  {
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(71))));
  t19 = fg149_meta(t17, t18);
  v4 = ((int64_t)(t19));
  t20 = ((int64_t)(v4));
  t21 = ((int64_t)(((int64_t)UINT64_C(3))));
  t22 = ((t20) == (t21));
  if (t22) {
  {
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(((int64_t)UINT64_C(44))));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = fg149_action(t23, t24, t25, t26);
  v5 = ((int64_t)(t27));
  goto gg_done;
  }
  }
  t28 = ((int64_t)(v4));
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  t30 = ((t28) == (t29));
  if (t30) {
  {
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(20))));
  t33 = fg149_meta(t31, t32);
  t34 = ((int64_t)(t33));
  t35 = ((int64_t)(v0));
  t36 = ((int64_t)(((int64_t)UINT64_C(70))));
  t37 = fg149_meta(t35, t36);
  t38 = ((int64_t)(t37));
  v3 = ((int64_t)(((int64_t)gg_mod(t34, t38))));
  }
  } else {
  t39 = ((int64_t)(v4));
  t40 = ((int64_t)(((int64_t)UINT64_C(2))));
  t41 = ((t39) == (t40));
  if (t41) {
  {
  t42 = ((int64_t)(v0));
  t43 = ((int64_t)(((int64_t)UINT64_C(72))));
  t44 = fg149_meta(t42, t43);
  v3 = ((int64_t)(t44));
  }
  } else {
  {
  goto gg_done;
  }
  }
  }
  t45 = ((int64_t)(v3));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = ((t45) >= (t46));
  t48 = t47;
  if (t48) {
  t49 = ((int64_t)(v3));
  t50 = ((int64_t)(v0));
  t51 = ((int64_t)(((int64_t)UINT64_C(70))));
  t52 = fg149_meta(t50, t51);
  t53 = ((int64_t)(t52));
  t48 = ((t49) < (t53));
  }
  if (!(t48)) gg_fail("assertion failed");
  }
  }
  t54 = ((int64_t)(v0));
  t55 = ((int64_t)(((int64_t)UINT64_C(40))));
  t56 = ((int64_t)(v2));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = fg149_action(t54, t55, t56, t57);
  t59 = ((int64_t)(t58));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = ((t59) == (t60));
  if (t61) {
  {
  goto gg_done;
  }
  }
  t62 = v1;
  if (t62) {
  {
  t64 = v2;
  t63 = 0;
label0:;
  if (t63 >= t64) goto label2;
  v6 = t63;
  {
  t65 = ((int64_t)(v0));
  t66 = ((int64_t)(((int64_t)UINT64_C(41))));
  t67 = ((int64_t)(v6));
  t68 = ((int64_t)(((int64_t)UINT64_C(0))));
  t69 = fg149_action(t65, t66, t67, t68);
  v7 = ((int64_t)(t69));
  }
label1:;
  t63++; goto label0;
label2:;
  }
  } else {
  {
  t71 = ((int64_t)(v0));
  t72 = ((int64_t)(((int64_t)UINT64_C(100))));
  t73 = ((int64_t)(v3));
  t74 = ((int64_t)(((int64_t)((uint64_t)(t72) + (uint64_t)(t73)))));
  t75 = fg149_meta(t71, t74);
  t76 = t75;
  t70 = 0;
label3:;
  if (t70 >= t76) goto label5;
  v8 = t70;
  {
  t77 = ((int64_t)(v0));
  t78 = ((int64_t)(((int64_t)UINT64_C(500000))));
  t79 = ((int64_t)(v3));
  t80 = ((int64_t)(((int64_t)UINT64_C(512))));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t79) * (uint64_t)(t80)))));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t78) + (uint64_t)(t81)))));
  t83 = ((int64_t)(v8));
  t84 = ((int64_t)(((int64_t)((uint64_t)(t82) + (uint64_t)(t83)))));
  t85 = fg149_meta(t77, t84);
  v9 = ((int64_t)(t85));
  t86 = ((int64_t)(v9));
  t87 = ((int64_t)(((int64_t)UINT64_C(0))));
  t88 = ((t86) >= (t87));
  t89 = t88;
  if (t89) {
  t90 = ((int64_t)(v9));
  t91 = ((int64_t)(v2));
  t89 = ((t90) < (t91));
  }
  if (!(t89)) gg_fail("assertion failed");
  t92 = ((int64_t)(v0));
  t93 = ((int64_t)(((int64_t)UINT64_C(41))));
  t94 = ((int64_t)(v9));
  t95 = ((int64_t)(((int64_t)UINT64_C(0))));
  t96 = fg149_action(t92, t93, t94, t95);
  v10 = ((int64_t)(t96));
  }
label4:;
  t70++; goto label3;
label5:;
  }
  }
  t97 = ((int64_t)(v0));
  t98 = ((int64_t)(((int64_t)UINT64_C(42))));
  t99 = ((int64_t)(((int64_t)UINT64_C(0))));
  t100 = ((int64_t)(((int64_t)UINT64_C(0))));
  t101 = fg149_action(t97, t98, t99, t100);
  v11 = ((int64_t)(t101));
  t102 = ((int64_t)(v0));
  t103 = ((int64_t)(((int64_t)UINT64_C(43))));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  t105 = ((int64_t)(((int64_t)UINT64_C(0))));
  t106 = fg149_action(t102, t103, t104, t105);
  v12 = ((int64_t)(t106));
  }
gg_done:;
}
static uint8_t gg_function_15(gg_slice p0, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint8_t * t4;
  uint8_t t5;
  uint8_t t6;
  uint8_t t7;
  gg_slice v0;
  int64_t v1;
  v0 = p0;
  {
  t2 = (v0).len;
  t1 = 0;
label0:;
  if (t1 >= t2) goto label2;
  v1 = t1;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t3 = v1;
  t4 = ((uint8_t *)(v0).data) + gg_index(t3, (v0).len);
  t5 = ((uint8_t)((*t4)));
  t6 = ((uint8_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) != (t6));
  if (t7) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  }
label1:;
  t1++; goto label0;
label2:;
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_24(int64_t p0, gg_slice p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  gg_slice t5;
  uint8_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t v0;
  gg_slice v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(14))));
  t3 = fg149_meta(t1, t2);
  t4 = ((int64_t)(t3));
  v2 = gg_function_12(t0, t4, base + 0);
  t5 = v1;
  t6 = gg_function_15(t5, base + 0);
  t7 = t6;
  if (t7) {
  {
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(27))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fg149_action(t8, t9, t10, t11);
  v3 = ((int64_t)(t12));
  }
  }
  }
gg_done:;
}
static void gg_function_18(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(7))));
  t2 = ((int64_t)(v1));
  fg149_set(t0, t1, t2);
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(12))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = fg149_action(t3, t4, t5, t6);
  v2 = ((int64_t)(t7));
  }
gg_done:;
}
static void gg_function_25(int64_t p0, int64_t p1, gg_slice p2, int64_t base) {
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
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  uint32_t t17;
  int64_t t18;
  uint32_t t19;
  uint32_t t20;
  uint32_t t21;
  uint32_t t22;
  uint32_t t23;
  int64_t t24;
  uint32_t t25;
  uint32_t t26;
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
  gg_slice t39;
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
  int64_t v0;
  int64_t v1;
  gg_slice v2;
  int64_t v3;
  uint32_t v4;
  uint32_t v5;
  uint32_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(10))));
  t7 = fg149_meta(t5, t6);
  t8 = ((int64_t)(t7));
  t3 = ((t4) <= (t8));
  }
  if (!(t3)) gg_fail("assertion failed");
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(9))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = fg149_action(t9, t10, t11, t12);
  v3 = ((int64_t)(t13));
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(18))));
  t16 = fg149_meta(t14, t15);
  v4 = ((uint32_t)(((uint32_t)(t16))));
  t17 = ((uint32_t)(v4));
  t18 = ((int64_t)(((int64_t)UINT64_C(16))));
  t19 = ((uint32_t)(((t17) >> ((uint64_t)(t18) & 31))));
  t20 = ((uint32_t)(((int64_t)UINT64_C(1))));
  t21 = ((uint32_t)(((uint32_t)((uint64_t)(t19) + (uint64_t)(t20)))));
  t22 = ((uint32_t)(((int64_t)UINT64_C(65535))));
  v5 = ((uint32_t)(((t21) & (t22))));
  t23 = ((uint32_t)(v5));
  t24 = ((int64_t)(((int64_t)UINT64_C(16))));
  t25 = ((uint32_t)(((uint32_t)((uint64_t)(t23) << ((uint64_t)(t24) & 31)))));
  t26 = ((uint32_t)(((uint32_t)(v1))));
  v6 = ((uint32_t)(((t25) | (t26))));
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(23))));
  t29 = ((int64_t)(((int64_t)(((int32_t)(v6))))));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = fg149_action(t27, t28, t29, t30);
  v7 = ((int64_t)(t31));
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(11))));
  t34 = fg149_meta(t32, t33);
  t35 = ((int64_t)(t34));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = ((t35) != (t36));
  if (t37) {
  {
  t38 = ((int64_t)(v0));
  t39 = v2;
  gg_function_24(t38, t39, base + 0);
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_18(t40, t41, base + 0);
  }
  } else {
  {
  t42 = ((int64_t)(v0));
  t43 = ((int64_t)(((int64_t)UINT64_C(12))));
  t44 = ((int64_t)(((int64_t)UINT64_C(0))));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = fg149_action(t42, t43, t44, t45);
  v8 = ((int64_t)(t46));
  }
  }
  t47 = ((int64_t)(v0));
  t48 = ((int64_t)(((int64_t)UINT64_C(10))));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = fg149_action(t47, t48, t49, t50);
  v9 = ((int64_t)(t51));
  }
gg_done:;
}
static uint8_t gg_function_20(int64_t p0, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
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
  int64_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
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
  uint8_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(15))));
  t3 = fg149_meta(t1, t2);
  t4 = ((int64_t)(t3));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) != (t5));
  t7 = t6;
  if (!t7) {
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(12))));
  t10 = fg149_meta(t8, t9);
  t11 = ((int64_t)(t10));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t11) != (t12));
  }
  t13 = t7;
  t14 = t13;
  if (!t14) {
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(11))));
  t17 = fg149_meta(t15, t16);
  t18 = ((int64_t)(t17));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t18) != (t19));
  }
  t20 = t14;
  if (t20) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(18))));
  t23 = fg149_meta(t21, t22);
  v1 = ((int64_t)(t23));
  t24 = ((int64_t)(v1));
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(17))));
  t27 = fg149_meta(t25, t26);
  t28 = ((int64_t)(t27));
  t29 = ((t24) != (t28));
  if (t29) {
  {
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(((int64_t)UINT64_C(13))));
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(16))));
  t35 = fg149_meta(t33, t34);
  t36 = ((int64_t)(t35));
  t37 = ((int64_t)(v1));
  t38 = ((int64_t)(((int64_t)UINT64_C(65535))));
  t39 = ((int64_t)(((t37) & (t38))));
  t40 = ((t36) < (t39));
  if (t40) {
  {
  t32 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t41 = ((int64_t)(t32));
  fg149_set(t30, t31, t41);
  t42 = ((int64_t)(v0));
  t43 = ((int64_t)(((int64_t)UINT64_C(14))));
  t44 = ((int64_t)(v1));
  fg149_set(t42, t43, t44);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_5(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) == (t10));
  t12 = t11;
  if (!t12) {
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)gg_div(t14, t15))));
  t12 = ((t13) <= (t16));
  }
  t8 = t12;
  }
  if (!(t8)) gg_fail("assertion failed");
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(v1));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_21(int64_t p0, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
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
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(131072))));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(13))));
  t4 = fg149_meta(t2, t3);
  t5 = ((int64_t)(t4));
  v1 = gg_function_5(t1, t5, base + 0);
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t6 = ((int64_t)(v0));
  t7 = gg_function_20(t6, base + 0);
  t8 = t7;
  if (!((!(t8)))) goto label2;
  {
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(v1));
  t11 = ((t9) >= (t10));
  if (t11) {
  {
  goto label2;
  }
  }
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(15))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = fg149_action(t12, t13, t14, t15);
  v3 = ((int64_t)(t16));
  t17 = v2;
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t17) + (uint64_t)(t18)));
  }
  goto label1;
label2:;
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(15))));
  t21 = fg149_meta(t19, t20);
  t22 = ((int64_t)(t21));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t0 = ((t22) != (t23));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_22(int64_t p0, int64_t base) {
  uint8_t t0;
  int64_t t1;
  uint8_t t2;
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
  int64_t t17;
  int64_t t18;
  int64_t t19;
  uint8_t t20;
  uint8_t t21;
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
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = gg_function_21(t1, base + 0);
  t3 = t2;
  if (t3) {
  {
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(19))));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg149_action(t4, t5, t6, t7);
  v1 = ((int64_t)(t8));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(15))));
  t11 = fg149_meta(t9, t10);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t0 = ((t12) != (t13));
  goto gg_done;
  }
  }
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(9))));
  t16 = ((int64_t)(((int64_t)UINT64_C(1))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = fg149_action(t14, t15, t16, t17);
  v2 = ((int64_t)(t18));
label1:;
  t19 = ((int64_t)(v0));
  t20 = gg_function_20(t19, base + 0);
  t21 = t20;
  if (!((!(t21)))) goto label2;
  {
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(11))));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = fg149_action(t22, t23, t24, t25);
  v3 = ((int64_t)(t26));
  }
  goto label1;
label2:;
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(10))));
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = fg149_action(t27, t28, t29, t30);
  v4 = ((int64_t)(t31));
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(15))));
  t34 = fg149_meta(t32, t33);
  t35 = ((int64_t)(t34));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t0 = ((t35) != (t36));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_23(int64_t p0, gg_slice p1, int64_t base) {
  int64_t t0;
  gg_slice t1;
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
  uint8_t t17;
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
  int64_t t32;
  uint8_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint8_t t40;
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
  int64_t v0;
  gg_slice v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  uint8_t v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v0));
  t1 = v1;
  gg_function_24(t0, t1, base + 0);
label1:;
  if (!(((uint8_t)UINT64_C(1)))) goto label2;
  {
label4:;
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(11))));
  t4 = fg149_meta(t2, t3);
  t5 = ((int64_t)(t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t5) != (t6)))) goto label5;
  {
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(9))));
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = fg149_action(t7, t8, t9, t10);
  v2 = ((int64_t)(t11));
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(11))));
  t14 = fg149_meta(t12, t13);
  t15 = ((int64_t)(t14));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t15) != (t16));
  if (t17) {
  {
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(11))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = fg149_action(t18, t19, t20, t21);
  v3 = ((int64_t)(t22));
  }
  }
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(((int64_t)UINT64_C(10))));
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = fg149_action(t23, t24, t25, t26);
  v4 = ((int64_t)(t27));
  }
  goto label4;
label5:;
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(12))));
  t30 = fg149_meta(t28, t29);
  t31 = ((int64_t)(t30));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = ((t31) != (t32));
  if (t33) {
  {
  goto label2;
  }
  }
  t34 = ((int64_t)(v0));
  v5 = gg_function_22(t34, base + 0);
  t35 = ((int64_t)(v0));
  t36 = ((int64_t)(((int64_t)UINT64_C(15))));
  t37 = fg149_meta(t35, t36);
  t38 = ((int64_t)(t37));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = ((t38) != (t39));
  if (t40) {
  {
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(((int64_t)UINT64_C(13))));
  t43 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg149_set(t41, t42, t43);
  t44 = ((int64_t)(v0));
  t45 = ((int64_t)(((int64_t)UINT64_C(25))));
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(((int64_t)UINT64_C(16))));
  t48 = fg149_meta(t46, t47);
  t49 = ((int64_t)(t48));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = fg149_action(t44, t45, t49, t50);
  v6 = ((int64_t)(t51));
  }
  }
  }
  goto label1;
label2:;
  }
gg_done:;
}
static void gg_function_19(int64_t p0, int64_t base) {
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
  uint8_t t13;
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
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
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
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(18))));
  t2 = fg149_meta(t0, t1);
  t3 = ((int64_t)(t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(65535))));
  v1 = ((int64_t)(((t3) & (t4))));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((t5) == (t6));
  if (t7) {
  {
  goto gg_done;
  }
  }
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(9))));
  t10 = fg149_meta(t8, t9);
  t11 = ((int64_t)(t10));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t11) != (t12));
  if (t13) {
  {
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(20))));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = fg149_action(t14, t15, t16, t17);
  v2 = ((int64_t)(t18));
  goto gg_done;
  }
  }
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(19))));
  t21 = fg149_meta(t19, t20);
  v3 = ((int64_t)(t21));
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(16))));
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = fg149_action(t22, t23, t24, t25);
  v4 = ((int64_t)(t26));
  t27 = ((int64_t)(v4));
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) - (uint64_t)(t29)))));
  t31 = ((t27) == (t30));
  if (t31) {
  {
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(17))));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = fg149_action(t32, t33, t34, t35);
  v5 = ((int64_t)(t36));
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(18))));
  t39 = ((int64_t)(((int64_t)UINT64_C(1))));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  t41 = fg149_action(t37, t38, t39, t40);
  v6 = ((int64_t)(t41));
  goto gg_done;
  }
  }
label1:;
  t42 = ((int64_t)(v0));
  t43 = ((int64_t)(((int64_t)UINT64_C(19))));
  t44 = fg149_meta(t42, t43);
  t45 = ((int64_t)(t44));
  t46 = ((int64_t)(v3));
  if (!(((t45) == (t46)))) goto label2;
  {
  t47 = ((int64_t)(v0));
  t48 = ((int64_t)(((int64_t)UINT64_C(15))));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = fg149_action(t47, t48, t49, t50);
  v7 = ((int64_t)(t51));
  }
  goto label1;
label2:;
  t52 = ((int64_t)(v0));
  t53 = ((int64_t)(((int64_t)UINT64_C(19))));
  t54 = ((int64_t)(((int64_t)UINT64_C(0))));
  t55 = ((int64_t)(((int64_t)UINT64_C(0))));
  t56 = fg149_action(t52, t53, t54, t55);
  v8 = ((int64_t)(t56));
  }
gg_done:;
}
static void gg_function_17(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(9))));
  t2 = fg149_meta(t0, t1);
  t3 = ((int64_t)(t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) != (t4));
  if (t5) {
  {
  goto gg_done;
  }
  }
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(9))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = fg149_action(t6, t7, t8, t9);
  v2 = ((int64_t)(t10));
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(((int64_t)UINT64_C(11))));
  t13 = fg149_meta(t11, t12);
  t14 = ((int64_t)(t13));
  t15 = ((int64_t)(v1));
  t16 = ((t14) != (t15));
  if (t16) {
  {
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(v1));
  gg_function_18(t17, t18, base + 0);
  }
  }
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(10))));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = fg149_action(t19, t20, t21, t22);
  v3 = ((int64_t)(t23));
  }
gg_done:;
}
static int64_t gg_function_16(gg_slice p0, gg_slice p1, uint8_t p2, int64_t p3, int64_t base) {
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
  int64_t t19;
  uint8_t * t20;
  int64_t t21;
  uint8_t * t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t * t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  uint8_t * t34;
  uint8_t t35;
  uint8_t t36;
  uint8_t t37;
  int64_t t38;
  uint8_t * t39;
  int64_t t40;
  int64_t t41;
  gg_slice v0;
  gg_slice v1;
  uint8_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)((v0).len));
  t2 = ((int64_t)(((int64_t)UINT64_C(512))));
  t3 = ((t1) == (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)((v1).len));
  t6 = ((int64_t)(((int64_t)UINT64_C(512))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t9) >= (t10));
  }
  t11 = t8;
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)(((int64_t)UINT64_C(512))));
  t12 = ((t13) <= (t14));
  }
  if (!(t12)) gg_fail("assertion failed");
  t15 = v2;
  t16 = (!(t15));
  if (t16) {
  {
  t18 = ((int64_t)UINT64_C(512));
  t17 = 0;
label0:;
  if (t17 >= t18) goto label2;
  v4 = t17;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t19 = v4;
  t20 = ((uint8_t *)(v1).data) + gg_index(t19, (v1).len);
  /* FG_DIRECT_SLICE_INDEX */
  t21 = v4;
  t22 = ((uint8_t *)(v0).data) + gg_index(t21, (v0).len);
  (*t20) = ((uint8_t)((*t22)));
  }
label1:;
  t17++; goto label0;
label2:;
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
  }
  t24 = ((int64_t)UINT64_C(512));
  t23 = 0;
label3:;
  if (t23 >= t24) goto label5;
  v5 = t23;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t25 = v5;
  t26 = ((uint8_t *)(v1).data) + gg_index(t25, (v1).len);
  (*t26) = ((uint8_t)(((int64_t)UINT64_C(0))));
  }
label4:;
  t23++; goto label3;
label5:;
  t28 = ((int64_t)UINT64_C(512));
  t27 = 0;
label6:;
  if (t27 >= t28) goto label8;
  v6 = t27;
  {
  t29 = ((int64_t)(v3));
  t30 = ((int64_t)(v6));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t30)))));
  t32 = ((int64_t)(((int64_t)UINT64_C(512))));
  v7 = ((int64_t)(((int64_t)gg_mod(t31, t32))));
  /* FG_DIRECT_SLICE_INDEX */
  t33 = v7;
  t34 = ((uint8_t *)(v0).data) + gg_index(t33, (v0).len);
  t35 = ((uint8_t)((*t34)));
  t36 = ((uint8_t)(((int64_t)UINT64_C(0))));
  t37 = ((t35) != (t36));
  if (t37) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t38 = v7;
  t39 = ((uint8_t *)(v1).data) + gg_index(t38, (v1).len);
  (*t39) = ((uint8_t)(((int64_t)UINT64_C(1))));
  t40 = ((int64_t)(v7));
  t41 = ((int64_t)(((int64_t)UINT64_C(1))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t41)))));
  goto gg_done;
  }
  }
  }
label7:;
  t27++; goto label6;
label8:;
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_10(int64_t p0, int64_t p1, uint8_t p2, uint8_t p3, int64_t base) {
  uint8_t t0;
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
  uint8_t t12;
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
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  uint8_t t37;
  uint8_t t38;
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
  uint8_t t50;
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
  int64_t t64;
  int64_t t65;
  int64_t t66;
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
  int64_t v0;
  int64_t v1;
  uint8_t v2;
  uint8_t v3;
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
  v3 = p3;
  {
  t0 = v2;
  if (t0) {
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(9))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = fg149_action(t1, t2, t3, t4);
  v4 = ((int64_t)(t5));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(6))));
  t8 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg149_set(t6, t7, t8);
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(7))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg149_set(t9, t10, t11);
  t12 = v3;
  if (t12) {
  {
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(12))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = fg149_action(t13, t14, t15, t16);
  v5 = ((int64_t)(t17));
  }
  }
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(10))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = fg149_action(t18, t19, t20, t21);
  v6 = ((int64_t)(t22));
  }
  }
  t24 = v1;
  t23 = 0;
label0:;
  if (t23 >= t24) goto label2;
  v7 = t23;
  {
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(14))));
  t27 = ((int64_t)(v7));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t28)))));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = fg149_action(t25, t26, t29, t30);
  v8 = ((int64_t)(t31));
  t32 = ((int64_t)(v8));
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(22))));
  t35 = fg149_meta(t33, t34);
  t36 = ((int64_t)(t35));
  t37 = ((t32) == (t36));
  t38 = t37;
  if (!t38) {
  t39 = ((int64_t)(v8));
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(23))));
  t42 = fg149_meta(t40, t41);
  t43 = ((int64_t)(t42));
  t38 = ((t39) == (t43));
  }
  if (!(t38)) gg_fail("assertion failed");
  }
label1:;
  t23++; goto label0;
label2:;
  t44 = v3;
  if (t44) {
  {
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(8))));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = fg149_action(t45, t46, t47, t48);
  v9 = ((int64_t)(t49));
  }
  }
  t50 = v2;
  if (t50) {
  {
  t51 = ((int64_t)(v0));
  t52 = ((int64_t)(((int64_t)UINT64_C(7))));
  t53 = ((int64_t)(((int64_t)UINT64_C(0))));
  t54 = ((int64_t)(((int64_t)UINT64_C(0))));
  t55 = fg149_action(t51, t52, t53, t54);
  v10 = ((int64_t)(t55));
  }
  }
  t56 = ((int64_t)(v0));
  t57 = ((int64_t)(((int64_t)UINT64_C(3))));
  t58 = ((int64_t)(v0));
  t59 = ((int64_t)(((int64_t)UINT64_C(7))));
  t60 = fg149_meta(t58, t59);
  t61 = ((int64_t)(t60));
  t62 = ((int64_t)(v0));
  t63 = ((int64_t)(((int64_t)UINT64_C(10))));
  t64 = fg149_meta(t62, t63);
  t65 = ((int64_t)(t64));
  t66 = gg_function_5(t61, t65, base + 0);
  t67 = ((int64_t)(((int64_t)UINT64_C(0))));
  t68 = fg149_action(t56, t57, t66, t67);
  v11 = ((int64_t)(t68));
  t69 = ((int64_t)(v0));
  t70 = ((int64_t)(((int64_t)UINT64_C(1))));
  t71 = ((int64_t)(v0));
  t72 = ((int64_t)(((int64_t)UINT64_C(6))));
  t73 = fg149_meta(t71, t72);
  t74 = ((int64_t)(t73));
  t75 = ((int64_t)(((int64_t)UINT64_C(0))));
  t76 = fg149_action(t69, t70, t74, t75);
  v12 = ((int64_t)(t76));
  }
gg_done:;
}
static void gg_function_14(int64_t p0, int64_t base) {
  int64_t t0;
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
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  uint8_t t20;
  uint8_t t21;
  int64_t v0;
  uint8_t v1;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = fg149_meta(t0, t1);
  t3 = ((int64_t)(t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) == (t4));
  if (t5) {
  {
  goto gg_done;
  }
  }
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(9))));
  t8 = fg149_meta(t6, t7);
  t9 = ((int64_t)(t8));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  v1 = ((t9) == (t10));
  t11 = ((int64_t)(v0));
  t13 = v1;
  if (t13) {
  {
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(10))));
  t16 = fg149_meta(t14, t15);
  t17 = ((int64_t)(t16));
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t17) - (uint64_t)(t18)))));
  }
  } else {
  {
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t19 = ((int64_t)(t12));
  t20 = v1;
  t21 = v1;
  gg_function_10(t11, t19, t20, t21, base + 0);
  }
gg_done:;
}
static uint8_t gg_function_13(gg_slice p0, int64_t base) {
  /* FG_SHARED_BODY:15 */
  return gg_function_15(p0, base);
}
static void gg_function_11(int64_t p0, gg_slice p1, int64_t base) {
  /* FG_SHARED_BODY:24 */
  gg_function_24(p0, p1, base);
}
static void gg_function_9(int64_t p0, int64_t p1, gg_slice p2, int64_t base) {
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
  uint8_t * t11;
  int64_t t12;
  int64_t v0;
  int64_t v1;
  gg_slice v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)UINT64_C(512));
  t0 = 0;
label0:;
  if (t0 >= t1) goto label2;
  v3 = t0;
  {
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(10000))));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(512))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t3) + (uint64_t)(t6)))));
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) + (uint64_t)(t8)))));
  /* FG_DIRECT_SLICE_INDEX */
  t10 = v3;
  t11 = ((uint8_t *)(v2).data) + gg_index(t10, (v2).len);
  t12 = ((int64_t)(((int64_t)((*t11)))));
  fg149_set(t2, t9, t12);
  }
label1:;
  t0++; goto label0;
label2:;
  }
gg_done:;
}
static int64_t gg_function_8(gg_slice p0, gg_slice p1, uint8_t p2, int64_t p3, int64_t base) {
  /* FG_SHARED_BODY:16 */
  return gg_function_16(p0, p1, p2, p3, base);
}
static int64_t gg_function_3(int64_t p0, gg_slice p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
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
  uint8_t t30;
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
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  uint8_t t90;
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
  uint8_t t109;
  uint8_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  uint8_t t117;
  int64_t t118;
  int64_t t119;
  gg_slice t120;
  gg_slice t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  uint8_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  gg_slice t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  uint8_t t139;
  int64_t t140;
  int64_t t141;
  uint8_t t142;
  uint8_t t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  uint8_t t151;
  int64_t t152;
  int64_t t153;
  uint8_t t154;
  uint8_t t155;
  int64_t t156;
  int64_t t157;
  gg_slice t158;
  gg_slice t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  int64_t t164;
  uint8_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  gg_slice t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  uint8_t t177;
  int64_t t178;
  int64_t t179;
  uint8_t t180;
  uint8_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  int64_t t185;
  gg_slice t186;
  gg_slice t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  uint8_t t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  gg_slice t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  uint8_t t203;
  int64_t t204;
  gg_slice t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t v0;
  gg_slice v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  gg_type_8 v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = fg149_meta(t1, t2);
  v2 = ((int64_t)(t3));
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) <= (t5));
  t7 = t6;
  if (!t7) {
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)UINT64_C(512))));
  t7 = ((t8) > (t9));
  }
  t10 = t7;
  t11 = t10;
  if (!t11) {
  t12 = ((int64_t)((v1).len));
  t13 = ((int64_t)(((int64_t)UINT64_C(512))));
  t11 = ((t12) != (t13));
  }
  t14 = t11;
  if (t14) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(7))));
  t18 = fg149_meta(t16, t17);
  t19 = ((int64_t)(t18));
  v3 = gg_function_5(t15, t19, base + 0);
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(6))));
  t24 = fg149_meta(t22, t23);
  t25 = ((int64_t)(t24));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = fg149_action(t20, t21, t25, t26);
  t28 = ((int64_t)(t27));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = ((t28) == (t29));
  if (t30) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg149_set(t31, t32, t33);
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(((int64_t)UINT64_C(1))));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg149_set(t34, t35, t36);
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(2))));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg149_set(t37, t38, t39);
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(3))));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg149_set(t40, t41, t42);
  t43 = ((int64_t)(v0));
  t44 = ((int64_t)(((int64_t)UINT64_C(4))));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg149_set(t43, t44, t45);
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(((int64_t)UINT64_C(5))));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg149_set(t46, t47, t48);
  t49 = ((int64_t)(v0));
  t50 = ((int64_t)(((int64_t)UINT64_C(6))));
  t51 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg149_set(t49, t50, t51);
  t52 = ((int64_t)(v0));
  t53 = ((int64_t)(((int64_t)UINT64_C(7))));
  t54 = ((int64_t)(v0));
  t55 = ((int64_t)(((int64_t)UINT64_C(2))));
  t56 = fg149_meta(t54, t55);
  t57 = ((int64_t)(t56));
  fg149_set(t52, t53, t57);
  t58 = ((int64_t)(v0));
  t59 = ((int64_t)(((int64_t)UINT64_C(8))));
  t60 = ((int64_t)UINT64_C(1));
  t61 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t60)))));
  fg149_set(t58, t59, t61);
  t62 = ((int64_t)(v0));
  t63 = ((int64_t)(((int64_t)UINT64_C(9))));
  t64 = ((int64_t)(v2));
  fg149_set(t62, t63, t64);
  t65 = ((int64_t)(v0));
  t66 = ((int64_t)(((int64_t)UINT64_C(10))));
  t67 = ((int64_t)(v0));
  t68 = ((int64_t)(((int64_t)UINT64_C(3))));
  t69 = fg149_meta(t67, t68);
  t70 = ((int64_t)(t69));
  fg149_set(t65, t66, t70);
  t71 = ((int64_t)(v0));
  t72 = ((int64_t)(((int64_t)UINT64_C(11))));
  t73 = ((int64_t)(v0));
  t74 = ((int64_t)(((int64_t)UINT64_C(4))));
  t75 = fg149_meta(t73, t74);
  t76 = ((int64_t)(t75));
  fg149_set(t71, t72, t76);
  t77 = ((int64_t)(v0));
  t78 = ((int64_t)(((int64_t)UINT64_C(12))));
  t79 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg149_set(t77, t78, t79);
  t80 = ((int64_t)(v0));
  t81 = ((int64_t)(((int64_t)UINT64_C(17))));
  t82 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg149_set(t80, t81, t82);
  t83 = ((int64_t)(v0));
  t84 = ((int64_t)(((int64_t)UINT64_C(2))));
  t85 = ((int64_t)(v3));
  t86 = ((int64_t)(((int64_t)UINT64_C(0))));
  t87 = fg149_action(t83, t84, t85, t86);
  t88 = ((int64_t)(t87));
  t89 = ((int64_t)(((int64_t)UINT64_C(0))));
  t90 = ((t88) == (t89));
  if (t90) {
  {
  t91 = ((int64_t)(v0));
  t92 = ((int64_t)(((int64_t)UINT64_C(1))));
  t93 = ((int64_t)(v0));
  t94 = ((int64_t)(((int64_t)UINT64_C(6))));
  t95 = fg149_meta(t93, t94);
  t96 = ((int64_t)(t95));
  t97 = ((int64_t)(((int64_t)UINT64_C(0))));
  t98 = fg149_action(t91, t92, t96, t97);
  v4 = ((int64_t)(t98));
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t99 = ((int64_t)(v0));
  t100 = ((int64_t)(((int64_t)UINT64_C(4))));
  t101 = ((int64_t)(v3));
  t102 = ((int64_t)(((int64_t)UINT64_C(0))));
  t103 = fg149_action(t99, t100, t101, t102);
  v5 = ((int64_t)(t103));
  t105 = v2;
  t104 = 0;
label0:;
  if (t104 >= t105) goto label2;
  v6 = t104;
  {
  t106 = ((int64_t)(v0));
  t107 = ((int64_t)(((int64_t)UINT64_C(16))));
  t108 = ((int64_t)(v6));
  fg149_set(t106, t107, t108);
  }
label1:;
  t104++; goto label0;
label2:;
  memset(&t109, 0, sizeof(t109));
  t110 = ((uint8_t)(t109));
  for (t111=0; t111<512; t111++) (v7).data[t111] = t110;
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t112 = ((int64_t)(v0));
  t113 = ((int64_t)(((int64_t)UINT64_C(9))));
  t114 = fg149_meta(t112, t113);
  t115 = ((int64_t)(t114));
  t116 = ((int64_t)(((int64_t)UINT64_C(0))));
  t117 = ((t115) != (t116));
  if (t117) {
  {
  t119 = v2;
  t118 = 0;
label3:;
  if (t118 >= t119) goto label5;
  v9 = t118;
  {
  t120 = v1;
  t121 = gg_view((v7).data, 512);
  t122 = ((int64_t)(v0));
  t123 = ((int64_t)(((int64_t)UINT64_C(5))));
  t124 = fg149_meta(t122, t123);
  t125 = ((int64_t)(t124));
  t126 = ((int64_t)(((int64_t)UINT64_C(0))));
  t127 = ((t125) != (t126));
  t128 = ((int64_t)(v8));
  v8 = gg_function_16(t120, t121, t127, t128, base + 0);
  t129 = ((int64_t)(v0));
  t130 = ((int64_t)(v9));
  t131 = gg_view((v7).data, 512);
  gg_function_9(t129, t130, t131, base + 0);
  }
label4:;
  t118++; goto label3;
label5:;
  }
  } else {
  {
  t132 = ((int64_t)(v0));
  t133 = ((int64_t)(((int64_t)UINT64_C(5))));
  t134 = ((int64_t)(((int64_t)UINT64_C(0))));
  t135 = ((int64_t)(((int64_t)UINT64_C(0))));
  t136 = fg149_action(t132, t133, t134, t135);
  t137 = ((int64_t)(t136));
  t138 = ((int64_t)(((int64_t)UINT64_C(0))));
  t139 = ((t137) != (t138));
  if (t139) {
  {
  t140 = ((int64_t)(v0));
  t141 = ((int64_t)(((int64_t)UINT64_C(0))));
  t142 = ((uint8_t)UINT64_C(0));
  t143 = ((uint8_t)UINT64_C(0));
  gg_function_10(t140, t141, t142, t143, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t144 = ((int64_t)(v0));
  t145 = ((int64_t)(((int64_t)UINT64_C(6))));
  t146 = ((int64_t)(((int64_t)UINT64_C(0))));
  t147 = ((int64_t)(((int64_t)UINT64_C(0))));
  t148 = fg149_action(t144, t145, t146, t147);
  t149 = ((int64_t)(t148));
  t150 = ((int64_t)(((int64_t)UINT64_C(0))));
  t151 = ((t149) != (t150));
  if (t151) {
  {
  t152 = ((int64_t)(v0));
  t153 = ((int64_t)(((int64_t)UINT64_C(0))));
  t154 = ((uint8_t)UINT64_C(1));
  t155 = ((uint8_t)UINT64_C(0));
  gg_function_10(t152, t153, t154, t155, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
  v11 = ((int64_t)(((int64_t)UINT64_C(1))));
label7:;
  t156 = ((int64_t)(v11));
  t157 = ((int64_t)(v2));
  if (!(((t156) < (t157)))) goto label8;
  {
  t158 = v1;
  t159 = gg_view((v7).data, 512);
  t160 = ((int64_t)(v0));
  t161 = ((int64_t)(((int64_t)UINT64_C(5))));
  t162 = fg149_meta(t160, t161);
  t163 = ((int64_t)(t162));
  t164 = ((int64_t)(((int64_t)UINT64_C(0))));
  t165 = ((t163) != (t164));
  t166 = ((int64_t)(v8));
  v8 = gg_function_16(t158, t159, t165, t166, base + 0);
  t167 = ((int64_t)(v0));
  t168 = ((int64_t)(v11));
  t169 = gg_view((v7).data, 512);
  gg_function_9(t167, t168, t169, base + 0);
  t170 = ((int64_t)(v0));
  t171 = ((int64_t)(((int64_t)UINT64_C(13))));
  t172 = ((int64_t)(v11));
  t173 = ((int64_t)(((int64_t)UINT64_C(0))));
  t174 = fg149_action(t170, t171, t172, t173);
  t175 = ((int64_t)(t174));
  t176 = ((int64_t)(((int64_t)UINT64_C(0))));
  t177 = ((t175) != (t176));
  if (t177) {
  {
  t178 = ((int64_t)(v0));
  t179 = ((int64_t)(v10));
  t180 = ((uint8_t)UINT64_C(1));
  t181 = ((uint8_t)UINT64_C(1));
  gg_function_10(t178, t179, t180, t181, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t182 = v10;
  t183 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = ((int64_t)((uint64_t)(t182) + (uint64_t)(t183)));
  t184 = v11;
  t185 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t184) + (uint64_t)(t185)));
  }
  goto label7;
label8:;
  t186 = v1;
  t187 = gg_view((v7).data, 512);
  t188 = ((int64_t)(v0));
  t189 = ((int64_t)(((int64_t)UINT64_C(5))));
  t190 = fg149_meta(t188, t189);
  t191 = ((int64_t)(t190));
  t192 = ((int64_t)(((int64_t)UINT64_C(0))));
  t193 = ((t191) != (t192));
  t194 = ((int64_t)(v8));
  v8 = gg_function_16(t186, t187, t193, t194, base + 0);
  t195 = ((int64_t)(v0));
  t196 = ((int64_t)(((int64_t)UINT64_C(0))));
  t197 = gg_view((v7).data, 512);
  gg_function_9(t195, t196, t197, base + 0);
  t198 = ((int64_t)(v0));
  t199 = ((int64_t)(((int64_t)UINT64_C(11))));
  t200 = fg149_meta(t198, t199);
  t201 = ((int64_t)(t200));
  t202 = ((int64_t)(((int64_t)UINT64_C(0))));
  t203 = ((t201) == (t202));
  if (t203) {
  {
  t204 = ((int64_t)(v0));
  t205 = gg_view((v7).data, 512);
  gg_function_24(t204, t205, base + 0);
  }
  }
  }
  }
  t206 = ((int64_t)(v0));
  t207 = ((int64_t)(((int64_t)UINT64_C(0))));
  t208 = fg149_meta(t206, t207);
  t0 = ((int64_t)(t208));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_1(int64_t p0, int64_t p1, gg_slice p2, gg_slice p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint8_t t4;
  int64_t t5;
  gg_slice t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  gg_slice t14;
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  gg_slice t20;
  gg_slice t21;
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
  uint8_t t34;
  uint8_t t35;
  int64_t t36;
  int64_t t37;
  uint8_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint8_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  uint8_t t51;
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
  uint8_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  uint8_t t73;
  int64_t t74;
  uint8_t t75;
  uint8_t t76;
  int64_t t77;
  int64_t t78;
  uint8_t t79;
  int64_t t80;
  uint8_t t81;
  uint8_t t82;
  int64_t t83;
  int64_t t84;
  uint8_t t85;
  int64_t t86;
  uint8_t t87;
  uint8_t t88;
  int64_t t89;
  int64_t t90;
  uint8_t t91;
  int64_t t92;
  gg_slice t93;
  int64_t t94;
  int64_t t95;
  uint8_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  gg_slice t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  int64_t t106;
  gg_slice t107;
  int64_t t108;
  int64_t t109;
  uint8_t t110;
  uint8_t t111;
  int64_t t112;
  int64_t t113;
  uint8_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  uint8_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t v0;
  int64_t v1;
  gg_slice v2;
  gg_slice v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v0));
  fg149_witness(t1);
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t2) == (t3));
  if (t4) {
  {
  t5 = ((int64_t)(v1));
  t6 = v2;
  t0 = gg_function_3(t5, t6, base + 0);
  goto gg_done;
  }
  }
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(1))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t10 = ((int64_t)(v1));
  gg_function_14(t10, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(((int64_t)UINT64_C(2))));
  t13 = ((t11) == (t12));
  if (t13) {
  {
  t14 = v2;
  t15 = gg_function_15(t14, base + 0);
  t16 = t15;
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
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(3))));
  t19 = ((t17) == (t18));
  if (t19) {
  {
  t20 = v2;
  t21 = v3;
  t22 = ((int64_t)(v1));
  t23 = ((int64_t)(((int64_t)UINT64_C(20))));
  t24 = fg149_meta(t22, t23);
  t25 = ((int64_t)(t24));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((t25) != (t26));
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(((int64_t)UINT64_C(21))));
  t30 = fg149_meta(t28, t29);
  t31 = ((int64_t)(t30));
  t0 = gg_function_16(t20, t21, t27, t31, base + 0);
  goto gg_done;
  }
  }
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(4))));
  t34 = ((t32) == (t33));
  t35 = t34;
  if (!t35) {
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(((int64_t)UINT64_C(5))));
  t35 = ((t36) == (t37));
  }
  t38 = t35;
  if (t38) {
  {
  t39 = ((int64_t)(v1));
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(((int64_t)UINT64_C(4))));
  t43 = ((t41) == (t42));
  if (t43) {
  {
  t40 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t44 = ((int64_t)(t40));
  gg_function_17(t39, t44, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(6))));
  t47 = ((t45) == (t46));
  if (t47) {
  {
  t48 = ((int64_t)(v1));
  gg_function_19(t48, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t49 = ((int64_t)(v0));
  t50 = ((int64_t)(((int64_t)UINT64_C(7))));
  t51 = ((t49) == (t50));
  if (t51) {
  {
  t52 = ((int64_t)(v1));
  t53 = ((int64_t)(((int64_t)UINT64_C(21))));
  t54 = ((int64_t)(v1));
  t55 = ((int64_t)(((int64_t)UINT64_C(20))));
  t56 = fg149_meta(t54, t55);
  t57 = ((int64_t)(t56));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = fg149_action(t52, t53, t57, t58);
  v4 = ((int64_t)(t59));
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t60 = ((int64_t)(v0));
  t61 = ((int64_t)(((int64_t)UINT64_C(8))));
  t62 = ((t60) == (t61));
  if (t62) {
  {
  t63 = ((int64_t)(v1));
  t64 = ((int64_t)(((int64_t)UINT64_C(22))));
  t65 = ((int64_t)(v1));
  t66 = ((int64_t)(((int64_t)UINT64_C(20))));
  t67 = fg149_meta(t65, t66);
  t68 = ((int64_t)(t67));
  t69 = ((int64_t)(((int64_t)UINT64_C(0))));
  t70 = fg149_action(t63, t64, t68, t69);
  t0 = ((int64_t)(t70));
  goto gg_done;
  }
  }
  t71 = ((int64_t)(v0));
  t72 = ((int64_t)(((int64_t)UINT64_C(9))));
  t73 = ((t71) == (t72));
  if (t73) {
  {
  t74 = ((int64_t)(v1));
  t75 = gg_function_20(t74, base + 0);
  t76 = t75;
  if (t76) {
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
  t77 = ((int64_t)(v0));
  t78 = ((int64_t)(((int64_t)UINT64_C(10))));
  t79 = ((t77) == (t78));
  if (t79) {
  {
  t80 = ((int64_t)(v1));
  t81 = gg_function_21(t80, base + 0);
  t82 = t81;
  if (t82) {
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
  t83 = ((int64_t)(v0));
  t84 = ((int64_t)(((int64_t)UINT64_C(11))));
  t85 = ((t83) == (t84));
  if (t85) {
  {
  t86 = ((int64_t)(v1));
  t87 = gg_function_22(t86, base + 0);
  t88 = t87;
  if (t88) {
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
  t89 = ((int64_t)(v0));
  t90 = ((int64_t)(((int64_t)UINT64_C(12))));
  t91 = ((t89) == (t90));
  if (t91) {
  {
  t92 = ((int64_t)(v1));
  t93 = v2;
  gg_function_23(t92, t93, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t94 = ((int64_t)(v0));
  t95 = ((int64_t)(((int64_t)UINT64_C(13))));
  t96 = ((t94) == (t95));
  if (t96) {
  {
  t97 = ((int64_t)(v1));
  t98 = ((int64_t)(v1));
  t99 = ((int64_t)(((int64_t)UINT64_C(20))));
  t100 = fg149_meta(t98, t99);
  t101 = ((int64_t)(t100));
  t102 = v2;
  gg_function_25(t97, t101, t102, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t103 = ((int64_t)(v0));
  t104 = ((int64_t)(((int64_t)UINT64_C(15))));
  t105 = ((t103) == (t104));
  if (t105) {
  {
  t106 = ((int64_t)(v1));
  t107 = v2;
  gg_function_24(t106, t107, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t108 = ((int64_t)(v0));
  t109 = ((int64_t)(((int64_t)UINT64_C(16))));
  t110 = ((t108) == (t109));
  t111 = t110;
  if (!t111) {
  t112 = ((int64_t)(v0));
  t113 = ((int64_t)(((int64_t)UINT64_C(17))));
  t111 = ((t112) == (t113));
  }
  t114 = t111;
  if (t114) {
  {
  t115 = ((int64_t)(v1));
  t116 = ((int64_t)(v0));
  t117 = ((int64_t)(((int64_t)UINT64_C(17))));
  t118 = ((t116) == (t117));
  gg_function_26(t115, t118, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t119 = ((int64_t)(v0));
  t120 = ((int64_t)(((int64_t)UINT64_C(18))));
  if (!(((t119) == (t120)))) gg_fail("assertion failed");
  t121 = ((int64_t)(v1));
  t122 = ((int64_t)(v1));
  t123 = ((int64_t)(((int64_t)UINT64_C(20))));
  t124 = fg149_meta(t122, t123);
  t125 = ((int64_t)(t124));
  t0 = gg_function_12(t121, t125, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_0(int64_t base) {
  gg_type_2 t0;
  gg_type_2 t1;
  int64_t t2;
  int64_t t3;
  gg_slice t4;
  gg_slice t5;
  gg_type_2 v0;
  gg_type_2 v1;
  int64_t v2;
  {
  memset(&t0, 0, sizeof(t0));
  v0 = t0;
  memset(&t1, 0, sizeof(t1));
  v1 = t1;
  t2 = ((int64_t)(((int64_t)UINT64_C(2))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = gg_view((v0).data, 0);
  t5 = gg_view((v1).data, 0);
  v2 = gg_function_1(t2, t3, t4, t5, base + 0);
  }
gg_done:;
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
