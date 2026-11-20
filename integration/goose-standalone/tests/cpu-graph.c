#define _POSIX_C_SOURCE 200809L

#include "graph-execute.h"
#include "goose-host-llm.h"
#include "main.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void fg_goose_host_ensure_init(void);
extern int64_t fg_cuda_model_state_bytes(void);
extern int64_t fg_cuda_dense_bytes(void);
extern int64_t fg_cuda_load_model_state(sl_i64 ranges, int64_t bytes);
extern int64_t fg_cuda_state_read(uint64_t host, int64_t bytes);
extern int64_t fg_cuda_state_write(uint64_t host, int64_t bytes);

static void check_model_step(const char * path) {
    fg_goose_host_ensure_init();
    hose_host_load(path);
    const int32_t invalid = 4;
    float * rejected = NULL;
    assert(hose_step(&invalid, 1, &rejected) == -1 && rejected == NULL);
    const float expected = 1.0f / sqrtf(1.0f / 32.0f + 1e-5f);
    const int32_t token = 2;
    for (int i = 0; i < 2; ++i) {
        float * logits = NULL;
        assert(hose_step(&token, 1, &logits) == 4);
        assert(logits);
        assert(logits[0] == 0.0f && logits[1] == 0.0f && logits[3] == 0.0f);
        assert(fabsf(logits[2] - expected) < 1e-5f);
        free(logits);
    }
    int64_t tokens[2] = {0};
    unsigned char out[8] = {0};
    assert(hose_generate((const unsigned char *) "ab", 2, 2, tokens, out, sizeof out, NULL) == 2);
    assert(tokens[0] == 2 && tokens[1] == 2 && memcmp(out, "abab", 4) == 0);
    printf("two steps, logit %.6f; generated 2,2 (abab)\n", expected);
}

static void check_chunked_generation(const char * path) {
    enum { prompt_size = 267, vocabulary = 4 };
    int64_t prompt[prompt_size], first = -1;
    int32_t batch[prompt_size];
    for (int i = 0; i < prompt_size; ++i) {
        batch[i] = (i * 7 + i / 5) % 3;
        prompt[i] = batch[i];
    }
    fg_goose_host_ensure_init();
    hose_host_load_context(path, 262144);
    assert(hose_context_capacity() == 262144);
    float * all = NULL, * expected_state = NULL, * actual_state = NULL;
    assert(hose_step(batch, prompt_size, &all) == prompt_size * vocabulary);
    const float * last = all + (prompt_size - 1) * vocabulary;
    int32_t next = 0;
    for (int i = 1; i < vocabulary; ++i) {
        if (last[i] > last[next]) next = i;
    }
    assert(hose_step(&next, 1, &expected_state) == vocabulary);
    double timing[3];
    assert(hose_generate_ids(prompt, prompt_size, 1, false, &first, timing) == 1);
    assert(first == next && timing[2] == prompt_size);
    assert(hose_step(&next, 1, &actual_state) == vocabulary);
    for (int i = 0; i < vocabulary; ++i) {
        assert(fabsf(expected_state[i] - actual_state[i]) < 1e-6f);
    }
    assert(fabsf(expected_state[next]) > 0.1f);
    free(all);
    free(expected_state);
    free(actual_state);
    printf("%d-token chunked generation preserves the next-step logits at 262144 context\n", prompt_size);
}

static void check_resident_state(const char * path) {
    fg_goose_host_ensure_init();
    hose_host_load_context(path, 256);
    assert(fg_cuda_model_state_bytes() > 0);
    assert(fg_cuda_dense_bytes() > 0);

    enum { steps = 4, vocab = 4 };
    const int32_t prompt[] = {2, 1, 0, 2, 1};
    const int32_t continuation[] = {2, 0, 1};
    float * device[steps] = {0};
    float * reference[steps] = {0};
    assert(hose_step(prompt, 5, &device[0]) == 5 * vocab);
    assert(hose_step(continuation, 1, &device[1]) == vocab);

    // Exercise an actual fallback between resident graph steps. The CPU has
    // to read the authoritative GDN matrix and then publish its own update.
    assert(setenv("GOOSE_CUDA", "0", 1) == 0);
    assert(hose_step(continuation + 1, 1, &device[2]) == vocab);
    assert(unsetenv("GOOSE_CUDA") == 0);
    assert(hose_step(continuation + 2, 1, &device[3]) == vocab);

    hose_reset_state();
    assert(setenv("GOOSE_CUDA", "0", 1) == 0);
    assert(hose_step(prompt, 5, &reference[0]) == 5 * vocab);
    for (int step = 1; step < steps; ++step) {
        assert(hose_step(continuation + step - 1, 1, &reference[step]) == vocab);
    }
    double max_error = 0.0;
    for (int step = 0; step < steps; ++step) {
        const int count = step == 0 ? 5 * vocab : vocab;
        for (int i = 0; i < count; ++i) {
            const double error = fabs((double) device[step][i] - reference[step][i]);
            assert(error < 1e-5);
            max_error = fmax(max_error, error);
        }
        free(device[step]);
        free(reference[step]);
    }
    assert(unsetenv("GOOSE_CUDA") == 0);
    printf("Resident KV/GDN reset and mixed CUDA/CPU continuation: max error %.9g\n", max_error);
}

