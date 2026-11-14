// NULL-arm regression for P7-04 S4.
//
// The weighted source carries the S3b unweighted row quantizers verbatim, and
// S4 also had to add three type ids (q4_0/q4_1/q5_0/q5_1) that S3b refused.
// This harness runs the S3b corpus through qz_quantize_chunk(..., NULL) of two
// independently compiled ports -- the current S4 port and the accepted S3b
// source -- and prints a SHA-256 over every destination byte and byte count.
// The two digests must be equal: the S3b behaviour is unchanged.
//
// It is built twice by tests/build.sh with -DRT_H=<generated header>; nothing
// here depends on either source beyond the S3b entry points.
//
// Corpus: 16 S3b types x 9 block-multiple lengths x 17 value patterns x the
// S3b geometry set (1-4 rows, start offsets), plus the q8_K row entry. The
// value patterns and the seed formula are the S3b fixture's, so the compared
// bytes are the bytes the S3b gate accepted.

#include "runtime-gen.h"

// The frozen EXP611 snapshot exports qz_*; this port exports qw_*.
#ifdef S3B_PORT
#define QZ_CHUNK qz_quantize_chunk
#define QZ_ROW   qz_quantize_row
#else
#define QZ_CHUNK qw_quantize_chunk
#define QZ_ROW   qw_quantize_row
#endif
#include "ggml.h"

#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// ---- the port's extern primitives (same set as the weighted fixture) ------

static uint8_t g_scratch[8192] __attribute__((aligned(16)));

extern "C" {

float qz_ld_f32(uint64_t a) { float v; memcpy(&v, (const void *) (uintptr_t) a, 4); return v; }
void qz_st_f32(uint64_t a, float v) { memcpy((void *) (uintptr_t) a, &v, 4); }
float qz_ld_f16(uint64_t a) { ggml_fp16_t h; memcpy(&h, (const void *) (uintptr_t) a, 2); return ggml_fp16_to_fp32(h); }
void qz_st_f16(uint64_t a, float v) { ggml_fp16_t h = ggml_fp32_to_fp16(v); memcpy((void *) (uintptr_t) a, &h, 2); }
void qz_st_u8(uint64_t a, int64_t v) { uint8_t b = (uint8_t) v; memcpy((void *) (uintptr_t) a, &b, 1); }
int64_t qz_ld_u8(uint64_t a) { uint8_t b; memcpy(&b, (const void *) (uintptr_t) a, 1); return b; }
void qz_st_u16(uint64_t a, int64_t v) { uint8_t b[2] = { (uint8_t) v, (uint8_t) (v >> 8) }; memcpy((void *) (uintptr_t) a, b, 2); }
void qz_st_u32(uint64_t a, int64_t v) { uint8_t b[4] = { (uint8_t) v, (uint8_t) (v >> 8), (uint8_t) (v >> 16), (uint8_t) (v >> 24) }; memcpy((void *) (uintptr_t) a, b, 4); }
float qz_abs(float v) { return fabsf(v); }
float qz_sqrtf(float v) { return sqrtf(v); }
float qz_log2f(float v) { return log2f(v); }
float qz_floorf(float v) { return floorf(v); }
int64_t qz_lroundf(float v) { return (int64_t) lroundf(v); }
float qz_ldexpf(float v, int64_t e) { return ldexpf(v, (int) e); }
int64_t qz_bits(float v) { uint32_t b; memcpy(&b, &v, 4); return (int64_t) b; }
float qz_of_bits(int64_t b) { uint32_t u = (uint32_t) b; float v; memcpy(&v, &u, 4); return v; }
int64_t qz_nearest(float fval) {
    float val = fval + 12582912.0f;
    int32_t i;
    memcpy(&i, &val, 4);
    return (int64_t) ((i & 0x007fffff) - 0x00400000);
}
uint64_t qz_tmp(int64_t off) { return (uint64_t) (uintptr_t) (g_scratch + off); }
void qz_fail(int64_t code) { fprintf(stderr, "port failure %lld\n", (long long) code); abort(); }

static int iq1_cmp(const void * l, const void * r) {
    const float * a = (const float *) l;
    const float * b = (const float *) r;
    return *a < *b ? -1 : *a > *b ? 1 : 0;
}
void qz_sort16(uint64_t pairs, int64_t n) {
    qsort((void *) (uintptr_t) pairs, (size_t) n, 2 * sizeof(float), iq1_cmp);
}

// The IQ tables come from build/native-tables.c (the native constructors), as
// in the weighted fixture; the port sees them through its qz_iq* externs.
void tbl_init_iq2(int type);
void tbl_init_iq3(int grid_size);
uint64_t tbl_iq2_grid(int g, int i);
int tbl_iq2_map(int g, int u);
int tbl_iq2_nb(int g, int off);
uint64_t tbl_iq3_grid(int g, int i);
int tbl_iq3_map(int g, int u);
int tbl_iq3_nb(int g, int off);

int64_t qz_iq2_grid(int64_t g, int64_t i) { return (int64_t) tbl_iq2_grid((int) g, (int) i); }
int64_t qz_iq2_map(int64_t g, int64_t u) { return tbl_iq2_map((int) g, (int) u); }
int64_t qz_iq2_nb(int64_t g, int64_t off) { return tbl_iq2_nb((int) g, (int) off); }
int64_t qz_iq3_grid(int64_t g, int64_t i) { return (int64_t) tbl_iq3_grid((int) g, (int) i); }
int64_t qz_iq3_map(int64_t g, int64_t u) { return tbl_iq3_map((int) g, (int) u); }
int64_t qz_iq3_nb(int64_t g, int64_t off) { return tbl_iq3_nb((int) g, (int) off); }

} // extern "C"

