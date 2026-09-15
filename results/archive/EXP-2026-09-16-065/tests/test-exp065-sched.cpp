// EXP-065 model-free scheduler checks for the async device->host split-input readback.
//
// Modes:
//   prefill - reproduces the accepted 395-token mul_mat_id output (EXP-038/041/043/062/064
//             sched sha 39505119...e91510). Host MoE weights with the op offloaded to CUDA,
//             so the weights branch and the generic host->device input copy run.
//   decode  - 3-token mul_mat_id on the CPU with CUDA-produced ids and activations. The op
//             runs on the CPU, so both CUDA inputs cross device -> host and exercise the
//             path changed by EXP-065. The output must be byte-identical with the flag off
//             and on.
//
// Environment: GGML_SCHED_READBACK_ASYNC=1 enables the async device->host readback.
// Run from the tests directory; the run script compares the output files.

#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cuda.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>

static int checks_fail = 0;

static void check(bool ok, const char * name) {
    printf("CHECK %s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) {
        checks_fail++;
    }
    fflush(stdout);
}

static void write_out(const char * path, const std::vector<float> & v) {
    FILE * f = fopen(path, "wb");
    if (f == nullptr) {
        printf("cannot open %s\n", path);
        exit(2);
    }
    fwrite(v.data(), sizeof(float), v.size(), f);
    fclose(f);
}

static void fill_weights(std::vector<float> & w) {
    for (size_t i = 0; i < w.size(); i++) {
        w[i] = (float) ((int) (i % 251) - 125) * 0.0039f;
    }
}

static void fill_ids(std::vector<int32_t> & h, int64_t n_tokens, int64_t top_k, int64_t n_expert) {
    for (int64_t t = 0; t < n_tokens; t++) {
        for (int64_t k = 0; k < top_k; k++) {
            h[t * top_k + k] = (int32_t) ((t * 3 + k * 2) % n_expert);
        }
    }
}

static void fill_activations(std::vector<float> & b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        b[i] = (float) ((i % 97) * 0.011 - 0.5);
    }
}

static int mode_prefill(const char * out_path) {
    const int64_t n = 512, m = 4096, n_expert = 10, n_tokens = 395, top_k = 4;
    const size_t expert_bytes = (size_t) n * m * sizeof(float);

    ggml_backend_t cuda = ggml_backend_init_by_type(GGML_BACKEND_DEVICE_TYPE_GPU, nullptr);
    ggml_backend_t cpu  = ggml_backend_init_by_type(GGML_BACKEND_DEVICE_TYPE_CPU, nullptr);
    if (cuda == nullptr || cpu == nullptr) {
        printf("backend init failed\n");
        return 2;
    }

    ggml_backend_buffer_t wbuf = ggml_backend_buft_alloc_buffer(ggml_backend_cpu_buffer_type(), expert_bytes * n_expert);
    ggml_backend_buffer_set_usage(wbuf, GGML_BACKEND_BUFFER_USAGE_WEIGHTS);
    ggml_backend_buffer_t idbuf = ggml_backend_buft_alloc_buffer(ggml_backend_cpu_buffer_type(), n_tokens * top_k * sizeof(int32_t) + 1024);

    struct ggml_init_params params = { /*mem_size*/ 1 << 20, /*mem_buffer*/ nullptr, /*no_alloc*/ true };
    ggml_context * ctx = ggml_init(params);

    ggml_tensor * as  = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, n, m, n_expert);
    ggml_backend_tensor_alloc(wbuf, as, ggml_backend_buffer_get_base(wbuf));
    ggml_tensor * ids = ggml_new_tensor_2d(ctx, GGML_TYPE_I32, top_k, n_tokens);
    ggml_backend_tensor_alloc(idbuf, ids, ggml_backend_buffer_get_base(idbuf));
    ggml_tensor * b   = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, n, 1, n_tokens);
    ggml_backend_buffer_t abuf = ggml_backend_buft_alloc_buffer(ggml_backend_cuda_buffer_type(0), ggml_nbytes(b));
    ggml_backend_tensor_alloc(abuf, b, ggml_backend_buffer_get_base(abuf));

    std::vector<float> wsrc((size_t) n * m * n_expert);
    fill_weights(wsrc);
    ggml_backend_tensor_set(as, wsrc.data(), 0, ggml_nbytes(as));

    std::vector<int32_t> hids(n_tokens * top_k);
    fill_ids(hids, n_tokens, top_k, n_expert);
    ggml_backend_tensor_set(ids, hids.data(), 0, ggml_nbytes(ids));

    std::vector<float> bsrc((size_t) n * n_tokens);
    fill_activations(bsrc, bsrc.size());
    ggml_backend_tensor_set(b, bsrc.data(), 0, ggml_nbytes(b));

    ggml_cgraph * graph = ggml_new_graph(ctx);
    ggml_tensor * out = ggml_mul_mat_id(ctx, as, b, ids);
    check(ids->ne[1] == b->ne[2] && b->ne[2] == 395, "prefill: token dimensions match 395");
    ggml_build_forward_expand(graph, out);

    ggml_backend_t backends[2] = { cuda, cpu };
    ggml_backend_sched_t sched = ggml_backend_sched_new(backends, nullptr, 2, 64, false, false);
    ggml_backend_sched_set_tensor_backend(sched, out, cuda);
    if (!ggml_backend_sched_alloc_graph(sched, graph)) {
        printf("prefill: sched alloc failed\n");
        return 2;
    }
    check(ggml_backend_sched_get_tensor_backend(sched, out) == cuda, "prefill: MUL_MAT_ID offloaded to CUDA");

    enum ggml_status st = ggml_backend_sched_graph_compute(sched, graph);
    check(st == GGML_STATUS_SUCCESS, "prefill: sched graph compute");

    std::vector<float> hout(ggml_nbytes(out) / sizeof(float));
    ggml_backend_tensor_get(out, hout.data(), 0, ggml_nbytes(out));
    write_out(out_path, hout);

    ggml_backend_sched_free(sched);
    ggml_backend_buffer_free(abuf);
    ggml_backend_buffer_free(idbuf);
    ggml_backend_buffer_free(wbuf);
    ggml_free(ctx);
    ggml_backend_free(cuda);
    ggml_backend_free(cpu);
    return checks_fail == 0 ? 0 : 1;
}