static void check_resident_convolution(void) {
    enum { taps = 4, keep = taps - 1, channels = 131 };
    struct ggml_context * persistent = ggml_init((struct ggml_init_params){ 1024 * 1024, NULL, false });
    assert(persistent);
    struct ggml_tensor * history = ggml_new_tensor_2d(persistent, GGML_TYPE_F32, keep, channels);
    const uint64_t address = (uint64_t) (uintptr_t) history->data;
    const int64_t bytes = (int64_t) ggml_nbytes(history);
    int64_t ranges[] = {(int64_t) address, bytes, 0, 2};
    assert(fg_cuda_load_model_state((sl_i64) { ranges, 4 }, bytes) == bytes);
    float expected[channels * keep];
    for (int i = 0; i < channels * keep; ++i) expected[i] = (float) (i % 19 - 9) / 16.0f;
    memcpy(history->data, expected, sizeof(expected));
    assert(fg_cuda_state_write(address, bytes) == 0);

    const int chunks[] = {1, 2, 7, 1};
    for (size_t pass = 0; pass < sizeof(chunks) / sizeof(*chunks); ++pass) {
        const int n = chunks[pass];
        const int time = keep + n;
        const bool device = pass != 1;
        struct ggml_context * ctx = ggml_init((struct ggml_init_params){ 1024 * 1024, NULL, true });
        assert(ctx);
        struct ggml_tensor * projection = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, channels, n);
        struct ggml_tensor * scaled = ggml_scale(ctx, projection, 2.0f);
        struct ggml_tensor * input = ggml_concat(ctx, history, ggml_transpose(ctx, scaled), 0);
        struct ggml_tensor * tail = ggml_view_2d(ctx, input, keep, channels, input->nb[1], n * sizeof(float));
        struct ggml_tensor * store = ggml_cpy(ctx, tail, history);
        struct ggml_tensor * filter = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, taps, channels);
        struct ggml_tensor * conv = ggml_ssm_conv(ctx, input, filter);
        struct ggml_tensor * contiguous = ggml_cont(ctx, ggml_transpose(ctx, input));
        struct ggml_cgraph * graph = ggml_new_graph_custom(ctx, 128, false);
        ggml_build_forward_expand(graph, store);
        ggml_build_forward_expand(graph, conv);
        ggml_build_forward_expand(graph, contiguous);
        void * storage = fg_standalone_graph_alloc(graph);
        assert(storage);
        float * samples = projection->data;
        float * weights = filter->data;
        for (int i = 0; i < channels * n; ++i) samples[i] = (float) ((i + 17 * (int) pass) % 37 - 18) / 16.0f;
        for (int i = 0; i < taps * channels; ++i) weights[i] = (float) (i % 13 - 6) / 32.0f;
        // The registered device history is authoritative between graph steps.
        for (int i = 0; i < channels * keep; ++i) ((float *) history->data)[i] = -17.0f;
        if (!device) assert(setenv("GOOSE_CUDA", "0", 1) == 0);
        assert(fg_standalone_graph_compute(graph) == GGML_STATUS_SUCCESS);
        if (!device) assert(unsetenv("GOOSE_CUDA") == 0);
        const float * copied = contiguous->data;
        const float * observed = conv->data;
        for (int channel = 0; channel < channels; ++channel) {
            float want[keep + 7];
            for (int t = 0; t < time; ++t) {
                want[t] = t < keep ? expected[keep * channel + t] : 2.0f * samples[channels * (t - keep) + channel];
                assert(copied[t * channels + channel] == want[t]);
            }
            for (int t = 0; t < n; ++t) {
                float sum = 0.0f;
                for (int j = 0; j < taps; ++j) sum += want[t + j] * weights[taps * channel + j];
                assert(observed[t * channels + channel] == sum);
            }
            for (int j = 0; j < keep; ++j) {
                const int at = keep * channel + j;
                expected[at] = want[n + j];
                assert(((float *) history->data)[at] == (device ? -17.0f : expected[at]));
            }
        }
        assert(fg_cuda_state_read(address, bytes) == 0);
        assert(memcmp(history->data, expected, sizeof(expected)) == 0);
        free(storage);
        ggml_free(ctx);
    }
    puts("Resident SSM history matches all 393 values across CUDA/CPU continuation");
    ggml_free(persistent);
}

static void check_resident_q4_rows(void) {
    enum { width = 64, stride = width + 4, count = 4, capacity = 16, row_bytes = width / 32 * 18 };
    struct ggml_context * ctx = ggml_init((struct ggml_init_params){ 1024 * 1024, NULL, true });
    assert(ctx);
    struct ggml_tensor * input = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, stride, count);
    struct ggml_tensor * source = ggml_view_2d(ctx, input, width, count, input->nb[1], 0);
    struct ggml_tensor * ids = ggml_new_tensor_1d(ctx, GGML_TYPE_I64, count);
    struct ggml_tensor * cache = ggml_new_tensor_2d(ctx, GGML_TYPE_Q4_0, width, capacity);
    struct ggml_tensor * set = ggml_set_rows(ctx, cache, source, ids);
    struct ggml_cgraph * graph = ggml_new_graph_custom(ctx, 64, false);
    ggml_build_forward_expand(graph, set);
    void * storage = fg_standalone_graph_alloc(graph);
    assert(storage);
    const uint64_t address = (uint64_t) (uintptr_t) cache->data;
    const int64_t length = (int64_t) ggml_nbytes(cache);
    int64_t ranges[] = {(int64_t) address, length, 0, 0};
    assert(fg_cuda_load_model_state((sl_i64) {ranges, 4}, length) == length);

    unsigned char expected[capacity * row_bytes] = {0};
    memset(cache->data, 0, sizeof expected);
    const int64_t rows[2][count] = {{4, 5, 6, 7}, {8, 4, 8, 10}};
    for (int pass = 0; pass < 2; ++pass) {
        memcpy(ids->data, rows[pass], sizeof rows[pass]);
        for (int row = 0; row < count; ++row) {
            float * data = (float *) input->data + row * stride;
            for (int i = 0; i < width; ++i) {
                const float v = (float) ((i * 7 + row * 3 + pass * 11) % 17 - 8) / 4.0f;
                data[i] = pass ? v : row == 0 ? 0.0f : row == 3 ? ldexpf(v, -141) : v;
            }
            if (pass == 0 && row == 1) {
                data[0] = -2.0f;
                data[1] = 2.0f;
            }
            unsigned char * dest = expected + rows[pass][row] * row_bytes;
            assert(qz_quantize_row(GGML_TYPE_Q4_0, (uint64_t) (uintptr_t) data,
                (uint64_t) (uintptr_t) dest, width) == row_bytes);
        }
        assert(fg_standalone_graph_compute(graph) == GGML_STATUS_SUCCESS);
        // The first dispatch leaves the host copy untouched. The sparse
        // follow-up must retrieve preserved GPU rows before uploading the
        // containing interval.
        if (pass == 0) assert(memcmp(cache->data, expected, sizeof expected) != 0);
    }
    assert(fg_cuda_state_read(address, length) == 0);
    assert(memcmp(cache->data, expected, sizeof expected) == 0);
    puts("Resident Q4 writes: packed GPU bytes and sparse CPU updates match exactly");
    free(storage);
    ggml_free(ctx);
}

