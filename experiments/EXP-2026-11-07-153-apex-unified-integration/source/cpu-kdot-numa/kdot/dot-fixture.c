#define _POSIX_C_SOURCE 200112L
#include "ggml-cpu.h"
#include "ggml-cpu/quants.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

extern float fg006_dot_q4_K_q8_K(int n, const void * x, const void * y);
extern float fg006_dot_q6_K_q8_K(int n, const void * x, const void * y);

static uint32_t rng = 20261107u;
static uint32_t next32(void) { rng = rng * 1664525u + 1013904223u; return rng; }
static float nextf(void) { return ((int32_t)(next32() >> 8) - 8388608) * (1.0f / 8388608.0f); }
static int bits_eq(float a, float b) { uint32_t x, y; memcpy(&x, &a, 4); memcpy(&y, &b, 4); return x == y; }
static uint64_t hash64(const void * p, size_t n) {
    const uint8_t * b = p; uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
    return h;
}

int main(int argc, char ** argv) {
    if (argc != 2) return 2;
    ggml_cpu_init();
    FILE * out = fopen(argv[1], "wb");
    if (!out) return 2;
    int cases = 0, failures = 0;
    {
        unsigned char d4[144] = {0}, d6[210] = {0}, d8[292] = {0};
        const int bad[] = { 0, 255, -256 };
        for (int i = 0; i < 3; ++i) {
            int ok = fg006_dot_q4_K_q8_K(bad[i], d4, d8) == 0.0f &&
                     fg006_dot_q6_K_q8_K(bad[i], d6, d8) == 0.0f;
            if (!ok) failures++;
            fprintf(out, "{\"name\":\"dot-invalid-n%d\",\"negative\":true,\"pass\":%s}\n",
                    bad[i], ok ? "true" : "false");
            cases++;
        }
    }
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
            float n4, g4, n6, g6, ng4, ng6;
            ggml_vec_dot_q4_K_q8_K(n, &n4, 0, q4, 0, q8, 0, 1);
            ggml_vec_dot_q4_K_q8_K_generic(n, &ng4, 0, q4, 0, q8, 0, 1);
            g4 = fg006_dot_q4_K_q8_K(n, q4, q8);
            ggml_vec_dot_q6_K_q8_K(n, &n6, 0, q6, 0, q8, 0, 1);
            ggml_vec_dot_q6_K_q8_K_generic(n, &ng6, 0, q6, 0, q8, 0, 1);
            g6 = fg006_dot_q6_K_q8_K(n, q6, q8);
            if (nb == 1 && pattern == 2) fprintf(stderr, "p2 n4=%a ng4=%a g4=%a n6=%a ng6=%a g6=%a q4h=%016llx q8h=%016llx\n",
                    n4, ng4, g4, n6, ng6, g6,
                    (unsigned long long) hash64(q4, 144), (unsigned long long) hash64(q8, 292));
            int ok4 = bits_eq(n4, g4), ok6 = bits_eq(n6, g6);
            if (!ok4 || !ok6) {
                failures++;
                fprintf(stderr, "diff nb=%d p=%d q4=%d q6=%d n4=%a g4=%a n6=%a g6=%a\n",
                        nb, pattern, ok4, ok6, n4, g4, n6, g6);
            }
            fprintf(out, "{\"name\":\"dot-nb%d-p%d\",\"type4\":\"%016llx\",\"type6\":\"%016llx\","
                    "\"q4_native\":\"%016llx\",\"q4_goose\":\"%016llx\","
                    "\"q6_native\":\"%016llx\",\"q6_goose\":\"%016llx\","
                    "\"q4_equal\":%s,\"q6_equal\":%s}\n",
                    nb, pattern,
                    (unsigned long long) hash64(q4, (size_t)n / 256 * 144),
                    (unsigned long long) hash64(q6, (size_t)n / 256 * 210),
                    (unsigned long long) hash64(&n4, 4), (unsigned long long) hash64(&g4, 4),
                    (unsigned long long) hash64(&n6, 4), (unsigned long long) hash64(&g6, 4),
                    ok4 ? "true" : "false", ok6 ? "true" : "false");
            cases++;
        }
    }
    fprintf(out, "{\"summary\":\"%s\",\"cases\":%d,\"failures\":%d}\n",
            failures ? "FAILED" : "PASS", cases, failures);
    fclose(out);
    printf("FG006_KQUANT_DOT %s cases=%d failures=%d\n", failures ? "FAILED" : "PASS", cases, failures);
    return failures ? 1 : 0;
}
