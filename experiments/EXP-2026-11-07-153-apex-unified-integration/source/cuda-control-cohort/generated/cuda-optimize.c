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


static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(int64_t a, int64_t b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, int64_t value);
static uint8_t gg_eq_2(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, uint8_t value);
static uint8_t gg_eq_4(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, uint8_t value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_1(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_2(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_4(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base);
static int64_t gg_function_2(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_7(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_19(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_26(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_27(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_32(int64_t p0, int64_t p1, int64_t base);
static uint8_t gg_function_35(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_36(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t base);
static int64_t gg_function_38(int64_t p0, int64_t p1, int64_t base);
static uint8_t gg_function_39(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_46(int64_t p0, int64_t base);
static int64_t gg_function_32(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = fg14_s_vec_data(t1);
  t3 = ((int64_t)(t2));
  t4 = ((int64_t)(v1));
  t5 = fg14_s_frame_get(t3, t4);
  t0 = ((int64_t)(t5));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_46(int64_t p0, int64_t base) {
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
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = fg14_s_vec_len(t1);
  v1 = ((int64_t)(t2));
  v2 = ((int64_t)(((int64_t)UINT64_C(1))));
label1:;
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(v1));
  if (!(((t3) < (t4)))) goto label2;
  {
  t5 = ((int64_t)(v0));
  t6 = fg14_s_vec_data(t5);
  t7 = ((int64_t)(t6));
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) - (uint64_t)(t9)))));
  t11 = ((int64_t)(v0));
  t12 = fg14_s_vec_data(t11);
  t13 = ((int64_t)(t12));
  t14 = ((int64_t)(v2));
  t15 = fg14_s_frame_get(t13, t14);
  t16 = ((int64_t)(t15));
  t17 = fg14_s_frame_set(t7, t10, t16);
  (void)(t17);
  t18 = v2;
  t19 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t18) + (uint64_t)(t19)));
  }
  goto label1;
label2:;
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(((int64_t)UINT64_C(1))));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) - (uint64_t)(t22)))));
  t24 = fg14_s_vec_resize(t20, t23);
  t0 = ((int64_t)(t24));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_27(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = fg14_s_vec_len(t1);
  v2 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  t7 = fg14_s_vec_resize(t3, t6);
  (void)(t7);
  t8 = ((int64_t)(v0));
  t9 = fg14_s_vec_data(t8);
  t10 = ((int64_t)(t9));
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(v1));
  t13 = fg14_s_frame_set(t10, t11, t12);
  (void)(t13);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_26(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t0 = gg_function_27(t1, t2, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_39(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = fg14_s_vec_len(t1);
  v2 = ((int64_t)(t2));
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v2));
  if (!(((t3) < (t4)))) goto label2;
  {
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(v3));
  t7 = gg_function_32(t5, t6, base + 0);
  t8 = ((int64_t)(t7));
  t9 = ((int64_t)(v1));
  t10 = ((t8) == (t9));
  if (t10) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t11 = v3;
  t12 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t11) + (uint64_t)(t12)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_38(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = fg14_s_vec_len(t1);
  v2 = ((int64_t)(t2));
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v2));
  if (!(((t3) < (t4)))) goto label2;
  {
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(v3));
  t8 = gg_function_32(t6, t7, base + 0);
  t9 = fg14_s_t_depends(t5, t8);
  t10 = ((int64_t)(t9));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  t12 = ((t10) == (t11));
  if (t12) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t13 = v3;
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t13) + (uint64_t)(t14)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_36(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t base) {
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
  uint8_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
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
  uint8_t t35;
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
  int64_t t60;
  int64_t t61;
  int64_t t62;
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
  int64_t t75;
  uint8_t t76;
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
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  uint8_t t95;
  uint8_t t96;
  uint8_t t97;
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
  uint8_t t115;
  int64_t t116;
  int64_t t117;
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
  uint8_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  uint8_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
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
  uint8_t t216;
  int64_t t217;
  int64_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  int64_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  int64_t t227;
  int64_t t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  int64_t t234;
  uint8_t t235;
  uint8_t t236;
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
  int64_t t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
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
  int64_t v26;
  int64_t v27;
  int64_t v28;
  int64_t v29;
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
  t1 = ((int64_t)(v9));
  t2 = fg14_s_vec_len(t1);
  v10 = ((int64_t)(t2));
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t3 = ((int64_t)(v11));
  t4 = ((int64_t)(v4));
  if (!(((t3) < (t4)))) goto label2;
  {
  t5 = ((int64_t)(v9));
  t6 = ((int64_t)(((int64_t)UINT64_C(8))));
  t7 = fg14_s_vec_new(t6);
  t8 = ((int64_t)(t7));
  v12 = gg_function_26(t5, t8, base + 0);
  t9 = v11;
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t9) + (uint64_t)(t10)));
  }
  goto label1;