static void check_scalar_and_recurrent(void) {
    struct ggml_context * ctx = ggml_init((struct ggml_init_params){ 1024 * 1024, NULL, true });
    assert(ctx);
    struct ggml_tensor * a = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 4);
    struct ggml_tensor * b = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 4);
    struct ggml_tensor * add = ggml_add(ctx, a, b);
    struct ggml_tensor * mul = ggml_mul(ctx, add, b);
    struct ggml_tensor * view = ggml_view_1d(ctx, mul, 2, sizeof(float));
    struct ggml_tensor * copy = ggml_cpy(ctx, mul, ggml_new_tensor_2d(ctx, GGML_TYPE_F32, 2, 2));
    struct ggml_tensor * q = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, 1, 1, 1, 1);
    struct ggml_tensor * k = ggml_dup_tensor(ctx, q);
    struct ggml_tensor * v = ggml_dup_tensor(ctx, q);
    struct ggml_tensor * gate = ggml_dup_tensor(ctx, q);
    struct ggml_tensor * beta = ggml_dup_tensor(ctx, q);
    struct ggml_tensor * initial = ggml_dup_tensor(ctx, q);
    struct ggml_tensor * gdn = ggml_gated_delta_net(ctx, q, k, v, gate, beta, initial, 1);
    struct ggml_cgraph * graph = ggml_new_graph_custom(ctx, 64, false);
    ggml_build_forward_expand(graph, view);
    ggml_build_forward_expand(graph, copy);
    ggml_build_forward_expand(graph, gdn);
    void * storage = fg_standalone_graph_alloc(graph);
    assert(storage);
    memcpy(a->data, (float[]){1, 2, 3, 4}, sizeof(float) * 4);
    memcpy(b->data, (float[]){2, 3, 4, 5}, sizeof(float) * 4);
    *(float *) q->data = 2;
    *(float *) k->data = 1;
    *(float *) v->data = 3;
    *(float *) gate->data = 0;
    *(float *) beta->data = 0.5f;
    *(float *) initial->data = 1;
    assert(fg_standalone_graph_compute(graph) == GGML_STATUS_SUCCESS);
    assert(memcmp(view->data, (float[]){15, 28}, sizeof(float) * 2) == 0);
    assert(memcmp(copy->data, (float[]){6, 15, 28, 45}, sizeof(float) * 4) == 0);
    assert(memcmp(gdn->data, (float[]){4, 2}, sizeof(float) * 2) == 0);
    puts("ADD/MUL/VIEW 15,28; GDN 4,2");
    free(storage);
    ggml_free(ctx);
}

static void check_gdn_snapshots(int width, bool column_gate) {
    enum { heads = 2, tokens = 3, seqs = 2, snapshots = 2 };
    const int square = width * width, scores = width * heads * tokens * seqs, state_size = square * heads * seqs;
    struct ggml_context * ctx = ggml_init((struct ggml_init_params){ 1024 * 1024, NULL, true });
    assert(ctx);
    struct ggml_tensor * qk_storage = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, width + 1, tokens);
    struct ggml_tensor * q = ggml_view_4d(ctx, qk_storage, width, 1, tokens, 1,
        qk_storage->nb[1], qk_storage->nb[1], qk_storage->nb[2], 0);
    struct ggml_tensor * k_data = ggml_dup_tensor(ctx, width == 33 ? q : qk_storage);
    struct ggml_tensor * k = width == 33 ? k_data :
        ggml_view_4d(ctx, k_data, width, 1, tokens, 1, k_data->nb[1], k_data->nb[1], k_data->nb[2], 0);
    struct ggml_tensor * v = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, width, heads, tokens, seqs);
    struct ggml_tensor * gate = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, column_gate ? width : 1, heads, tokens, seqs);
    struct ggml_tensor * beta = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, 1, heads, tokens, seqs);
    struct ggml_tensor * initial = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, width, width, heads, seqs);
    struct ggml_tensor * gdn = ggml_gated_delta_net(ctx, q, k, v, gate, beta, initial, snapshots);
    struct ggml_cgraph * graph = ggml_new_graph_custom(ctx, 64, false);
    ggml_build_forward_expand(graph, gdn);
    void * storage = fg_standalone_graph_alloc(graph);
    assert(storage);

    struct ggml_tensor * inputs[] = { qk_storage, k_data, v, gate, beta, initial };
    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        float * data = inputs[i]->data;
        for (int j = 0; j < ggml_nelements(inputs[i]); ++j) {
            data[j] = (float) ((5 * j + (int) i) % 13 - 6) / 64.0f;
        }
    }
    assert(fg_standalone_graph_compute(graph) == GGML_STATUS_SUCCESS);
    const float * actual = gdn->data;
    const float * queries = q->data, * keys = k->data, * values = v->data;
    const float * gates = gate->data, * rates = beta->data;
    float max_error = 0.0f;
    for (int seq = 0; seq < seqs; ++seq) {
        for (int head = 0; head < heads; ++head) {
            const int task = seq * heads + head;
            float expected[square];
            memcpy(expected, (float *) initial->data + task * square, sizeof expected);
            for (int token = 0; token < tokens; ++token) {
                const int input = (seq * tokens + token) * heads + head;
                const float * query = queries + token * (width + 1);
                const float * key = keys + token * (k->nb[2] / sizeof(float));
                const float * gate_row = gates + input * gate->ne[0];
                for (int row = 0; row < width; ++row) {
                    float * state = expected + row * width;
                    float predicted = 0.0f;
                    for (int col = 0; col < width; ++col) {
                        state[col] *= expf(gate_row[column_gate ? col : 0]);
                        predicted = fmaf(key[col], state[col], predicted);
                    }
                    const float delta = (values[input * width + row] - predicted) * rates[input];
                    float score = 0.0f;
                    for (int col = 0; col < width; ++col) {
                        state[col] = fmaf(key[col], delta, state[col]);
                        score = fmaf(query[col], state[col], score);
                    }
                    const float error = fabsf(actual[input * width + row] - score / sqrtf(width));
                    assert(error < 1e-6f);
                    max_error = fmaxf(max_error, error);
                }
                const int slot = tokens - 1 - token;
                if (slot < snapshots) {
                    const float * actual_state = actual + scores + slot * state_size + task * square;
                    for (int i = 0; i < square; ++i) {
                        const float error = fabsf(actual_state[i] - expected[i]);
                        assert(error < 1e-6f);
                        max_error = fmaxf(max_error, error);
                    }
                }
            }
        }
    }
    printf("GDN width %d, %s gate, snapshots: max error %.9g\n", width,
           column_gate ? "column" : "scalar", (double) max_error);
    free(storage);
    ggml_free(ctx);
}

