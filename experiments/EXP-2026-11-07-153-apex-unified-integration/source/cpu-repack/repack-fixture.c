// Native AVX2 vs Goose branch checks for the Q4_Kx8 repack provider.
#define _GNU_SOURCE
#define GGML_COMMON_DECL_CPP
#include "ggml.h"
#include "ggml-cpu.h"
#include "ggml-backend.h"
#include "ggml-cpu/repack.h"
#include "ggml-cpu/quants.h"
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
int fg006_repack_q4_Kx8(void *, const void *, int64_t, int64_t);
int fg006_quantize_mat_q8_K_4x8(const float *, void *, int64_t);
void fg006_gemv_q4_K_8x8_q8_K(int, float *, size_t, const void *, const void *, int, int);
void fg006_gemm_q4_K_8x8_q8_K(int, float *, size_t, const void *, const void *, int, int);
}

#define NB 2
#define N (NB * 256)
#define NROW 8
#define Q4SZ (NROW * NB * 144)
#define RPSZ (NB * 1152)
#define Q8CSZ (NB * 292)
#define Q8MSZ (NB * 1168)

static uint32_t rng = 20261108u;
static uint32_t next32(void) { rng = rng * 1664525u + 1013904223u; return rng; }
static float nextf(void) { return ((int32_t)(next32() >> 8) - 8388608) * (1.0f / 8388608.0f); }

static int bits_eq(float a, float b) {
    uint32_t x, y; memcpy(&x, &a, 4); memcpy(&y, &b, 4); return x == y;
}
static int cmp_floats(const char * tag, const float * a, const float * b, int n) {
    int bad = 0;
    for (int i = 0; i < n; ++i) if (!bits_eq(a[i], b[i])) {
        if (bad < 4) fprintf(stderr, "%s[%d] %a != %a\n", tag, i, a[i], b[i]);
        ++bad;
    }
    return bad;
}
static void write_file(const void * p, size_t n) {
    FILE * f = fopen("/work/cpu-final/repack-native.bin", "wb");
    if (!f || fwrite(p, 1, n, f) != n) { fprintf(stderr, "write expected failed\n"); exit(2); }
    fclose(f);
}
static int read_file(void * p, size_t n) {
    FILE * f = fopen("/work/cpu-final/repack-native.bin", "rb");
    if (!f || fread(p, 1, n, f) != n) return 0;
    fclose(f); return 1;
}

struct expected {
    float gemv[8];
    float gemm[4][8];
    uint8_t repack[RPSZ];
    uint8_t q8[Q8MSZ];
};