static int mode_decode(const char * out_path) {
    const int64_t n = 512, m = 4096, n_expert = 10, n_tokens = 3, top_k = 4;
    const size_t expert_bytes = (size_t) n * m * sizeof(float);

    ggml_backend_t cuda = ggml_backend_init_by_type(GGML_BACKEND_DEVICE_TYPE_GPU, nullptr);
    ggml_backend_t cpu  = ggml_backend_init_by_type(GGML_BACKEND_DEVICE_TYPE_CPU, nullptr);
    if (cuda == nullptr || cpu == nullptr) {
        printf("backend init failed\n");
        return 2;
    }

    ggml_backend_buffer_t wbuf = ggml_backend_buft_alloc_buffer(ggml_backend_cpu_buffer_type(), expert_bytes * n_expert);
    ggml_backend_buffer_set_usage(wbuf, GGML_BACKEND_BUFFER_USAGE_WEIGHTS);

    struct ggml_init_params params = { /*mem_size*/ 1 << 20, /*mem_buffer*/ nullptr, /*no_alloc*/ true };
    ggml_context * ctx = ggml_init(params);

    ggml_tensor * as  = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, n, m, n_expert);
    ggml_backend_tensor_alloc(wbuf, as, ggml_backend_buffer_get_base(wbuf));
    // ids and activations are produced on the device, as in decode
    ggml_tensor * ids = ggml_new_tensor_2d(ctx, GGML_TYPE_I32, top_k, n_tokens);
    ggml_backend_buffer_t idbuf = ggml_backend_buft_alloc_buffer(ggml_backend_cuda_buffer_type(0), ggml_nbytes(ids));
    ggml_backend_tensor_alloc(idbuf, ids, ggml_backend_buffer_get_base(idbuf));
    ggml_tensor * b   = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, n, 1, n_tokens);
    ggml_backend_buffer_t abuf = ggml_backend_buft_alloc_buffer(ggml_backend_cuda_buffer_type(0), ggml_nbytes(b));
    ggml_backend_tensor_alloc(abuf, b, ggml_backend_buffer_get_base(abuf));

    std::vector<float> wsrc((size_t) n * m * n_expert);
    fill_weights(wsrc);
    ggml_backend_tensor_set(as, wsrc.data(), 0, ggml_nbytes(as));

    std::vector<int32_t> hids(n_tokens * top_k);
    fill_ids(hids, n_tokens, top_k, n_expert);
    ggml_backend_tensor_set(ids, hids.data(), 0, ggml_nbytes(ids));

    std::vector<float> bsrc((size_t) n * n_tokens);
    fill_activations(bsrc, bsrc.size());
    ggml_backend_tensor_set(b, bsrc.data(), 0, ggml_nbytes(b));

    ggml_cgraph * graph = ggml_new_graph(ctx);
    ggml_tensor * out = ggml_mul_mat_id(ctx, as, b, ids);
    check(ids->ne[1] == b->ne[2] && b->ne[2] == 3, "decode: token dimensions match 3");
    ggml_build_forward_expand(graph, out);

    // no offload hint: host expert weights keep MUL_MAT_ID on the CPU, so the CUDA ids and
    // activations cross device -> host before the CPU split runs
    ggml_backend_t backends[2] = { cuda, cpu };
    ggml_backend_sched_t sched = ggml_backend_sched_new(backends, nullptr, 2, 64, false, false);
    if (!ggml_backend_sched_alloc_graph(sched, graph)) {
        printf("decode: sched alloc failed\n");
        return 2;
    }
    check(ggml_backend_sched_get_tensor_backend(sched, out) == cpu, "decode: MUL_MAT_ID runs on the CPU");
    check(ggml_backend_sched_get_n_splits(sched) == 1, "decode: single CPU split");

    enum ggml_status st = ggml_backend_sched_graph_compute(sched, graph);
    check(st == GGML_STATUS_SUCCESS, "decode: sched graph compute");

    std::vector<float> hout(ggml_nbytes(out) / sizeof(float));
    ggml_backend_tensor_get(out, hout.data(), 0, ggml_nbytes(out));
    write_out(out_path, hout);

    ggml_backend_sched_free(sched);
    ggml_backend_buffer_free(abuf);
    ggml_backend_buffer_free(idbuf);
    ggml_backend_buffer_free(wbuf);
    ggml_free(ctx);
    ggml_backend_free(cuda);
    ggml_backend_free(cpu);
    return checks_fail == 0 ? 0 : 1;
}

int main(int argc, char ** argv) {
    if (argc < 3) {
        printf("usage: %s prefill|decode <out-file>\n", argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "prefill") == 0) {
        return mode_prefill(argv[2]);
    }
    if (strcmp(argv[1], "decode") == 0) {
        return mode_decode(argv[2]);
    }
    printf("unknown mode %s\n", argv[1]);
    return 1;
}
