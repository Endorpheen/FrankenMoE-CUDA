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
typedef struct { int64_t data[2]; } gg_type_2;
#pragma pack(pop)
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(int64_t a, int64_t b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, int64_t value);
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, gg_type_2 value);
static uint8_t gg_eq_3(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, gg_slice value);
static uint8_t gg_eq_4(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, uint8_t value);
static uint8_t gg_eq_6(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, uint8_t value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_1(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((gg_type_2*)a.data)[i],((gg_type_2*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b) {
return gg_equal_elements_1(gg_view((a).data, 2),gg_view((b).data, 2));
}
static void gg_render_2(gg_reference out, gg_type_2 value) {
int64_t i; gg_slice view=gg_view((value).data, 2);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((int64_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(gg_slice a, gg_slice b) {
return gg_equal_elements_1(a,b);
}
static void gg_render_3(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((int64_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_4(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_6(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static void gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, gg_slice p4, int64_t base);
static void gg_function_2(int64_t p0, int64_t p1, int64_t p2, gg_slice p3, int64_t base);
static int64_t gg_function_4(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static uint8_t gg_function_5(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_6(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_7(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_8(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_9(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_10(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_12(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_13(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_14(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_15(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_16(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_17(int64_t p0, int64_t p1, int64_t base);
static void gg_function_20(int64_t p0, int64_t base);
static void gg_function_118(int64_t p0, gg_slice p1, int64_t base);
static void gg_function_123(int64_t p0, uint8_t p1, int64_t base);
static int64_t gg_function_15(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t t13;
  int64_t t14;
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
  t10 = ((int64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) - (uint64_t)(t11)))));
  t8 = ((t9) <= (t12));
  }
  if (!(t8)) gg_fail("assertion failed");
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(v1));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_7(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
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
  if (!(t4)) gg_fail("assertion failed");
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) == (t8));
  t10 = t9;
  if (!t10) {
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t11) == (t12));
  }
  t13 = t10;
  if (t13) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t16 = ((int64_t)(v1));
  t17 = ((int64_t)(((int64_t)gg_div(t15, t16))));
  if (!(((t14) <= (t17)))) gg_fail("assertion failed");
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(v1));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_16(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
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
  int64_t t21;
  int64_t t22;
  uint8_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint8_t t30;
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  uint8_t t34;
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
  uint8_t t51;
  int64_t t52;
  int64_t t53;
  uint8_t t54;
  uint8_t t55;
  int64_t t56;
  int64_t t57;
  uint8_t t58;
  uint8_t t59;
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
  uint8_t t70;
  uint8_t t71;
  int64_t t72;
  int64_t t73;
  uint8_t t74;
  uint8_t t75;
  int64_t t76;
  int64_t t77;
  uint8_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
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
  t3 = ((int64_t)(((int64_t)UINT64_C(61))));
  t4 = fg143_ctl_meta(t1, t2, t3);
  t5 = ((int64_t)(t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) == (t6));
  t8 = t7;
  if (!t8) {
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(((int64_t)UINT64_C(62))));
  t12 = fg143_ctl_meta(t9, t10, t11);
  t13 = ((int64_t)(t12));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t13) != (t14));
  }
  t15 = t8;
  t16 = t15;
  if (!t16) {
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(((int64_t)UINT64_C(63))));
  t20 = fg143_ctl_meta(t17, t18, t19);
  t21 = ((int64_t)(t20));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t21) == (t22));
  }
  t23 = t16;
  if (t23) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t24 = ((int64_t)(v0));
  t25 = ((int64_t)(v1));
  t26 = ((int64_t)(((int64_t)UINT64_C(2))));
  t27 = fg143_ctl_meta(t24, t25, t26);
  v3 = ((int64_t)(t27));
  t28 = ((int64_t)(v3));
  t29 = ((int64_t)(((int64_t)UINT64_C(14))));
  t30 = ((t28) == (t29));
  t31 = t30;
  if (!t31) {
  t32 = ((int64_t)(v3));
  t33 = ((int64_t)(((int64_t)UINT64_C(13))));
  t31 = ((t32) == (t33));
  }
  t34 = t31;
  t35 = t34;
  if (!t35) {
  t36 = ((int64_t)(v3));
  t37 = ((int64_t)(((int64_t)UINT64_C(12))));
  t35 = ((t36) == (t37));
  }
  t38 = t35;
  t39 = t38;
  if (!t39) {
  t40 = ((int64_t)(v3));
  t41 = ((int64_t)(((int64_t)UINT64_C(11))));
  t39 = ((t40) == (t41));
  }
  t42 = t39;
  t43 = t42;
  if (!t43) {
  t44 = ((int64_t)(v3));
  t45 = ((int64_t)(((int64_t)UINT64_C(10))));
  t43 = ((t44) == (t45));
  }
  t46 = t43;
  t47 = t46;
  if (!t47) {
  t48 = ((int64_t)(v3));
  t49 = ((int64_t)(((int64_t)UINT64_C(23))));
  t47 = ((t48) == (t49));
  }
  t50 = t47;
  t51 = t50;
  if (!t51) {
  t52 = ((int64_t)(v3));
  t53 = ((int64_t)(((int64_t)UINT64_C(16))));
  t51 = ((t52) == (t53));
  }
  t54 = t51;
  t55 = t54;
  if (!t55) {
  t56 = ((int64_t)(v3));
  t57 = ((int64_t)(((int64_t)UINT64_C(17))));
  t55 = ((t56) == (t57));
  }
  t58 = t55;
  t59 = t58;
  if (!t59) {
  t60 = ((int64_t)(v3));
  t61 = ((int64_t)(((int64_t)UINT64_C(22))));
  t59 = ((t60) == (t61));
  }
  t62 = t59;
  t63 = t62;
  if (!t63) {
  t64 = ((int64_t)(v3));
  t65 = ((int64_t)(((int64_t)UINT64_C(18))));
  t63 = ((t64) == (t65));
  }
  t66 = t63;
  t67 = t66;
  if (!t67) {
  t68 = ((int64_t)(v3));
  t69 = ((int64_t)(((int64_t)UINT64_C(21))));
  t67 = ((t68) == (t69));
  }
  t70 = t67;
  t71 = t70;
  if (!t71) {
  t72 = ((int64_t)(v3));
  t73 = ((int64_t)(((int64_t)UINT64_C(19))));
  t71 = ((t72) == (t73));
  }
  t74 = t71;
  t75 = t74;
  if (!t75) {
  t76 = ((int64_t)(v3));
  t77 = ((int64_t)(((int64_t)UINT64_C(29))));
  t75 = ((t76) == (t77));
  }
  t78 = t75;
  if (t78) {
  {
  t79 = ((int64_t)(((int64_t)UINT64_C(64))));
  t80 = ((int64_t)(v2));
  t81 = ((int64_t)(v0));
  t82 = ((int64_t)(v1));
  t83 = ((int64_t)(((int64_t)UINT64_C(64))));
  t84 = fg143_ctl_meta(t81, t82, t83);
  t85 = ((int64_t)(t84));
  t86 = gg_function_7(t80, t85, base + 0);
  t0 = gg_function_15(t79, t86, base + 0);
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_123(int64_t p0, uint8_t p1, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  uint8_t t19;
  int64_t t20;
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
  uint8_t t31;
  uint8_t t32;
  uint8_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t v0;
  uint8_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  t0 = v1;
  if (t0) {
  {
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(10))));
  t3 = fg143_job_get(t1, t2);
  v2 = ((int64_t)(t3));
  }
  }
  t4 = v1;
  if (t4) {
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(11))));
  t7 = fg143_job_get(t5, t6);
  v3 = ((int64_t)(t7));
  }
  }
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t8) > (t9));
  t11 = t10;
  if (t11) {
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t12) >= (t13));
  }
  t14 = t11;
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(v2));
  t17 = ((int64_t)(v3));
  t15 = ((t16) < (t17));
  }
  t18 = t15;
  t19 = t18;
  if (t19) {
  t20 = ((int64_t)(v3));
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(4))));
  t23 = fg143_job_get(t21, t22);
  t24 = ((int64_t)(t23));
  t19 = ((t20) <= (t24));
  }
  if (!(t19)) gg_fail("assertion failed");
  t25 = ((int64_t)(v2));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(5))));
  t30 = ((int64_t)(v3));
  fg143_job_set(t28, t29, t30);
  }
  }
  t31 = v1;
  t32 = (!(t31));
  if (t32) {
  {
  fg143_job_barrier();
  }
  }
  t33 = v1;
  t34 = (!(t33));
  if (t34) {
  {
  t35 = ((int64_t)(v0));
  t36 = ((int64_t)(v2));
  fg143_job_place(t35, t36);
  }
  }
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(v2));
  fg143_job_compute(t37, t38);
  t39 = ((int64_t)(((int64_t)UINT64_C(4))));
  fg143_ctl_witness(t39);
  }
