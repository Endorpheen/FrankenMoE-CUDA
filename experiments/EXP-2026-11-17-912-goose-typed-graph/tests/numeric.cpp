#include "ggml.h"
#include "ggml-cpu.h"
#include "runtime.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// Test-only interposition. A positive legacy control proves the counter works
// before the Goose-owned chain is required to make zero packet reads.
static uint64_t packet_reads;
extern "C" int64_t fg158_arg(int64_t packet, int64_t index) {
    ++packet_reads;
    return (int64_t) ((const uint64_t *) (uintptr_t) packet)[index];
}

static void require(bool value, const char * message) {
    if (!value) { std::fprintf(stderr, "FAIL %s\n", message); std::exit(3); }
}

static void fill_weight(ggml_tensor * tensor, int seed) {
    const int64_t count = ggml_nelements(tensor);
    std::vector<float> values((size_t) count);
    for (int64_t i = 0; i < count; ++i) values[(size_t) i] = (float) ((i * 13 + seed) % 29 - 14) / 128.0f;
    if (tensor->type == GGML_TYPE_F32) {
        std::memcpy(tensor->data, values.data(), count * sizeof(float));
    } else {
        const size_t bytes = ggml_quantize_chunk(tensor->type, values.data(), tensor->data,
                                                0, tensor->ne[1], tensor->ne[0], nullptr);
        require(bytes == ggml_nbytes(tensor), "weight quantization byte count");
    }
}

int main() {
    goose_init(0, nullptr);
    int cases = 0;
    int64_t compared = 0;
    for (bool materialized : {true, false}) for (int tokens : {1, 3}) for (bool quantized : {false, true}) {
        ggml_context * context = ggml_init({8 * 1024 * 1024, nullptr, false});
        require(context != nullptr, "context");
        ggml_tensor * input = ggml_new_tensor_2d(context, GGML_TYPE_F32, 256, tokens);
        ggml_tensor * norm = ggml_new_tensor_1d(context, GGML_TYPE_F32, 256);
        ggml_tensor * w0 = ggml_new_tensor_2d(context, quantized ? GGML_TYPE_Q4_K : GGML_TYPE_F32, 256, 256);
        ggml_tensor * w1 = ggml_new_tensor_2d(context, quantized ? GGML_TYPE_Q6_K : GGML_TYPE_F32, 256, 256);
        for (int64_t i = 0; i < ggml_nelements(input); ++i) ((float *) input->data)[i] = (float) ((i * 17) % 23 - 11) / 32.0f;
        for (int i = 0; i < 256; ++i) ((float *) norm->data)[i] = 1.0f + (float) (i % 7 - 3) * 0.03f;
        fill_weight(w0, 3);
        fill_weight(w1, 11);
        int64_t packet[7] = {0, (int64_t) (uintptr_t) context, (int64_t) (uintptr_t) input, 0, 0, GGML_OP_DUP, 0};
        packet_reads = 0;
        require(gb_common((int64_t) (uintptr_t) packet) != 0 && packet_reads > 0, "positive packet-read witness");
        packet_reads = 0;
        int64_t work[16] = {0}, handles[5] = {0};
        float epsilon = 1e-6f;
        int32_t bits;
        std::memcpy(&bits, &epsilon, sizeof bits);
        ggml_tensor * result = (ggml_tensor *) (uintptr_t) ggt_chain(
            (int64_t) (uintptr_t) context, (int64_t) (uintptr_t) input,
            (int64_t) (uintptr_t) norm, (int64_t) (uintptr_t) w0, (int64_t) (uintptr_t) w1,
            GGML_OP_RMS_NORM, GGML_OP_MUL, GGML_OP_ADD, bits, {work, 16}, {handles, 5});
        require(result != nullptr && packet_reads == 0, "Goose chain used a native argument packet");
        ggml_tensor * reference[5];
        reference[0] = ggml_rms_norm(context, input, epsilon);
        reference[1] = ggml_mul(context, reference[0], norm);
        reference[2] = ggml_mul_mat(context, w0, reference[1]);
        reference[3] = ggml_mul_mat(context, w1, reference[2]);
        reference[4] = ggml_add(context, reference[3], input);
        for (int node = 0; node < 5; ++node) {
            ggml_tensor * candidate = (ggml_tensor *) (uintptr_t) handles[node];
            require(candidate != nullptr, "missing intermediate");
            if (candidate->op != reference[node]->op || candidate->type != reference[node]->type ||
                std::memcmp(candidate->ne, reference[node]->ne, sizeof candidate->ne) ||
                std::memcmp(candidate->nb, reference[node]->nb, sizeof candidate->nb) ||
                std::memcmp(candidate->op_params, reference[node]->op_params, sizeof candidate->op_params)) {
                std::fprintf(stderr, "META node=%d tokens=%d quantized=%d op=%d/%d type=%d/%d param0=%d/%d\n",
                             node, tokens, quantized, candidate->op, reference[node]->op, candidate->type, reference[node]->type,
                             candidate->op_params[0], reference[node]->op_params[0]);
                require(false, "intermediate metadata differs");
            }
            if (materialized) {
                ggml_set_output(candidate);
                ggml_set_output(reference[node]);
            }
        }
        ggml_cgraph * graph = ggml_new_graph(context);
        ggml_build_forward_expand(graph, result);
        ggml_build_forward_expand(graph, reference[4]);
        require(ggml_graph_compute_with_ctx(context, graph, 4) == GGML_STATUS_SUCCESS, "CPU graph compute");
        for (int node = 0; node < 5; ++node) {
            // The default RMS+MUL fusion deliberately does not write RMS alone.
            // The first arm requests every output and compares all five; this
            // second arm checks the default graph's materialized values/final.
            if (!materialized && node == 0) continue;
            ggml_tensor * candidate = (ggml_tensor *) (uintptr_t) handles[node];
            require(candidate && ggml_are_same_shape(candidate, reference[node]), "intermediate shape");
            const size_t bytes = ggml_nbytes(reference[node]);
            if (bytes != ggml_nbytes(candidate) || std::memcmp(candidate->data, reference[node]->data, bytes)) {
                const float * a = (const float *) candidate->data, * b = (const float *) reference[node]->data;
                double maximum = 0;
                int64_t first = -1;
                for (int64_t i = 0; i < ggml_nelements(candidate); ++i) {
                    if (std::memcmp(a + i, b + i, sizeof(float)) && first < 0) first = i;
                    maximum = std::fmax(maximum, std::fabs((double) a[i] - b[i]));
                }
                std::fprintf(stderr, "NUM node=%d tokens=%d quantized=%d first=%lld value=%.9g/%.9g max_abs=%.9g\n",
                             node, tokens, quantized, (long long) first, first < 0 ? 0 : a[first], first < 0 ? 0 : b[first], maximum);
                require(false, "intermediate FP32 bits differ");
            }
            for (int64_t i = 0; i < ggml_nelements(candidate); ++i) require(std::isfinite(((float *) candidate->data)[i]), "nonfinite output");
            compared += ggml_nelements(candidate);
        }
        require(std::memcmp(result->data, input->data, ggml_nbytes(input)) != 0, "chain is an input copy");
        ggml_free(context);
        ++cases;
    }
    std::printf("PASS Goose-owned graph chain cases=%d intermediate_values=%lld requested_outputs=1 default_fusion=1 packet_reads=0 positive_control=1\n",
                cases, (long long) compared);
}
