// EXP-2026-09-16-074 G5: scheduler-level invariant for the pinned-source direct-DMA path.
//
// Builds the accepted 395-token MUL_MAT_ID prefill shape through ggml_backend_sched so the real
// ggml-backend.cpp weights branch calls set_tensor_async_pinned_ring, then hashes the graph
// output. The hash must be identical with the pinned source on, off, and in fallback.

#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-backend-impl.h"
#include "ggml-cuda.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

static uint64_t fnv1a64(const uint8_t * p, size_t n) {
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}

int main(int argc, char ** argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <out-file>\n", argv[0]); return 1; }
    const char * out_path = argv[1];

    const int64_t n = 512, m = 4096, n_expert = 10, n_tokens = 395, top_k = 4;
    const size_t expert_bytes = (size_t) n * m * sizeof(float);

    ggml_backend_t cuda = ggml_backend_init_by_type(GGML_BACKEND_DEVICE_TYPE_GPU, nullptr);
    ggml_backend_t cpu  = ggml_backend_init_by_type(GGML_BACKEND_DEVICE_TYPE_CPU, nullptr);
    if (!cuda || !cpu) { fprintf(stderr, "missing backend\n"); return 2; }

    ggml_backend_buffer_t wbuf = ggml_backend_buft_alloc_buffer(ggml_backend_cpu_buffer_type(), expert_bytes * n_expert);
    ggml_backend_buffer_set_usage(wbuf, GGML_BACKEND_BUFFER_USAGE_WEIGHTS);
    ggml_backend_buffer_t idbuf = ggml_backend_buft_alloc_buffer(ggml_backend_cpu_buffer_type(), n_tokens * top_k * sizeof(int32_t) + 1024);

    struct ggml_init_params params = { /*mem_size*/ 1u << 20, /*mem_buffer*/ nullptr, /*no_alloc*/ true };
    ggml_context * ctx = ggml_init(params);

    ggml_tensor * as  = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, n, m, n_expert);
    ggml_backend_tensor_alloc(wbuf, as, ggml_backend_buffer_get_base(wbuf));
    ggml_tensor * ids = ggml_new_tensor_2d(ctx, GGML_TYPE_I32, top_k, n_tokens);
    ggml_backend_tensor_alloc(idbuf, ids, ggml_backend_buffer_get_base(idbuf));
    ggml_tensor * b   = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, n, 1, n_tokens);
    ggml_backend_buffer_t abuf = ggml_backend_buft_alloc_buffer(ggml_backend_cuda_buffer_type(0), ggml_nbytes(b));
    ggml_backend_tensor_alloc(abuf, b, ggml_backend_buffer_get_base(abuf));

    std::vector<float> wsrc((size_t) n * m * n_expert);
    for (size_t i = 0; i < wsrc.size(); i++) wsrc[i] = (float) ((int) (i % 251) - 125) * 0.0039f;
    ggml_backend_tensor_set(as, wsrc.data(), 0, ggml_nbytes(as));

    // adjacent, sparse, first and last experts in a bulk-prefill routing
    std::vector<int32_t> hids(n_tokens * top_k);
    for (int64_t t = 0; t < n_tokens; t++) {
        for (int64_t k = 0; k < top_k; k++) {
            hids[t * top_k + k] = (int32_t) ((t * 3 + k * 2) % n_expert);
        }
    }
    ggml_backend_tensor_set(ids, hids.data(), 0, ggml_nbytes(ids));

    std::vector<float> bsrc((size_t) n * n_tokens);
    for (size_t i = 0; i < bsrc.size(); i++) bsrc[i] = (float) ((i % 97) * 0.011 - 0.5);
    ggml_backend_tensor_set(b, bsrc.data(), 0, ggml_nbytes(b));

    ggml_cgraph * graph = ggml_new_graph(ctx);
    ggml_tensor * out = ggml_mul_mat_id(ctx, as, b, ids);
    ggml_build_forward_expand(graph, out);

    ggml_backend_t backends[2] = { cuda, cpu };
    ggml_backend_sched_t sched = ggml_backend_sched_new(backends, nullptr, 2, 64, false, false);
    ggml_backend_sched_set_tensor_backend(sched, out, cuda);
    if (!ggml_backend_sched_alloc_graph(sched, graph)) { fprintf(stderr, "sched alloc failed\n"); return 2; }
    if (ggml_backend_sched_graph_compute(sched, graph) != GGML_STATUS_SUCCESS) { fprintf(stderr, "compute failed\n"); return 2; }

    std::vector<float> hout(ggml_nbytes(out) / sizeof(float));
    ggml_backend_tensor_get(out, hout.data(), 0, ggml_nbytes(out));

    FILE * f = fopen(out_path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", out_path); return 2; }
    fwrite(hout.data(), 1, ggml_nbytes(out), f);
    fclose(f);
    printf("SCHED out_sha %016llx bytes %zu\n",
           (unsigned long long) fnv1a64((const uint8_t *) hout.data(), ggml_nbytes(out)), ggml_nbytes(out));

    ggml_backend_sched_free(sched);
    ggml_backend_buffer_free(abuf);
    ggml_backend_buffer_free(idbuf);
    ggml_backend_buffer_free(wbuf);
    ggml_free(ctx);
    ggml_backend_free(cuda);
    ggml_backend_free(cpu);
    return 0;
}
