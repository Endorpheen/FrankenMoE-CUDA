#include "graph-execute.h"
#include "goose-host-llm.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void fg_goose_host_ensure_init(void);

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
    enum { prompt_size = 137, vocabulary = 4 };
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
    puts("137-token chunked generation preserves the next-step logits at 262144 context");
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
    struct ggml_cgraph * graph = ggml_new_graph_custom(ctx, 64, false);
    ggml_build_forward_expand(graph, out);
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
    free(decoded_k);
    free(decoded_v);
    free(storage);
    ggml_free(ctx);
}

static void check_quantized_matrix(enum ggml_type kind, bool indexed) {
    struct ggml_context * ctx = ggml_init((struct ggml_init_params){ 1024 * 1024, NULL, true });
    assert(ctx);
    const int64_t planes = indexed ? 3 : 2;
    struct ggml_tensor * a = ggml_new_tensor_3d(ctx, kind, 256, 2, planes);
    struct ggml_tensor * b = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, 256, 2, 4, indexed ? 1 : 2);
    struct ggml_tensor * ids = indexed ? ggml_new_tensor_2d(ctx, GGML_TYPE_I32, 4, 4) : NULL;
    struct ggml_tensor * product = indexed ? ggml_mul_mat_id(ctx, a, b, ids) : ggml_mul_mat(ctx, a, b);
    struct ggml_cgraph * graph = ggml_new_graph_custom(ctx, 64, false);
    ggml_build_forward_expand(graph, product);
    void * storage = fg_standalone_graph_alloc(graph);
    assert(storage);
    const struct ggml_type_traits * traits = ggml_get_type_traits(kind);
    float original[256], decoded[256];
    for (int i = 0; i < ggml_nelements(b); ++i) {
        ((float *) b->data)[i] = (float) ((i + i / 256) % 7 - 3) * 0.125f;
    }
    if (indexed) {
        for (int i = 0; i < 16; ++i) {
            ((int32_t *) ids->data)[i] = (i / 4 + i % 4) % 2;
        }
    }
    for (int p = 0; p < planes; ++p) {
        if (indexed && p == 2) continue;
        for (int row = 0; row < 2; ++row) {
            for (int i = 0; i < 256; ++i) {
                original[i] = (float) ((i + row + 3 * p) % 13 - 6) * 0.25f;
            }
            unsigned char * packed = (unsigned char *) a->data + (size_t) p * a->nb[2] + (size_t) row * a->nb[1];
            traits->from_float_ref(original, packed, 256);
        }
    }
    assert(fg_standalone_graph_compute(graph) == GGML_STATUS_SUCCESS);
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
                    traits->to_float(packed, decoded, 256);
                    float expected = 0;
                    for (int i = 0; i < 256; ++i) expected = fmaf(decoded[i], right[i], expected);
                    assert(fabsf(got[row] - expected) < 1e-5f);
                }
            }
        }
    }
    printf("%s %s matches dequantized reference\n", ggml_type_name(kind), indexed ? "indexed" : "broadcast");
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
    check_quantized_matrix(GGML_TYPE_Q4_K, false);
    check_quantized_matrix(GGML_TYPE_Q6_K, true);
    if (argc == 2) check_model_step(argv[1]);
    if (argc == 3 && strcmp(argv[1], "--chunked") == 0) check_chunked_generation(argv[2]);
}
