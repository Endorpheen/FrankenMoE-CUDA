#define _POSIX_C_SOURCE 200112L
#include "ggml.h"
#include "ggml-cpu.h"
#include "ggml-backend.h"
#include "ggml-cpu/quants.h"
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rng = 20261107u;
static uint32_t next32(void) { rng = rng * 1664525u + 1013904223u; return rng; }
static float nextf(void) { return ((int32_t)(next32() >> 8) - 8388608) * (1.0f / 8388608.0f); }
static uint64_t hash64(const void * p, size_t n) {
    const uint8_t * b = p; uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
    return h;
}
static struct ggml_context * ctx_new(void) {
    struct ggml_init_params p = { 64u * 1024u * 1024u, NULL, false };
    struct ggml_context * c = ggml_init(p); if (!c) abort(); return c;
}
static void fill_weights(struct ggml_tensor * w) {
    const int64_t K = w->ne[0], M = w->ne[1];
    float * row = malloc((size_t) K * sizeof(float));
    if (!row) abort();
    size_t rowbytes = ggml_row_size(w->type, K);
    for (int64_t m = 0; m < M; ++m) {
        for (int64_t i = 0; i < K; ++i) row[i] = nextf() * (1.0f + (float) (i % 7));
        void * dst = (uint8_t *) w->data + (size_t) m * rowbytes;
        if (w->type == GGML_TYPE_Q4_K) quantize_row_q4_K(row, dst, K);
        else if (w->type == GGML_TYPE_Q6_K) quantize_row_q6_K(row, dst, K);
        else abort();
    }
    free(row);
}
static void fill_acts(struct ggml_tensor * a) {
    int64_t n = ggml_nelements(a);
    float * p = a->data;
    for (int64_t i = 0; i < n; ++i) p[i] = nextf() * 3.0f;
}
int main(int argc, char ** argv) {
    if (argc != 2) return 2;
    ggml_cpu_init();
    ggml_numa_init(GGML_NUMA_STRATEGY_DISABLED);
    FILE * out = fopen(argv[1], "wb");
    if (!out) return 2;
    int cases = 0, failures = 0;
    const enum ggml_type types[] = { GGML_TYPE_Q4_K, GGML_TYPE_Q6_K };
    const int ns[] = { 1, 4, 7 };
    for (int ti = 0; ti < 2; ++ti) {
        for (int ni = 0; ni < 3; ++ni) {
            for (int threads = 1; threads <= 8; threads = threads == 1 ? 3 : threads == 3 ? 8 : 9) {
                struct ggml_context * c = ctx_new();
                const int64_t K = 2048, M = 17;
                struct ggml_tensor * w = ggml_new_tensor_2d(c, types[ti], K, M);
                struct ggml_tensor * a = ggml_new_tensor_2d(c, GGML_TYPE_F32, K, ns[ni]);
                fill_weights(w);
                fill_acts(a);
                struct ggml_tensor * t = ggml_mul_mat(c, w, a);
                struct ggml_backend * b = ggml_backend_cpu_init();
                if (!b) abort();
                ggml_backend_cpu_set_n_threads(b, threads);
                struct ggml_cgraph * g = ggml_new_graph(c);
                ggml_build_forward_expand(g, t);
                enum ggml_status st = ggml_backend_graph_compute(b, g);
                int ok = st == GGML_STATUS_SUCCESS;
                if (!ok) failures++;
                char name[96];
                snprintf(name, sizeof(name), "kdot-%s-n%d-t%d",
                        types[ti] == GGML_TYPE_Q4_K ? "q4k" : "q6k", ns[ni], threads);
                fprintf(out, "{\"name\":\"%s\",\"type\":%d,\"rows\":%d,\"threads\":%d,"
                        "\"bytes\":%zu,\"hash\":\"%016" PRIx64 "\",\"status\":%d}\n",
                        name, (int) types[ti], ns[ni], threads, ggml_nbytes(t),
                        ok ? hash64(t->data, ggml_nbytes(t)) : 0ull, (int) st);
                cases++;
                ggml_backend_free(b);
                ggml_free(c);
            }
        }
    }
    fprintf(out, "{\"summary\":\"%s\",\"cases\":%d,\"failures\":%d}\n",
            failures ? "FAILED" : "PASS", cases, failures);
    fclose(out);
    printf("FG006_KDOT_BACKEND %s cases=%d failures=%d\n", failures ? "FAILED" : "PASS", cases, failures);
    return failures ? 1 : 0;
}
