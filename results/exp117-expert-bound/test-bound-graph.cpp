#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cuda.h"
#include "llama-moe-bound.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static void require(bool ok, const char * message) {
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

int main(int argc, char ** argv) {
    require(argc == 4, "usage: test-bound-graph Q4_K|Q6_K control|safe|unsafe output.bin");
    const ggml_type type = std::string(argv[1]) == "Q6_K" ? GGML_TYPE_Q6_K : GGML_TYPE_Q4_K;
    const std::string mode = argv[2];
    require(mode == "control" || mode == "safe" || mode == "unsafe", "test mode");
    constexpr int width = 256, experts = 256, tokens = 512, used = 2;
    auto * gpu = ggml_backend_cuda_init(0);
    require(gpu != nullptr, "CUDA backend");
    auto supported = (bool (*)(ggml_backend_t)) ggml_backend_reg_get_proc_address(
        ggml_backend_dev_backend_reg(ggml_backend_get_device(gpu)),
        "ggml_backend_moe_launch_bound_supported_v1");
    require(supported && supported(gpu), "supported sm89 backend capability");
    ggml_init_params params = {8*1024*1024, nullptr, true};
    auto * host_ctx = ggml_init(params);
    auto * device_ctx = ggml_init(params);
    std::array<ggml_tensor *, 3> weights, copies, outputs;
    std::array<ggml_cgraph *, 3> graphs;
    auto * x = ggml_new_tensor_3d(device_ctx, GGML_TYPE_F32, width, 1, tokens);
    auto * ids = ggml_new_tensor_2d(device_ctx, GGML_TYPE_I32, used, tokens);
    for (unsigned j = 0; j < 3; ++j) {
        weights[j] = ggml_new_tensor_3d(host_ctx, type, width, width, experts);
        copies[j] = ggml_dup_tensor(device_ctx, weights[j]);
        outputs[j] = ggml_mul_mat_id(device_ctx, copies[j], x, ids);
        graphs[j] = ggml_new_graph_custom(device_ctx, 16, false);
        ggml_build_forward_expand(graphs[j], outputs[j]);
    }
    auto * host = ggml_backend_alloc_ctx_tensors_from_buft(host_ctx, ggml_backend_cuda_host_buffer_type());
    auto * device = ggml_backend_alloc_ctx_tensors_from_buft(device_ctx, ggml_backend_cuda_buffer_type(0));
    require(host && device, "tensor storage");
    std::vector<float> w(width*width*experts), input(width*tokens), result(width*used*tokens);
    for (size_t i = 0; i < input.size(); ++i) input[i] = std::sin(float(i)*0.017f);
    ggml_backend_tensor_set(x, input.data(), 0, ggml_nbytes(x));
    for (unsigned j = 0; j < 3; ++j) {
        for (size_t i = 0; i < w.size(); ++i) w[i] = std::sin(float(i)*0.003f + j)*0.05f;
        require(ggml_quantize_chunk(type, w.data(), weights[j]->data, 0, width*experts, width, nullptr) == ggml_nbytes(weights[j]), "quantize");
        ggml_backend_tensor_set(copies[j], weights[j]->data, 0, ggml_nbytes(weights[j]));
    }
    FILE * out = std::fopen(argv[3], "wb");
    require(out != nullptr, "output file");
    std::vector<int32_t> routes(used*tokens), counts(experts);
    for (int iteration = 0; iteration < 8; ++iteration) {
        std::fill(counts.begin(), counts.end(), 0);
        int32_t maximum = 0;
        for (int t = 0; t < tokens; ++t) {
            for (int e = 0; e < used; ++e) {
                int id = iteration < 4 ? (t*used + e) % experts : e;
                routes[t*used + e] = id;
                maximum = std::max(maximum, ++counts[id]);
            }
        }
        ggml_backend_tensor_set(ids, routes.data(), 0, ggml_nbytes(ids));
        for (unsigned j = 0; j < 3; ++j) {
            // Simulate a reused scheduler identity with mutable routing values.
            graphs[j]->uid = 17;
            auto get_granularity = (int (*)(ggml_backend_t, const ggml_tensor *)) ggml_backend_reg_get_proc_address(
                ggml_backend_dev_backend_reg(ggml_backend_get_device(gpu)),
                "ggml_backend_moe_launch_bound_granularity_v1");
            require(get_granularity != nullptr, "backend tile granularity API");
            int granularity = get_granularity(gpu, outputs[j]);
            require(granularity == 128, "proven J=128 dispatch");
            const int32_t bound = maximum + (iteration == 2 ? 1 : 0);
            if (mode == "safe") {
                ggml_moe_set_launch_bound(graphs[j], outputs[j], bound, granularity);
                if (iteration == 2) require(graphs[j]->uid == 17, "same grid retains UID");
            }
            if (mode == "unsafe") outputs[j]->op_params[4] = ggml_moe_launch_bound_value(outputs[j], bound, granularity);
            require(ggml_backend_graph_compute_async(gpu, graphs[j]) == GGML_STATUS_SUCCESS, "compute");
        }
        ggml_backend_synchronize(gpu);
        for (auto * tensor : outputs) {
            ggml_backend_tensor_get(tensor, result.data(), 0, ggml_nbytes(tensor));
            for (float v : result) require(std::isfinite(v), "finite output");
            require(std::fwrite(result.data(), sizeof(float), result.size(), out) == result.size(), "write output");
        }
    }
    std::fclose(out);
    ggml_backend_buffer_free(device);
    ggml_backend_buffer_free(host);
    ggml_backend_free(gpu);
    ggml_free(device_ctx); ggml_free(host_ctx);
    fprintf(stderr, "DONE: %s %s, fixed UID, balanced->hot routing, 8 iterations\n", argv[1], argv[2]);
}