label2:;
  t11 = ((int64_t)(v1));
  t12 = fg14_s_g_n(t11);
  v13 = ((int64_t)(t12));
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t15 = ((int64_t)(v14));
  t16 = ((int64_t)(v13));
  t17 = ((t15) < (t16));
  t18 = t17;
  if (t18) {
  t19 = ((int64_t)(v15));
  t20 = ((int64_t)(v4));
  t18 = ((t19) < (t20));
  }
  if (!(t18)) goto label5;
  {
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(v14));
  t23 = fg14_s_g_node(t21, t22);
  v16 = ((int64_t)(t23));
  t24 = ((int64_t)(v16));
  t25 = fg14_s_node_is_view_or_noop(t24);
  t26 = ((int64_t)(t25));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = ((t26) == (t27));
  t29 = t28;
  if (t29) {
  t30 = ((int64_t)(v16));
  t31 = ((int64_t)(v2));
  t32 = fg14_s_t_depends(t30, t31);
  t33 = ((int64_t)(t32));
  t34 = ((int64_t)(((int64_t)UINT64_C(1))));
  t29 = ((t33) == (t34));
  }
  t35 = t29;
  if (t35) {
  {
  t36 = ((int64_t)(v9));
  t37 = ((int64_t)(v10));
  t38 = ((int64_t)(v15));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t38)))));
  t40 = gg_function_32(t36, t39, base + 0);
  t41 = ((int64_t)(v16));
  t42 = gg_function_26(t40, t41, base + 0);
  (void)(t42);
  t43 = v15;
  t44 = ((int64_t)(((int64_t)UINT64_C(1))));
  v15 = ((int64_t)((uint64_t)(t43) + (uint64_t)(t44)));
  }
  }
  t45 = v14;
  t46 = ((int64_t)(((int64_t)UINT64_C(1))));
  v14 = ((int64_t)((uint64_t)(t45) + (uint64_t)(t46)));
  }
  goto label4;
label5:;
  t47 = ((int64_t)(v15));
  t48 = ((int64_t)(v4));
  t49 = ((t47) != (t48));
  if (t49) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = ((int64_t)UINT64_C(1));
  v18 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t50)))));
  t51 = ((int64_t)(v3));
  t52 = ((int64_t)(((int64_t)UINT64_C(1))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t51) + (uint64_t)(t52)))));
label7:;
  t53 = ((int64_t)(v14));
  t54 = ((int64_t)(v13));
  if (!(((t53) < (t54)))) goto label8;
  {
  t55 = ((int64_t)(v1));
  t56 = ((int64_t)(v14));
  t57 = fg14_s_g_node(t55, t56);
  v19 = ((int64_t)(t57));
  v20 = ((int64_t)(((int64_t)UINT64_C(0))));
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t58 = ((int64_t)(v11));
  t59 = ((int64_t)(v4));
  if (!(((t58) < (t59)))) goto label11;
  {
  t60 = ((int64_t)(v19));
  t61 = ((int64_t)(v9));
  t62 = ((int64_t)(v10));
  t63 = ((int64_t)(v11));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t62) + (uint64_t)(t63)))));
  t65 = gg_function_32(t61, t64, base + 0);
  t66 = gg_function_38(t60, t65, base + 0);
  t67 = ((int64_t)(t66));
  t68 = ((int64_t)(((int64_t)UINT64_C(1))));
  t69 = ((t67) == (t68));
  if (t69) {
  {
  t70 = v20;
  t71 = ((int64_t)(((int64_t)UINT64_C(1))));
  v20 = ((int64_t)((uint64_t)(t70) + (uint64_t)(t71)));
  }
  }
  t72 = v11;
  t73 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t72) + (uint64_t)(t73)));
  }
  goto label10;
label11:;
  t74 = ((int64_t)(v20));
  t75 = ((int64_t)(((int64_t)UINT64_C(2))));
  t76 = ((t74) >= (t75));
  if (t76) {
  {
  v17 = ((int64_t)(v19));
  v18 = ((int64_t)(v14));
  goto label8;
  }
  }
  v21 = ((int64_t)(((int64_t)UINT64_C(0))));
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t77 = ((int64_t)(v11));
  t78 = ((int64_t)(v4));
  if (!(((t77) < (t78)))) goto label14;
  {
  t79 = ((int64_t)(v19));
  t80 = ((int64_t)(v9));
  t81 = ((int64_t)(v10));
  t82 = ((int64_t)(v11));
  t83 = ((int64_t)(((int64_t)((uint64_t)(t81) + (uint64_t)(t82)))));
  t84 = gg_function_32(t80, t83, base + 0);
  t85 = gg_function_38(t79, t84, base + 0);
  t86 = ((int64_t)(t85));
  t87 = ((int64_t)(((int64_t)UINT64_C(1))));
  t88 = ((t86) == (t87));
  if (t88) {
  {
  t89 = ((int64_t)(v9));
  t90 = ((int64_t)(v10));
  t91 = ((int64_t)(v11));
  t92 = ((int64_t)(((int64_t)((uint64_t)(t90) + (uint64_t)(t91)))));
  t93 = gg_function_32(t89, t92, base + 0);
  t94 = ((int64_t)(v19));
  t95 = gg_function_39(t93, t94, base + 0);
  t96 = t95;
  t97 = (!(t96));
  if (t97) {
  {
  t98 = ((int64_t)(v9));
  t99 = ((int64_t)(v10));
  t100 = ((int64_t)(v11));
  t101 = ((int64_t)(((int64_t)((uint64_t)(t99) + (uint64_t)(t100)))));
  t102 = gg_function_32(t98, t101, base + 0);
  t103 = ((int64_t)(v19));
  t104 = gg_function_26(t102, t103, base + 0);
  (void)(t104);
  }
  }
  v21 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t105 = v11;
  t106 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t105) + (uint64_t)(t106)));
  }
  goto label13;