int main(int argc, char ** argv) {
    int bound = argc > 1 && strcmp(argv[1], "bound") == 0;
    ggml_cpu_init();
    float w[NROW][N];
    uint8_t raw[Q4SZ];
    for (int r = 0; r < NROW; ++r) {
        for (int i = 0; i < N; ++i) w[r][i] = nextf() * (1.0f + (i % 7));
        quantize_row_q4_K(w[r], raw + r * NB * 144, N);
    }

    uint8_t * native_rp = (uint8_t *) calloc(1, RPSZ);
    uint8_t * goose_rp = (uint8_t *) calloc(1, RPSZ);
    if (!native_rp || !goose_rp) return 2;
    if (fg006_repack_q4_Kx8(goose_rp, raw, NROW, NB) != 0) return 3;

    struct ggml_init_params ip = { 64u * 1024u * 1024u, NULL, false };
    struct ggml_context * ctx = ggml_init(ip);
    struct ggml_tensor * t = ggml_new_tensor_2d(ctx, GGML_TYPE_Q4_K, N, NROW);
    ggml_backend_buffer_type_t buft = ggml_backend_cpu_repack_buffer_type();
    if (!buft) { fprintf(stderr, "repack buft unavailable\n"); return 4; }
    size_t asz = ggml_backend_buft_get_alloc_size(buft, t);
    ggml_backend_buffer_t buf = ggml_backend_buft_alloc_buffer(buft, asz);
    if (!buf) { fprintf(stderr, "repack buffer alloc failed\n"); return 4; }
    if (ggml_backend_buffer_init_tensor(buf, t) != GGML_STATUS_SUCCESS) return 4;
    t->buffer = buf;
    ggml_backend_tensor_set(t, raw, 0, sizeof(raw));
    memcpy(native_rp, t->data, RPSZ);

    int repack_bad = memcmp(native_rp, goose_rp, RPSZ) != 0;
    if (repack_bad) {
        int first = 0; while (first < RPSZ && native_rp[first] == goose_rp[first]) ++first;
        fprintf(stderr, "repack mismatch at %d native=%02x goose=%02x\n", first, native_rp[first], goose_rp[first]);
    }

    float x[4][N];
    for (int m = 0; m < 4; ++m) for (int i = 0; i < N; ++i) x[m][i] = nextf() * 3.0f;
    uint8_t nq[Q8MSZ], gq[Q8MSZ];
    memset(nq, 0, sizeof(nq)); memset(gq, 0, sizeof(gq));
    ggml_quantize_mat_q8_K_4x8(&x[0][0], nq, N);
    fg006_quantize_mat_q8_K_4x8(&x[0][0], gq, N);
    int q8_bad = memcmp(nq, gq, Q8MSZ) != 0;
    if (q8_bad) {
        int first = 0; while (first < Q8MSZ && nq[first] == gq[first]) ++first;
        fprintf(stderr, "q8_Kx4 mismatch at %d native=%02x goose=%02x\n", first, nq[first], gq[first]);
    }

    int bs_bad = 0;
    for (int m = 0; m < 4; ++m) for (int s = 0; s < 16; ++s) {
        int sum = 0;
        for (int u = 0; u < 16; ++u) {
            int p = s * 16 + u;
            sum += (int8_t) nq[16 + (p / 8) * 32 + m * 8 + (p % 8)];
        }
        int idx = m * 4 + (s / 4) * 16 + (s % 4);
        int stored = (int16_t) (nq[1040 + 2 * idx] | (nq[1041 + 2 * idx] << 8));
        if (sum != stored) {
            if (bs_bad < 4) fprintf(stderr, "bsums m%d s%d calc=%d stored=%d idx=%d\n", m, s, sum, stored, idx);
            ++bs_bad;
        }
    }

    float act[N];
    for (int i = 0; i < N; ++i) act[i] = nextf() * 3.0f;
    uint8_t aq[Q8CSZ];
    quantize_row_q8_K(act, aq, N);
    float ngemv[8], ggemv[8];
    memset(ngemv, 0, sizeof(ngemv)); memset(ggemv, 0, sizeof(ggemv));
    ggml_gemv_q4_K_8x8_q8_K(N, ngemv, 8, native_rp, aq, 1, 8);
    fg006_gemv_q4_K_8x8_q8_K(N, ggemv, 8, native_rp, aq, 1, 8);
    int gemv_bad = cmp_floats("gemv", ngemv, ggemv, 8);

    float ngemm[4][8], ggemm[4][8];
    memset(ngemm, 0, sizeof(ngemm)); memset(ggemm, 0, sizeof(ggemm));
    ggml_gemm_q4_K_8x8_q8_K(N, &ngemm[0][0], 8, native_rp, nq, 4, 8);
    fg006_gemm_q4_K_8x8_q8_K(N, &ggemm[0][0], 8, native_rp, nq, 4, 8);
    int gemm_bad = cmp_floats("gemm", &ngemm[0][0], &ggemm[0][0], 32);

    struct expected e;
    memset(&e, 0, sizeof(e));
    memcpy(e.gemv, ngemv, sizeof(e.gemv));
    memcpy(e.gemm, ngemm, sizeof(e.gemm));
    memcpy(e.repack, native_rp, RPSZ);
    memcpy(e.q8, nq, Q8MSZ);
    if (!bound) write_file(&e, sizeof(e));

    if (bound) {
        struct expected got;
        if (!read_file(&got, sizeof(got))) return 5;
        float bgemv[8], bgemm[4][8];
        uint8_t bq[Q8MSZ];
        ggml_quantize_mat_q8_K_4x8(&x[0][0], bq, N);
        ggml_gemv_q4_K_8x8_q8_K(N, bgemv, 8, native_rp, aq, 1, 8);
        ggml_gemm_q4_K_8x8_q8_K(N, &bgemm[0][0], 8, native_rp, bq, 4, 8);
        int bq8 = memcmp(bq, got.q8, Q8MSZ) != 0;
        int bg = cmp_floats("bound_gemv", got.gemv, bgemv, 8);
        int bm = cmp_floats("bound_gemm", &got.gemm[0][0], &bgemm[0][0], 32);
        uint64_t (*counts)(int) = (uint64_t (*)(int)) dlsym(RTLD_DEFAULT, "fg006_binding_count");
        if (!counts || counts(0) + counts(1) + counts(2) == 0) {
            fprintf(stderr, "bound mode: binding counters absent\n"); return 6;
        }
        if (repack_bad || bq8 || bg || bm) {
            fprintf(stderr, "FG006_REPACK_BOUND FAIL repack=%d q8=%d gemv=%d gemm=%d calls=%llu/%llu/%llu\n",
                    repack_bad, bq8, bg, bm, (unsigned long long) counts(0),
                    (unsigned long long) counts(1), (unsigned long long) counts(2));
            return 1;
        }
        fprintf(stderr, "FG006_REPACK_BOUND PASS calls=%llu/%llu/%llu\n",
                (unsigned long long) counts(0), (unsigned long long) counts(1),
                (unsigned long long) counts(2));
        return 0;
    }

    if (repack_bad || q8_bad || bs_bad || gemv_bad || gemm_bad) {
        fprintf(stderr, "FG006_REPACK FAIL repack=%d q8=%d bsums=%d gemv=%d gemm=%d\n",
                repack_bad, q8_bad, bs_bad, gemv_bad, gemm_bad);
        return 1;
    }
    fprintf(stderr, "FG006_REPACK PASS repack=%d q8=%d gemv=%d gemm=%d\n", RPSZ, Q8MSZ, 8, 32);
    return 0;
}