static void check_quantized_cache_attention(void) {
    struct ggml_context * ctx = ggml_init((struct ggml_init_params){ 1024 * 1024, NULL, true });
    assert(ctx);
    struct ggml_tensor * q = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, 32, 2);
    struct ggml_tensor * k = ggml_new_tensor_2d(ctx, GGML_TYPE_Q4_0, 32, 2);
    struct ggml_tensor * v = ggml_dup_tensor(ctx, k);
    struct ggml_tensor * rows = ggml_dup_tensor(ctx, q);
    struct ggml_tensor * indices = ggml_new_tensor_1d(ctx, GGML_TYPE_I64, 2);
    struct ggml_tensor * mask = ggml_new_tensor_2d(ctx, GGML_TYPE_F16, 2, 2);
    struct ggml_tensor * set = ggml_set_rows(ctx, v, rows, indices);
    struct ggml_tensor * output = ggml_flash_attn_ext(ctx, q, k, set, mask, 1.0f, 0.0f, 0.0f);
    struct ggml_cgraph * graph = ggml_new_graph_custom(ctx, 64, false);
    ggml_build_forward_expand(graph, output);
    void * storage = fg_standalone_graph_alloc(graph);
    assert(storage);
    memset(q->data, 0, ggml_nbytes(q));
    memset(k->data, 0, ggml_nbytes(k));
    const int64_t ids[] = {0, 1};
    memcpy(indices->data, ids, sizeof ids);
    const ggml_fp16_t biases[] = { 0, 0, ggml_fp32_to_fp16(-INFINITY), 0 };
    memcpy(mask->data, biases, sizeof biases);
    for (int i = 0; i < 64; ++i) ((float *) rows->data)[i] = i < 32 ? 1.0f : 3.0f;
    assert(fg_standalone_graph_compute(graph) == GGML_STATUS_SUCCESS);
    for (int i = 0; i < 64; ++i) assert(((float *) output->data)[i] == (i < 32 ? 2.0f : 3.0f));
    puts("Q4 cache attention: mean 2, masked 3");
    free(storage);
    ggml_free(ctx);
}

static void check_split_q4_attention(void) {
    enum { width = 256, kv_heads = 2, heads = 4, keys = 517, positions = 3 };
    struct ggml_context * ctx = ggml_init((struct ggml_init_params){ 1024 * 1024, NULL, true });
    assert(ctx);
    struct ggml_tensor * q_data = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, width, heads, positions);
    struct ggml_tensor * k_data = ggml_new_tensor_3d(ctx, GGML_TYPE_Q4_0, width, kv_heads, keys);
    struct ggml_tensor * v_data = ggml_dup_tensor(ctx, k_data);
    struct ggml_tensor * q = ggml_permute(ctx, q_data, 0, 2, 1, 3);
    struct ggml_tensor * k = ggml_permute(ctx, k_data, 0, 2, 1, 3);
    struct ggml_tensor * v = ggml_permute(ctx, v_data, 0, 2, 1, 3);
    struct ggml_tensor * mask = ggml_new_tensor_2d(ctx, GGML_TYPE_F16, keys, positions);
    const float scale = 1.0f / sqrtf(width);
    struct ggml_tensor * out = ggml_flash_attn_ext(ctx, q, k, v, mask, scale, 0.0f, 0.0f);
    struct ggml_tensor * causal = ggml_flash_attn_ext(ctx, q, k, v, NULL, scale, 0.0f, 0.0f);
    causal->op_params[5] = 1;
    struct ggml_cgraph * graph = ggml_new_graph_custom(ctx, 64, false);
    ggml_build_forward_expand(graph, out);
    ggml_build_forward_expand(graph, causal);
    void * storage = fg_standalone_graph_alloc(graph);
    assert(storage);
    float * query = q_data->data;
    for (int i = 0; i < positions * heads * width; ++i) {
        query[i] = (float) ((13 * i + i / 19) % 27 - 13) / 16.0f;
    }
    const struct ggml_type_traits * q4 = ggml_get_type_traits(GGML_TYPE_Q4_0);
    float * decoded_k = malloc(keys * kv_heads * width * sizeof(float));
    float * decoded_v = malloc(keys * kv_heads * width * sizeof(float));
    assert(decoded_k && decoded_v);
    for (int row = 0; row < keys * kv_heads; ++row) {
        float src[width];
        for (int i = 0; i < width; ++i) src[i] = (float) ((17 * row + i * 7) % 21 - 10) / 8.0f;
        unsigned char * kp = (unsigned char *) k_data->data + row * k_data->nb[1];
        q4->from_float_ref(src, kp, width);
        q4->to_float(kp, decoded_k + row * width, width);
        for (int i = 0; i < width; ++i) src[i] = (float) ((3 * row + i * 11) % 31 - 11) / 8.0f;
        unsigned char * vp = (unsigned char *) v_data->data + row * v_data->nb[1];
        q4->from_float_ref(src, vp, width);
        q4->to_float(vp, decoded_v + row * width, width);
    }
    ggml_fp16_t * bias = mask->data;
    for (int p = 0; p < positions; ++p) {
        for (int row = 0; row < keys; ++row) {
            const bool valid = p == 0 ? row % 13 != 0 : p == 1 && row >= 256;
            bias[p * keys + row] = ggml_fp32_to_fp16(valid ? (float) (row % 7 - 3) / 8.0f : -INFINITY);
        }
    }
    assert(fg_standalone_graph_compute(graph) == GGML_STATUS_SUCCESS);
    const float * actual = out->data;
    double peak_error = 0.0;
    for (int p = 0; p < positions; ++p) {
        for (int h = 0; h < heads; ++h) {
            const float * qi = query + (p * heads + h) * width;
            const int group = h / (heads / kv_heads);
            double total = 0.0, sum[width] = {0};
            for (int row = 0; row < keys; ++row) {
                const float b = ggml_fp16_to_fp32(bias[p * keys + row]);
                if (b == -INFINITY) continue;
                const int at = (row * kv_heads + group) * width;
                double dot = 0.0;
                for (int i = 0; i < width; ++i) dot += (double) qi[i] * decoded_k[at + i];
                const double weight = exp(dot * scale + b);
                total += weight;
                for (int i = 0; i < width; ++i) sum[i] += weight * decoded_v[at + i];
            }
            for (int i = 0; i < width; ++i) {
                const double expected = total > 0.0 ? sum[i] / total : 0.0;
                const double error = fabs(actual[(p * heads + h) * width + i] - expected);
                assert(error < 3e-5);
                peak_error = fmax(peak_error, error);
            }
        }
    }
    printf("Split Q4 attention: 517 keys, GQA, padded and fully masked rows, max error %.9g\n", peak_error);
    const int prefixes[] = {0, 254, 255, 510, 514};
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(*prefixes); ++i) {
        const int prefix = prefixes[i];
        causal->op_params[5] = prefix + 1;
        for (int pos = 0; pos < positions; ++pos) {
            for (int row = 0; row < keys; ++row) {
                bias[pos * keys + row] = ggml_fp32_to_fp16(row <= prefix + pos ? 0.0f : -INFINITY);
            }
        }
        assert(fg_standalone_graph_compute(graph) == GGML_STATUS_SUCCESS);
        assert(memcmp(out->data, causal->data, ggml_nbytes(out)) == 0);
    }
    puts("Implicit Q4 causal attention matches the explicit mask bytes at five prefix boundaries");
    free(decoded_k);
    free(decoded_v);
    free(storage);
    ggml_free(ctx);
}