label14:;
  t107 = ((int64_t)(v21));
  t108 = ((int64_t)(((int64_t)UINT64_C(0))));
  t109 = ((t107) == (t108));
  t110 = t109;
  if (t110) {
  t111 = ((int64_t)(v19));
  t112 = fg14_s_node_is_view_or_noop(t111);
  t113 = ((int64_t)(t112));
  t114 = ((int64_t)(((int64_t)UINT64_C(0))));
  t110 = ((t113) != (t114));
  }
  t115 = t110;
  if (t115) {
  {
  t116 = ((int64_t)(v9));
  t117 = ((int64_t)(v10));
  t118 = gg_function_32(t116, t117, base + 0);
  t119 = ((int64_t)(v19));
  t120 = gg_function_26(t118, t119, base + 0);
  (void)(t120);
  }
  }
  t121 = v14;
  t122 = ((int64_t)(((int64_t)UINT64_C(1))));
  v14 = ((int64_t)((uint64_t)(t121) + (uint64_t)(t122)));
  }
  goto label7;
label8:;
  t123 = ((int64_t)(v17));
  t124 = ((int64_t)(((int64_t)UINT64_C(0))));
  t125 = ((t123) == (t124));
  t126 = t125;
  if (!t126) {
  t127 = ((int64_t)(v18));
  t128 = ((int64_t)(((int64_t)UINT64_C(0))));
  t126 = ((t127) < (t128));
  }
  t129 = t126;
  if (t129) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v22 = ((int64_t)(((int64_t)UINT64_C(0))));
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label16:;
  t130 = ((int64_t)(v11));
  t131 = ((int64_t)(v4));
  if (!(((t130) < (t131)))) goto label17;
  {
  t132 = v22;
  t133 = ((int64_t)(v9));
  t134 = ((int64_t)(v10));
  t135 = ((int64_t)(v11));
  t136 = ((int64_t)(((int64_t)((uint64_t)(t134) + (uint64_t)(t135)))));
  t137 = gg_function_32(t133, t136, base + 0);
  t138 = fg14_s_vec_len(t137);
  t139 = ((int64_t)(t138));
  v22 = ((int64_t)((uint64_t)(t132) + (uint64_t)(t139)));
  t140 = v11;
  t141 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t140) + (uint64_t)(t141)));
  }
  goto label16;
label17:;
  t142 = ((int64_t)(v18));
  t143 = ((int64_t)(v3));
  t144 = ((int64_t)(((int64_t)((uint64_t)(t142) - (uint64_t)(t143)))));
  t145 = ((int64_t)(((int64_t)UINT64_C(1))));
  t146 = ((int64_t)(((int64_t)((uint64_t)(t144) - (uint64_t)(t145)))));
  t147 = ((int64_t)(v22));
  t148 = ((t146) != (t147));
  if (t148) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t149 = ((int64_t)(v0));
  t150 = ((int64_t)(v4));
  t151 = fg14_res_conc_new(t149, t150);
  v23 = ((int64_t)(t151));
  t152 = ((int64_t)(v23));
  t153 = ((int64_t)(((int64_t)UINT64_C(0))));
  t154 = ((t152) == (t153));
  if (t154) {
  {
  t155 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t155)))));
  goto gg_done;
  }
  }
  t156 = ((int64_t)(v23));
  t157 = ((int64_t)(((int64_t)UINT64_C(5))));
  t158 = ((int64_t)(v17));
  t159 = fg14_res_conc_set(t156, t157, t158);
  (void)(t159);
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label19:;
  t160 = ((int64_t)(v11));
  t161 = ((int64_t)(v4));
  if (!(((t160) < (t161)))) goto label20;
  {
  t162 = ((int64_t)(v9));
  t163 = ((int64_t)(v10));
  t164 = ((int64_t)(v11));
  t165 = ((int64_t)(((int64_t)((uint64_t)(t163) + (uint64_t)(t164)))));
  v24 = gg_function_32(t162, t165, base + 0);
  t166 = ((int64_t)(v24));
  t167 = fg14_s_vec_len(t166);
  v25 = ((int64_t)(t167));
  v26 = ((int64_t)(((int64_t)UINT64_C(0))));
label22:;
  t168 = ((int64_t)(v26));
  t169 = ((int64_t)(v25));
  if (!(((t168) < (t169)))) goto label23;
  {
  t170 = ((int64_t)(v23));
  t171 = ((int64_t)(v24));
  t172 = ((int64_t)(v26));
  t173 = gg_function_32(t171, t172, base + 0);
  t174 = ((int64_t)(v11));
  t175 = ((int64_t)(((int64_t)UINT64_C(1))));
  t176 = ((int64_t)(((int64_t)((uint64_t)(t174) + (uint64_t)(t175)))));
  t177 = fg14_res_conc_stream_set(t170, t173, t176);
  (void)(t177);
  t178 = v26;
  t179 = ((int64_t)(((int64_t)UINT64_C(1))));
  v26 = ((int64_t)((uint64_t)(t178) + (uint64_t)(t179)));
  }
  goto label22;
label23:;
  t180 = v11;
  t181 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t180) + (uint64_t)(t181)));
  }
  goto label19;
