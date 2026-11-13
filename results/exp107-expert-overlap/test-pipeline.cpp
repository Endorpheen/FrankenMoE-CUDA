#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cuda.h"
#include "ggml-expert-pipeline.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>

static void require(bool ok, const char * message) {
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

int main(int argc, char ** argv) {
    require(argc == 4, "usage: test-pipeline Q4_K|Q6_K control|overlap output.bin");
    const ggml_type type = std::string(argv[1]) == "Q6_K" ? GGML_TYPE_Q6_K : GGML_TYPE_Q4_K;
    const bool overlap = std::string(argv[2]) == "overlap";
    ggml_backend_t gpu = ggml_backend_cuda_init(0);
    require(gpu != nullptr, "CUDA backend");
    auto fn = (ggml_expert_pipeline_fn) ggml_backend_reg_get_proc_address(
        ggml_backend_dev_backend_reg(ggml_backend_get_device(gpu)), "ggml_backend_expert_pipeline_v1");
    require(fn != nullptr, "pipeline interface");
    ggml_init_params params = {8*1024*1024, nullptr, true};
    auto * weights_ctx = ggml_init(params);
    auto * copies_ctx = ggml_init(params);
    auto * data_ctx = ggml_init(params);
    std::array<ggml_tensor *, 3> weights, copies, outputs;
    std::array<ggml_cgraph *, 3> graphs;
    auto * x = ggml_new_tensor_3d(data_ctx, GGML_TYPE_F32, 256, 1, 64);
    auto * ids = ggml_new_tensor_2d(data_ctx, GGML_TYPE_I32, 2, 64);
    for (unsigned i = 0; i < 3; ++i) {
        weights[i] = ggml_new_tensor_3d(weights_ctx, type, 256, 256, 4);
        copies[i] = ggml_dup_tensor(copies_ctx, weights[i]);
        outputs[i] = ggml_mul_mat_id(data_ctx, copies[i], x, ids);
        graphs[i] = ggml_new_graph_custom(data_ctx, 16, false);
        ggml_build_forward_expand(graphs[i], outputs[i]);
    }
    auto host = ggml_backend_alloc_ctx_tensors_from_buft(weights_ctx, ggml_backend_cuda_host_buffer_type());
    auto device = ggml_backend_alloc_ctx_tensors_from_buft(data_ctx, ggml_backend_cuda_buffer_type(0));
    require(host && device, "tensor storage");
    ggml_backend_buffer_set_usage(host, GGML_BACKEND_BUFFER_USAGE_WEIGHTS);
    std::vector<float> w(256*256*4), input(256*64);
    std::vector<int32_t> routes(2*64);
    for (size_t i = 0; i < input.size(); ++i) input[i] = std::sin(float(i)*0.017f);
    for (size_t i = 0; i < routes.size(); ++i) routes[i] = (i + i/2) % 4;
    ggml_backend_tensor_set(x, input.data(), 0, ggml_nbytes(x));
    ggml_backend_tensor_set(ids, routes.data(), 0, ggml_nbytes(ids));
    for (unsigned j = 0; j < 3; ++j) {
        for (size_t i = 0; i < w.size(); ++i) w[i] = std::sin(float(i)*0.003f + j)*0.05f;
        require(ggml_quantize_chunk(type, w.data(), weights[j]->data, 0, 256*4, 256, nullptr) == ggml_nbytes(weights[j]), "quantize weights");
    }
    ggml_backend_buffer_t original = nullptr;
    if (overlap) {
        size_t size = 0;
        require(fn(gpu, GGML_EXPERT_QUERY, weights[0], copies[0], &size), "pinned query");
        ggml_tensor large = *copies[0]; large.ne[2] = 8192;
        size_t rejected_size = 0;
        require(!fn(gpu, GGML_EXPERT_QUERY, weights[0], &large, &rejected_size), "budget refusal");
        ggml_tensor unsupported = *weights[0]; unsupported.type = GGML_TYPE_F32;
        require(!fn(gpu, GGML_EXPERT_QUERY, &unsupported, copies[0], &rejected_size), "unsupported type refusal");
        require(fn(gpu, GGML_EXPERT_RESERVE, nullptr, nullptr, &size), "ring reserve");
        size_t index = 0;
        for (unsigned j = 0; j < 3; ++j) require(fn(gpu, GGML_EXPERT_BIND, weights[j], copies[j], &index), "ring binding");
    } else {
        original = ggml_backend_alloc_ctx_tensors_from_buft(copies_ctx, ggml_backend_cuda_buffer_type(0));
        require(original != nullptr, "control storage");
    }
    FILE * out = std::fopen(argv[3], "wb");
    require(out != nullptr, "output file");
    std::vector<float> result(256*2*64);
    for (unsigned iteration = 0; iteration < 8; ++iteration) {
        if (overlap && iteration == 4 && getenv("GGML_EXPERT_PIPELINE_TEST_REBIND")) {
            size_t size = 0;
            require(!fn(gpu, GGML_EXPERT_RESERVE, nullptr, nullptr, &size), "mid-run release");
            for (auto * t : copies) {
                t->buffer = nullptr;
                t->data = nullptr;
            }
            require(fn(gpu, GGML_EXPERT_QUERY, weights[0], copies[0], &size), "rebind query");
            require(fn(gpu, GGML_EXPERT_RESERVE, nullptr, nullptr, &size), "rebind reserve");
            size_t slot = 0;
            for (unsigned j = 0; j < 3; ++j) require(fn(gpu, GGML_EXPERT_BIND, weights[j], copies[j], &slot), "rebind slots");
        }
        for (unsigned j = 0; j < 3; ++j) {
            if (overlap) require(fn(gpu, GGML_EXPERT_BEGIN, weights[j], copies[j], nullptr), "begin upload");
            ggml_backend_tensor_set_async(gpu, copies[j], weights[j]->data, 0, ggml_nbytes(weights[j]));
            if (overlap) require(fn(gpu, GGML_EXPERT_END, weights[j], copies[j], nullptr), "upload ready");
            require(ggml_backend_graph_compute_async(gpu, graphs[j]) == GGML_STATUS_SUCCESS, "compute");
        }
        ggml_backend_synchronize(gpu);
        for (unsigned j = 0; j < 3; ++j) {
            ggml_backend_tensor_get(outputs[j], result.data(), 0, ggml_nbytes(outputs[j]));
            for (float v : result) require(std::isfinite(v), "finite output");
            require(std::fwrite(result.data(), sizeof(float), result.size(), out) == result.size(), "save output");
        }
    }
    std::fclose(out);
    if (overlap) {
        size_t zero = 0;
        require(!fn(gpu, GGML_EXPERT_RESERVE, nullptr, nullptr, &zero), "release on ineligible graph");
    }
    ggml_backend_buffer_free(original);
    ggml_backend_buffer_free(device);
    ggml_backend_free(gpu);
    ggml_backend_buffer_free(host);
    ggml_free(data_ctx); ggml_free(copies_ctx); ggml_free(weights_ctx);
    fprintf(stderr, "PASS: %s %s, 8 iterations, 3 projections, release and fallback guards\n", argv[1], argv[2]);
}