static void check_routed_matrix(enum ggml_type kind, bool indexed) {
    struct ggml_context * ctx = ggml_init((struct ggml_init_params){ 1024 * 1024, NULL, true });
    assert(ctx);
    const bool f32 = kind == GGML_TYPE_F32;
    const int width = f32 ? (indexed ? 17 : 99) : 256;
    const int64_t planes = indexed ? 3 : 2;
    const int64_t batches = indexed ? 1 : 2;
    struct ggml_tensor * a_data = ggml_new_tensor_3d(ctx, kind, width + (f32 ? 3 : 0), 2, planes);
    struct ggml_tensor * b_data = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, width + (f32 ? 5 : 0), 2, 4, batches);
    struct ggml_tensor * a = f32 ? ggml_view_4d(ctx, a_data, width, 2, planes, 1,
        a_data->nb[1], a_data->nb[2], a_data->nb[3], 0) : a_data;
    struct ggml_tensor * b = f32 ? ggml_view_4d(ctx, b_data, width, 2, 4, batches,
        b_data->nb[1], b_data->nb[2], b_data->nb[3], 0) : b_data;
    struct ggml_tensor * ids = indexed ? ggml_new_tensor_2d(ctx, GGML_TYPE_I32, 4, 4) : NULL;
    struct ggml_tensor * product = indexed ? ggml_mul_mat_id(ctx, a, b, ids) : ggml_mul_mat(ctx, a, b);
    struct ggml_cgraph * graph = ggml_new_graph_custom(ctx, 64, false);
    ggml_build_forward_expand(graph, product);
    void * storage = fg_standalone_graph_alloc(graph);
    assert(storage);
    const struct ggml_type_traits * traits = ggml_get_type_traits(kind);
    float original[width], decoded[width];
    for (int i = 0; i < ggml_nelements(b_data); ++i) {
        ((float *) b_data->data)[i] = f32 ? (float) ((i * 5 + i / 13) % 27 - 13) / 31.0f :
            (float) ((i + i / width) % 7 - 3) * 0.125f;
    }
    if (indexed) {
        for (int i = 0; i < 16; ++i) {
            ((int32_t *) ids->data)[i] = (i / 4 + i % 4) % 2;
        }
    }
    for (int p = 0; p < planes; ++p) {
        if (indexed && p == 2) continue;
        for (int row = 0; row < 2; ++row) {
            for (int i = 0; i < width; ++i) {
                original[i] = f32 ? (float) ((i * 7 + row + 3 * p) % 41 - 20) / 37.0f :
                    (float) ((i + row + 3 * p) % 13 - 6) * 0.25f;
            }
            unsigned char * packed = (unsigned char *) a->data + (size_t) p * a->nb[2] + (size_t) row * a->nb[1];
            if (f32) memcpy(packed, original, sizeof original);
            else traits->from_float_ref(original, packed, width);
        }
    }
    const char * setting = getenv("GOOSE_CUDA");
    const bool compare = f32 && (!setting || strcmp(setting, "0") != 0);
    void * reference = NULL;
    if (compare) {
        reference = malloc(ggml_nbytes(product));
        assert(reference);
        assert(setenv("GOOSE_CUDA", "0", 1) == 0);
        assert(fg_standalone_graph_compute(graph) == GGML_STATUS_SUCCESS);
        memcpy(reference, product->data, ggml_nbytes(product));
        assert(unsetenv("GOOSE_CUDA") == 0);
    }
    assert(fg_standalone_graph_compute(graph) == GGML_STATUS_SUCCESS);
    if (reference) {
        assert(memcmp(reference, product->data, ggml_nbytes(product)) == 0);
        free(reference);
    }
    for (int i3 = 0; i3 < product->ne[3]; ++i3) {
        for (int i2 = 0; i2 < product->ne[2]; ++i2) {
            for (int i1 = 0; i1 < product->ne[1]; ++i1) {
                int p = indexed ? ((int32_t *) ids->data)[4 * i2 + i1] : i2 / 2;
                const float * right = (const float *) ((unsigned char *) b->data +
                    (size_t) i3 * b->nb[3] + (size_t) i2 * b->nb[2] + (size_t) (i1 % 2) * b->nb[1]);
                const float * got = (const float *) ((unsigned char *) product->data +
                    (size_t) i3 * product->nb[3] + (size_t) i2 * product->nb[2] + (size_t) i1 * product->nb[1]);
                for (int row = 0; row < 2; ++row) {
                    const unsigned char * packed = (const unsigned char *) a->data +
                        (size_t) p * a->nb[2] + (size_t) row * a->nb[1];
                    if (f32) memcpy(decoded, packed, sizeof decoded);
                    else traits->to_float(packed, decoded, width);
                    double expected = 0.0;
                    for (int i = 0; i < width; ++i) expected += (double) decoded[i] * right[i];
                    assert(fabs(got[row] - expected) < 1e-5);
                }
            }
        }
    }
    printf("%s %s at width %d matches reference%s\n", ggml_type_name(kind),
           indexed ? "indexed" : "broadcast", width, compare ? " and CPU result bytes" : "");
    free(storage);
    ggml_free(ctx);
}