label20:;
  t182 = ((int64_t)(v3));
  t183 = ((int64_t)(((int64_t)UINT64_C(1))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t182) + (uint64_t)(t183)))));
label25:;
  t184 = ((int64_t)(v14));
  t185 = ((int64_t)(v18));
  if (!(((t184) < (t185)))) goto label26;
  {
  t186 = ((int64_t)(v23));
  t187 = ((int64_t)(v1));
  t188 = ((int64_t)(v14));
  t189 = fg14_s_g_node(t187, t188);
  t190 = ((int64_t)(t189));
  t191 = fg14_res_conc_order_push(t186, t190);
  (void)(t191);
  t192 = v14;
  t193 = ((int64_t)(((int64_t)UINT64_C(1))));
  v14 = ((int64_t)((uint64_t)(t192) + (uint64_t)(t193)));
  }
  goto label25;
label26:;
  t194 = ((int64_t)(v0));
  t195 = ((int64_t)(v2));
  t196 = ((int64_t)(v23));
  t197 = fg14_res_stream_ctx_insert(t194, t195, t196);
  (void)(t197);
  t198 = ((int64_t)(v7));
  t199 = ((int64_t)(v3));
  t200 = gg_function_26(t198, t199, base + 0);
  (void)(t200);
  t201 = ((int64_t)(v8));
  t202 = ((int64_t)(v18));
  t203 = gg_function_26(t201, t202, base + 0);
  (void)(t203);
  v27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t204 = ((int64_t)(v3));
  t205 = ((int64_t)(((int64_t)UINT64_C(1))));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t204) + (uint64_t)(t205)))));
label28:;
  t206 = ((int64_t)(v28));
  t207 = ((int64_t)(v18));
  if (!(((t206) < (t207)))) goto label29;
  {
  t208 = ((int64_t)(v9));
  t209 = ((int64_t)(v10));
  t210 = ((int64_t)(v27));
  t211 = ((int64_t)(((int64_t)((uint64_t)(t209) + (uint64_t)(t210)))));
  v29 = gg_function_32(t208, t211, base + 0);
  t212 = ((int64_t)(v29));
  t213 = fg14_s_vec_len(t212);
  t214 = ((int64_t)(t213));
  t215 = ((int64_t)(((int64_t)UINT64_C(0))));
  t216 = ((t214) == (t215));
  if (t216) {
  {
  t217 = ((int64_t)(v27));
  t218 = ((int64_t)(((int64_t)UINT64_C(1))));
  t219 = ((int64_t)(((int64_t)((uint64_t)(t217) + (uint64_t)(t218)))));
  t220 = ((int64_t)(v4));
  v27 = ((int64_t)(((int64_t)gg_mod(t219, t220))));
  goto label28;
  }
  }
  t221 = ((int64_t)(v1));
  t222 = ((int64_t)(v28));
  t223 = ((int64_t)(v29));
  t224 = ((int64_t)(((int64_t)UINT64_C(0))));
  t225 = gg_function_32(t223, t224, base + 0);
  t226 = fg14_s_g_set_node_at(t221, t222, t225);
  (void)(t226);
  t227 = v28;
  t228 = ((int64_t)(((int64_t)UINT64_C(1))));
  v28 = ((int64_t)((uint64_t)(t227) + (uint64_t)(t228)));
  t229 = ((int64_t)(v29));
  t230 = gg_function_46(t229, base + 0);
  (void)(t230);
label31:;
  t231 = ((int64_t)(v29));
  t232 = fg14_s_vec_len(t231);
  t233 = ((int64_t)(t232));
  t234 = ((int64_t)(((int64_t)UINT64_C(0))));
  t235 = ((t233) > (t234));
  t236 = t235;
  if (t236) {
  t237 = ((int64_t)(v29));
  t238 = ((int64_t)(((int64_t)UINT64_C(0))));
  t239 = gg_function_32(t237, t238, base + 0);
  t240 = fg14_s_node_is_view_or_noop(t239);
  t241 = ((int64_t)(t240));
  t242 = ((int64_t)(((int64_t)UINT64_C(0))));
  t236 = ((t241) != (t242));
  }
  if (!(t236)) goto label32;
  {
  t243 = ((int64_t)(v1));
  t244 = ((int64_t)(v28));
  t245 = ((int64_t)(v29));
  t246 = ((int64_t)(((int64_t)UINT64_C(0))));
  t247 = gg_function_32(t245, t246, base + 0);
  t248 = fg14_s_g_set_node_at(t243, t244, t247);
  (void)(t248);
  t249 = v28;
  t250 = ((int64_t)(((int64_t)UINT64_C(1))));
  v28 = ((int64_t)((uint64_t)(t249) + (uint64_t)(t250)));
  t251 = ((int64_t)(v29));
  t252 = gg_function_46(t251, base + 0);
  (void)(t252);
  }
  goto label31;
label32:;
  t253 = ((int64_t)(v27));
  t254 = ((int64_t)(((int64_t)UINT64_C(1))));
  t255 = ((int64_t)(((int64_t)((uint64_t)(t253) + (uint64_t)(t254)))));
  t256 = ((int64_t)(v4));
  v27 = ((int64_t)(((int64_t)gg_mod(t255, t256))));
  }
  goto label28;
