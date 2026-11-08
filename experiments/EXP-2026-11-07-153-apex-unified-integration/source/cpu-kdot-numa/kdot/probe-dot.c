#define _POSIX_C_SOURCE 200112L
#include "ggml-cpu/quants.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static uint32_t rng = 20261107u;
static uint32_t next32(void) { rng = rng * 1664525u + 1013904223u; return rng; }
static float nextf(void) { return ((int32_t)(next32() >> 8) - 8388608) * (1.0f / 8388608.0f); }
static int bit_eq(float a, float b) { uint32_t x, y; memcpy(&x, &a, 4); memcpy(&y, &b, 4); return x == y; }

int main(void) {
    int q4_diff = 0, q6_diff = 0, q4_cases = 0, q6_cases = 0;
    for (int nb = 1; nb <= 8; ++nb) {
        for (int pattern = 0; pattern < 16; ++pattern) {
            int n = nb * 256;
            float x[8 * 256];
            for (int i = 0; i < n; ++i) {
                if (pattern == 0) x[i] = 0.0f;
                else if (pattern == 1) x[i] = -0.0f;
                else if (pattern == 2) x[i] = 1.0f;
                else if (pattern == 3) x[i] = -1.0f;
                else if (pattern == 4) x[i] = (i & 1) ? -1.0f : 1.0f;
                else if (pattern == 5) x[i] = (float)((i % 31) - 15) * 0.5f;
                else if (pattern < 12) x[i] = nextf() * powf(10.0f, (float)(pattern - 8));
                else x[i] = nextf() * 1e-6f;
            }
            unsigned char q4[8 * 144], q6[8 * 210], q8[8 * 292];
            quantize_row_q4_K(x, q4, n);
            quantize_row_q6_K(x, q6, n);
            quantize_row_q8_K(x, q8, n);
            float a, b;
            ggml_vec_dot_q4_K_q8_K(n, &a, 0, q4, 0, q8, 0, 1);
            ggml_vec_dot_q4_K_q8_K_generic(n, &b, 0, q4, 0, q8, 0, 1);
            q4_cases++;
            if (!bit_eq(a, b)) { if (q4_diff < 8) fprintf(stderr, "q4 diff nb=%d p=%d %a %a\n", nb, pattern, a, b); q4_diff++; }
            ggml_vec_dot_q6_K_q8_K(n, &a, 0, q6, 0, q8, 0, 1);
            ggml_vec_dot_q6_K_q8_K_generic(n, &b, 0, q6, 0, q8, 0, 1);
            q6_cases++;
            if (!bit_eq(a, b)) { if (q6_diff < 8) fprintf(stderr, "q6 diff nb=%d p=%d %a %a\n", nb, pattern, a, b); q6_diff++; }
        }
    }
    printf("{\"q4_cases\":%d,\"q4_bit_diff\":%d,\"q6_cases\":%d,\"q6_bit_diff\":%d}\n",
            q4_cases, q4_diff, q6_cases, q6_diff);
    return (q4_diff == 0 && q6_diff == 0) ? 0 : 1;
}