static void check_activation_pipeline(void) {
    enum { width = 32, rows = 3, chain = 260 };
    struct ggml_context * ctx = ggml_init((struct ggml_init_params){ 1024 * 1024, NULL, true });
    assert(ctx);
    struct ggml_tensor * identity = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, width, width);
    struct ggml_tensor * input = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, width, rows);
    struct ggml_tensor * bias = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, width);
    struct ggml_tensor * first = ggml_mul_mat(ctx, identity, input);
    struct ggml_tensor * view = ggml_view_2d(ctx, first, width, rows - 1, first->nb[1], first->nb[1]);
    struct ggml_tensor * second = ggml_mul_mat(ctx, identity, view);
    struct ggml_tensor * current = ggml_cpy(ctx, ggml_add(ctx, second, bias), second);
    struct ggml_tensor * previous = NULL;
    for (int i = 0; i < chain; ++i) {
        previous = current;
        current = ggml_mul_mat(ctx, identity, current);
    }
    struct ggml_cgraph * graph = ggml_new_graph_custom(ctx, 512, false);
    ggml_build_forward_expand(graph, current);
    void * storage = fg_standalone_graph_alloc(graph);
    assert(storage && previous);

    memset(identity->data, 0, ggml_nbytes(identity));
    for (int i = 0; i < width; ++i) ((float *) identity->data)[i * (width + 1)] = 1.0f;
    for (int i = 0; i < rows * width; ++i) ((float *) input->data)[i] = (float) (i - 7) / 4.0f;
    for (int i = 0; i < width; ++i) ((float *) bias->data)[i] = 2.0f;
    memset(previous->data, 0xA5, ggml_nbytes(previous));
    assert(fg_standalone_graph_compute_output(graph, current) == GGML_STATUS_SUCCESS);
    for (int i = 0; i < (rows - 1) * width; ++i) {
        const float expected = ((float *) input->data)[i + width] + 2.0f;
        assert(((float *) current->data)[i] == expected);
    }
    for (size_t i = 0; i < ggml_nbytes(previous); ++i) {
        assert(((const unsigned char *) previous->data)[i] == 0xA5);
    }
    assert(fg_standalone_graph_compute(graph) == GGML_STATUS_SUCCESS);
    assert(memcmp(previous->data, current->data, ggml_nbytes(current)) == 0);
    puts("CUDA activation pipeline: views, CPU overwrite, arena rollover and selective output verified");
    free(storage);
    ggml_free(ctx);
}

static void check_cuda_vectors(void) {
    enum { width = 257, rows = 3, planes = 2 };
    struct ggml_context * ctx = ggml_init((struct ggml_init_params){ 1024 * 1024, NULL, true });
    assert(ctx);
    struct ggml_tensor * input = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, width + 3, rows, planes);
    struct ggml_tensor * x = ggml_view_3d(ctx, input, width, rows, planes, input->nb[1], input->nb[2], 0);
    struct ggml_tensor * vec = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, width);
    struct ggml_tensor * packed = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, 2 * width, rows, planes);
    struct ggml_tensor * add = ggml_add(ctx, x, vec);
    struct ggml_tensor * outputs[] = {
        add, ggml_sub(ctx, x, vec), ggml_mul(ctx, x, vec), ggml_div(ctx, x, vec),
        ggml_scale_bias(ctx, x, 1.125f, -0.4375f), ggml_clamp(ctx, x, -2.0f, 3.0f),
        ggml_sigmoid(ctx, x), ggml_silu(ctx, x), ggml_softplus(ctx, x),
        ggml_swiglu_split(ctx, x, add), ggml_swiglu(ctx, packed), ggml_swiglu_swapped(ctx, packed),
        ggml_rms_norm(ctx, x, 1e-5f), ggml_l2_norm(ctx, x, 1e-5f),
    };
    const size_t count = sizeof(outputs) / sizeof(*outputs);
    struct ggml_cgraph * graph = ggml_new_graph_custom(ctx, 128, false);
    for (size_t i = 0; i < count; ++i) ggml_build_forward_expand(graph, outputs[i]);
    void * storage = fg_standalone_graph_alloc(graph);
    assert(storage);
    for (int i = 0; i < ggml_nelements(input); ++i) {
        ((float *) input->data)[i] = i < width ? 0.0f : (float) ((i * 7) % 37 - 18) / 8.0f;
    }
    ((float *) input->data)[width + 3] = 25.0f;
    ((float *) input->data)[width + 4] = -25.0f;
    for (int i = 0; i < width; ++i) ((float *) vec->data)[i] = 0.25f + (float) (i % 19) / 11.0f;
    for (int i = 0; i < ggml_nelements(packed); ++i) {
        ((float *) packed->data)[i] = (float) ((3 * i + i / 7) % 33 - 16) / 13.0f;
    }

    assert(setenv("GOOSE_CUDA", "0", 1) == 0);
    assert(fg_standalone_graph_compute(graph) == GGML_STATUS_SUCCESS);
    float * reference[count];
    for (size_t i = 0; i < count; ++i) {
        reference[i] = malloc(ggml_nbytes(outputs[i]));
        assert(reference[i]);
        memcpy(reference[i], outputs[i]->data, ggml_nbytes(outputs[i]));
    }
    assert(unsetenv("GOOSE_CUDA") == 0);
    assert(fg_standalone_graph_compute(graph) == GGML_STATUS_SUCCESS);
    double max_error = 0.0;
    for (size_t j = 0; j < count; ++j) {
        for (int i = 0; i < ggml_nelements(outputs[j]); ++i) {
            const double expected = reference[j][i];
            const double error = fabs((double) ((float *) outputs[j]->data)[i] - expected);
            assert(error < 3e-6 * fmax(1.0, fabs(expected)));
            max_error = fmax(max_error, error);
        }
        free(reference[j]);
    }
    printf("CUDA vector operations: %zu cases across strided rows, max error %.9g\n", count, max_error);
    free(storage);
    ggml_free(ctx);
}