label29:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_35(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  int64_t t18;
  int64_t t19;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = fg14_s_vec_len(t1);
  v3 = ((int64_t)(t2));
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t3 = ((int64_t)(v4));
  t4 = ((int64_t)(v3));
  if (!(((t3) < (t4)))) goto label2;
  {
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(v4));
  t8 = gg_function_32(t6, t7, base + 0);
  t9 = ((int64_t)(t8));
  t10 = ((t5) >= (t9));
  t11 = t10;
  if (t11) {
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(v4));
  t15 = gg_function_32(t13, t14, base + 0);
  t16 = ((int64_t)(t15));
  t11 = ((t12) <= (t16));
  }
  t17 = t11;
  if (t17) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t18 = v4;
  t19 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t18) + (uint64_t)(t19)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_19(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  uint8_t t30;
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
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  uint8_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  uint8_t t70;
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
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  uint8_t t95;
  uint8_t t96;
  int64_t t97;
  int64_t t98;
  uint8_t t99;
  uint8_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  uint8_t t111;
  uint8_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  uint8_t t116;
  uint8_t t117;
  uint8_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
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
  int64_t t153;
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
  int64_t v26;
  int64_t v27;
  int64_t v28;
  v0 = p0;
  v1 = p1;
  {
  t1 = fg14_s_map_new();
  v2 = ((int64_t)(t1));
  t2 = fg14_s_map_new();
  v3 = ((int64_t)(t2));
  t3 = ((int64_t)(((int64_t)UINT64_C(8))));
  t4 = fg14_s_vec_new(t3);
  v4 = ((int64_t)(t4));
  t5 = ((int64_t)(((int64_t)UINT64_C(8))));
  t6 = fg14_s_vec_new(t5);
  v5 = ((int64_t)(t6));
  t7 = ((int64_t)(((int64_t)UINT64_C(8))));
  t8 = fg14_s_vec_new(t7);
  v6 = ((int64_t)(t8));
  t9 = ((int64_t)(((int64_t)UINT64_C(8))));
  t10 = fg14_s_vec_new(t9);
  v7 = ((int64_t)(t10));
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t11) == (t12));
  t14 = t13;
  if (!t14) {
  t15 = ((int64_t)(v3));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t15) == (t16));
  }
  t17 = t14;
  t18 = t17;
  if (!t18) {
  t19 = ((int64_t)(v4));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t19) == (t20));
  }
  t21 = t18;
  t22 = t21;
  if (!t22) {
  t23 = ((int64_t)(v5));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t23) == (t24));
  }
  t25 = t22;
  t26 = t25;
  if (!t26) {
  t27 = ((int64_t)(v6));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((t27) == (t28));
  }
  t29 = t26;
  t30 = t29;
  if (!t30) {
  t31 = ((int64_t)(v7));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = ((t31) == (t32));
  }
  t33 = t30;
  if (t33) {
  {
  t34 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t34)))));
  goto gg_done;
  }
  }
  t35 = ((int64_t)(v1));
  t36 = fg14_s_g_n(t35);
  v8 = ((int64_t)(t36));
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t37 = ((int64_t)(v9));
  t38 = ((int64_t)(v8));
  if (!(((t37) < (t38)))) goto label2;
  {
  t39 = ((int64_t)(v1));
  t40 = ((int64_t)(v9));
  t41 = fg14_s_g_node(t39, t40);
  v10 = ((int64_t)(t41));
  t42 = ((int64_t)(v3));
  t43 = ((int64_t)(v10));
  t44 = ((int64_t)(v9));
  t45 = fg14_s_map_put(t42, t43, t44);
  v11 = ((int64_t)(t45));
  t46 = ((int64_t)(v10));
  t47 = fg14_s_node_is_view_or_noop(t46);
  t48 = ((int64_t)(t47));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = ((t48) == (t49));
  t51 = t50;
  if (t51) {
  t52 = ((int64_t)(v10));
  t53 = fg14_s_t_nrows_at(t52);
  t54 = ((int64_t)(t53));
  t55 = ((int64_t)(((int64_t)UINT64_C(1))));
  t51 = ((t54) <= (t55));
  }
  t56 = t51;
  if (t56) {
  {
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t57 = ((int64_t)(v12));
  t58 = ((int64_t)(((int64_t)UINT64_C(10))));
  if (!(((t57) < (t58)))) goto label5;
  {
  t59 = ((int64_t)(v10));
  t60 = ((int64_t)(v12));
  t61 = fg14_s_t_src_at(t59, t60);
  v13 = ((int64_t)(t61));
  t62 = ((int64_t)(v13));
  t63 = ((int64_t)(((int64_t)UINT64_C(0))));
  t64 = ((t62) != (t63));
  if (t64) {
  {
  t65 = ((int64_t)(v2));
  t66 = ((int64_t)(v13));
  t67 = fg14_s_map_get(t65, t66);
  v14 = ((int64_t)(t67));
  t68 = ((int64_t)(v14));
  t69 = ((int64_t)(((int64_t)UINT64_C(0))));
  t70 = ((t68) < (t69));
  if (t70) {
  {
  v14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t71 = ((int64_t)(v4));
  t72 = ((int64_t)(v13));
  t73 = gg_function_26(t71, t72, base + 0);
  (void)(t73);
  }
  }
  t74 = ((int64_t)(v2));
  t75 = ((int64_t)(v13));
  t76 = ((int64_t)(v14));
  t77 = ((int64_t)(((int64_t)UINT64_C(1))));
  t78 = ((int64_t)(((int64_t)((uint64_t)(t76) + (uint64_t)(t77)))));
  t79 = fg14_s_map_put(t74, t75, t78);
  (void)(t79);
  }
  }
  t80 = v12;
  t81 = ((int64_t)(((int64_t)UINT64_C(1))));
  v12 = ((int64_t)((uint64_t)(t80) + (uint64_t)(t81)));
  }
  goto label4;
label5:;
  }
  }
  t82 = v9;
  t83 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)((uint64_t)(t82) + (uint64_t)(t83)));
  }
  goto label1;
