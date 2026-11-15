// Differential gate for the CPU MoE block switch (P1-12 integration).
//
// Measures the shipped bridge (integration/p1-12/src/fg-goose-cpu-simd.cpp), not
// a copy: with the switch active a call must go to the Goose kernel and equal the
// native kernel bit for bit; with FG_GOOSE_CPU_SIMD_NATIVE=1 the same call must
// take the native arm; and a multi-block row must never be routed to the
// one-block Goose entry point.
//
// Output: FG665 mode=<goose|native> cases=N failures=K goose_calls=C
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fg-goose-cpu-simd.h"

typedef struct { uint16_t d; uint16_t dmin; uint8_t scales[12]; uint8_t qs[128]; } block_q4_K;
typedef struct { uint8_t ql[128]; uint8_t qh[64]; int8_t scales[16]; uint16_t d; } block_q6_K;
typedef struct { uint16_t d; int8_t qs[32]; } block_q8_0;
typedef struct { uint16_t d; uint8_t qs[16]; } block_q4_0;
typedef struct { float d; int8_t qs[256]; int16_t bsums[16]; } block_q8_K;

extern "C" {
void quantize_row_q4_K_ref(const float * x, void * y, int64_t k);
void quantize_row_q6_K_ref(const float * x, void * y, int64_t k);
void quantize_row_q8_0_ref(const float * x, void * y, int64_t k);
void quantize_row_q4_0_ref(const float * x, void * y, int64_t k);
void quantize_row_q8_K_ref(const float * x, void * y, int64_t k);
void ggml_vec_dot_q4_K_q8_K_generic(int n, float * s, size_t bs, const void * vx, size_t bx, const void * vy, size_t by, int nrc);
void ggml_vec_dot_q6_K_q8_K_generic(int n, float * s, size_t bs, const void * vx, size_t bx, const void * vy, size_t by, int nrc);
void ggml_vec_dot_q8_0_q8_0_generic(int n, float * s, size_t bs, const void * vx, size_t bx, const void * vy, size_t by, int nrc);
void ggml_vec_dot_q4_0_q8_0_generic(int n, float * s, size_t bs, const void * vx, size_t bx, const void * vy, size_t by, int nrc);
}

static int cases;
static int failures;
static uint32_t rng_state;

static uint32_t rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static void expect(const char * name, int iter, float got, float want) {
    ++cases;
    uint32_t g, w;
    memcpy(&g, &got, 4);
    memcpy(&w, &want, 4);
    if (g != w) {
        if (failures < 12) {
            printf("FAIL %s iter=%d bridge=%.9g native=%.9g (bits %08x/%08x)\n",
                   name, iter, got, want, g, w);
        }
        ++failures;
    }
}

int main(int argc, char ** argv) {
    const int iters = argc > 1 ? atoi(argv[1]) : 256;
    const int active = fg_goose_cpu_simd_active();
    const int available = fg_goose_cpu_simd_available();
    const long calls_before = fg_goose_cpu_simd_calls();
    float src[256];

    block_q4_K a4k;
    block_q6_K a6k;
    block_q8_0 a80, b80;
    block_q4_0 a40;
    block_q8_K b8k;

    for (int it = 0; it < iters; ++it) {
        rng_state = 0x9e3779b9u + (uint32_t) it * 2654435761u;
        for (int i = 0; i < 256; ++i) {
            src[i] = (float) ((int32_t) (rng_next() % 4096) - 2048) / 1024.0f;
        }

        quantize_row_q8_K_ref(src, &b8k, 256);
        quantize_row_q8_0_ref(src, &b80, 256);

        quantize_row_q4_K_ref(src, &a4k, 256);
        float got = 0.0f, want = 0.0f;
        ggml_vec_dot_q4_K_q8_K_generic(256, &want, 0, &a4k, 0, &b8k, 0, 1);
        if (fg_goose_dot_q4_k_q8_k(1, &a4k, &b8k, &got)) {
            expect("q4_K x q8_K", it, got, want);
        } else if (active && available) {
            ++cases; ++failures;
            if (failures <= 12) printf("FAIL q4_K x q8_K iter=%d: switch did not route the call\n", it);
        }

        quantize_row_q6_K_ref(src, &a6k, 256);
        got = 0.0f; want = 0.0f;
        ggml_vec_dot_q6_K_q8_K_generic(256, &want, 0, &a6k, 0, &b8k, 0, 1);
        if (fg_goose_dot_q6_k_q8_k(1, &a6k, &b8k, &got)) {
            expect("q6_K x q8_K", it, got, want);
        } else if (active && available) {
            ++cases; ++failures;
            if (failures <= 12) printf("FAIL q6_K x q8_K iter=%d: switch did not route the call\n", it);
        }

        quantize_row_q8_0_ref(src, &a80, 256);
        got = 0.0f; want = 0.0f;
        ggml_vec_dot_q8_0_q8_0_generic(32, &want, 0, &a80, 0, &b80, 0, 1);
        if (fg_goose_dot_q8_0_q8_0(1, &a80, &b80, &got)) {
            expect("q8_0 x q8_0", it, got, want);
        } else if (active && available) {
            ++cases; ++failures;
            if (failures <= 12) printf("FAIL q8_0 x q8_0 iter=%d: switch did not route the call\n", it);
        }

        quantize_row_q4_0_ref(src, &a40, 256);
        got = 0.0f; want = 0.0f;
        ggml_vec_dot_q4_0_q8_0_generic(32, &want, 0, &a40, 0, &b80, 0, 1);
        if (fg_goose_dot_q4_0_q8_0(1, &a40, &b80, &got)) {
            expect("q4_0 x q8_0", it, got, want);
        } else if (active && available) {
            ++cases; ++failures;
            if (failures <= 12) printf("FAIL q4_0 x q8_0 iter=%d: switch did not route the call\n", it);
        }
    }

    // A multi-block row must never reach the one-block entry point: dropping the
    // tail would be a silent wrong answer, so the bridge has to refuse it.
    {
        static block_q4_K rows[4];
        static block_q8_K acts[4];
        memset(rows, 0, sizeof rows);
        memset(acts, 0, sizeof acts);
        float out = 1.0f;
        const int routed = fg_goose_dot_q4_k_q8_k(4, rows, acts, &out);
        ++cases;
        if (routed != 0) {
            ++failures;
            printf("FAIL multi-block row was routed to the one-block kernel\n");
        }
        if (out != 1.0f) {
            ++failures;
            printf("FAIL multi-block row wrote the output value\n");
        }
    }

    const long goose_calls = fg_goose_cpu_simd_calls() - calls_before;
    printf("FG665 mode=%s available=%d cases=%d failures=%d goose_calls=%ld\n",
           active ? "goose" : "native", available, cases, failures, goose_calls);
    return failures == 0 ? 0 : 1;
}