static void check_cuda_position(void) {
    enum { width = 128, heads = 3, tokens = 7, sequences = 2, channels = 131 };
    const int modes[] = {
        GGML_ROPE_TYPE_NORMAL, GGML_ROPE_TYPE_NEOX, GGML_ROPE_TYPE_MROPE,
        GGML_ROPE_TYPE_IMROPE, GGML_ROPE_TYPE_VISION,
    };
    const int taps[] = { 3, 4, 5, 7, 9, 15 };
    struct ggml_context * ctx = ggml_init((struct ggml_init_params){ 1024 * 1024, NULL, true });
    assert(ctx);
    struct ggml_cgraph * graph = ggml_new_graph_custom(ctx, 128, false);
    struct ggml_tensor * positions = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, 4 * tokens);
    struct ggml_tensor * factors = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, width / 2);
    struct ggml_tensor * rope_inputs[2];
    struct ggml_tensor * outputs[16];
    size_t count = 0;
    for (int half = 0; half < 2; ++half) {
        struct ggml_tensor * src = ggml_new_tensor_4d(ctx, half ? GGML_TYPE_F16 : GGML_TYPE_F32,
                width + 3, heads, tokens, sequences);
        rope_inputs[half] = src;
        struct ggml_tensor * x = ggml_view_4d(ctx, src, width, heads, tokens, sequences,
                src->nb[1], src->nb[2], src->nb[3], 0);
        for (size_t i = 0; i < sizeof(modes) / sizeof(*modes); ++i) {
            const int mode = modes[i];
            const int dims = mode == GGML_ROPE_TYPE_VISION ? width / 2 : 48;
            int sections[4] = { 12, 8, 4, 0 };
            struct ggml_tensor * pos = (mode & GGML_ROPE_TYPE_MROPE) ? positions :
                ggml_view_1d(ctx, positions, tokens, 0);
            struct ggml_tensor * result = half ?
                ggml_rope_multi_back(ctx, x, pos, factors, dims, sections, mode,
                        4096, 10000.0f, 0.125f, 1.0f, 1.1f, 32.0f, 1.0f) :
                ggml_rope_multi(ctx, x, pos, factors, dims, sections, mode,
                        4096, 10000.0f, 0.125f, 1.0f, 1.1f, 32.0f, 1.0f);
            if (mode != GGML_ROPE_TYPE_VISION) ggml_rope_set_offset(result, 6);
            outputs[count++] = result;
        }
    }
    const size_t rope_count = count;
    struct ggml_tensor * inputs[6];
    struct ggml_tensor * weights[6];
    for (size_t i = 0; i < sizeof(taps) / sizeof(*taps); ++i) {
        const int time = taps[i] - 1 + tokens;
        inputs[i] = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, time + 2, channels, sequences);
        struct ggml_tensor * x = ggml_view_3d(ctx, inputs[i], time, channels, sequences,
                inputs[i]->nb[1], inputs[i]->nb[2], 0);
        weights[i] = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, taps[i] + 1, channels);
        struct ggml_tensor * w = ggml_view_2d(ctx, weights[i], taps[i], channels, weights[i]->nb[1], 0);
        outputs[count++] = ggml_ssm_conv(ctx, x, w);
    }
    assert(count == sizeof(outputs) / sizeof(*outputs));
    for (size_t i = 0; i < count; ++i) ggml_build_forward_expand(graph, outputs[i]);
    void * storage = fg_standalone_graph_alloc(graph);
    assert(storage);
    const int steps[tokens] = { 0, 1, 123, 8192, 32767, 32768, 262144 };
    for (int i = 0; i < 4 * tokens; ++i) {
        ((int32_t *) positions->data)[i] = steps[i % tokens] + 1237 * (i / tokens);
    }
    for (int i = 0; i < width / 2; ++i) ((float *) factors->data)[i] = 0.9375f + i / 64.0f;
    for (int half = 0; half < 2; ++half) {
        for (int i = 0; i < ggml_nelements(rope_inputs[half]); ++i) {
            const float value = (float) ((13 * i + i / 7) % 41 - 20) / 16.0f;
            if (half) ((ggml_fp16_t *) rope_inputs[half]->data)[i] = ggml_fp32_to_fp16(value);
            else ((float *) rope_inputs[half]->data)[i] = value;
        }
    }
    for (size_t j = 0; j < sizeof(taps) / sizeof(*taps); ++j) {
        for (int i = 0; i < ggml_nelements(inputs[j]); ++i) {
            ((float *) inputs[j]->data)[i] = (float) ((7 * i + i / 13) % 37 - 18) / 17.0f;
        }
        for (int i = 0; i < ggml_nelements(weights[j]); ++i) {
            ((float *) weights[j]->data)[i] = (float) ((19 * i) % 43 - 21) / 32.0f;
        }
    }
    assert(setenv("GOOSE_CUDA", "0", 1) == 0);
    assert(fg_standalone_graph_compute(graph) == GGML_STATUS_SUCCESS);
    void * reference[16];
    for (size_t i = 0; i < count; ++i) {
        reference[i] = malloc(ggml_nbytes(outputs[i]));
        assert(reference[i]);
        memcpy(reference[i], outputs[i]->data, ggml_nbytes(outputs[i]));
    }
    assert(unsetenv("GOOSE_CUDA") == 0);
    assert(fg_standalone_graph_compute(graph) == GGML_STATUS_SUCCESS);
    double f32_rope_error = 0.0, f16_rope_error = 0.0, conv_error = 0.0;
    for (size_t j = 0; j < count; ++j) {
        const bool half = outputs[j]->type == GGML_TYPE_F16;
        double * max_error = j >= rope_count ? &conv_error : half ? &f16_rope_error : &f32_rope_error;
        for (int i = 0; i < ggml_nelements(outputs[j]); ++i) {
            const double expected = half ? ggml_fp16_to_fp32(((ggml_fp16_t *) reference[j])[i]) :
                ((float *) reference[j])[i];
            const double actual = half ? ggml_fp16_to_fp32(((ggml_fp16_t *) outputs[j]->data)[i]) :
                ((float *) outputs[j]->data)[i];
            const double error = fabs(actual - expected);
            if (!(error <= (half ? 1e-3 : 4e-6) * fmax(1.0, fabs(expected)))) {
                fprintf(stderr, "CUDA position case %zu element %d, expected %.9g, got %.9g\n",
                        j, i, expected, actual);
                abort();
            }
            *max_error = fmax(*max_error, error);
        }
        free(reference[j]);
    }
    printf("CUDA position operations: %zu RoPE, %zu SSM convolution; max errors F32 %.9g, F16 %.9g, SSM %.9g\n",
            rope_count, count - rope_count, f32_rope_error, f16_rope_error, conv_error);
    free(storage);
    ggml_free(ctx);
}