label2:;
  t84 = ((int64_t)(v4));
  t85 = fg14_s_vec_len(t84);
  v15 = ((int64_t)(t85));
  v16 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t86 = ((int64_t)(v16));
  t87 = ((int64_t)(v15));
  if (!(((t86) < (t87)))) goto label8;
  {
  t88 = ((int64_t)(v4));
  t89 = ((int64_t)(v16));
  v17 = gg_function_32(t88, t89, base + 0);
  t90 = ((int64_t)(v2));
  t91 = ((int64_t)(v17));
  t92 = fg14_s_map_get(t90, t91);
  v18 = ((int64_t)(t92));
  t93 = ((int64_t)(v18));
  t94 = ((int64_t)(((int64_t)UINT64_C(3))));
  t95 = ((t93) >= (t94));
  t96 = t95;
  if (t96) {
  t97 = ((int64_t)(v18));
  t98 = ((int64_t)(((int64_t)UINT64_C(3))));
  t96 = ((t97) <= (t98));
  }
  t99 = t96;
  t100 = t99;
  if (t100) {
  t101 = ((int64_t)(v17));
  t102 = fg14_s_t_name_attn(t101);
  t103 = ((int64_t)(t102));
  t104 = ((int64_t)(((int64_t)UINT64_C(1))));
  t100 = ((t103) == (t104));
  }
  t105 = t100;
  if (t105) {
  {
  t106 = ((int64_t)(v3));
  t107 = ((int64_t)(v17));
  t108 = fg14_s_map_get(t106, t107);
  v19 = ((int64_t)(t108));
  t109 = ((int64_t)(v19));
  t110 = ((int64_t)(((int64_t)UINT64_C(0))));
  t111 = ((t109) >= (t110));
  t112 = t111;
  if (t112) {
  t113 = ((int64_t)(v5));
  t114 = ((int64_t)(v6));
  t115 = ((int64_t)(v19));
  t116 = gg_function_35(t113, t114, t115, base + 0);
  t117 = t116;
  t112 = (!(t117));
  }
  t118 = t112;
  if (t118) {
  {
  t119 = ((int64_t)(v0));
  t120 = ((int64_t)(v1));
  t121 = ((int64_t)(v17));
  t122 = ((int64_t)(v19));
  t123 = ((int64_t)(v18));
  t124 = ((int64_t)(v3));
  t125 = ((int64_t)(v2));
  t126 = ((int64_t)(v5));
  t127 = ((int64_t)(v6));
  t128 = ((int64_t)(v7));
  t129 = gg_function_36(t119, t120, t121, t122, t123, t124, t125, t126, t127, t128, base + 0);
  (void)(t129);
  }
  }
  }
  }
  t130 = v16;
  t131 = ((int64_t)(((int64_t)UINT64_C(1))));
  v16 = ((int64_t)((uint64_t)(t130) + (uint64_t)(t131)));
  }
  goto label7;
