// Differential fixture for the Goose Q8_K quantizer against the native reference.
//
// The gate is byte-for-byte equality of the whole block_q8_K array: the native code
// writes d, the 256 int8 quants and the 16 int16 group sums, and any deviation in the
// rounding, the -127 scale or the group sums shows up immediately.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#define GGML_COMMON_DECL_C
#include "ggml-common.h"

// Native reference from ggml-quants.c (linked in from libggml-base.so).
void quantize_row_q8_K_ref(const float * x, block_q8_K * y, int64_t k);

// Goose entry, generated into the same translation unit.
uint8_t fg190q_enter(int64_t k, const float * src, block_q8_K * dst);

static int checks, failures;
static void check(int ok, const char * what) {
    ++checks;
    if (!ok) { ++failures; fprintf(stderr, "FAIL %s\n", what); }
}

static uint32_t rng_state = 190001u;
static float frand(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return ((float)(rng_state >> 8) / 16777216.0f) * 4.0f - 2.0f;
}

int main(int argc, char ** argv) {
    (void) argc; (void) argv;
    const int64_t sizes[] = { 256, 512, 2560, 4096 };
    for (unsigned s = 0; s < sizeof(sizes)/sizeof(sizes[0]); ++s) {
        const int64_t k = sizes[s];
        const int64_t nb = k/256;
        float * x = malloc((size_t)k*sizeof(float));
        block_q8_K * a = calloc((size_t)nb, sizeof(block_q8_K));
        block_q8_K * b = calloc((size_t)nb, sizeof(block_q8_K));
        if (!x || !a || !b) { fprintf(stderr, "oom\n"); return 2; }
        for (int64_t i = 0; i < k; ++i) x[i] = frand();
        // Include one all-zero super-block so the early-out path is exercised.
        if (k >= 512) for (int64_t i = 256; i < 512; ++i) x[i] = 0.0f;

        quantize_row_q8_K_ref(x, b, k);
        int ok = fg190q_enter(k, x, a);
        check(ok == 1, "entry returned success");

        int same = memcmp(a, b, (size_t)nb*sizeof(block_q8_K)) == 0;
        check(same, "bytes identical");
        if (!same) {
            for (int64_t i = 0; i < nb; ++i) {
                if (memcmp(&a[i], &b[i], sizeof(block_q8_K)) != 0) {
                    int dq = 0, db = 0;
                    for (int j = 0; j < 256; ++j) if (a[i].qs[j] != b[i].qs[j]) ++dq;
                    for (int j = 0; j < 16; ++j) if (a[i].bsums[j] != b[i].bsums[j]) ++db;
                    fprintf(stderr, "  block %lld: d goose=%.9g native=%.9g, qs diffs=%d, bsums diffs=%d\n",
                            (long long)i, (double)a[i].d, (double)b[i].d, dq, db);
                }
            }
        }
        free(x); free(a); free(b);
    }
    printf("FG190Q_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