gg_done:;
}
static int64_t gg_function_8(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t0 = ((int64_t)(v0));
  }
  } else {
  {
  t0 = ((int64_t)(v1));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_118(int64_t p0, gg_slice p1, int64_t base) {
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
  uint8_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
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
  int64_t t45;
  int64_t t46;
  int64_t t47;
  uint8_t t48;
  int64_t t49;
  int64_t * t50;
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
  uint8_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  uint8_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  uint8_t t96;
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
  int64_t t110;
  int64_t t111;
  int64_t * t112;
  int64_t t113;
  int64_t v0;
  gg_slice v1;
  int64_t v2;
  uint8_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)((v1).len));
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = fg143_job_get(t4, t5);
  t7 = ((int64_t)(t6));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t7) != (t8));
  }
  if (!(t3)) gg_fail("assertion failed");
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  t11 = fg143_job_get(t9, t10);
  v2 = ((int64_t)(t11));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) > (t13));
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(2))));
  t18 = fg143_job_get(t16, t17);
  t19 = ((int64_t)(t18));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t19) >= (t20));
  }
  t21 = t15;
  t22 = t21;
  if (t22) {
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(((int64_t)UINT64_C(2))));
  t25 = fg143_job_get(t23, t24);
  t26 = ((int64_t)(t25));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = ((t26) == (t27));
  t29 = t28;
  if (!t29) {
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(((int64_t)UINT64_C(3))));
  t32 = fg143_job_get(t30, t31);
  t33 = ((int64_t)(t32));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = ((t33) != (t34));
  }
  t22 = t29;
  }
  if (!(t22)) gg_fail("assertion failed");
  t35 = ((int64_t)(v0));
  t36 = ((int64_t)(((int64_t)UINT64_C(6))));
  t37 = fg143_job_get(t35, t36);
  t38 = ((int64_t)(t37));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  v3 = ((t38) == (t39));
  t40 = v3;
  if (t40) {
  {
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(v2));
  fg143_job_create(t41, t42);
  t43 = ((int64_t)(v0));
  t44 = ((int64_t)(((int64_t)UINT64_C(6))));
  t45 = fg143_job_get(t43, t44);
  t46 = ((int64_t)(t45));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = ((t46) == (t47));
  if (t48) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t49 = ((int64_t)UINT64_C(0));
  t50 = ((int64_t *)(v1).data) + gg_index(t49, (v1).len);
  t51 = ((int64_t)(v0));
  t52 = ((int64_t)(((int64_t)UINT64_C(14))));
  t53 = fg143_job_get(t51, t52);
  (*t50) = ((int64_t)(t53));
  t54 = ((int64_t)(((int64_t)UINT64_C(3))));
  fg143_ctl_witness(t54);
  goto gg_done;
  }
  }
  }
  } else {
  {
  t55 = ((int64_t)(v0));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg143_job_set(t55, t56, t57);
  t58 = ((int64_t)(v0));
  t59 = ((int64_t)(((int64_t)UINT64_C(1))));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg143_job_set(t58, t59, t60);
  t61 = ((int64_t)(v0));
  t62 = ((int64_t)(((int64_t)UINT64_C(2))));
  t63 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg143_job_set(t61, t62, t63);
  t64 = ((int64_t)(v0));
  t65 = ((int64_t)(((int64_t)UINT64_C(3))));
  t66 = ((int64_t)UINT64_C(1));
  t67 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t66)))));
  fg143_job_set(t64, t65, t67);
  t68 = ((int64_t)(v0));
  t69 = ((int64_t)(((int64_t)UINT64_C(4))));
  t70 = ((int64_t)(v0));
  t71 = ((int64_t)(((int64_t)UINT64_C(12))));
  t72 = fg143_job_get(t70, t71);
  t73 = ((int64_t)(t72));
  fg143_job_set(t68, t69, t73);
  }
  }
  t74 = ((int64_t)(v0));
  t75 = ((int64_t)(((int64_t)UINT64_C(6))));
  t76 = fg143_job_get(t74, t75);
  t77 = ((int64_t)(t76));
  t78 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t77) != (t78)))) gg_fail("assertion failed");
  t79 = ((int64_t)(v0));
  t80 = ((int64_t)(((int64_t)UINT64_C(13))));
  t81 = fg143_job_get(t79, t80);
  t82 = ((int64_t)(t81));
  t83 = ((int64_t)(((int64_t)UINT64_C(0))));
  t84 = ((t82) != (t83));
  if (t84) {
  {
  t85 = ((int64_t)(v2));
  t86 = ((int64_t)(v0));
  t87 = ((int64_t)(((int64_t)UINT64_C(4))));
  t88 = fg143_job_get(t86, t87);
  t89 = ((int64_t)(t88));
  if (!(((t85) <= (t89)))) gg_fail("assertion failed");
  t90 = ((int64_t)(v2));
  t91 = ((int64_t)(((int64_t)UINT64_C(1))));
  t92 = ((t90) > (t91));
  if (t92) {
  {
  t93 = ((int64_t)(v0));
  t94 = ((int64_t)(v2));
  fg143_job_team(t93, t94);
  }
  } else {
  {
  t95 = ((int64_t)(v0));
  t96 = ((uint8_t)UINT64_C(1));
  gg_function_123(t95, t96, base + 0);
  }
  }
  }
  } else {
  {
  t97 = ((int64_t)(v2));
  t98 = ((int64_t)(v0));
  t99 = ((int64_t)(((int64_t)UINT64_C(4))));
  t100 = fg143_job_get(t98, t99);
  t101 = ((int64_t)(t100));
  v4 = gg_function_8(t97, t101, base + 0);
  t102 = ((int64_t)(v0));
  t103 = ((int64_t)(v4));
  fg149_job_kickoff(t102, t103);
  t104 = ((int64_t)(v0));
  t105 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg143_job_compute(t104, t105);
  }
  }
  fg143_job_clear_affinity();
  t106 = ((int64_t)(v0));
  t107 = ((int64_t)(((int64_t)UINT64_C(7))));
  t108 = fg143_job_get(t106, t107);
  v5 = ((int64_t)(t108));
  t109 = v3;
  if (t109) {
  {
  t110 = ((int64_t)(v0));
  fg143_job_free(t110);
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t111 = ((int64_t)UINT64_C(0));
  t112 = ((int64_t *)(v1).data) + gg_index(t111, (v1).len);
  (*t112) = ((int64_t)(v5));
  t113 = ((int64_t)(((int64_t)UINT64_C(3))));
  fg143_ctl_witness(t113);
  }
gg_done:;
}
static uint8_t gg_function_5(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t11;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t2 = ((int64_t)UINT64_C(4));
  t1 = 0;
label0:;
  if (t1 >= t2) goto label2;
  v2 = t1;
  {
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(10))));
  t6 = ((int64_t)(v2));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t5) + (uint64_t)(t6)))));
  t8 = fg143_ctl_meta(t3, t4, t7);
  t9 = ((int64_t)(t8));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) == (t10));
  if (t11) {
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
static void gg_function_20(int64_t p0, int64_t base) {
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
  uint8_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
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
  int64_t t40;
  uint8_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  uint8_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  uint8_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  uint8_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  uint8_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  uint8_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint8_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  uint8_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  uint8_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  uint8_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  uint8_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  uint8_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  uint8_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  uint8_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  uint8_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  uint8_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  uint8_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  uint8_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  uint8_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  uint8_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  uint8_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  uint8_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  uint8_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  uint8_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  uint8_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  uint8_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  uint8_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  uint8_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  uint8_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  uint8_t t161;
  int64_t t162;
  int64_t t163;
  int64_t t164;
  uint8_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  uint8_t t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  uint8_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  uint8_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  uint8_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  uint8_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  uint8_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  uint8_t t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  uint8_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  uint8_t t201;
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
  uint8_t t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  uint8_t t217;
  int64_t t218;
  int64_t t219;
  int64_t t220;
  uint8_t t221;
  int64_t t222;
  int64_t t223;
  int64_t t224;
  uint8_t t225;
  int64_t t226;
  int64_t t227;
  int64_t t228;
  uint8_t t229;
  int64_t t230;
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
  uint8_t t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  uint8_t t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  uint8_t t249;
  int64_t t250;
  int64_t t251;
  int64_t t252;
  uint8_t t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  uint8_t t257;
  int64_t t258;
  int64_t t259;
  int64_t t260;
  uint8_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  uint8_t t265;
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
  uint8_t t277;
  int64_t t278;
  int64_t t279;
  int64_t t280;
  uint8_t t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  uint8_t t285;
  int64_t t286;
  int64_t t287;
  int64_t t288;
  uint8_t t289;
  int64_t t290;
  int64_t t291;
  int64_t t292;
  uint8_t t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  uint8_t t297;
  int64_t t298;
  int64_t t299;
  int64_t t300;
  int64_t t301;
  int64_t t302;
  int64_t t303;
  uint8_t t304;
  uint8_t t305;
  int64_t t306;
  int64_t t307;
  int64_t t308;
  int64_t t309;
  int64_t t310;
  int64_t t311;
  uint8_t t312;
  int64_t t313;
  int64_t t314;
  int64_t t315;
  uint8_t t316;
  int64_t t317;
  int64_t t318;
  int64_t t319;
  uint8_t t320;
  int64_t t321;
  int64_t t322;
  int64_t t323;
  uint8_t t324;
  int64_t t325;
  int64_t t326;
  int64_t t327;
  uint8_t t328;
  int64_t t329;
  int64_t t330;
  int64_t t331;
  uint8_t t332;
  int64_t t333;
  int64_t t334;
  int64_t t335;
  uint8_t t336;
  int64_t t337;
  int64_t t338;
  int64_t t339;
  uint8_t t340;
  int64_t t341;
  int64_t t342;
  int64_t t343;
  uint8_t t344;
  int64_t t345;
  int64_t t346;
  int64_t t347;
  uint8_t t348;
  int64_t t349;
  int64_t t350;
  int64_t t351;
  uint8_t t352;
  int64_t t353;
  int64_t t354;
  int64_t t355;
  uint8_t t356;
  int64_t t357;
  int64_t t358;
  int64_t t359;
  uint8_t t360;
  int64_t t361;
  int64_t t362;
  int64_t t363;
  uint8_t t364;
  int64_t t365;
  int64_t t366;
  int64_t t367;
  uint8_t t368;
  int64_t t369;
  int64_t t370;
  int64_t t371;
  uint8_t t372;
  int64_t t373;
  int64_t t374;
  int64_t t375;
  uint8_t t376;
  int64_t t377;
  int64_t t378;
  int64_t t379;
  uint8_t t380;
  int64_t t381;
  int64_t t382;
  int64_t t383;
  uint8_t t384;
  int64_t t385;
  int64_t t386;
  int64_t t387;
  uint8_t t388;
  int64_t t389;
  int64_t t390;
  int64_t t391;
  uint8_t t392;
  int64_t t393;
  int64_t t394;
  int64_t t395;
  uint8_t t396;
  int64_t t397;
  int64_t t398;
  int64_t t399;
  uint8_t t400;
  int64_t t401;
  int64_t t402;
  int64_t t403;
  uint8_t t404;
  int64_t t405;
  int64_t t406;
  int64_t t407;
  uint8_t t408;
  int64_t t409;
  int64_t t410;
  int64_t t411;
  uint8_t t412;
  uint8_t t413;
  int64_t t414;
  int64_t t415;
  uint8_t t416;
  uint8_t t417;
  int64_t t418;
  int64_t t419;
  uint8_t t420;
  uint8_t t421;
  int64_t t422;
  int64_t t423;
  uint8_t t424;
  int64_t t425;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg143_ctl_meta(t0, t1, t2);
  v1 = ((int64_t)(t3));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) == (t5));
  t7 = t6;
  if (!t7) {
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = gg_function_5(t8, t9, base + 0);
  t7 = t10;
  }
  t11 = t7;
  if (t11) {
  {
  goto gg_done;
  }
  }
  t12 = ((int64_t)(v0));
  t13 = fg143_ctl_extra_compute(t12);
  t14 = t13;
  if (t14) {
  {
  goto gg_done;
  }
  }
  t15 = ((int64_t)(v1));
  t16 = ((int64_t)(((int64_t)UINT64_C(1))));
  t17 = ((t15) == (t16));
  if (t17) {
  {
  t18 = ((int64_t)(v0));
  fg143_ctl_leaf_dup(t18);
  }
  } else {
  t19 = ((int64_t)(v1));
  t20 = ((int64_t)(((int64_t)UINT64_C(2))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  t22 = ((int64_t)(v0));
  fg143_ctl_leaf_add(t22);
  }
  } else {
  t23 = ((int64_t)(v1));
  t24 = ((int64_t)(((int64_t)UINT64_C(3))));
  t25 = ((t23) == (t24));
  if (t25) {
  {
  t26 = ((int64_t)(v0));
  fg143_ctl_leaf_add_id(t26);
  }
  } else {
  t27 = ((int64_t)(v1));
  t28 = ((int64_t)(((int64_t)UINT64_C(4))));
  t29 = ((t27) == (t28));
  if (t29) {
  {
  t30 = ((int64_t)(v0));
  fg143_ctl_leaf_add1(t30);
  }
  } else {
  t31 = ((int64_t)(v1));
  t32 = ((int64_t)(((int64_t)UINT64_C(5))));
  t33 = ((t31) == (t32));
  if (t33) {
  {
  t34 = ((int64_t)(v0));
  fg143_ctl_leaf_acc(t34);
  }
  } else {
  t35 = ((int64_t)(v1));
  t36 = ((int64_t)(((int64_t)UINT64_C(6))));
  t37 = ((t35) == (t36));
  if (t37) {
  {
  t38 = ((int64_t)(v0));
  fg143_ctl_leaf_sub(t38);
  }
  } else {
  t39 = ((int64_t)(v1));
  t40 = ((int64_t)(((int64_t)UINT64_C(7))));
  t41 = ((t39) == (t40));
  if (t41) {
  {
  t42 = ((int64_t)(v0));
  fg143_ctl_leaf_mul(t42);
  }
  } else {
  t43 = ((int64_t)(v1));
  t44 = ((int64_t)(((int64_t)UINT64_C(8))));
  t45 = ((t43) == (t44));
  if (t45) {
  {
  t46 = ((int64_t)(v0));
  fg143_ctl_leaf_div(t46);
  }
  } else {
  t47 = ((int64_t)(v1));
  t48 = ((int64_t)(((int64_t)UINT64_C(9))));
  t49 = ((t47) == (t48));
  if (t49) {
  {
  t50 = ((int64_t)(v0));
  fg143_ctl_leaf_sqr(t50);
  }
  } else {
  t51 = ((int64_t)(v1));
  t52 = ((int64_t)(((int64_t)UINT64_C(10))));
  t53 = ((t51) == (t52));
  if (t53) {
  {
  t54 = ((int64_t)(v0));
  fg143_ctl_leaf_sqrt(t54);
  }
  } else {
  t55 = ((int64_t)(v1));
  t56 = ((int64_t)(((int64_t)UINT64_C(11))));
  t57 = ((t55) == (t56));
  if (t57) {
  {
  t58 = ((int64_t)(v0));
  fg143_ctl_leaf_log(t58);
  }
  } else {
  t59 = ((int64_t)(v1));
  t60 = ((int64_t)(((int64_t)UINT64_C(12))));
  t61 = ((t59) == (t60));
  if (t61) {
  {
  t62 = ((int64_t)(v0));
  fg143_ctl_leaf_sin(t62);
  }
  } else {
  t63 = ((int64_t)(v1));
  t64 = ((int64_t)(((int64_t)UINT64_C(13))));
  t65 = ((t63) == (t64));
  if (t65) {
  {
  t66 = ((int64_t)(v0));
  fg143_ctl_leaf_cos(t66);
  }
  } else {
  t67 = ((int64_t)(v1));
  t68 = ((int64_t)(((int64_t)UINT64_C(14))));
  t69 = ((t67) == (t68));
  if (t69) {
  {
  t70 = ((int64_t)(v0));
  fg143_ctl_leaf_sum(t70);
  }
  } else {
  t71 = ((int64_t)(v1));
  t72 = ((int64_t)(((int64_t)UINT64_C(15))));
  t73 = ((t71) == (t72));
  if (t73) {
  {
  t74 = ((int64_t)(v0));
  fg143_ctl_leaf_sum_rows(t74);
  }
  } else {
  t75 = ((int64_t)(v1));
  t76 = ((int64_t)(((int64_t)UINT64_C(16))));
  t77 = ((t75) == (t76));
  if (t77) {
  {
  t78 = ((int64_t)(v0));
  fg143_ctl_leaf_cumsum(t78);
  }
  } else {
  t79 = ((int64_t)(v1));
  t80 = ((int64_t)(((int64_t)UINT64_C(17))));
  t81 = ((t79) == (t80));
  if (t81) {
  {
  t82 = ((int64_t)(v0));
  fg143_ctl_leaf_mean(t82);
  }
  } else {
  t83 = ((int64_t)(v1));
  t84 = ((int64_t)(((int64_t)UINT64_C(18))));
  t85 = ((t83) == (t84));
  if (t85) {
  {
  t86 = ((int64_t)(v0));
  fg143_ctl_leaf_argmax(t86);
  }
  } else {
  t87 = ((int64_t)(v1));
  t88 = ((int64_t)(((int64_t)UINT64_C(19))));
  t89 = ((t87) == (t88));
  if (t89) {
  {
  t90 = ((int64_t)(v0));
  fg143_ctl_leaf_count_equal(t90);
  }
  } else {
  t91 = ((int64_t)(v1));
  t92 = ((int64_t)(((int64_t)UINT64_C(20))));
  t93 = ((t91) == (t92));
  if (t93) {
  {
  t94 = ((int64_t)(v0));
  fg143_ctl_leaf_repeat(t94);
  }
  } else {
  t95 = ((int64_t)(v1));
  t96 = ((int64_t)(((int64_t)UINT64_C(21))));
  t97 = ((t95) == (t96));
  if (t97) {
  {
  t98 = ((int64_t)(v0));
  fg143_ctl_leaf_repeat_back(t98);
  }
  } else {
  t99 = ((int64_t)(v1));
  t100 = ((int64_t)(((int64_t)UINT64_C(22))));
  t101 = ((t99) == (t100));
  if (t101) {
  {
  t102 = ((int64_t)(v0));
  fg143_ctl_leaf_concat(t102);
  }
  } else {
  t103 = ((int64_t)(v1));
  t104 = ((int64_t)(((int64_t)UINT64_C(23))));
  t105 = ((t103) == (t104));
  if (t105) {
  {
  t106 = ((int64_t)(v0));
  fg143_ctl_leaf_silu_back(t106);
  }
  } else {
  t107 = ((int64_t)(v1));
  t108 = ((int64_t)(((int64_t)UINT64_C(24))));
  t109 = ((t107) == (t108));
  if (t109) {
  {
  t110 = ((int64_t)(v0));
  fg143_ctl_leaf_norm(t110);
  }
  } else {
  t111 = ((int64_t)(v1));
  t112 = ((int64_t)(((int64_t)UINT64_C(25))));
  t113 = ((t111) == (t112));
  if (t113) {
  {
  t114 = ((int64_t)(v0));
  fg143_ctl_leaf_rms_norm(t114);
  }
  } else {
  t115 = ((int64_t)(v1));
  t116 = ((int64_t)(((int64_t)UINT64_C(26))));
  t117 = ((t115) == (t116));
  if (t117) {
  {
  t118 = ((int64_t)(v0));
  fg143_ctl_leaf_rms_norm_back(t118);
  }
  } else {
  t119 = ((int64_t)(v1));
  t120 = ((int64_t)(((int64_t)UINT64_C(27))));
  t121 = ((t119) == (t120));
  if (t121) {
  {
  t122 = ((int64_t)(v0));
  fg143_ctl_leaf_group_norm(t122);
  }
  } else {
  t123 = ((int64_t)(v1));
  t124 = ((int64_t)(((int64_t)UINT64_C(28))));
  t125 = ((t123) == (t124));
  if (t125) {
  {
  t126 = ((int64_t)(v0));
  fg143_ctl_leaf_l2_norm(t126);
  }
  } else {
  t127 = ((int64_t)(v1));
  t128 = ((int64_t)(((int64_t)UINT64_C(29))));
  t129 = ((t127) == (t128));
  if (t129) {
  {
  t130 = ((int64_t)(v0));
  fg143_ctl_leaf_mul_mat(t130);
  }
  } else {
  t131 = ((int64_t)(v1));
  t132 = ((int64_t)(((int64_t)UINT64_C(30))));
  t133 = ((t131) == (t132));
  if (t133) {
  {
  t134 = ((int64_t)(v0));
  fg143_ctl_leaf_mul_mat_id(t134);
  }
  } else {
  t135 = ((int64_t)(v1));
  t136 = ((int64_t)(((int64_t)UINT64_C(31))));
  t137 = ((t135) == (t136));
  if (t137) {
  {
  t138 = ((int64_t)(v0));
  fg143_ctl_leaf_out_prod(t138);
  }
  } else {
  t139 = ((int64_t)(v1));
  t140 = ((int64_t)(((int64_t)UINT64_C(32))));
  t141 = ((t139) == (t140));
  if (t141) {
  {
  t142 = ((int64_t)(v0));
  fg143_ctl_leaf_scale(t142);
  }
  } else {
  t143 = ((int64_t)(v1));
  t144 = ((int64_t)(((int64_t)UINT64_C(33))));
  t145 = ((t143) == (t144));
  if (t145) {
  {
  t146 = ((int64_t)(v0));
  fg143_ctl_leaf_set(t146);
  }
  } else {
  t147 = ((int64_t)(v1));
  t148 = ((int64_t)(((int64_t)UINT64_C(34))));
  t149 = ((t147) == (t148));
  if (t149) {
  {
  t150 = ((int64_t)(v0));
  fg143_ctl_leaf_cpy(t150);
  }
  } else {
  t151 = ((int64_t)(v1));
  t152 = ((int64_t)(((int64_t)UINT64_C(35))));
  t153 = ((t151) == (t152));
  if (t153) {
  {
  t154 = ((int64_t)(v0));
  fg143_ctl_leaf_cont(t154);
  }
  } else {
  t155 = ((int64_t)(v1));
  t156 = ((int64_t)(((int64_t)UINT64_C(40))));
  t157 = ((t155) == (t156));
  if (t157) {
  {
  t158 = ((int64_t)(v0));
  fg143_ctl_leaf_get_rows(t158);
  }
  } else {
  t159 = ((int64_t)(v1));
  t160 = ((int64_t)(((int64_t)UINT64_C(41))));
  t161 = ((t159) == (t160));
  if (t161) {
  {
  t162 = ((int64_t)(v0));
  fg143_ctl_leaf_get_rows_back(t162);
  }
  } else {
  t163 = ((int64_t)(v1));
  t164 = ((int64_t)(((int64_t)UINT64_C(42))));
  t165 = ((t163) == (t164));
  if (t165) {
  {
  t166 = ((int64_t)(v0));
  fg143_ctl_leaf_set_rows(t166);
  }
  } else {
  t167 = ((int64_t)(v1));
  t168 = ((int64_t)(((int64_t)UINT64_C(43))));
  t169 = ((t167) == (t168));
  if (t169) {
  {
  t170 = ((int64_t)(v0));
  fg143_ctl_leaf_diag(t170);
  }
  } else {
  t171 = ((int64_t)(v1));
  t172 = ((int64_t)(((int64_t)UINT64_C(44))));
  t173 = ((t171) == (t172));
  if (t173) {
  {
  t174 = ((int64_t)(v0));
  fg143_ctl_leaf_diag_mask_inf(t174);
  }
  } else {
  t175 = ((int64_t)(v1));
  t176 = ((int64_t)(((int64_t)UINT64_C(45))));
  t177 = ((t175) == (t176));
  if (t177) {
  {
  t178 = ((int64_t)(v0));
  fg143_ctl_leaf_diag_mask_zero(t178);
  }
  } else {
  t179 = ((int64_t)(v1));
  t180 = ((int64_t)(((int64_t)UINT64_C(46))));
  t181 = ((t179) == (t180));
  if (t181) {
  {
  t182 = ((int64_t)(v0));
  fg143_ctl_leaf_soft_max(t182);
  }
  } else {
  t183 = ((int64_t)(v1));
  t184 = ((int64_t)(((int64_t)UINT64_C(47))));
  t185 = ((t183) == (t184));
  if (t185) {
  {
  t186 = ((int64_t)(v0));
  fg143_ctl_leaf_soft_max_back(t186);
  }
  } else {
  t187 = ((int64_t)(v1));
  t188 = ((int64_t)(((int64_t)UINT64_C(48))));
  t189 = ((t187) == (t188));
  if (t189) {
  {
  t190 = ((int64_t)(v0));
  fg143_ctl_leaf_rope(t190);
  }
  } else {
  t191 = ((int64_t)(v1));
  t192 = ((int64_t)(((int64_t)UINT64_C(49))));
  t193 = ((t191) == (t192));
  if (t193) {
  {
  t194 = ((int64_t)(v0));
  fg143_ctl_leaf_rope_back(t194);
  }
  } else {
  t195 = ((int64_t)(v1));
  t196 = ((int64_t)(((int64_t)UINT64_C(50))));
  t197 = ((t195) == (t196));
  if (t197) {
  {
  t198 = ((int64_t)(v0));
  fg143_ctl_leaf_clamp(t198);
  }
  } else {
  t199 = ((int64_t)(v1));
  t200 = ((int64_t)(((int64_t)UINT64_C(51))));
  t201 = ((t199) == (t200));
  if (t201) {
  {
  t202 = ((int64_t)(v0));
  fg143_ctl_leaf_conv_transpose_1d(t202);
  }
  } else {
  t203 = ((int64_t)(v1));
  t204 = ((int64_t)(((int64_t)UINT64_C(52))));
  t205 = ((t203) == (t204));
  if (t205) {
  {
  t206 = ((int64_t)(v0));
  fg143_ctl_leaf_im2col(t206);
  }
  } else {
  t207 = ((int64_t)(v1));
  t208 = ((int64_t)(((int64_t)UINT64_C(53))));
  t209 = ((t207) == (t208));
  if (t209) {
  {
  t210 = ((int64_t)(v0));
  fg143_ctl_leaf_im2col_back(t210);
  }
  } else {
  t211 = ((int64_t)(v1));
  t212 = ((int64_t)(((int64_t)UINT64_C(54))));
  t213 = ((t211) == (t212));
  if (t213) {
  {
  t214 = ((int64_t)(v0));
  fg143_ctl_leaf_im2col_3d(t214);
  }
  } else {
  t215 = ((int64_t)(v1));
  t216 = ((int64_t)(((int64_t)UINT64_C(55))));
  t217 = ((t215) == (t216));
  if (t217) {
  {
  t218 = ((int64_t)(v0));
  fg143_ctl_leaf_col2im_1d(t218);
  }
  } else {
  t219 = ((int64_t)(v1));
  t220 = ((int64_t)(((int64_t)UINT64_C(56))));
  t221 = ((t219) == (t220));
  if (t221) {
  {
  t222 = ((int64_t)(v0));
  fg143_ctl_leaf_conv_2d(t222);
  }
  } else {
  t223 = ((int64_t)(v1));
  t224 = ((int64_t)(((int64_t)UINT64_C(57))));
  t225 = ((t223) == (t224));
  if (t225) {
  {
  t226 = ((int64_t)(v0));
  fg143_ctl_leaf_conv_3d(t226);
  }
  } else {
  t227 = ((int64_t)(v1));
  t228 = ((int64_t)(((int64_t)UINT64_C(58))));
  t229 = ((t227) == (t228));
  if (t229) {
  {
  t230 = ((int64_t)(v0));
  fg143_ctl_leaf_conv_2d_dw(t230);
  }
  } else {
  t231 = ((int64_t)(v1));
  t232 = ((int64_t)(((int64_t)UINT64_C(59))));
  t233 = ((t231) == (t232));
  if (t233) {
  {
  t234 = ((int64_t)(v0));
  fg143_ctl_leaf_conv_transpose_2d(t234);
  }
  } else {
  t235 = ((int64_t)(v1));
  t236 = ((int64_t)(((int64_t)UINT64_C(60))));
  t237 = ((t235) == (t236));
  if (t237) {
  {
  t238 = ((int64_t)(v0));
  fg143_ctl_leaf_pool_1d(t238);
  }
  } else {
  t239 = ((int64_t)(v1));
  t240 = ((int64_t)(((int64_t)UINT64_C(61))));
  t241 = ((t239) == (t240));
  if (t241) {
  {
  t242 = ((int64_t)(v0));
  fg143_ctl_leaf_pool_2d(t242);
  }
  } else {
  t243 = ((int64_t)(v1));
  t244 = ((int64_t)(((int64_t)UINT64_C(62))));
  t245 = ((t243) == (t244));
  if (t245) {
  {
  t246 = ((int64_t)(v0));
  fg143_ctl_leaf_pool_2d_back(t246);
  }
  } else {
  t247 = ((int64_t)(v1));
  t248 = ((int64_t)(((int64_t)UINT64_C(63))));
  t249 = ((t247) == (t248));
  if (t249) {
  {
  t250 = ((int64_t)(v0));
  fg143_ctl_leaf_upscale(t250);
  }
  } else {
  t251 = ((int64_t)(v1));
  t252 = ((int64_t)(((int64_t)UINT64_C(64))));
  t253 = ((t251) == (t252));
  if (t253) {
  {
  t254 = ((int64_t)(v0));
  fg143_ctl_leaf_pad(t254);
  }
  } else {
  t255 = ((int64_t)(v1));
  t256 = ((int64_t)(((int64_t)UINT64_C(65))));
  t257 = ((t255) == (t256));
  if (t257) {
  {
  t258 = ((int64_t)(v0));
  fg143_ctl_leaf_pad_reflect_1d(t258);
  }
  } else {
  t259 = ((int64_t)(v1));
  t260 = ((int64_t)(((int64_t)UINT64_C(66))));
  t261 = ((t259) == (t260));
  if (t261) {
  {
  t262 = ((int64_t)(v0));
  fg143_ctl_leaf_roll(t262);
  }
  } else {
  t263 = ((int64_t)(v1));
  t264 = ((int64_t)(((int64_t)UINT64_C(67))));
  t265 = ((t263) == (t264));
  if (t265) {
  {
  t266 = ((int64_t)(v0));
  fg143_ctl_leaf_arange(t266);
  }
  } else {
  t267 = ((int64_t)(v1));
  t268 = ((int64_t)(((int64_t)UINT64_C(68))));
  t269 = ((t267) == (t268));
  if (t269) {
  {
  t270 = ((int64_t)(v0));
  fg143_ctl_leaf_timestep_embedding(t270);
  }
  } else {
  t271 = ((int64_t)(v1));
  t272 = ((int64_t)(((int64_t)UINT64_C(69))));
  t273 = ((t271) == (t272));
  if (t273) {
  {
  t274 = ((int64_t)(v0));
  fg143_ctl_leaf_argsort(t274);
  }
  } else {
  t275 = ((int64_t)(v1));
  t276 = ((int64_t)(((int64_t)UINT64_C(70))));
  t277 = ((t275) == (t276));
  if (t277) {
  {
  t278 = ((int64_t)(v0));
  fg143_ctl_leaf_top_k(t278);
  }
  } else {
  t279 = ((int64_t)(v1));
  t280 = ((int64_t)(((int64_t)UINT64_C(71))));
  t281 = ((t279) == (t280));
  if (t281) {
  {
  t282 = ((int64_t)(v0));
  fg143_ctl_leaf_leaky_relu(t282);
  }
  } else {
  t283 = ((int64_t)(v1));
  t284 = ((int64_t)(((int64_t)UINT64_C(72))));
  t285 = ((t283) == (t284));
  if (t285) {
  {
  t286 = ((int64_t)(v0));
  fg143_ctl_leaf_tri(t286);
  }
  } else {
  t287 = ((int64_t)(v1));
  t288 = ((int64_t)(((int64_t)UINT64_C(73))));
  t289 = ((t287) == (t288));
  if (t289) {
  {
  t290 = ((int64_t)(v0));
  fg143_ctl_leaf_fill(t290);
  }
  } else {
  t291 = ((int64_t)(v1));
  t292 = ((int64_t)(((int64_t)UINT64_C(74))));
  t293 = ((t291) == (t292));
  if (t293) {
  {
  t294 = ((int64_t)(v0));
  fg143_ctl_leaf_flash_attn_ext(t294);
  }
  } else {
  t295 = ((int64_t)(v1));
  t296 = ((int64_t)(((int64_t)UINT64_C(75))));
  t297 = ((t295) == (t296));
  if (t297) {
  {
  t298 = ((int64_t)(v0));
  t299 = ((int64_t)(((int64_t)UINT64_C(0))));
  t300 = ((int64_t)(((int64_t)UINT64_C(8))));
  t301 = fg143_ctl_meta(t298, t299, t300);
  v2 = ((int64_t)(t301));
  t302 = ((int64_t)(v2));
  t303 = ((int64_t)(((int64_t)UINT64_C(0))));
  t304 = ((t302) == (t303));
  t305 = t304;
  if (!t305) {
  t306 = ((int64_t)(v2));
  t307 = ((int64_t)(((int64_t)UINT64_C(1))));
  t305 = ((t306) == (t307));
  }
  if (!(t305)) gg_fail("assertion failed");
  t308 = ((int64_t)(v0));
  t309 = ((int64_t)(v2));
  fg143_ctl_leaf_flash_attn_back(t308, t309);
  }
  } else {
  t310 = ((int64_t)(v1));
  t311 = ((int64_t)(((int64_t)UINT64_C(76))));
  t312 = ((t310) == (t311));
  if (t312) {
  {
  t313 = ((int64_t)(v0));
  fg143_ctl_leaf_ssm_conv(t313);
  }
  } else {
  t314 = ((int64_t)(v1));
  t315 = ((int64_t)(((int64_t)UINT64_C(77))));
  t316 = ((t314) == (t315));
  if (t316) {
  {
  t317 = ((int64_t)(v0));
  fg143_ctl_leaf_ssm_scan(t317);
  }
  } else {
  t318 = ((int64_t)(v1));
  t319 = ((int64_t)(((int64_t)UINT64_C(78))));
  t320 = ((t318) == (t319));
  if (t320) {
  {
  t321 = ((int64_t)(v0));
  fg143_ctl_leaf_win_part(t321);
  }
  } else {
  t322 = ((int64_t)(v1));
  t323 = ((int64_t)(((int64_t)UINT64_C(79))));
  t324 = ((t322) == (t323));
  if (t324) {
  {
  t325 = ((int64_t)(v0));
  fg143_ctl_leaf_win_unpart(t325);
  }
  } else {
  t326 = ((int64_t)(v1));
  t327 = ((int64_t)(((int64_t)UINT64_C(91))));
  t328 = ((t326) == (t327));
  if (t328) {
  {
  t329 = ((int64_t)(v0));
  fg143_ctl_leaf_unary(t329);
  }
  } else {
  t330 = ((int64_t)(v1));
  t331 = ((int64_t)(((int64_t)UINT64_C(100))));
  t332 = ((t330) == (t331));
  if (t332) {
  {
  t333 = ((int64_t)(v0));
  fg143_ctl_leaf_glu(t333);
  }
  } else {
  t334 = ((int64_t)(v1));
  t335 = ((int64_t)(((int64_t)UINT64_C(80))));
  t336 = ((t334) == (t335));
  if (t336) {
  {
  t337 = ((int64_t)(v0));
  fg143_ctl_leaf_get_rel_pos(t337);
  }
  } else {
  t338 = ((int64_t)(v1));
  t339 = ((int64_t)(((int64_t)UINT64_C(81))));
  t340 = ((t338) == (t339));
  if (t340) {
  {
  t341 = ((int64_t)(v0));
  fg143_ctl_leaf_add_rel_pos(t341);
  }
  } else {
  t342 = ((int64_t)(v1));
  t343 = ((int64_t)(((int64_t)UINT64_C(82))));
  t344 = ((t342) == (t343));
  if (t344) {
  {
  t345 = ((int64_t)(v0));
  fg143_ctl_leaf_rwkv_wkv6(t345);
  }
  } else {
  t346 = ((int64_t)(v1));
  t347 = ((int64_t)(((int64_t)UINT64_C(83))));
  t348 = ((t346) == (t347));
  if (t348) {
  {
  t349 = ((int64_t)(v0));
  fg143_ctl_leaf_gated_linear_attn(t349);
  }
  } else {
  t350 = ((int64_t)(v1));
  t351 = ((int64_t)(((int64_t)UINT64_C(84))));
  t352 = ((t350) == (t351));
  if (t352) {
  {
  t353 = ((int64_t)(v0));
  fg143_ctl_leaf_rwkv_wkv7(t353);
  }
  } else {
  t354 = ((int64_t)(v1));
  t355 = ((int64_t)(((int64_t)UINT64_C(85))));
  t356 = ((t354) == (t355));
  if (t356) {
  {
  t357 = ((int64_t)(v0));
  fg143_ctl_leaf_solve_tri(t357);
  }
  } else {
  t358 = ((int64_t)(v1));
  t359 = ((int64_t)(((int64_t)UINT64_C(86))));
  t360 = ((t358) == (t359));
  if (t360) {
  {
  t361 = ((int64_t)(v0));
  fg143_ctl_leaf_gated_delta_net(t361);
  }
  } else {
  t362 = ((int64_t)(v1));
  t363 = ((int64_t)(((int64_t)UINT64_C(87))));
  t364 = ((t362) == (t363));
  if (t364) {
  {
  t365 = ((int64_t)(v0));
  fg143_ctl_leaf_lightning_indexer(t365);
  }
  } else {
  t366 = ((int64_t)(v1));
  t367 = ((int64_t)(((int64_t)UINT64_C(88))));
  t368 = ((t366) == (t367));
  if (t368) {
  {
  t369 = ((int64_t)(v0));
  fg143_ctl_leaf_dsv4_hc_comb(t369);
  }
  } else {
  t370 = ((int64_t)(v1));
  t371 = ((int64_t)(((int64_t)UINT64_C(89))));
  t372 = ((t370) == (t371));
  if (t372) {
  {
  t373 = ((int64_t)(v0));
  fg143_ctl_leaf_dsv4_hc_pre(t373);
  }
  } else {
  t374 = ((int64_t)(v1));
  t375 = ((int64_t)(((int64_t)UINT64_C(90))));
  t376 = ((t374) == (t375));
  if (t376) {
  {
  t377 = ((int64_t)(v0));
  fg143_ctl_leaf_dsv4_hc_post(t377);
  }
  } else {
  t378 = ((int64_t)(v1));
  t379 = ((int64_t)(((int64_t)UINT64_C(92))));
  t380 = ((t378) == (t379));
  if (t380) {
  {
  t381 = ((int64_t)(v0));
  fg143_ctl_leaf_map_custom1(t381);
  }
  } else {
  t382 = ((int64_t)(v1));
  t383 = ((int64_t)(((int64_t)UINT64_C(93))));
  t384 = ((t382) == (t383));
  if (t384) {
  {
  t385 = ((int64_t)(v0));
  fg143_ctl_leaf_map_custom2(t385);
  }
  } else {
  t386 = ((int64_t)(v1));
  t387 = ((int64_t)(((int64_t)UINT64_C(94))));
  t388 = ((t386) == (t387));
  if (t388) {
  {
  t389 = ((int64_t)(v0));
  fg143_ctl_leaf_map_custom3(t389);
  }
  } else {
  t390 = ((int64_t)(v1));
  t391 = ((int64_t)(((int64_t)UINT64_C(95))));
  t392 = ((t390) == (t391));
  if (t392) {
  {
  t393 = ((int64_t)(v0));
  fg143_ctl_leaf_custom(t393);
  }
  } else {
  t394 = ((int64_t)(v1));
  t395 = ((int64_t)(((int64_t)UINT64_C(96))));
  t396 = ((t394) == (t395));
  if (t396) {
  {
  t397 = ((int64_t)(v0));
  fg143_ctl_leaf_cross_entropy_loss(t397);
  }
  } else {
  t398 = ((int64_t)(v1));
  t399 = ((int64_t)(((int64_t)UINT64_C(97))));
  t400 = ((t398) == (t399));
  if (t400) {
  {
  t401 = ((int64_t)(v0));
  fg143_ctl_leaf_cross_entropy_loss_back(t401);
  }
  } else {
  t402 = ((int64_t)(v1));
  t403 = ((int64_t)(((int64_t)UINT64_C(98))));
  t404 = ((t402) == (t403));
  if (t404) {
  {
  t405 = ((int64_t)(v0));
  fg143_ctl_leaf_opt_step_adamw(t405);
  }
  } else {
  t406 = ((int64_t)(v1));
  t407 = ((int64_t)(((int64_t)UINT64_C(99))));
  t408 = ((t406) == (t407));
  if (t408) {
  {
  t409 = ((int64_t)(v0));
  fg143_ctl_leaf_opt_step_sgd(t409);
  }
  } else {
  t410 = ((int64_t)(v1));
  t411 = ((int64_t)(((int64_t)UINT64_C(36))));
  t412 = ((t410) == (t411));
  t413 = t412;
  if (!t413) {
  t414 = ((int64_t)(v1));
  t415 = ((int64_t)(((int64_t)UINT64_C(37))));
  t413 = ((t414) == (t415));
  }
  t416 = t413;
  t417 = t416;
  if (!t417) {
  t418 = ((int64_t)(v1));
  t419 = ((int64_t)(((int64_t)UINT64_C(39))));
  t417 = ((t418) == (t419));
  }
  t420 = t417;
  t421 = t420;
  if (!t421) {
  t422 = ((int64_t)(v1));
  t423 = ((int64_t)(((int64_t)UINT64_C(38))));
  t421 = ((t422) == (t423));
  }
  t424 = t421;
  if (t424) {
  {
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  t425 = ((int64_t)(((int64_t)UINT64_C(2))));
  fg143_ctl_witness(t425);
  }
gg_done:;
}
static int64_t gg_function_6(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)UINT64_C(3))));
  t4 = ((t5) < (t6));
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
  t14 = ((int64_t)(((int64_t)UINT64_C(4))));
  t12 = ((t13) < (t14));
  }
  if (!(t12)) gg_fail("assertion failed");
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(v1));
  t17 = ((int64_t)(((int64_t)UINT64_C(20))));
  t18 = ((int64_t)(((int64_t)UINT64_C(4))));
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t20)))));
  t22 = ((int64_t)(v3));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t22)))));
  t24 = fg143_ctl_meta(t15, t16, t23);
  t0 = ((int64_t)(t24));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_4(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
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
  int64_t t16;
  uint8_t t17;
  int64_t t18;
  int64_t t19;
  uint8_t t20;
  uint8_t t21;
  int64_t t22;
  int64_t t23;
  uint8_t t24;
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  uint8_t t32;
  uint8_t t33;
  int64_t t34;
  int64_t t35;
  uint8_t t36;
  int64_t t37;
  int64_t t38;
  uint8_t t39;
  uint8_t t40;
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
  uint8_t t52;
  uint8_t t53;
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
  uint8_t t83;
  uint8_t t84;
  int64_t t85;
  int64_t t86;
  uint8_t t87;
  uint8_t t88;
  int64_t t89;
  int64_t t90;
  uint8_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  uint8_t t100;
  uint8_t t101;
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
  uint8_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  uint8_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  uint8_t t127;
  uint8_t t128;
  int64_t t129;
  int64_t t130;
  uint8_t t131;
  uint8_t t132;
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
  uint8_t t144;
  int64_t t145;
  int64_t t146;
  uint8_t t147;
  uint8_t t148;
  int64_t t149;
  int64_t t150;
  uint8_t t151;
  uint8_t t152;
  int64_t t153;
  int64_t t154;
  uint8_t t155;
  uint8_t t156;
  int64_t t157;
  int64_t t158;
  uint8_t t159;
  uint8_t t160;
  int64_t t161;
  int64_t t162;
  uint8_t t163;
  uint8_t t164;
  int64_t t165;
  int64_t t166;
  uint8_t t167;
  uint8_t t168;
  int64_t t169;
  int64_t t170;
  uint8_t t171;
  uint8_t t172;
  int64_t t173;
  int64_t t174;
  uint8_t t175;
  uint8_t t176;
  int64_t t177;
  int64_t t178;
  uint8_t t179;
  uint8_t t180;
  int64_t t181;
  int64_t t182;
  uint8_t t183;
  uint8_t t184;
  int64_t t185;
  int64_t t186;
  uint8_t t187;
  uint8_t t188;
  int64_t t189;
  int64_t t190;
  uint8_t t191;
  uint8_t t192;
  int64_t t193;
  int64_t t194;
  uint8_t t195;
  uint8_t t196;
  int64_t t197;
  int64_t t198;
  uint8_t t199;
  uint8_t t200;
  int64_t t201;
  int64_t t202;
  uint8_t t203;
  uint8_t t204;
  int64_t t205;
  int64_t t206;
  uint8_t t207;
  uint8_t t208;
  int64_t t209;
  int64_t t210;
  uint8_t t211;
  uint8_t t212;
  int64_t t213;
  int64_t t214;
  uint8_t t215;
  uint8_t t216;
  int64_t t217;
  int64_t t218;
  uint8_t t219;
  uint8_t t220;
  int64_t t221;
  int64_t t222;
  uint8_t t223;
  uint8_t t224;
  int64_t t225;
  int64_t t226;
  uint8_t t227;
  uint8_t t228;
  int64_t t229;
  int64_t t230;
  uint8_t t231;
  uint8_t t232;
  int64_t t233;
  int64_t t234;
  uint8_t t235;
  uint8_t t236;
  int64_t t237;
  int64_t t238;
  uint8_t t239;
  uint8_t t240;
  int64_t t241;
  int64_t t242;
  uint8_t t243;
  int64_t t244;
  int64_t t245;
  uint8_t t246;
  uint8_t t247;
  int64_t t248;
  int64_t t249;
  uint8_t t250;
  uint8_t t251;
  int64_t t252;
  int64_t t253;
  uint8_t t254;
  uint8_t t255;
  int64_t t256;
  int64_t t257;
  uint8_t t258;
  uint8_t t259;
  int64_t t260;
  int64_t t261;
  uint8_t t262;
  uint8_t t263;
  int64_t t264;
  int64_t t265;
  uint8_t t266;
  uint8_t t267;
  int64_t t268;
  int64_t t269;
  uint8_t t270;
  uint8_t t271;
  int64_t t272;
  int64_t t273;
  uint8_t t274;
  uint8_t t275;
  int64_t t276;
  int64_t t277;
  uint8_t t278;
  uint8_t t279;
  int64_t t280;
  int64_t t281;
  uint8_t t282;
  uint8_t t283;
  int64_t t284;
  int64_t t285;
  uint8_t t286;
  uint8_t t287;
  int64_t t288;
  int64_t t289;
  uint8_t t290;
  uint8_t t291;
  int64_t t292;
  int64_t t293;
  uint8_t t294;
  uint8_t t295;
  int64_t t296;
  int64_t t297;
  uint8_t t298;
  uint8_t t299;
  int64_t t300;
  int64_t t301;
  uint8_t t302;
  uint8_t t303;
  int64_t t304;
  int64_t t305;
  uint8_t t306;
  uint8_t t307;
  int64_t t308;
  int64_t t309;
  uint8_t t310;
  uint8_t t311;
  int64_t t312;
  int64_t t313;
  uint8_t t314;
  uint8_t t315;
  int64_t t316;
  int64_t t317;
  uint8_t t318;
  uint8_t t319;
  int64_t t320;
  int64_t t321;
  uint8_t t322;
  uint8_t t323;
  int64_t t324;
  int64_t t325;
  uint8_t t326;
  uint8_t t327;
  int64_t t328;
  int64_t t329;
  uint8_t t330;
  uint8_t t331;
  int64_t t332;
  int64_t t333;
  uint8_t t334;
  uint8_t t335;
  int64_t t336;
  int64_t t337;
  uint8_t t338;
  uint8_t t339;
  int64_t t340;
  int64_t t341;
  uint8_t t342;
  uint8_t t343;
  int64_t t344;
  int64_t t345;
  uint8_t t346;
  uint8_t t347;
  int64_t t348;
  int64_t t349;
  uint8_t t350;
  uint8_t t351;
  int64_t t352;
  int64_t t353;
  uint8_t t354;
  uint8_t t355;
  int64_t t356;
  int64_t t357;
  uint8_t t358;
  uint8_t t359;
  int64_t t360;
  int64_t t361;
  uint8_t t362;
  uint8_t t363;
  int64_t t364;
  int64_t t365;
  uint8_t t366;
  uint8_t t367;
  int64_t t368;
  int64_t t369;
  uint8_t t370;
  uint8_t t371;
  int64_t t372;
  int64_t t373;
  uint8_t t374;
  uint8_t t375;
  int64_t t376;
  int64_t t377;
  uint8_t t378;
  uint8_t t379;
  int64_t t380;
  int64_t t381;
  uint8_t t382;
  uint8_t t383;
  int64_t t384;
  int64_t t385;
  uint8_t t386;
  uint8_t t387;
  int64_t t388;
  int64_t t389;
  uint8_t t390;
  uint8_t t391;
  int64_t t392;
  int64_t t393;
  uint8_t t394;
  uint8_t t395;
  int64_t t396;
  int64_t t397;
  uint8_t t398;
  uint8_t t399;
  int64_t t400;
  int64_t t401;
  uint8_t t402;
  uint8_t t403;
  int64_t t404;
  int64_t t405;
  uint8_t t406;
  uint8_t t407;
  int64_t t408;
  int64_t t409;
  uint8_t t410;
  uint8_t t411;
  int64_t t412;
  int64_t t413;
  uint8_t t414;
  uint8_t t415;
  int64_t t416;
  int64_t t417;
  uint8_t t418;
  uint8_t t419;
  int64_t t420;
  int64_t t421;
  uint8_t t422;
  uint8_t t423;
  int64_t t424;
  int64_t t425;
  uint8_t t426;
  uint8_t t427;
  int64_t t428;
  int64_t t429;
  uint8_t t430;
  uint8_t t431;
  int64_t t432;
  int64_t t433;
  uint8_t t434;
  uint8_t t435;
  int64_t t436;
  int64_t t437;
  uint8_t t438;
  uint8_t t439;
  int64_t t440;
  int64_t t441;
  uint8_t t442;
  uint8_t t443;
  int64_t t444;
  int64_t t445;
  uint8_t t446;
  uint8_t t447;
  int64_t t448;
  int64_t t449;
  uint8_t t450;
  uint8_t t451;
  int64_t t452;
  int64_t t453;
  uint8_t t454;
  uint8_t t455;
  int64_t t456;
  int64_t t457;
  uint8_t t458;
  uint8_t t459;
  int64_t t460;
  int64_t t461;
  uint8_t t462;
  uint8_t t463;
  int64_t t464;
  int64_t t465;
  uint8_t t466;
  uint8_t t467;
  int64_t t468;
  int64_t t469;
  uint8_t t470;
  uint8_t t471;
  int64_t t472;
  int64_t t473;
  uint8_t t474;
  uint8_t t475;
  int64_t t476;
  int64_t t477;
  uint8_t t478;
  uint8_t t479;
  int64_t t480;
  int64_t t481;
  uint8_t t482;
  uint8_t t483;
  int64_t t484;
  int64_t t485;
  uint8_t t486;
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
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t1) > (t2)))) gg_fail("assertion failed");
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(v1));
  t5 = gg_function_5(t3, t4, base + 0);
  t6 = t5;
  if (t6) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = fg143_ctl_meta(t7, t8, t9);
  v3 = ((int64_t)(t10));
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(((int64_t)UINT64_C(8))));
  t14 = fg143_ctl_meta(t11, t12, t13);
  v4 = ((int64_t)(t14));
  t15 = ((int64_t)(v3));
  t16 = ((int64_t)(((int64_t)UINT64_C(91))));
  t17 = ((t15) == (t16));
  if (t17) {
  {
  t18 = ((int64_t)(v4));
  t19 = ((int64_t)(((int64_t)UINT64_C(8))));
  t20 = ((t18) == (t19));
  t21 = t20;
  if (!t21) {
  t22 = ((int64_t)(v4));
  t23 = ((int64_t)(((int64_t)UINT64_C(16))));
  t21 = ((t22) == (t23));
  }
  t24 = t21;
  t25 = t24;
  if (!t25) {
  t26 = ((int64_t)(v4));
  t27 = ((int64_t)(((int64_t)UINT64_C(9))));
  t25 = ((t26) == (t27));
  }
  t28 = t25;
  t29 = t28;
  if (!t29) {
  t30 = ((int64_t)(v4));
  t31 = ((int64_t)(((int64_t)UINT64_C(10))));
  t29 = ((t30) == (t31));
  }
  t32 = t29;
  t33 = t32;
  if (!t33) {
  t34 = ((int64_t)(v4));
  t35 = ((int64_t)(((int64_t)UINT64_C(17))));
  t33 = ((t34) == (t35));
  }
  t36 = t33;
  if (t36) {
  {
  t0 = ((int64_t)(v2));
  goto gg_done;
  }
  }
  t37 = ((int64_t)(v4));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = ((t37) >= (t38));
  t40 = t39;
  if (t40) {
  t41 = ((int64_t)(v4));
  t42 = ((int64_t)(v0));
  t43 = ((int64_t)(v1));
  t44 = ((int64_t)(((int64_t)UINT64_C(60))));
  t45 = fg143_ctl_meta(t42, t43, t44);
  t46 = ((int64_t)(t45));
  t40 = ((t41) < (t46));
  }
  if (!(t40)) gg_fail("assertion failed");
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t47 = ((int64_t)(v3));
  t48 = ((int64_t)(((int64_t)UINT64_C(100))));
  t49 = ((t47) == (t48));
  if (t49) {
  {
  t50 = ((int64_t)(v4));
  t51 = ((int64_t)(((int64_t)UINT64_C(0))));
  t52 = ((t50) >= (t51));
  t53 = t52;
  if (t53) {
  t54 = ((int64_t)(v4));
  t55 = ((int64_t)(v0));
  t56 = ((int64_t)(v1));
  t57 = ((int64_t)(((int64_t)UINT64_C(59))));
  t58 = fg143_ctl_meta(t55, t56, t57);
  t59 = ((int64_t)(t58));
  t53 = ((t54) < (t59));
  }
  if (!(t53)) gg_fail("assertion failed");
  t0 = ((int64_t)(v2));
  goto gg_done;
  }
  }
  t60 = ((int64_t)(v3));
  t61 = ((int64_t)(((int64_t)UINT64_C(46))));
  t62 = ((t60) == (t61));
  if (t62) {
  {
  t63 = ((int64_t)(v2));
  t64 = ((int64_t)(v0));
  t65 = ((int64_t)(v1));
  t66 = ((int64_t)(((int64_t)UINT64_C(0))));
  t67 = ((int64_t)(((int64_t)UINT64_C(1))));
  t68 = gg_function_6(t64, t65, t66, t67, base + 0);
  t69 = ((int64_t)(v0));
  t70 = ((int64_t)(v1));
  t71 = ((int64_t)(((int64_t)UINT64_C(0))));
  t72 = ((int64_t)(((int64_t)UINT64_C(2))));
  t73 = gg_function_6(t69, t70, t71, t72, base + 0);
  t74 = gg_function_7(t68, t73, base + 0);
  t75 = ((int64_t)(v0));
  t76 = ((int64_t)(v1));
  t77 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = ((int64_t)(((int64_t)UINT64_C(3))));
  t79 = gg_function_6(t75, t76, t77, t78, base + 0);
  t80 = gg_function_7(t74, t79, base + 0);
  t0 = gg_function_8(t63, t80, base + 0);
  goto gg_done;
  }
  }
  t81 = ((int64_t)(v3));
  t82 = ((int64_t)(((int64_t)UINT64_C(82))));
  t83 = ((t81) == (t82));
  t84 = t83;
  if (!t84) {
  t85 = ((int64_t)(v3));
  t86 = ((int64_t)(((int64_t)UINT64_C(84))));
  t84 = ((t85) == (t86));
  }
  t87 = t84;
  t88 = t87;
  if (!t88) {
  t89 = ((int64_t)(v3));
  t90 = ((int64_t)(((int64_t)UINT64_C(83))));
  t88 = ((t89) == (t90));
  }
  t91 = t88;
  if (t91) {
  {
  t92 = ((int64_t)(v2));
  t93 = ((int64_t)(v0));
  t94 = ((int64_t)(v1));
  t95 = ((int64_t)(((int64_t)UINT64_C(1))));
  t96 = ((int64_t)(((int64_t)UINT64_C(1))));
  t97 = gg_function_6(t93, t94, t95, t96, base + 0);
  t0 = gg_function_8(t92, t97, base + 0);
  goto gg_done;
  }
  }
  t98 = ((int64_t)(v3));
  t99 = ((int64_t)(((int64_t)UINT64_C(92))));
  t100 = ((t98) == (t99));
  t101 = t100;
  if (!t101) {
  t102 = ((int64_t)(v3));
  t103 = ((int64_t)(((int64_t)UINT64_C(93))));
  t101 = ((t102) == (t103));
  }
  t104 = t101;
  t105 = t104;
  if (!t105) {
  t106 = ((int64_t)(v3));
  t107 = ((int64_t)(((int64_t)UINT64_C(94))));
  t105 = ((t106) == (t107));
  }
  t108 = t105;
  t109 = t108;
  if (!t109) {
  t110 = ((int64_t)(v3));
  t111 = ((int64_t)(((int64_t)UINT64_C(95))));
  t109 = ((t110) == (t111));
  }
  t112 = t109;
  if (t112) {
  {
  t113 = ((int64_t)(v0));
  t114 = ((int64_t)(v1));
  t115 = ((int64_t)(((int64_t)UINT64_C(7))));
  t116 = fg143_ctl_meta(t113, t114, t115);
  v5 = ((int64_t)(t116));
  t117 = ((int64_t)(v5));
  t118 = ((int64_t)UINT64_C(1));
  t119 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t118)))));
  t120 = ((t117) == (t119));
  if (t120) {
  {
  v6 = ((int64_t)(v2));
  }
  } else {
  {
  t121 = ((int64_t)(v5));
  t122 = ((int64_t)(v2));
  v6 = gg_function_8(t121, t122, base + 0);
  }
  }
  t123 = ((int64_t)(v6));
  t124 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t123) > (t124)))) gg_fail("assertion failed");
  t0 = ((int64_t)(v6));
  goto gg_done;
  }
  }
  t125 = ((int64_t)(v3));
  t126 = ((int64_t)(((int64_t)UINT64_C(6))));
  t127 = ((t125) == (t126));
  t128 = t127;
  if (!t128) {
  t129 = ((int64_t)(v3));
  t130 = ((int64_t)(((int64_t)UINT64_C(9))));
  t128 = ((t129) == (t130));
  }
  t131 = t128;
  t132 = t131;
  if (!t132) {
  t133 = ((int64_t)(v3));
  t134 = ((int64_t)(((int64_t)UINT64_C(10))));
  t132 = ((t133) == (t134));
  }
  t135 = t132;
  t136 = t135;
  if (!t136) {
  t137 = ((int64_t)(v3));
  t138 = ((int64_t)(((int64_t)UINT64_C(11))));
  t136 = ((t137) == (t138));
  }
  t139 = t136;
  t140 = t139;
  if (!t140) {
  t141 = ((int64_t)(v3));
  t142 = ((int64_t)(((int64_t)UINT64_C(12))));
  t140 = ((t141) == (t142));
  }
  t143 = t140;
  t144 = t143;
  if (!t144) {
  t145 = ((int64_t)(v3));
  t146 = ((int64_t)(((int64_t)UINT64_C(13))));
  t144 = ((t145) == (t146));
  }
  t147 = t144;
  t148 = t147;
  if (!t148) {
  t149 = ((int64_t)(v3));
  t150 = ((int64_t)(((int64_t)UINT64_C(14))));
  t148 = ((t149) == (t150));
  }
  t151 = t148;
  t152 = t151;
  if (!t152) {
  t153 = ((int64_t)(v3));
  t154 = ((int64_t)(((int64_t)UINT64_C(15))));
  t152 = ((t153) == (t154));
  }
  t155 = t152;
  t156 = t155;
  if (!t156) {
  t157 = ((int64_t)(v3));
  t158 = ((int64_t)(((int64_t)UINT64_C(17))));
  t156 = ((t157) == (t158));
  }
  t159 = t156;
  t160 = t159;
  if (!t160) {
  t161 = ((int64_t)(v3));
  t162 = ((int64_t)(((int64_t)UINT64_C(18))));
  t160 = ((t161) == (t162));
  }
  t163 = t160;
  t164 = t163;
  if (!t164) {
  t165 = ((int64_t)(v3));
  t166 = ((int64_t)(((int64_t)UINT64_C(20))));
  t164 = ((t165) == (t166));
  }
  t167 = t164;
  t168 = t167;
  if (!t168) {
  t169 = ((int64_t)(v3));
  t170 = ((int64_t)(((int64_t)UINT64_C(21))));
  t168 = ((t169) == (t170));
  }
  t171 = t168;
  t172 = t171;
  if (!t172) {
  t173 = ((int64_t)(v3));
  t174 = ((int64_t)(((int64_t)UINT64_C(71))));
  t172 = ((t173) == (t174));
  }
  t175 = t172;
  t176 = t175;
  if (!t176) {
  t177 = ((int64_t)(v3));
  t178 = ((int64_t)(((int64_t)UINT64_C(40))));
  t176 = ((t177) == (t178));
  }
  t179 = t176;
  t180 = t179;
  if (!t180) {
  t181 = ((int64_t)(v3));
  t182 = ((int64_t)(((int64_t)UINT64_C(32))));
  t180 = ((t181) == (t182));
  }
  t183 = t180;
  t184 = t183;
  if (!t184) {
  t185 = ((int64_t)(v3));
  t186 = ((int64_t)(((int64_t)UINT64_C(33))));
  t184 = ((t185) == (t186));
  }
  t187 = t184;
  t188 = t187;
  if (!t188) {
  t189 = ((int64_t)(v3));
  t190 = ((int64_t)(((int64_t)UINT64_C(36))));
  t188 = ((t189) == (t190));
  }
  t191 = t188;
  t192 = t191;
  if (!t192) {
  t193 = ((int64_t)(v3));
  t194 = ((int64_t)(((int64_t)UINT64_C(37))));
  t192 = ((t193) == (t194));
  }
  t195 = t192;
  t196 = t195;
  if (!t196) {
  t197 = ((int64_t)(v3));
  t198 = ((int64_t)(((int64_t)UINT64_C(38))));
  t196 = ((t197) == (t198));
  }
  t199 = t196;
  t200 = t199;
  if (!t200) {
  t201 = ((int64_t)(v3));
  t202 = ((int64_t)(((int64_t)UINT64_C(39))));
  t200 = ((t201) == (t202));
  }
  t203 = t200;
  t204 = t203;
  if (!t204) {
  t205 = ((int64_t)(v3));
  t206 = ((int64_t)(((int64_t)UINT64_C(41))));
  t204 = ((t205) == (t206));
  }
  t207 = t204;
  t208 = t207;
  if (!t208) {
  t209 = ((int64_t)(v3));
  t210 = ((int64_t)(((int64_t)UINT64_C(43))));
  t208 = ((t209) == (t210));
  }
  t211 = t208;
  t212 = t211;
  if (!t212) {
  t213 = ((int64_t)(v3));
  t214 = ((int64_t)(((int64_t)UINT64_C(50))));
  t212 = ((t213) == (t214));
  }
  t215 = t212;
  t216 = t215;
  if (!t216) {
  t217 = ((int64_t)(v3));
  t218 = ((int64_t)(((int64_t)UINT64_C(60))));
  t216 = ((t217) == (t218));
  }
  t219 = t216;
  t220 = t219;
  if (!t220) {
  t221 = ((int64_t)(v3));
  t222 = ((int64_t)(((int64_t)UINT64_C(61))));
  t220 = ((t221) == (t222));
  }
  t223 = t220;
  t224 = t223;
  if (!t224) {
  t225 = ((int64_t)(v3));
  t226 = ((int64_t)(((int64_t)UINT64_C(62))));
  t224 = ((t225) == (t226));
  }
  t227 = t224;
  t228 = t227;
  if (!t228) {
  t229 = ((int64_t)(v3));
  t230 = ((int64_t)(((int64_t)UINT64_C(78))));
  t228 = ((t229) == (t230));
  }
  t231 = t228;
  t232 = t231;
  if (!t232) {
  t233 = ((int64_t)(v3));
  t234 = ((int64_t)(((int64_t)UINT64_C(79))));
  t232 = ((t233) == (t234));
  }
  t235 = t232;
  t236 = t235;
  if (!t236) {
  t237 = ((int64_t)(v3));
  t238 = ((int64_t)(((int64_t)UINT64_C(80))));
  t236 = ((t237) == (t238));
  }
  t239 = t236;
  t240 = t239;
  if (!t240) {
  t241 = ((int64_t)(v3));
  t242 = ((int64_t)(((int64_t)UINT64_C(0))));
  t240 = ((t241) == (t242));
  }
  t243 = t240;
  if (t243) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t244 = ((int64_t)(v3));
  t245 = ((int64_t)(((int64_t)UINT64_C(34))));
  t246 = ((t244) == (t245));
  t247 = t246;
  if (!t247) {
  t248 = ((int64_t)(v3));
  t249 = ((int64_t)(((int64_t)UINT64_C(1))));
  t247 = ((t248) == (t249));
  }
  t250 = t247;
  t251 = t250;
  if (!t251) {
  t252 = ((int64_t)(v3));
  t253 = ((int64_t)(((int64_t)UINT64_C(35))));
  t251 = ((t252) == (t253));
  }
  t254 = t251;
  t255 = t254;
  if (!t255) {
  t256 = ((int64_t)(v3));
  t257 = ((int64_t)(((int64_t)UINT64_C(2))));
  t255 = ((t256) == (t257));
  }
  t258 = t255;
  t259 = t258;
  if (!t259) {
  t260 = ((int64_t)(v3));
  t261 = ((int64_t)(((int64_t)UINT64_C(3))));
  t259 = ((t260) == (t261));
  }
  t262 = t259;
  t263 = t262;
  if (!t263) {
  t264 = ((int64_t)(v3));
  t265 = ((int64_t)(((int64_t)UINT64_C(4))));
  t263 = ((t264) == (t265));
  }
  t266 = t263;
  t267 = t266;
  if (!t267) {
  t268 = ((int64_t)(v3));
  t269 = ((int64_t)(((int64_t)UINT64_C(5))));
  t267 = ((t268) == (t269));
  }
  t270 = t267;
  t271 = t270;
  if (!t271) {
  t272 = ((int64_t)(v3));
  t273 = ((int64_t)(((int64_t)UINT64_C(16))));
  t271 = ((t272) == (t273));
  }
  t274 = t271;
  t275 = t274;
  if (!t275) {
  t276 = ((int64_t)(v3));
  t277 = ((int64_t)(((int64_t)UINT64_C(72))));
  t275 = ((t276) == (t277));
  }
  t278 = t275;
  t279 = t278;
  if (!t279) {
  t280 = ((int64_t)(v3));
  t281 = ((int64_t)(((int64_t)UINT64_C(73))));
  t279 = ((t280) == (t281));
  }
  t282 = t279;
  t283 = t282;
  if (!t283) {
  t284 = ((int64_t)(v3));
  t285 = ((int64_t)(((int64_t)UINT64_C(19))));
  t283 = ((t284) == (t285));
  }
  t286 = t283;
  t287 = t286;
  if (!t287) {
  t288 = ((int64_t)(v3));
  t289 = ((int64_t)(((int64_t)UINT64_C(85))));
  t287 = ((t288) == (t289));
  }
  t290 = t287;
  t291 = t290;
  if (!t291) {
  t292 = ((int64_t)(v3));
  t293 = ((int64_t)(((int64_t)UINT64_C(86))));
  t291 = ((t292) == (t293));
  }
  t294 = t291;
  t295 = t294;
  if (!t295) {
  t296 = ((int64_t)(v3));
  t297 = ((int64_t)(((int64_t)UINT64_C(88))));
  t295 = ((t296) == (t297));
  }
  t298 = t295;
  t299 = t298;
  if (!t299) {
  t300 = ((int64_t)(v3));
  t301 = ((int64_t)(((int64_t)UINT64_C(89))));
  t299 = ((t300) == (t301));
  }
  t302 = t299;
  t303 = t302;
  if (!t303) {
  t304 = ((int64_t)(v3));
  t305 = ((int64_t)(((int64_t)UINT64_C(90))));
  t303 = ((t304) == (t305));
  }
  t306 = t303;
  t307 = t306;
  if (!t307) {
  t308 = ((int64_t)(v3));
  t309 = ((int64_t)(((int64_t)UINT64_C(23))));
  t307 = ((t308) == (t309));
  }
  t310 = t307;
  t311 = t310;
  if (!t311) {
  t312 = ((int64_t)(v3));
  t313 = ((int64_t)(((int64_t)UINT64_C(7))));
  t311 = ((t312) == (t313));
  }
  t314 = t311;
  t315 = t314;
  if (!t315) {
  t316 = ((int64_t)(v3));
  t317 = ((int64_t)(((int64_t)UINT64_C(8))));
  t315 = ((t316) == (t317));
  }
  t318 = t315;
  t319 = t318;
  if (!t319) {
  t320 = ((int64_t)(v3));
  t321 = ((int64_t)(((int64_t)UINT64_C(24))));
  t319 = ((t320) == (t321));
  }
  t322 = t319;
  t323 = t322;
  if (!t323) {
  t324 = ((int64_t)(v3));
  t325 = ((int64_t)(((int64_t)UINT64_C(25))));
  t323 = ((t324) == (t325));
  }
  t326 = t323;
  t327 = t326;
  if (!t327) {
  t328 = ((int64_t)(v3));
  t329 = ((int64_t)(((int64_t)UINT64_C(26))));
  t327 = ((t328) == (t329));
  }
  t330 = t327;
  t331 = t330;
  if (!t331) {
  t332 = ((int64_t)(v3));
  t333 = ((int64_t)(((int64_t)UINT64_C(28))));
  t331 = ((t332) == (t333));
  }
  t334 = t331;
  t335 = t334;
  if (!t335) {
  t336 = ((int64_t)(v3));
  t337 = ((int64_t)(((int64_t)UINT64_C(27))));
  t335 = ((t336) == (t337));
  }
  t338 = t335;
  t339 = t338;
  if (!t339) {
  t340 = ((int64_t)(v3));
  t341 = ((int64_t)(((int64_t)UINT64_C(22))));
  t339 = ((t340) == (t341));
  }
  t342 = t339;
  t343 = t342;
  if (!t343) {
  t344 = ((int64_t)(v3));
  t345 = ((int64_t)(((int64_t)UINT64_C(29))));
  t343 = ((t344) == (t345));
  }
  t346 = t343;
  t347 = t346;
  if (!t347) {
  t348 = ((int64_t)(v3));
  t349 = ((int64_t)(((int64_t)UINT64_C(30))));
  t347 = ((t348) == (t349));
  }
  t350 = t347;
  t351 = t350;
  if (!t351) {
  t352 = ((int64_t)(v3));
  t353 = ((int64_t)(((int64_t)UINT64_C(31))));
  t351 = ((t352) == (t353));
  }
  t354 = t351;
  t355 = t354;
  if (!t355) {
  t356 = ((int64_t)(v3));
  t357 = ((int64_t)(((int64_t)UINT64_C(42))));
  t355 = ((t356) == (t357));
  }
  t358 = t355;
  t359 = t358;
  if (!t359) {
  t360 = ((int64_t)(v3));
  t361 = ((int64_t)(((int64_t)UINT64_C(45))));
  t359 = ((t360) == (t361));
  }
  t362 = t359;
  t363 = t362;
  if (!t363) {
  t364 = ((int64_t)(v3));
  t365 = ((int64_t)(((int64_t)UINT64_C(44))));
  t363 = ((t364) == (t365));
  }
  t366 = t363;
  t367 = t366;
  if (!t367) {
  t368 = ((int64_t)(v3));
  t369 = ((int64_t)(((int64_t)UINT64_C(47))));
  t367 = ((t368) == (t369));
  }
  t370 = t367;
  t371 = t370;
  if (!t371) {
  t372 = ((int64_t)(v3));
  t373 = ((int64_t)(((int64_t)UINT64_C(48))));
  t371 = ((t372) == (t373));
  }
  t374 = t371;
  t375 = t374;
  if (!t375) {
  t376 = ((int64_t)(v3));
  t377 = ((int64_t)(((int64_t)UINT64_C(49))));
  t375 = ((t376) == (t377));
  }
  t378 = t375;
  t379 = t378;
  if (!t379) {
  t380 = ((int64_t)(v3));
  t381 = ((int64_t)(((int64_t)UINT64_C(81))));
  t379 = ((t380) == (t381));
  }
  t382 = t379;
  t383 = t382;
  if (!t383) {
  t384 = ((int64_t)(v3));
  t385 = ((int64_t)(((int64_t)UINT64_C(52))));
  t383 = ((t384) == (t385));
  }
  t386 = t383;
  t387 = t386;
  if (!t387) {
  t388 = ((int64_t)(v3));
  t389 = ((int64_t)(((int64_t)UINT64_C(53))));
  t387 = ((t388) == (t389));
  }
  t390 = t387;
  t391 = t390;
  if (!t391) {
  t392 = ((int64_t)(v3));
  t393 = ((int64_t)(((int64_t)UINT64_C(54))));
  t391 = ((t392) == (t393));
  }
  t394 = t391;
  t395 = t394;
  if (!t395) {
  t396 = ((int64_t)(v3));
  t397 = ((int64_t)(((int64_t)UINT64_C(56))));
  t395 = ((t396) == (t397));
  }
  t398 = t395;
  t399 = t398;
  if (!t399) {
  t400 = ((int64_t)(v3));
  t401 = ((int64_t)(((int64_t)UINT64_C(57))));
  t399 = ((t400) == (t401));
  }
  t402 = t399;
  t403 = t402;
  if (!t403) {
  t404 = ((int64_t)(v3));
  t405 = ((int64_t)(((int64_t)UINT64_C(58))));
  t403 = ((t404) == (t405));
  }
  t406 = t403;
  t407 = t406;
  if (!t407) {
  t408 = ((int64_t)(v3));
  t409 = ((int64_t)(((int64_t)UINT64_C(55))));
  t407 = ((t408) == (t409));
  }
  t410 = t407;
  t411 = t410;
  if (!t411) {
  t412 = ((int64_t)(v3));
  t413 = ((int64_t)(((int64_t)UINT64_C(51))));
  t411 = ((t412) == (t413));
  }
  t414 = t411;
  t415 = t414;
  if (!t415) {
  t416 = ((int64_t)(v3));
  t417 = ((int64_t)(((int64_t)UINT64_C(59))));
  t415 = ((t416) == (t417));
  }
  t418 = t415;
  t419 = t418;
  if (!t419) {
  t420 = ((int64_t)(v3));
  t421 = ((int64_t)(((int64_t)UINT64_C(63))));
  t419 = ((t420) == (t421));
  }
  t422 = t419;
  t423 = t422;
  if (!t423) {
  t424 = ((int64_t)(v3));
  t425 = ((int64_t)(((int64_t)UINT64_C(64))));
  t423 = ((t424) == (t425));
  }
  t426 = t423;
  t427 = t426;
  if (!t427) {
  t428 = ((int64_t)(v3));
  t429 = ((int64_t)(((int64_t)UINT64_C(65))));
  t427 = ((t428) == (t429));
  }
  t430 = t427;
  t431 = t430;
  if (!t431) {
  t432 = ((int64_t)(v3));
  t433 = ((int64_t)(((int64_t)UINT64_C(66))));
  t431 = ((t432) == (t433));
  }
  t434 = t431;
  t435 = t434;
  if (!t435) {
  t436 = ((int64_t)(v3));
  t437 = ((int64_t)(((int64_t)UINT64_C(67))));
  t435 = ((t436) == (t437));
  }
  t438 = t435;
  t439 = t438;
  if (!t439) {
  t440 = ((int64_t)(v3));
  t441 = ((int64_t)(((int64_t)UINT64_C(68))));
  t439 = ((t440) == (t441));
  }
  t442 = t439;
  t443 = t442;
  if (!t443) {
  t444 = ((int64_t)(v3));
  t445 = ((int64_t)(((int64_t)UINT64_C(69))));
  t443 = ((t444) == (t445));
  }
  t446 = t443;
  t447 = t446;
  if (!t447) {
  t448 = ((int64_t)(v3));
  t449 = ((int64_t)(((int64_t)UINT64_C(70))));
  t447 = ((t448) == (t449));
  }
  t450 = t447;
  t451 = t450;
  if (!t451) {
  t452 = ((int64_t)(v3));
  t453 = ((int64_t)(((int64_t)UINT64_C(74))));
  t451 = ((t452) == (t453));
  }
  t454 = t451;
  t455 = t454;
  if (!t455) {
  t456 = ((int64_t)(v3));
  t457 = ((int64_t)(((int64_t)UINT64_C(75))));
  t455 = ((t456) == (t457));
  }
  t458 = t455;
  t459 = t458;
  if (!t459) {
  t460 = ((int64_t)(v3));
  t461 = ((int64_t)(((int64_t)UINT64_C(76))));
  t459 = ((t460) == (t461));
  }
  t462 = t459;
  t463 = t462;
  if (!t463) {
  t464 = ((int64_t)(v3));
  t465 = ((int64_t)(((int64_t)UINT64_C(77))));
  t463 = ((t464) == (t465));
  }
  t466 = t463;
  t467 = t466;
  if (!t467) {
  t468 = ((int64_t)(v3));
  t469 = ((int64_t)(((int64_t)UINT64_C(87))));
  t467 = ((t468) == (t469));
  }
  t470 = t467;
  t471 = t470;
  if (!t471) {
  t472 = ((int64_t)(v3));
  t473 = ((int64_t)(((int64_t)UINT64_C(96))));
  t471 = ((t472) == (t473));
  }
  t474 = t471;
  t475 = t474;
  if (!t475) {
  t476 = ((int64_t)(v3));
  t477 = ((int64_t)(((int64_t)UINT64_C(97))));
  t475 = ((t476) == (t477));
  }
  t478 = t475;
  t479 = t478;
  if (!t479) {
  t480 = ((int64_t)(v3));
  t481 = ((int64_t)(((int64_t)UINT64_C(98))));
  t479 = ((t480) == (t481));
  }
  t482 = t479;
  t483 = t482;
  if (!t483) {
  t484 = ((int64_t)(v3));
  t485 = ((int64_t)(((int64_t)UINT64_C(99))));
  t483 = ((t484) == (t485));
  }
  t486 = t483;
  if (t486) {
  {
  t0 = ((int64_t)(v2));
  goto gg_done;
  }
  }
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_9(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((t1) > (t2));
  if (t3) {
  {
  t0 = ((int64_t)(v0));
  }
  } else {
  {
  t0 = ((int64_t)(v1));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_17(int64_t p0, int64_t p1, int64_t base) {
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
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)UINT64_C(8))));
  t4 = fg143_ctl_meta(t1, t2, t3);
  t0 = ((int64_t)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_14(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t1) > (t2)))) gg_fail("assertion failed");
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) - (uint64_t)(t5)))));
  t7 = gg_function_15(t3, t6, base + 0);
  t8 = ((int64_t)(t7));
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)gg_div(t8, t9))));
  t11 = ((int64_t)(v1));
  t0 = gg_function_7(t10, t11, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_12(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = ((int64_t)UINT64_C(4));
  t1 = 0;
label0:;
  if (t1 >= t2) goto label2;
  v4 = t1;
  {
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v2));
  t7 = ((int64_t)(v4));
  t8 = gg_function_6(t4, t5, t6, t7, base + 0);
  v3 = gg_function_7(t3, t8, base + 0);
  }
label1:;
  t1++; goto label0;
label2:;
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_13(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
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
  t3 = ((int64_t)(((int64_t)UINT64_C(42))));
  t4 = fg143_ctl_meta(t1, t2, t3);
  v3 = ((int64_t)(t4));
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) > (t6));
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t9) >= (t10));
  }
  t11 = t8;
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(v3));
  t15 = ((int64_t)(((int64_t)gg_mod(t13, t14))));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t15) == (t16));
  }
  if (!(t12)) gg_fail("assertion failed");
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(v3));
  t19 = ((int64_t)(((int64_t)gg_div(t17, t18))));
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(((int64_t)UINT64_C(41))));
  t23 = fg143_ctl_meta(t20, t21, t22);
  t24 = ((int64_t)(t23));
  t0 = gg_function_7(t19, t24, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_10(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  uint8_t t34;
  uint8_t t35;
  int64_t t36;
  int64_t t37;
  uint8_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  uint8_t t45;
  uint8_t t46;
  int64_t t47;
  int64_t t48;
  uint8_t t49;
  uint8_t t50;
  int64_t t51;
  int64_t t52;
  uint8_t t53;
  uint8_t t54;
  int64_t t55;
  int64_t t56;
  uint8_t t57;
  uint8_t t58;
  int64_t t59;
  int64_t t60;
  uint8_t t61;
  uint8_t t62;
  int64_t t63;
  int64_t t64;
  uint8_t t65;
  uint8_t t66;
  int64_t t67;
  int64_t t68;
  uint8_t t69;
  uint8_t t70;
  int64_t t71;
  int64_t t72;
  uint8_t t73;
  uint8_t t74;
  int64_t t75;
  int64_t t76;
  uint8_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  uint8_t t84;
  uint8_t t85;
  int64_t t86;
  int64_t t87;
  uint8_t t88;
  uint8_t t89;
  int64_t t90;
  int64_t t91;
  uint8_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  uint8_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  uint8_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  uint8_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  uint8_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  uint8_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  uint8_t t140;
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
  uint8_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  uint8_t t163;
  int64_t t164;
  int64_t t165;
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
  int64_t t176;
  int64_t t177;
  int64_t t178;
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
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  int64_t t217;
  int64_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  uint8_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  int64_t t227;
  int64_t t228;
  int64_t t229;
  uint8_t t230;
  uint8_t t231;
  int64_t t232;
  int64_t t233;
  uint8_t t234;
  int64_t t235;
  int64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  uint8_t t241;
  int64_t t242;
  int64_t t243;
  uint8_t t244;
  uint8_t t245;
  int64_t t246;
  int64_t t247;
  uint8_t t248;
  int64_t t249;
  int64_t t250;
  int64_t t251;
  int64_t t252;
  int64_t t253;
  int64_t t254;
  uint8_t t255;
  uint8_t t256;
  int64_t t257;
  int64_t t258;
  uint8_t t259;
  uint8_t t260;
  int64_t t261;
  int64_t t262;
  uint8_t t263;
  int64_t t264;
  int64_t t265;
  int64_t t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  uint8_t t270;
  int64_t t271;
  int64_t t272;
  int64_t t273;
  int64_t t274;
  int64_t t275;
  int64_t t276;
  int64_t t277;
  uint8_t t278;
  uint8_t t279;
  int64_t t280;
  int64_t t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  int64_t t285;
  int64_t t286;
  uint8_t t287;
  uint8_t t288;
  int64_t t289;
  int64_t t290;
  int64_t t291;
  int64_t t292;
  int64_t t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  int64_t t297;
  uint8_t t298;
  uint8_t t299;
  int64_t t300;
  int64_t t301;
  uint8_t t302;
  uint8_t t303;
  int64_t t304;
  int64_t t305;
  uint8_t t306;
  uint8_t t307;
  int64_t t308;
  int64_t t309;
  int64_t t310;
  int64_t t311;
  uint8_t t312;
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
  uint8_t t341;
  uint8_t t342;
  int64_t t343;
  int64_t t344;
  uint8_t t345;
  int64_t t346;
  int64_t t347;
  int64_t t348;
  int64_t t349;
  int64_t t350;
  int64_t t351;
  uint8_t t352;
  int64_t t353;
  int64_t t354;
  uint8_t t355;
  uint8_t t356;
  int64_t t357;
  int64_t t358;
  uint8_t t359;
  uint8_t t360;
  int64_t t361;
  int64_t t362;
  int64_t t363;
  int64_t t364;
  int64_t t365;
  int64_t t366;
  int64_t t367;
  int64_t t368;
  int64_t t369;
  int64_t t370;
  int64_t t371;
  int64_t t372;
  int64_t t373;
  int64_t t374;
  int64_t t375;
  int64_t t376;
  int64_t t377;
  int64_t t378;
  uint8_t t379;
  int64_t t380;
  int64_t t381;
  int64_t t382;
  int64_t t383;
  int64_t t384;
  int64_t t385;
  uint8_t t386;
  int64_t t387;
  int64_t t388;
  int64_t t389;
  int64_t t390;
  int64_t t391;
  int64_t t392;
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
  int64_t t415;
  int64_t t416;
  int64_t t417;
  int64_t t418;
  int64_t t419;
  int64_t t420;
  int64_t t421;
  int64_t t422;
  int64_t t423;
  int64_t t424;
  int64_t t425;
  int64_t t426;
  int64_t t427;
  int64_t t428;
  int64_t t429;
  int64_t t430;
  int64_t t431;
  int64_t t432;
  int64_t t433;
  int64_t t434;
  int64_t t435;
  int64_t t436;
  int64_t t437;
  int64_t t438;
  int64_t t439;
  int64_t t440;
  int64_t t441;
  int64_t t442;
  int64_t t443;
  int64_t t444;
  int64_t t445;
  int64_t t446;
  int64_t t447;
  int64_t t448;
  int64_t t449;
  int64_t t450;
  uint8_t t451;
  int64_t t452;
  int64_t t453;
  uint8_t t454;
  uint8_t t455;
  int64_t t456;
  int64_t t457;
  uint8_t t458;
  uint8_t t459;
  int64_t t460;
  int64_t t461;
  uint8_t t462;
  int64_t t463;
  int64_t t464;
  int64_t t465;
  int64_t t466;
  int64_t t467;
  int64_t t468;
  int64_t t469;
  int64_t t470;
  int64_t t471;
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
  uint8_t t483;
  int64_t t484;
  int64_t t485;
  int64_t t486;
  int64_t t487;
  int64_t t488;
  int64_t t489;
  int64_t t490;
  int64_t t491;
  int64_t t492;
  int64_t t493;
  int64_t t494;
  int64_t t495;
  uint8_t t496;
  int64_t t497;
  int64_t t498;
  int64_t t499;
  int64_t t500;
  int64_t t501;
  int64_t t502;
  int64_t t503;
  int64_t t504;
  int64_t t505;
  int64_t t506;
  int64_t t507;
  uint8_t t508;
  int64_t t509;
  int64_t t510;
  int64_t t511;
  int64_t t512;
  int64_t t513;
  int64_t t514;
  int64_t t515;
  int64_t t516;
  int64_t t517;
  uint8_t t518;
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
  int64_t t530;
  int64_t t531;
  int64_t t532;
  int64_t t533;
  int64_t t534;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(v2));
  t4 = fg143_ctl_extra_work(t1, t2, t3);
  v4 = ((int64_t)(t4));
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(v4));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) >= (t6));
  if (t7) {
  {
  v5 = ((int64_t)(v4));
  }
  } else {
  {
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = fg143_ctl_meta(t8, t9, t10);
  v6 = ((int64_t)(t11));
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = fg143_ctl_meta(t12, t13, t14);
  v7 = ((int64_t)(t15));
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(((int64_t)UINT64_C(2))));
  t19 = fg143_ctl_meta(t16, t17, t18);
  v8 = ((int64_t)(t19));
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(((int64_t)UINT64_C(3))));
  t23 = fg143_ctl_meta(t20, t21, t22);
  v9 = ((int64_t)(t23));
  t24 = ((int64_t)(v0));
  t25 = ((int64_t)(v1));
  t26 = ((int64_t)(((int64_t)UINT64_C(10))));
  t27 = fg143_ctl_meta(t24, t25, t26);
  v10 = ((int64_t)(t27));
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(v1));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  v11 = gg_function_6(t28, t29, t30, t31, base + 0);
  t32 = ((int64_t)(v6));
  t33 = ((int64_t)(((int64_t)UINT64_C(34))));
  t34 = ((t32) == (t33));
  t35 = t34;
  if (!t35) {
  t36 = ((int64_t)(v6));
  t37 = ((int64_t)(((int64_t)UINT64_C(1))));
  t35 = ((t36) == (t37));
  }
  t38 = t35;
  if (t38) {
  {
  t39 = ((int64_t)(v0));
  t40 = ((int64_t)(v1));
  t41 = ((int64_t)(((int64_t)UINT64_C(5))));
  t42 = fg143_ctl_meta(t39, t40, t41);
  t43 = ((int64_t)(t42));
  t44 = ((int64_t)(((int64_t)UINT64_C(0))));
  t45 = ((t43) != (t44));
  t46 = t45;
  if (!t46) {
  t47 = ((int64_t)(v8));
  t48 = ((int64_t)(((int64_t)UINT64_C(1))));
  t49 = ((t47) == (t48));
  t50 = t49;
  if (t50) {
  t51 = ((int64_t)(v9));
  t52 = ((int64_t)(((int64_t)UINT64_C(30))));
  t50 = ((t51) == (t52));
  }
  t46 = t50;
  }
  t53 = t46;
  t54 = t53;
  if (!t54) {
  t55 = ((int64_t)(v8));
  t56 = ((int64_t)(((int64_t)UINT64_C(30))));
  t57 = ((t55) == (t56));
  t58 = t57;
  if (t58) {
  t59 = ((int64_t)(v9));
  t60 = ((int64_t)(((int64_t)UINT64_C(1))));
  t58 = ((t59) == (t60));
  }
  t54 = t58;
  }
  t61 = t54;
  t62 = t61;
  if (!t62) {
  t63 = ((int64_t)(v8));
  t64 = ((int64_t)(((int64_t)UINT64_C(0))));
  t65 = ((t63) == (t64));
  t66 = t65;
  if (t66) {
  t67 = ((int64_t)(v9));
  t68 = ((int64_t)(((int64_t)UINT64_C(26))));
  t66 = ((t67) == (t68));
  }
  t62 = t66;
  }
  t69 = t62;
  t70 = t69;
  if (!t70) {
  t71 = ((int64_t)(v8));
  t72 = ((int64_t)(((int64_t)UINT64_C(26))));
  t73 = ((t71) == (t72));
  t74 = t73;
  if (t74) {
  t75 = ((int64_t)(v9));
  t76 = ((int64_t)(((int64_t)UINT64_C(0))));
  t74 = ((t75) == (t76));
  }
  t70 = t74;
  }
  t77 = t70;
  if (t77) {
  {
  t78 = ((int64_t)(((int64_t)UINT64_C(4))));
  t79 = ((int64_t)(v10));
  t80 = gg_function_7(t78, t79, base + 0);
  t81 = ((int64_t)(v3));
  v5 = gg_function_7(t80, t81, base + 0);
  }
  }
  }
  } else {
  t82 = ((int64_t)(v6));
  t83 = ((int64_t)(((int64_t)UINT64_C(2))));
  t84 = ((t82) == (t83));
  t85 = t84;
  if (!t85) {
  t86 = ((int64_t)(v6));
  t87 = ((int64_t)(((int64_t)UINT64_C(3))));
  t85 = ((t86) == (t87));
  }
  t88 = t85;
  t89 = t88;
  if (!t89) {
  t90 = ((int64_t)(v6));
  t91 = ((int64_t)(((int64_t)UINT64_C(4))));
  t89 = ((t90) == (t91));
  }
  t92 = t89;
  if (t92) {
  {
  t93 = ((int64_t)(v0));
  t94 = ((int64_t)(v1));
  t95 = ((int64_t)(((int64_t)UINT64_C(6))));
  t96 = fg143_ctl_meta(t93, t94, t95);
  t97 = ((int64_t)(t96));
  t98 = ((int64_t)(((int64_t)UINT64_C(0))));
  t99 = ((t97) != (t98));
  if (t99) {
  {
  t100 = ((int64_t)(((int64_t)UINT64_C(4))));
  t101 = ((int64_t)(v11));
  t102 = gg_function_7(t100, t101, base + 0);
  t103 = ((int64_t)(v3));
  v5 = gg_function_7(t102, t103, base + 0);
  }
  }
  }
  } else {
  t104 = ((int64_t)(v6));
  t105 = ((int64_t)(((int64_t)UINT64_C(5))));
  t106 = ((t104) == (t105));
  if (t106) {
  {
  t107 = ((int64_t)(v0));
  t108 = ((int64_t)(v1));
  t109 = ((int64_t)(((int64_t)UINT64_C(6))));
  t110 = fg143_ctl_meta(t107, t108, t109);
  t111 = ((int64_t)(t110));
  t112 = ((int64_t)(((int64_t)UINT64_C(0))));
  t113 = ((t111) != (t112));
  if (t113) {
  {
  t114 = ((int64_t)(((int64_t)UINT64_C(4))));
  t115 = ((int64_t)(v0));
  t116 = ((int64_t)(v1));
  t117 = ((int64_t)(((int64_t)UINT64_C(1))));
  t118 = ((int64_t)(((int64_t)UINT64_C(0))));
  t119 = gg_function_6(t115, t116, t117, t118, base + 0);
  t120 = gg_function_7(t114, t119, base + 0);
  t121 = ((int64_t)(v3));
  v5 = gg_function_7(t120, t121, base + 0);
  }
  }
  }
  } else {
  t122 = ((int64_t)(v6));
  t123 = ((int64_t)(((int64_t)UINT64_C(19))));
  t124 = ((t122) == (t123));
  if (t124) {
  {
  t125 = ((int64_t)(v0));
  t126 = ((int64_t)(v1));
  t127 = ((int64_t)(((int64_t)UINT64_C(43))));
  t128 = fg143_ctl_meta(t125, t126, t127);
  t129 = ((int64_t)(t128));
  t130 = ((int64_t)(v3));
  v5 = gg_function_7(t129, t130, base + 0);
  }
  } else {
  t131 = ((int64_t)(v6));
  t132 = ((int64_t)(((int64_t)UINT64_C(29))));
  t133 = ((t131) == (t132));
  if (t133) {
  {
  t134 = ((int64_t)(v9));
  t135 = ((int64_t)(v0));
  t136 = ((int64_t)(v1));
  t137 = ((int64_t)(((int64_t)UINT64_C(40))));
  t138 = fg143_ctl_meta(t135, t136, t137);
  t139 = ((int64_t)(t138));
  t140 = ((t134) != (t139));
  if (t140) {
  {
  t141 = ((int64_t)(v0));
  t142 = ((int64_t)(v1));
  t143 = ((int64_t)(v0));
  t144 = ((int64_t)(v1));
  t145 = ((int64_t)(((int64_t)UINT64_C(1))));
  t146 = gg_function_12(t143, t144, t145, base + 0);
  v5 = gg_function_13(t141, t142, t146, base + 0);
  }
  }
  t147 = ((int64_t)(v5));
  t148 = ((int64_t)(((int64_t)UINT64_C(64))));
  t149 = gg_function_14(t147, t148, base + 0);
  t150 = ((int64_t)(v0));
  t151 = ((int64_t)(v1));
  t152 = ((int64_t)(v3));
  t153 = gg_function_16(t150, t151, t152, base + 0);
  v5 = gg_function_15(t149, t153, base + 0);
  }
  } else {
  t154 = ((int64_t)(v6));
  t155 = ((int64_t)(((int64_t)UINT64_C(30))));
  t156 = ((t154) == (t155));
  if (t156) {
  {
  t157 = ((int64_t)(v9));
  t158 = ((int64_t)(v0));
  t159 = ((int64_t)(v1));
  t160 = ((int64_t)(((int64_t)UINT64_C(40))));
  t161 = fg143_ctl_meta(t158, t159, t160);
  t162 = ((int64_t)(t161));
  t163 = ((t157) != (t162));
  if (t163) {
  {
  t164 = ((int64_t)(v0));
  t165 = ((int64_t)(v1));
  t166 = ((int64_t)(v0));
  t167 = ((int64_t)(v1));
  t168 = ((int64_t)(((int64_t)UINT64_C(1))));
  t169 = gg_function_12(t166, t167, t168, base + 0);
  t170 = gg_function_13(t164, t165, t169, base + 0);
  t171 = ((int64_t)(((int64_t)UINT64_C(8))));
  v5 = gg_function_15(t170, t171, base + 0);
  }
  }
  t172 = ((int64_t)(v0));
  t173 = ((int64_t)(v1));
  t174 = ((int64_t)(((int64_t)UINT64_C(0))));
  t175 = ((int64_t)(((int64_t)UINT64_C(2))));
  v12 = gg_function_6(t172, t173, t174, t175, base + 0);
  t176 = ((int64_t)(v5));
  t177 = ((int64_t)(v12));
  t178 = ((int64_t)(((int64_t)UINT64_C(8))));
  t179 = gg_function_7(t177, t178, base + 0);
  t180 = ((int64_t)(((int64_t)UINT64_C(8))));
  t181 = gg_function_15(t179, t180, base + 0);
  v5 = gg_function_15(t176, t181, base + 0);
  t182 = ((int64_t)(v5));
  t183 = ((int64_t)(v12));
  t184 = ((int64_t)(v0));
  t185 = ((int64_t)(v1));
  t186 = ((int64_t)(((int64_t)UINT64_C(2))));
  t187 = ((int64_t)(((int64_t)UINT64_C(0))));
  t188 = gg_function_6(t184, t185, t186, t187, base + 0);
  t189 = gg_function_7(t183, t188, base + 0);
  t190 = ((int64_t)(v0));
  t191 = ((int64_t)(v1));
  t192 = ((int64_t)(((int64_t)UINT64_C(2))));
  t193 = ((int64_t)(((int64_t)UINT64_C(1))));
  t194 = gg_function_6(t190, t191, t192, t193, base + 0);
  t195 = gg_function_7(t189, t194, base + 0);
  t196 = ((int64_t)(v0));
  t197 = ((int64_t)(v1));
  t198 = ((int64_t)(((int64_t)UINT64_C(54))));
  t199 = fg143_ctl_meta(t196, t197, t198);
  t200 = ((int64_t)(t199));
  t201 = gg_function_7(t195, t200, base + 0);
  t202 = ((int64_t)(((int64_t)UINT64_C(8))));
  t203 = gg_function_15(t201, t202, base + 0);
  v5 = gg_function_15(t182, t203, base + 0);
  t204 = ((int64_t)(v5));
  t205 = ((int64_t)(v0));
  t206 = ((int64_t)(v1));
  t207 = ((int64_t)(((int64_t)UINT64_C(53))));
  t208 = fg143_ctl_meta(t205, t206, t207);
  t209 = ((int64_t)(t208));
  t210 = ((int64_t)(v12));
  t211 = ((int64_t)(((int64_t)UINT64_C(1))));
  t212 = gg_function_15(t210, t211, base + 0);
  t213 = gg_function_7(t209, t212, base + 0);
  v5 = gg_function_15(t204, t213, base + 0);
  t214 = ((int64_t)(v5));
  t215 = ((int64_t)(((int64_t)UINT64_C(64))));
  t216 = gg_function_14(t214, t215, base + 0);
  t217 = ((int64_t)(v0));
  t218 = ((int64_t)(v1));
  t219 = ((int64_t)(v3));
  t220 = gg_function_16(t217, t218, t219, base + 0);
  v5 = gg_function_15(t216, t220, base + 0);
  }
  } else {
  t221 = ((int64_t)(v6));
  t222 = ((int64_t)(((int64_t)UINT64_C(31))));
  t223 = ((t221) == (t222));
  if (t223) {
  {
  t224 = ((int64_t)(v0));
  t225 = ((int64_t)(v1));
  t226 = ((int64_t)(((int64_t)UINT64_C(6))));
  t227 = fg143_ctl_meta(t224, t225, t226);
  t228 = ((int64_t)(t227));
  t229 = ((int64_t)(((int64_t)UINT64_C(0))));
  t230 = ((t228) != (t229));
  t231 = t230;
  if (!t231) {
  t232 = ((int64_t)(v8));
  t233 = ((int64_t)(((int64_t)UINT64_C(1))));
  t231 = ((t232) == (t233));
  }
  t234 = t231;
  if (t234) {
  {
  t235 = ((int64_t)(((int64_t)UINT64_C(4))));
  t236 = ((int64_t)(v11));
  t237 = gg_function_7(t235, t236, base + 0);
  t238 = ((int64_t)(v3));
  v5 = gg_function_7(t237, t238, base + 0);
  }
  }
  }
  } else {
  t239 = ((int64_t)(v6));
  t240 = ((int64_t)(((int64_t)UINT64_C(42))));
  t241 = ((t239) == (t240));
  if (t241) {
  {
  t242 = ((int64_t)(v8));
  t243 = ((int64_t)(((int64_t)UINT64_C(1))));
  t244 = ((t242) == (t243));
  t245 = t244;
  if (t245) {
  t246 = ((int64_t)(v7));
  t247 = ((int64_t)(((int64_t)UINT64_C(1))));
  t245 = ((t246) != (t247));
  }
  t248 = t245;
  if (t248) {
  {
  t249 = ((int64_t)(((int64_t)UINT64_C(4))));
  t250 = ((int64_t)(v11));
  t251 = gg_function_7(t249, t250, base + 0);
  t252 = ((int64_t)(v3));
  v5 = gg_function_7(t251, t252, base + 0);
  }
  }
  }
  } else {
  t253 = ((int64_t)(v6));
  t254 = ((int64_t)(((int64_t)UINT64_C(46))));
  t255 = ((t253) == (t254));
  t256 = t255;
  if (!t256) {
  t257 = ((int64_t)(v6));
  t258 = ((int64_t)(((int64_t)UINT64_C(48))));
  t256 = ((t257) == (t258));
  }
  t259 = t256;
  t260 = t259;
  if (!t260) {
  t261 = ((int64_t)(v6));
  t262 = ((int64_t)(((int64_t)UINT64_C(49))));
  t260 = ((t261) == (t262));
  }
  t263 = t260;
  if (t263) {
  {
  t264 = ((int64_t)(((int64_t)UINT64_C(4))));
  t265 = ((int64_t)(v10));
  t266 = gg_function_7(t264, t265, base + 0);
  t267 = ((int64_t)(v3));
  v5 = gg_function_7(t266, t267, base + 0);
  }
  } else {
  t268 = ((int64_t)(v6));
  t269 = ((int64_t)(((int64_t)UINT64_C(51))));
  t270 = ((t268) == (t269));
  if (t270) {
  {
  t271 = ((int64_t)(v0));
  t272 = ((int64_t)(v1));
  t273 = ((int64_t)(((int64_t)UINT64_C(0))));
  t274 = ((int64_t)(((int64_t)UINT64_C(3))));
  t275 = gg_function_6(t271, t272, t273, t274, base + 0);
  t276 = ((int64_t)(t275));
  t277 = ((int64_t)(((int64_t)UINT64_C(1))));
  t278 = ((t276) == (t277));
  t279 = t278;
  if (t279) {
  t280 = ((int64_t)(v0));
  t281 = ((int64_t)(v1));
  t282 = ((int64_t)(((int64_t)UINT64_C(1))));
  t283 = ((int64_t)(((int64_t)UINT64_C(2))));
  t284 = gg_function_6(t280, t281, t282, t283, base + 0);
  t285 = ((int64_t)(t284));
  t286 = ((int64_t)(((int64_t)UINT64_C(1))));
  t279 = ((t285) == (t286));
  }
  t287 = t279;
  t288 = t287;
  if (t288) {
  t289 = ((int64_t)(v0));
  t290 = ((int64_t)(v1));
  t291 = ((int64_t)(((int64_t)UINT64_C(1))));
  t292 = ((int64_t)(((int64_t)UINT64_C(3))));
  t293 = gg_function_6(t289, t290, t291, t292, base + 0);
  t294 = ((int64_t)(t293));
  t295 = ((int64_t)(((int64_t)UINT64_C(1))));
  t288 = ((t294) == (t295));
  }
  if (!(t288)) gg_fail("assertion failed");
  t296 = ((int64_t)(v8));
  t297 = ((int64_t)(((int64_t)UINT64_C(1))));
  t298 = ((t296) == (t297));
  t299 = t298;
  if (!t299) {
  t300 = ((int64_t)(v8));
  t301 = ((int64_t)(((int64_t)UINT64_C(30))));
  t299 = ((t300) == (t301));
  }
  t302 = t299;
  t303 = t302;
  if (!t303) {
  t304 = ((int64_t)(v8));
  t305 = ((int64_t)(((int64_t)UINT64_C(0))));
  t303 = ((t304) == (t305));
  }
  t306 = t303;
  t307 = t306;
  if (t307) {
  t308 = ((int64_t)(v9));
  t309 = ((int64_t)(((int64_t)UINT64_C(0))));
  t307 = ((t308) == (t309));
  }
  if (!(t307)) gg_fail("assertion failed");
  t310 = ((int64_t)(v8));
  t311 = ((int64_t)(((int64_t)UINT64_C(0))));
  t312 = ((t310) == (t311));
  if (t312) {
  {
  v13 = ((int64_t)(((int64_t)UINT64_C(4))));
  }
  } else {
  {
  v13 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  }
  t313 = ((int64_t)(v13));
  t314 = ((int64_t)(v11));
  t315 = ((int64_t)(v0));
  t316 = ((int64_t)(v1));
  t317 = ((int64_t)(((int64_t)UINT64_C(0))));
  t318 = ((int64_t)(((int64_t)UINT64_C(1))));
  t319 = gg_function_6(t315, t316, t317, t318, base + 0);
  t320 = gg_function_7(t314, t319, base + 0);
  t321 = ((int64_t)(v0));
  t322 = ((int64_t)(v1));
  t323 = ((int64_t)(((int64_t)UINT64_C(0))));
  t324 = ((int64_t)(((int64_t)UINT64_C(2))));
  t325 = gg_function_6(t321, t322, t323, t324, base + 0);
  t326 = gg_function_7(t320, t325, base + 0);
  t327 = ((int64_t)(v0));
  t328 = ((int64_t)(v1));
  t329 = ((int64_t)(((int64_t)UINT64_C(1))));
  t330 = ((int64_t)(((int64_t)UINT64_C(0))));
  t331 = gg_function_6(t327, t328, t329, t330, base + 0);
  t332 = ((int64_t)(v0));
  t333 = ((int64_t)(v1));
  t334 = ((int64_t)(((int64_t)UINT64_C(1))));
  t335 = ((int64_t)(((int64_t)UINT64_C(1))));
  t336 = gg_function_6(t332, t333, t334, t335, base + 0);
  t337 = gg_function_7(t331, t336, base + 0);
  t338 = gg_function_15(t326, t337, base + 0);
  v5 = gg_function_7(t313, t338, base + 0);
  }
  } else {
  t339 = ((int64_t)(v6));
  t340 = ((int64_t)(((int64_t)UINT64_C(56))));
  t341 = ((t339) == (t340));
  t342 = t341;
  if (!t342) {
  t343 = ((int64_t)(v6));
  t344 = ((int64_t)(((int64_t)UINT64_C(57))));
  t342 = ((t343) == (t344));
  }
  t345 = t342;
  if (t345) {
  {
  t346 = ((int64_t)(v0));
  t347 = ((int64_t)(v1));
  t348 = ((int64_t)(((int64_t)UINT64_C(58))));
  t349 = fg143_ctl_meta(t346, t347, t348);
  v5 = ((int64_t)(t349));
  }
  } else {
  t350 = ((int64_t)(v6));
  t351 = ((int64_t)(((int64_t)UINT64_C(59))));
  t352 = ((t350) == (t351));
  if (t352) {
  {
  t353 = ((int64_t)(v8));
  t354 = ((int64_t)(((int64_t)UINT64_C(0))));
  t355 = ((t353) == (t354));
  t356 = t355;
  if (!t356) {
  t357 = ((int64_t)(v8));
  t358 = ((int64_t)(((int64_t)UINT64_C(1))));
  t356 = ((t357) == (t358));
  }
  t359 = t356;
  t360 = t359;
  if (t360) {
  t361 = ((int64_t)(v9));
  t362 = ((int64_t)(((int64_t)UINT64_C(0))));
  t360 = ((t361) == (t362));
  }
  if (!(t360)) gg_fail("assertion failed");
  t363 = ((int64_t)(v0));
  t364 = ((int64_t)(v1));
  t365 = ((int64_t)(((int64_t)UINT64_C(44))));
  t366 = fg143_ctl_meta(t363, t364, t365);
  t367 = ((int64_t)(t366));
  t368 = ((int64_t)(v0));
  t369 = ((int64_t)(v1));
  t370 = ((int64_t)(((int64_t)UINT64_C(0))));
  t371 = gg_function_12(t368, t369, t370, base + 0);
  t372 = ((int64_t)(v0));
  t373 = ((int64_t)(v1));
  t374 = ((int64_t)(((int64_t)UINT64_C(1))));
  t375 = gg_function_12(t372, t373, t374, base + 0);
  t376 = gg_function_15(t371, t375, base + 0);
  v5 = gg_function_7(t367, t376, base + 0);
  }
  } else {
  t377 = ((int64_t)(v6));
  t378 = ((int64_t)(((int64_t)UINT64_C(70))));
  t379 = ((t377) == (t378));
  if (t379) {
  {
  t380 = ((int64_t)(((int64_t)UINT64_C(4))));
  t381 = ((int64_t)(v11));
  t382 = gg_function_7(t380, t381, base + 0);
  t383 = ((int64_t)(v3));
  v5 = gg_function_7(t382, t383, base + 0);
  }
  } else {
  t384 = ((int64_t)(v6));
  t385 = ((int64_t)(((int64_t)UINT64_C(74))));
  t386 = ((t384) == (t385));
  if (t386) {
  {
  t387 = ((int64_t)(v0));
  t388 = ((int64_t)(v1));
  t389 = ((int64_t)(((int64_t)UINT64_C(0))));
  t390 = ((int64_t)(((int64_t)UINT64_C(2))));
  v14 = gg_function_6(t387, t388, t389, t390, base + 0);
  t391 = ((int64_t)(v0));
  t392 = ((int64_t)(v1));
  t393 = ((int64_t)(((int64_t)UINT64_C(1))));
  t394 = ((int64_t)(((int64_t)UINT64_C(0))));
  v15 = gg_function_6(t391, t392, t393, t394, base + 0);
  t395 = ((int64_t)(v0));
  t396 = ((int64_t)(v1));
  t397 = ((int64_t)(((int64_t)UINT64_C(2))));
  t398 = ((int64_t)(((int64_t)UINT64_C(0))));
  v16 = gg_function_6(t395, t396, t397, t398, base + 0);
  t399 = ((int64_t)(v0));
  t400 = ((int64_t)(v1));
  t401 = ((int64_t)(((int64_t)UINT64_C(55))));
  t402 = fg143_ctl_meta(t399, t400, t401);
  v17 = ((int64_t)(t402));
  t403 = ((int64_t)(v0));
  t404 = ((int64_t)(v1));
  t405 = ((int64_t)(((int64_t)UINT64_C(56))));
  t406 = fg143_ctl_meta(t403, t404, t405);
  v18 = ((int64_t)(t406));
  t407 = ((int64_t)(((int64_t)UINT64_C(4))));
  t408 = ((int64_t)(v3));
  t409 = gg_function_7(t407, t408, base + 0);
  t410 = ((int64_t)(v17));
  t411 = ((int64_t)(v15));
  t412 = gg_function_7(t410, t411, base + 0);
  t413 = ((int64_t)(((int64_t)UINT64_C(2))));
  t414 = ((int64_t)(v17));
  t415 = gg_function_7(t413, t414, base + 0);
  t416 = ((int64_t)(v18));
  t417 = gg_function_7(t415, t416, base + 0);
  t418 = gg_function_15(t412, t417, base + 0);
  t419 = ((int64_t)(v17));
  t420 = ((int64_t)(v16));
  t421 = gg_function_7(t419, t420, base + 0);
  t422 = ((int64_t)(v18));
  t423 = ((int64_t)(v16));
  t424 = gg_function_7(t422, t423, base + 0);
  t425 = ((int64_t)(v18));
  t426 = ((int64_t)(v15));
  t427 = gg_function_7(t425, t426, base + 0);
  t428 = gg_function_15(t424, t427, base + 0);
  t429 = gg_function_15(t421, t428, base + 0);
  t430 = gg_function_15(t418, t429, base + 0);
  v19 = gg_function_7(t409, t430, base + 0);
  t431 = ((int64_t)(((int64_t)UINT64_C(4))));
  t432 = ((int64_t)(v14));
  t433 = ((int64_t)(v3));
  t434 = gg_function_7(t432, t433, base + 0);
  t435 = ((int64_t)(((int64_t)UINT64_C(2))));
  t436 = ((int64_t)(v16));
  t437 = gg_function_15(t435, t436, base + 0);
  t438 = gg_function_7(t434, t437, base + 0);
  t439 = ((int64_t)(v3));
  t440 = ((int64_t)(v15));
  t441 = ((int64_t)(((int64_t)UINT64_C(2))));
  t442 = ((int64_t)(v16));
  t443 = gg_function_7(t441, t442, base + 0);
  t444 = gg_function_15(t440, t443, base + 0);
  t445 = gg_function_7(t439, t444, base + 0);
  t446 = gg_function_15(t438, t445, base + 0);
  v20 = gg_function_7(t431, t446, base + 0);
  t447 = ((int64_t)(v19));
  t448 = ((int64_t)(v20));
  v5 = gg_function_9(t447, t448, base + 0);
  }
  } else {
  t449 = ((int64_t)(v6));
  t450 = ((int64_t)(((int64_t)UINT64_C(75))));
  t451 = ((t449) == (t450));
  if (t451) {
  {
  t452 = ((int64_t)(v9));
  t453 = ((int64_t)(((int64_t)UINT64_C(0))));
  t454 = ((t452) == (t453));
  t455 = t454;
  if (!t455) {
  t456 = ((int64_t)(v9));
  t457 = ((int64_t)(((int64_t)UINT64_C(1))));
  t455 = ((t456) == (t457));
  }
  t458 = t455;
  t459 = t458;
  if (!t459) {
  t460 = ((int64_t)(v9));
  t461 = ((int64_t)(((int64_t)UINT64_C(30))));
  t459 = ((t460) == (t461));
  }
  t462 = t459;
  if (t462) {
  {
  t463 = ((int64_t)(v11));
  t464 = ((int64_t)(v0));
  t465 = ((int64_t)(v1));
  t466 = ((int64_t)(((int64_t)UINT64_C(1))));
  t467 = ((int64_t)(((int64_t)UINT64_C(1))));
  t468 = gg_function_6(t464, t465, t466, t467, base + 0);
  t469 = ((int64_t)(v0));
  t470 = ((int64_t)(v1));
  t471 = ((int64_t)(((int64_t)UINT64_C(57))));
  t472 = fg143_ctl_meta(t469, t470, t471);
  t473 = ((int64_t)(t472));
  t474 = gg_function_14(t468, t473, base + 0);
  v21 = gg_function_9(t463, t474, base + 0);
  t475 = ((int64_t)(((int64_t)UINT64_C(16))));
  t476 = ((int64_t)(v21));
  t477 = gg_function_7(t475, t476, base + 0);
  t478 = ((int64_t)(v3));
  t479 = gg_function_7(t477, t478, base + 0);
  t480 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = gg_function_7(t479, t480, base + 0);
  }
  }
  }
  } else {
  t481 = ((int64_t)(v6));
  t482 = ((int64_t)(((int64_t)UINT64_C(96))));
  t483 = ((t481) == (t482));
  if (t483) {
  {
  t484 = ((int64_t)(v0));
  t485 = ((int64_t)(v1));
  t486 = ((int64_t)(((int64_t)UINT64_C(43))));
  t487 = fg143_ctl_meta(t484, t485, t486);
  t488 = ((int64_t)(t487));
  t489 = ((int64_t)(v3));
  t490 = ((int64_t)(v11));
  t491 = ((int64_t)(v3));
  t492 = gg_function_7(t490, t491, base + 0);
  t493 = gg_function_15(t489, t492, base + 0);
  v5 = gg_function_7(t488, t493, base + 0);
  }
  } else {
  t494 = ((int64_t)(v6));
  t495 = ((int64_t)(((int64_t)UINT64_C(86))));
  t496 = ((t494) == (t495));
  if (t496) {
  {
  t497 = ((int64_t)(v0));
  t498 = ((int64_t)(v1));
  t499 = ((int64_t)(((int64_t)UINT64_C(2))));
  t500 = ((int64_t)(((int64_t)UINT64_C(0))));
  v22 = gg_function_6(t497, t498, t499, t500, base + 0);
  t501 = ((int64_t)(v22));
  t503 = ((int64_t)(v0));
  t504 = ((int64_t)(v1));
  t505 = gg_function_17(t503, t504, base + 0);
  t506 = ((int64_t)(t505));
  t507 = ((int64_t)(((int64_t)UINT64_C(1))));
  t508 = ((t506) > (t507));
  if (t508) {
  {
  t509 = ((int64_t)(v22));
  t510 = ((int64_t)(v22));
  t502 = gg_function_7(t509, t510, base + 0);
  }
  } else {
  {
  t502 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t511 = ((int64_t)(t502));
  v23 = gg_function_15(t501, t511, base + 0);
  t512 = ((int64_t)(v23));
  t513 = ((int64_t)(((int64_t)UINT64_C(4))));
  t514 = gg_function_7(t512, t513, base + 0);
  t515 = ((int64_t)(v3));
  v5 = gg_function_7(t514, t515, base + 0);
  }
  } else {
  t516 = ((int64_t)(v6));
  t517 = ((int64_t)(((int64_t)UINT64_C(87))));
  t518 = ((t516) == (t517));
  if (t518) {
  {
  t519 = ((int64_t)(((int64_t)UINT64_C(4))));
  t520 = ((int64_t)(v0));
  t521 = ((int64_t)(v1));
  t522 = ((int64_t)(((int64_t)UINT64_C(1))));
  t523 = ((int64_t)(((int64_t)UINT64_C(0))));
  t524 = gg_function_6(t520, t521, t522, t523, base + 0);
  t525 = gg_function_7(t519, t524, base + 0);
  t526 = ((int64_t)(v3));
  v5 = gg_function_7(t525, t526, base + 0);
  }
  } else {
  {
  t527 = ((int64_t)(v6));
  t528 = ((int64_t)(((int64_t)UINT64_C(101))));
  if (!(((t527) != (t528)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  t529 = ((int64_t)(v5));
  t530 = ((int64_t)(v0));
  t531 = ((int64_t)(v1));
  t532 = ((int64_t)(v3));
  t533 = fg143_ctl_goose_work(t530, t531, t532);
  t534 = ((int64_t)(t533));
  t0 = gg_function_9(t529, t534, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_2(int64_t p0, int64_t p1, int64_t p2, gg_slice p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
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
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t * t60;
  int64_t t61;
  int64_t * t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t v0;
  int64_t v1;
  int64_t v2;
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
  {
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((v3).len));
  t5 = ((int64_t)(((int64_t)UINT64_C(2))));
  t3 = ((t4) >= (t5));
  }
  if (!(t3)) gg_fail("assertion failed");
  v4 = ((int64_t)(v2));
  t6 = ((int64_t)(v4));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t6) <= (t7));
  if (t8) {
  {
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)UINT64_C(1));
  t11 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t10)))));
  t12 = ((int64_t)(((int64_t)UINT64_C(50))));
  t13 = fg143_ctl_meta(t9, t11, t12);
  v5 = ((int64_t)(t13));
  t14 = ((int64_t)(v5));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t14) > (t15));
  if (t16) {
  {
  v4 = ((int64_t)(v5));
  }
  } else {
  {
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)UINT64_C(1));
  t19 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t18)))));
  t20 = ((int64_t)(((int64_t)UINT64_C(51))));
  t21 = fg143_ctl_meta(t17, t19, t20);
  v4 = ((int64_t)(t21));
  }
  }
  }
  }
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)UINT64_C(1));
  t24 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t23)))));
  t25 = ((int64_t)(((int64_t)UINT64_C(52))));
  t26 = fg143_ctl_meta(t22, t24, t25);
  t27 = ((int64_t)(t26));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = ((t27) != (t28));
  if (t29) {
  {
  v4 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t30 = ((int64_t)(v4));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t30) > (t31)))) gg_fail("assertion failed");
  v6 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = v1;
  t32 = 0;
label0:;
  if (t32 >= t33) goto label2;
  v8 = t32;
  {
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(v8));
  t36 = ((int64_t)(v4));
  v9 = gg_function_4(t34, t35, t36, base + 0);
  t37 = ((int64_t)(v9));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t37) > (t38)))) gg_fail("assertion failed");
  t39 = ((int64_t)(v6));
  t40 = ((int64_t)(v9));
  v6 = gg_function_9(t39, t40, base + 0);
  t41 = ((int64_t)(v7));
  t42 = ((int64_t)(v0));
  t43 = ((int64_t)(v8));
  t44 = ((int64_t)(v4));
  t45 = ((int64_t)(v9));
  t46 = gg_function_10(t42, t43, t44, t45, base + 0);
  v7 = gg_function_9(t41, t46, base + 0);
  }
label1:;
  t32++; goto label0;
label2:;
  t47 = ((int64_t)(v7));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = ((t47) > (t48));
  if (t49) {
  {
  t50 = ((int64_t)(v7));
  t51 = ((int64_t)(v0));
  t52 = ((int64_t)UINT64_C(1));
  t53 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t52)))));
  t54 = ((int64_t)(((int64_t)UINT64_C(53))));
  t55 = fg143_ctl_meta(t51, t53, t54);
  t56 = ((int64_t)(t55));
  t57 = ((int64_t)(v4));
  t58 = gg_function_7(t56, t57, base + 0);
  v7 = gg_function_15(t50, t58, base + 0);
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t59 = ((int64_t)UINT64_C(0));
  t60 = ((int64_t *)(v3).data) + gg_index(t59, (v3).len);
  (*t60) = ((int64_t)(v7));
  /* FG_DIRECT_SLICE_INDEX */
  t61 = ((int64_t)UINT64_C(1));
  t62 = ((int64_t *)(v3).data) + gg_index(t61, (v3).len);
  t63 = ((int64_t)(v6));
  t64 = ((int64_t)(v4));
  (*t62) = gg_function_8(t63, t64, base + 0);
  t65 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg143_ctl_witness(t65);
  }
gg_done:;
}
static void gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, gg_slice p4, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  gg_slice t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t * t13;
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
  uint8_t t24;
  int64_t t25;
  gg_slice t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  uint8_t t34;
  uint8_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t * t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  gg_slice v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(v3));
  t6 = v4;
  gg_function_2(t3, t4, t5, t6, base + 0);
  }
  } else {
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(1))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t10 = ((int64_t)((v4).len));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  if (!(((t10) >= (t11)))) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t12 = ((int64_t)UINT64_C(0));
  t13 = ((int64_t *)(v4).data) + gg_index(t12, (v4).len);
  t14 = ((int64_t)(v1));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((int64_t)(v3));
  (*t13) = gg_function_4(t14, t15, t16, base + 0);
  t17 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg143_ctl_witness(t17);
  }
  } else {
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(2))));
  t20 = ((t18) == (t19));
  if (t20) {
  {
  t21 = ((int64_t)(v1));
  gg_function_20(t21, base + 0);
  }
  } else {
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(3))));
  t24 = ((t22) == (t23));
  if (t24) {
  {
  t25 = ((int64_t)(v1));
  t26 = v4;
  gg_function_118(t25, t26, base + 0);
  }
  } else {
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(4))));
  t29 = ((t27) == (t28));
  if (t29) {
  {
  t30 = ((int64_t)(v1));
  t31 = ((uint8_t)UINT64_C(0));
  gg_function_123(t30, t31, base + 0);
  }
  } else {
  {
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(5))));
  t34 = ((t32) == (t33));
  t35 = t34;
  if (t35) {
  t36 = ((int64_t)((v4).len));
  t37 = ((int64_t)(((int64_t)UINT64_C(1))));
  t35 = ((t36) >= (t37));
  }
  if (!(t35)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t38 = ((int64_t)UINT64_C(0));
  t39 = ((int64_t *)(v4).data) + gg_index(t38, (v4).len);
  t40 = ((int64_t)(v1));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = ((int64_t)(v3));
  (*t39) = gg_function_16(t40, t41, t42, base + 0);
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_0(int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  gg_slice t7;
  gg_type_2 v0;
  {
  memset(&t0, 0, sizeof(t0));
  t1 = ((int64_t)(t0));
  for (t2=0; t2<2; t2++) (v0).data[t2] = t1;
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = gg_view((v0).data, 2);
  gg_function_1(t3, t4, t5, t6, t7, base + 0);
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