label8:;
  t132 = ((int64_t)(v2));
  t133 = fg14_s_map_free(t132);
  v20 = ((int64_t)(t133));
  t134 = ((int64_t)(v3));
  t135 = fg14_s_map_free(t134);
  v21 = ((int64_t)(t135));
  t136 = ((int64_t)(v4));
  t137 = fg14_s_vec_free(t136);
  v22 = ((int64_t)(t137));
  t138 = ((int64_t)(v5));
  t139 = fg14_s_vec_free(t138);
  v23 = ((int64_t)(t139));
  t140 = ((int64_t)(v6));
  t141 = fg14_s_vec_free(t140);
  v24 = ((int64_t)(t141));
  t142 = ((int64_t)(v7));
  t143 = fg14_s_vec_len(t142);
  v25 = ((int64_t)(t143));
  v26 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t144 = ((int64_t)(v26));
  t145 = ((int64_t)(v25));
  if (!(((t144) < (t145)))) goto label11;
  {
  t146 = ((int64_t)(v7));
  t147 = ((int64_t)(v26));
  t148 = gg_function_32(t146, t147, base + 0);
  t149 = fg14_s_vec_free(t148);
  v27 = ((int64_t)(t149));
  t150 = v26;
  t151 = ((int64_t)(((int64_t)UINT64_C(1))));
  v26 = ((int64_t)((uint64_t)(t150) + (uint64_t)(t151)));
  }
  goto label10;
label11:;
  t152 = ((int64_t)(v7));
  t153 = fg14_s_vec_free(t152);
  v28 = ((int64_t)(t153));
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_7(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t39;
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
  t2 = fg14_s_g_n(t1);
  v2 = ((int64_t)(t2));
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(((int64_t)UINT64_C(5))));
  t5 = ((int64_t)(((int64_t)((uint64_t)(t3) + (uint64_t)(t4)))));
  t6 = ((int64_t)(v2));
  if (!(((t5) < (t6)))) goto label2;
  {
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(v3));
  t9 = fg14_s_g_node(t7, t8);
  t10 = ((int64_t)(t9));
  t11 = fg14_s_t_op(t10);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(((int64_t)UINT64_C(30))));
  t14 = ((t12) == (t13));
  if (t14) {
  {
  t15 = ((int64_t)(v3));
  t16 = ((int64_t)(((int64_t)UINT64_C(3))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t15) + (uint64_t)(t16)))));