static void check_cuda_routing(void) {
    enum { experts = 257, selected = 11, tokens = 5, sequences = 2 };
    struct ggml_context * ctx = ggml_init((struct ggml_init_params){ 1024 * 1024, NULL, true });
    assert(ctx);
    struct ggml_tensor * logits = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, experts, tokens, sequences);
    struct ggml_tensor * probs = ggml_soft_max_ext(ctx, logits, NULL, 0.75f, 0.0f);
    struct ggml_tensor * ids = ggml_top_k(ctx, probs, selected);
    struct ggml_tensor * old = ggml_argsort_top_k(ctx, probs, selected);
    struct ggml_tensor * bank = ggml_reshape_4d(ctx, probs, 1, experts, tokens, sequences);
    struct ggml_tensor * picked = ggml_get_rows(ctx, bank, ids);
    struct ggml_tensor * weights = ggml_reshape_3d(ctx, picked, selected, tokens, sequences);
    struct ggml_tensor * sums = ggml_sum_rows(ctx, weights);
    struct ggml_tensor * normalized = ggml_div(ctx, weights, ggml_clamp(ctx, sums, 0x1p-14f, INFINITY));
    struct ggml_tensor * order_bank = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, 23, 2);
    struct ggml_tensor * order_src = ggml_view_2d(ctx, order_bank, 19, 2, order_bank->nb[1], 0);
    struct ggml_tensor * ascending = ggml_argsort(ctx, order_src, GGML_SORT_ORDER_ASC);
    struct ggml_tensor * unordered_top = ggml_top_k(ctx, order_src, 7);
    struct ggml_cgraph * graph = ggml_new_graph_custom(ctx, 128, false);
    ggml_build_forward_expand(graph, normalized);
    ggml_build_forward_expand(graph, old);
    ggml_build_forward_expand(graph, ascending);
    ggml_build_forward_expand(graph, unordered_top);
    void * storage = fg_standalone_graph_alloc(graph);
    assert(storage);
    for (int row = 0; row < tokens * sequences; ++row) {
        float * values = (float *) logits->data + row * experts;
        for (int col = 0; col < experts; ++col) {
            values[col] = row == 0 ? 1.0f : (float) ((13 * col + 7 * row) % 43 - 21) / 2;
        }
        if (row != 0) values[(row * 11) % experts] = -INFINITY;
    }
    for (int i = 0; i < ggml_nelements(order_bank); ++i) {
        ((float *) order_bank->data)[i] = i == 4 || i == 26 ? NAN :
            i == 23 ? -INFINITY : (float) ((13 * i) % 7 - 3);
    }

    assert(ggml_nbytes(ids) == 4 * selected * tokens * sequences);
    assert(setenv("GOOSE_CUDA", "0", 1) == 0);
    assert(fg_standalone_graph_compute(graph) == GGML_STATUS_SUCCESS);
    int32_t expected_ids[selected * tokens * sequences];
    int32_t expected_asc[38], expected_unordered[14];
    float expected_probs[experts * tokens * sequences], expected_weights[selected * tokens * sequences];
    memcpy(expected_ids, ids->data, sizeof(expected_ids));
    memcpy(expected_asc, ascending->data, sizeof(expected_asc));
    memcpy(expected_unordered, unordered_top->data, sizeof(expected_unordered));
    memcpy(expected_probs, probs->data, sizeof(expected_probs));
    memcpy(expected_weights, normalized->data, sizeof(expected_weights));
    for (int row = 0; row < tokens * sequences; ++row) {
        assert(memcmp(expected_ids + row * selected, (const int32_t *) old->data + row * experts,
                selected * sizeof(int32_t)) == 0);
    }
    for (int i = 0; i < selected; ++i) assert(expected_ids[i] == i);

    assert(unsetenv("GOOSE_CUDA") == 0);
    assert(fg_standalone_graph_compute(graph) == GGML_STATUS_SUCCESS);
    assert(memcmp(expected_ids, ids->data, sizeof(expected_ids)) == 0);
    assert(memcmp(expected_asc, ascending->data, sizeof(expected_asc)) == 0);
    assert(memcmp(expected_unordered, unordered_top->data, sizeof(expected_unordered)) == 0);
    double max_probs = 0.0, max_weights = 0.0;
    for (int i = 0; i < ggml_nelements(probs); ++i) {
        const double error = fabs((double) ((float *) probs->data)[i] - expected_probs[i]);
        assert(error <= 1e-7);
        max_probs = fmax(max_probs, error);
    }
    for (int i = 0; i < ggml_nelements(normalized); ++i) {
        const double error = fabs((double) ((float *) normalized->data)[i] - expected_weights[i]);
        assert(error <= 1e-7);
        max_weights = fmax(max_weights, error);
    }
    printf("CUDA routing: %d selected IDs match at %d experts; max errors probabilities %.9g, weights %.9g\n",
            selected * tokens * sequences, experts, max_probs, max_weights);
    free(storage);
    ggml_free(ctx);
}

int main(int argc, char ** argv) {
    check_scalar_and_recurrent();
    check_gdn_snapshots(33, false);
    check_gdn_snapshots(33, true);
    check_gdn_snapshots(16, false);
    check_gdn_snapshots(128, false);
    check_gdn_snapshots(128, true);
    check_quantized_cache_attention();
    check_split_q4_attention();
    check_routed_matrix(GGML_TYPE_Q4_K, false);
    check_routed_matrix(GGML_TYPE_Q6_K, true);
    check_routed_matrix(GGML_TYPE_F32, false);
    check_routed_matrix(GGML_TYPE_F32, true);
    if (argc == 2) {
        if (strcmp(argv[1], "--resident-rows") == 0) check_resident_q4_rows();
        else if (strcmp(argv[1], "--resident-conv") == 0) check_resident_convolution();
        else if (strcmp(argv[1], "--activations") == 0) check_activation_pipeline();
        else if (strcmp(argv[1], "--vectors") == 0) check_cuda_vectors();
        else if (strcmp(argv[1], "--position") == 0) check_cuda_position();
        else if (strcmp(argv[1], "--routing") == 0) check_cuda_routing();
        else check_model_step(argv[1]);
    }
    if (argc == 3 && strcmp(argv[1], "--chunked") == 0) check_chunked_generation(argv[2]);
    if (argc == 3 && strcmp(argv[1], "--resident") == 0) check_resident_state(argv[2]);
}