// ---- SHA-256 --------------------------------------------------------------

struct Sha256 {
    uint32_t h[8];
    uint64_t len;
    uint8_t  buf[64];
    size_t   fill;

    Sha256() {
        static const uint32_t iv[8] = {
            0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
            0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
        };
        memcpy(h, iv, sizeof(h));
        len = 0;
        fill = 0;
    }

    static uint32_t ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

    void block(const uint8_t * p) {
        static const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
        };
        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            w[i] = ((uint32_t) p[4*i] << 24) | ((uint32_t) p[4*i+1] << 16) |
                   ((uint32_t) p[4*i+2] << 8) | (uint32_t) p[4*i+3];
        }
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = ror(w[i-15], 7) ^ ror(w[i-15], 18) ^ (w[i-15] >> 3);
            uint32_t s1 = ror(w[i-2], 17) ^ ror(w[i-2], 19) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t s1 = ror(e, 6) ^ ror(e, 11) ^ ror(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t t1 = hh + s1 + ch + k[i] + w[i];
            uint32_t s0 = ror(a, 2) ^ ror(a, 13) ^ ror(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = s0 + maj;
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    void update(const void * data, size_t n) {
        const uint8_t * p = (const uint8_t *) data;
        len += n;
        while (n) {
            size_t take = 64 - fill;
            if (take > n) take = n;
            memcpy(buf + fill, p, take);
            fill += take; p += take; n -= take;
            if (fill == 64) { block(buf); fill = 0; }
        }
    }

    std::string hex() {
        uint64_t bits = len * 8;
        uint8_t pad = 0x80;
        update(&pad, 1);
        uint8_t zero = 0;
        while (fill != 56) update(&zero, 1);
        uint8_t tail[8];
        for (int i = 0; i < 8; i++) tail[i] = (uint8_t) (bits >> (56 - 8*i));
        update(tail, 8);
        char out[65];
        for (int i = 0; i < 8; i++) snprintf(out + 8*i, 9, "%08x", h[i]);
        return std::string(out, 64);
    }
};

// ---- S3b corpus -----------------------------------------------------------

static uint64_t sm64(uint64_t & s) {
    s += UINT64_C(0x9E3779B97F4A7C15);
    uint64_t z = s;
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

static float frand01(uint64_t & s) {
    return (float) ((sm64(s) >> 40)) * (1.0f / 16777216.0f);
}

static float f32_from_bits(uint32_t bits) {
    float f;
    memcpy(&f, &bits, sizeof(f));
    return f;
}

static const int PATTERN_COUNT = 17;

static bool pattern_supported(ggml_type tt, int pp) {
    if (tt == GGML_TYPE_IQ3_XXS || tt == GGML_TYPE_IQ3_S) {
        return !(pp == 2 || pp == 12);
    }
    if (tt == GGML_TYPE_IQ2_S || tt == GGML_TYPE_IQ1_M) {
        return pp != 12;
    }
    return true;
}

static float pattern_value(int p, uint64_t & s, int64_t j, int64_t n) {
    switch (p) {
        case 0:  return 0.0f;
        case 1:  return -0.0f;
        case 2:  return (j & 1) ? -FLT_MAX : FLT_MAX;
        case 3:  return f32_from_bits((j & 1) ? UINT32_C(0x80000001) : UINT32_C(0x00000001));
        case 4:  return frand01(s) * 2.0f - 1.0f;
        case 5:  return frand01(s) * 20.0f - 10.0f;
        case 6:  return (float) (j % 17 - 8) * 0.125f;
        case 7:  return (float) ((int) (j % 13) - 6) * 0.5f;
        case 8:  if (j == 0) return 1.0f;
                 if (j == 1) return -1.0f;
                 return (float) (j % 7) * 0.001f;
        case 9:  return j == 3 ? 1.0f : 0.0f;
        case 10: return 0.75f;
        case 11: return (float) (j - n / 2) / (float) n;
        case 12: return (j & 1) ? 1.0e30f : 1.0e-30f;
        case 13: if (j == 0) return 254.0f;
                 if (j == 1) return -254.0f;
                 {
                     int v = (int) (j % 253) + 1;
                     return (float) ((j & 2) ? -v : v);
                 }
        case 14: {
                     uint32_t bits = UINT32_C(0x00000001) + (uint32_t) (sm64(s) % UINT32_C(0x7FFFFF));
                     float f = f32_from_bits(bits);
                     return (j & 1) ? -f : f;
                 }
        case 15: return -(0.5f + frand01(s) * 1.5f);
        case 16: {
                     const int e = (int) (sm64(s) % 61) - 30;
                     const int k = e < 0 ? -e : e;
                     float scale = 1.0f;
                     for (int i = 0; i < k; i++) {
                         scale = scale * 2.0f;
                     }
                     if (e < 0) {
                         scale = 1.0f / scale;
                     }
                     const float v = scale * (0.5f + frand01(s));
                     return (j & 1) ? -v : v;
                 }
        default: return 0.0f;
    }
}

struct TypeRow {
    ggml_type   t;
    const char * name;
};

static const TypeRow TYPES[] = {
    { GGML_TYPE_Q2_K,    "q2_K"    },
    { GGML_TYPE_Q3_K,    "q3_K"    },
    { GGML_TYPE_Q4_K,    "q4_K"    },
    { GGML_TYPE_Q5_K,    "q5_K"    },
    { GGML_TYPE_Q6_K,    "q6_K"    },
    { GGML_TYPE_Q8_K,    "q8_K"    },
    { GGML_TYPE_TQ1_0,   "tq1_0"   },
    { GGML_TYPE_TQ2_0,   "tq2_0"   },
    { GGML_TYPE_MXFP4,   "mxfp4"   },
    { GGML_TYPE_NVFP4,   "nvfp4"   },
    { GGML_TYPE_IQ4_NL,  "iq4_nl"  },
    { GGML_TYPE_IQ4_XS,  "iq4_xs"  },
    { GGML_TYPE_IQ3_XXS, "iq3_xxs" },
    { GGML_TYPE_IQ3_S,   "iq3_s"   },
    { GGML_TYPE_IQ2_S,   "iq2_s"   },
    { GGML_TYPE_IQ1_M,   "iq1_m"   },
};

static const int64_t LENGTHS[] = { 32, 64, 96, 128, 160, 256, 384, 512, 1024 };

static Sha256 g_sha;
static int    g_cases = 0;
static long long g_bytes = 0;

static void run_case(ggml_type t, int p, int64_t nper, int64_t nrows, int64_t start_rows) {
    const int64_t blck = ggml_blck_size(t);
    if (nper % blck != 0) {
        return;
    }
    const int64_t row_size  = ggml_row_size(t, nper);
    const int64_t start     = start_rows * nper;
    const int64_t n_src     = start + nrows * nper;
    const int64_t dst_rows  = start_rows + nrows;
    const int64_t dst_bytes = dst_rows * row_size;

    std::vector<float>   src((size_t) n_src);
    uint64_t seed = UINT64_C(0x1234567890ABCDEF) ^ (uint64_t) t * 1315423911u
                    ^ (uint64_t) p * 2654435761u ^ (uint64_t) nper * 97u
                    ^ (uint64_t) nrows * 13u ^ (uint64_t) start_rows * 7u;
    for (int64_t j = 0; j < n_src; j++) {
        src[(size_t) j] = pattern_value(p, seed, j, nper);
    }

    std::vector<uint8_t> dst((size_t) dst_bytes, 0xA5);
    int64_t r = 0;
    if (t == GGML_TYPE_Q8_K) {
        if (start_rows != 0) {
            return;
        }
        r = QZ_ROW((int64_t) t, (uint64_t) (uintptr_t) src.data(),
                            (uint64_t) (uintptr_t) dst.data(), nrows * nper);
    } else {
        r = QZ_CHUNK((int64_t) t, (uint64_t) (uintptr_t) src.data(),
                              (uint64_t) (uintptr_t) dst.data(), start, nrows, nper);
    }
    g_sha.update(&r, sizeof(r));
    g_sha.update(dst.data(), dst.size());
    ++g_cases;
    g_bytes += (long long) dst.size();
}

int main(int argc, char ** argv) {
    goose_init(argc, argv);
    tbl_init_iq2(GGML_TYPE_IQ1_M);
    tbl_init_iq2(GGML_TYPE_IQ2_S);
    tbl_init_iq3(256);
    tbl_init_iq3(512);
    const bool quick = (argc > 1 && strcmp(argv[1], "--quick") == 0);
    const int passes = quick ? 1 : 2;

    for (int pass = 0; pass < passes; pass++) {
        for (const TypeRow & tr : TYPES) {
            const int64_t blck = ggml_blck_size(tr.t);
            for (int64_t nper : LENGTHS) {
                if (nper % blck != 0) {
                    continue;
                }
                if (pass == 1 && nper != 256 && nper != 512) {
                    continue;
                }
                for (int p = 0; p < PATTERN_COUNT; p++) {
                    if (!pattern_supported(tr.t, p)) {
                        continue;
                    }
                    run_case(tr.t, p, nper, 1, 0);
                    run_case(tr.t, p, nper, 2, 0);
                    run_case(tr.t, p, nper, 1, 2);
                    if (pass == 0 && (nper == 256 || nper == 512)) {
                        run_case(tr.t, p, nper, 4, 1);
                    }
                }
            }
        }
    }

    printf("%s cases=%d bytes=%lld\n", g_sha.hex().c_str(), g_cases, g_bytes);
    return 0;
}
