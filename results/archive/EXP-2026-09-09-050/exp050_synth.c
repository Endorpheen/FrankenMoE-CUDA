// EXP-050 model-free gate harness: exercise the CUDA-graph decision state machine
// on a tiny synthetic graph and verify that the env-gated diagnostics
// (GGML_GRAPH_DIAG) emit the expected decision events without changing results.
//
// Expected event sequence for the same graph object computed three times:
//   1st compute: decision=direct_first_call   (empty property snapshot)
//   2nd compute: decision=capture             (properties unchanged, warmup done)
//   3rd compute: decision=launch              (replay of the captured instance)
// Renaming node[0] in place (same tensor object => same graph key) then triggers:
//   4th compute: decision=direct_warmup_reset diff=node[0].name '...->...'
//   5th compute: decision=capture             (stable again)
//
// Build: see run-model-free-exp050.sh. No model files are loaded.

#include "ggml.h"
#include "ggml-alloc.h"
#include "ggml-backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static double checksum(const float * data, int n) {
    double acc = 0.0;
    for (int i = 0; i < n; i++) {
        acc += data[i] * (double) (i + 1);
    }
    return acc;
}

int main(void) {
    ggml_backend_dev_t dev = ggml_backend_dev_by_type(GGML_BACKEND_DEVICE_TYPE_GPU);
    if (dev == NULL) {
        fprintf(stderr, "gate: FAIL no GPU device\n");
        return 1;
    }
    ggml_backend_t backend = ggml_backend_dev_init(dev, NULL);
    if (backend == NULL) {
        fprintf(stderr, "gate: FAIL cannot init GPU backend\n");
        return 1;
    }
    ggml_backend_buffer_type_t buft = ggml_backend_get_default_buffer_type(backend);

    struct ggml_init_params ip = {
        /*.mem_size   =*/ 128 * 1024 * 1024,
        /*.mem_buffer =*/ NULL,
        /*.no_alloc   =*/ true,
    };
    struct ggml_context * ctx = ggml_init(ip);

    const int64_t k = 64, m = 8, n = 3;

    struct ggml_tensor * a = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, k, n); // activations
    struct ggml_tensor * w = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, k, m); // weights
    struct ggml_tensor * out = ggml_mul_mat(ctx, w, a);                    // [m, n]

    ggml_backend_buffer_t buf = ggml_backend_alloc_ctx_tensors_from_buft(ctx, buft);

    // fill inputs deterministically
    {
        const int na = (int) ggml_nelements(a);
        const int nw = (int) ggml_nelements(w);
        float * tmp = malloc(sizeof(float) * (na > nw ? na : nw));
        for (int i = 0; i < nw; i++) {
            tmp[i] = (float) ((i % 17) - 8) * 0.25f;
        }
        ggml_backend_tensor_set(w, tmp, 0, sizeof(float) * nw);
        for (int i = 0; i < na; i++) {
            tmp[i] = (float) ((i % 23) - 11) * 0.5f;
        }
        ggml_backend_tensor_set(a, tmp, 0, sizeof(float) * na);
        free(tmp);
    }

    struct ggml_cgraph * gf = ggml_new_graph(ctx);
    ggml_build_forward_expand(gf, out);

    const int rounds = 5;
    double sums[rounds];

    for (int r = 0; r < rounds; r++) {
        if (r == 3) {
            // same tensor object => same graph key; only the name changes, so the
            // property snapshot differs in exactly one field
            ggml_format_name(out, "exp050_renamed_r%d", r);
        }

        if (ggml_backend_graph_compute(backend, gf) != GGML_STATUS_SUCCESS) {
            fprintf(stderr, "gate: FAIL compute round %d\n", r);
            return 1;
        }

        const int no = (int) ggml_nelements(out);
        float * host = malloc(sizeof(float) * no);
        ggml_backend_tensor_get(out, host, 0, sizeof(float) * no);
        sums[r] = checksum(host, no);
        free(host);

        fprintf(stderr, "gate: round %d checksum=%.6f\n", r, sums[r]);
    }

    // all rounds must produce identical results (renaming does not change math)
    for (int r = 1; r < rounds; r++) {
        if (fabs(sums[r] - sums[0]) > 1e-6) {
            fprintf(stderr, "gate: FAIL result drift at round %d\n", r);
            return 1;
        }
    }

    ggml_backend_buffer_free(buf);
    ggml_free(ctx);
    ggml_backend_free(backend);

    fprintf(stderr, "gate: OK checksums identical across %d rounds\n", rounds);
    return 0;
}