label4:;
  t17 = ((int64_t)(v4));
  t18 = ((int64_t)(((int64_t)UINT64_C(2))));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t18)))));
  t20 = ((int64_t)(v2));
  if (!(((t19) < (t20)))) goto label5;
  {
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(v4));
  t23 = fg14_s_g_node(t21, t22);
  t24 = ((int64_t)(t23));
  t25 = fg14_s_t_op(t24);
  t26 = ((int64_t)(t25));
  t27 = ((int64_t)(((int64_t)UINT64_C(30))));
  t28 = ((t26) == (t27));
  t29 = t28;
  if (t29) {
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(v4));
  t32 = ((int64_t)(((int64_t)UINT64_C(1))));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t32)))));
  t34 = fg14_s_g_node(t30, t33);
  t35 = ((int64_t)(t34));
  t36 = fg14_s_t_op(t35);
  t37 = ((int64_t)(t36));
  t38 = ((int64_t)(((int64_t)UINT64_C(30))));
  t29 = ((t37) == (t38));
  }
  t39 = t29;
  if (t39) {
  {
  goto label5;
  }
  }
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(v4));
  t42 = fg14_s_g_node(t40, t41);
  t43 = ((int64_t)(t42));
  t44 = fg14_s_t_op(t43);
  t45 = ((int64_t)(t44));
  t46 = ((int64_t)(((int64_t)UINT64_C(29))));
  t47 = ((t45) == (t46));
  t48 = t47;
  if (t48) {
  t49 = ((int64_t)(v0));
  t50 = ((int64_t)(v3));
  t51 = ((int64_t)(v4));
  t52 = fg14_match_shared(t49, t50, t51);
  t53 = ((int64_t)(t52));
  t54 = ((int64_t)(((int64_t)UINT64_C(1))));
  t48 = ((t53) == (t54));
  }
  t55 = t48;
  if (t55) {
  {
  t56 = ((int64_t)(v0));
  t57 = ((int64_t)(v3));
  t58 = ((int64_t)(((int64_t)UINT64_C(3))));
  t59 = ((int64_t)(((int64_t)((uint64_t)(t57) + (uint64_t)(t58)))));
  t60 = ((int64_t)(v4));
  t61 = ((int64_t)(v4));
  t62 = ((int64_t)(((int64_t)UINT64_C(3))));
  t63 = ((int64_t)(((int64_t)((uint64_t)(t61) + (uint64_t)(t62)))));
  t64 = fg14_s_g_rotate3(t56, t59, t60, t63);
  (void)(t64);
  t65 = ((int64_t)(v0));
  t66 = ((int64_t)(v3));
  t67 = ((int64_t)(((int64_t)UINT64_C(2))));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t66) + (uint64_t)(t67)))));
  t69 = fg14_s_g_node(t65, t68);
  t70 = ((int64_t)(t69));
  t71 = ((int64_t)(((int64_t)UINT64_C(1))));
  t72 = fg14_s_t_src_at(t70, t71);
  v5 = ((int64_t)(t72));
  t73 = ((int64_t)(v1));
  t74 = ((int64_t)(v5));
  t75 = ((int64_t)(((int64_t)UINT64_C(1))));
  t76 = fg14_s_t_src_at(t74, t75);
  t77 = ((int64_t)(t76));
  t78 = ((int64_t)(v0));
  t79 = ((int64_t)(v3));
  t80 = ((int64_t)(((int64_t)UINT64_C(5))));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t79) + (uint64_t)(t80)))));
  t82 = fg14_s_g_node(t78, t81);
  t83 = ((int64_t)(t82));
  t84 = fg14_s_add_dep(t73, t77, t83);
  (void)(t84);
  t85 = ((int64_t)(v1));
  t86 = ((int64_t)(v5));
  t87 = ((int64_t)(((int64_t)UINT64_C(2))));
  t88 = fg14_s_t_src_at(t86, t87);
  t89 = ((int64_t)(t88));
  t90 = ((int64_t)(v0));
  t91 = ((int64_t)(v3));
  t92 = ((int64_t)(((int64_t)UINT64_C(5))));
  t93 = ((int64_t)(((int64_t)((uint64_t)(t91) + (uint64_t)(t92)))));
  t94 = fg14_s_g_node(t90, t93);
  t95 = ((int64_t)(t94));
  t96 = fg14_s_add_dep(t85, t89, t95);
  (void)(t96);
  t97 = v3;
  t98 = ((int64_t)(((int64_t)UINT64_C(5))));
  v3 = ((int64_t)((uint64_t)(t97) + (uint64_t)(t98)));
  goto label5;
  }
  }
  t99 = v4;
  t100 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t99) + (uint64_t)(t100)));
  }
  goto label4;
label5:;
  }
  }
  t101 = v3;
  t102 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t101) + (uint64_t)(t102)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_2(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  uint8_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
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
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v0));
  t3 = fg14_s_ctx_device(t2);
  t4 = ((int64_t)(t3));
  t5 = fg14_res_ctx_attach(t1, t4);
  v3 = ((int64_t)(t5));
  t6 = ((int64_t)(v0));
  t7 = fg14_s_ctx_device(t6);
  v4 = ((int64_t)(t7));
  t8 = fg14_s_fusion_disabled();
  t9 = ((int64_t)(t8));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) == (t10));
  if (t11) {
  {
  t12 = ((int64_t)(v4));
  t13 = fg14_s_device_set(t12);
  v5 = ((int64_t)(t13));
  t14 = ((int64_t)(v1));
  t15 = ((int64_t)(v2));
  t16 = gg_function_7(t14, t15, base + 0);
  (void)(t16);
  }
  }
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = fg14_s_have_cuda_graph();
  t18 = ((int64_t)(t17));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((t18) != (t19));
  t21 = t20;
  if (t21) {
  t22 = fg14_s_graph_opt_env();
  t23 = ((int64_t)(t22));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((t23) != (t24));
  }
  t25 = t21;
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)(v4));
  t28 = fg14_s_info_cc(t27);
  t29 = ((int64_t)(t28));
  t30 = ((int64_t)(((int64_t)UINT64_C(700))));
  t26 = ((t29) >= (t30));
  }
  t31 = t26;
  if (t31) {
  {
  v6 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t32 = ((int64_t)(v3));
  t33 = fg14_res_stream_ctx_reset(t32);
  v7 = ((int64_t)(t33));
  t34 = ((int64_t)(v6));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((t34) == (t35));
  if (t36) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t37 = ((int64_t)(v4));
  t38 = fg14_s_device_set(t37);
  v8 = ((int64_t)(t38));
  t39 = ((int64_t)(v3));
  t40 = ((int64_t)(v1));
  t0 = gg_function_19(t39, t40, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
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
  v5 = p5;
  v6 = p6;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(v3));
  t0 = gg_function_2(t4, t5, t6, base + 0);
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
  gg_header t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  {
  t0.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t0.len = 0;
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = gg_function_1(t1, t2, t3, t4, t5, t6, t7, base + 1);
  gg_integer(gg_ref(&t0, gg_get_stack(base + 0)), t8);
  gg_append(gg_ref(&t0, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t0).base, (t0).len));
  }
gg_done:;
  for (t9 = 0; t9 < 1; t9++) gg_stacks[base + t9].top = 0;
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
