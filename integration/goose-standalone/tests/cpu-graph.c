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

static void check_gdn_snapshots(bool column_gate) {
    enum { width = 33, heads = 2, tokens = 3, seqs = 2, snapshots = 2 };
    enum { square = width * width, scores = width * heads * tokens * seqs, state_size = square * heads * seqs };
    struct ggml_context * ctx = ggml_init((struct ggml_init_params){ 1024 * 1024, NULL, true });
    assert(ctx);
    struct ggml_tensor * qk_storage = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, width + 1, tokens);
    struct ggml_tensor * q = ggml_view_4d(ctx, qk_storage, width, 1, tokens, 1,
        qk_storage->nb[1], qk_storage->nb[1], qk_storage->nb[2], 0);
    struct ggml_tensor * k = ggml_dup_tensor(ctx, q);
    struct ggml_tensor * v = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, width, heads, tokens, seqs);
    struct ggml_tensor * gate = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, column_gate ? width : 1, heads, tokens, seqs);
    struct ggml_tensor * beta = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, 1, heads, tokens, seqs);
    struct ggml_tensor * initial = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, width, width, heads, seqs);
    struct ggml_tensor * gdn = ggml_gated_delta_net(ctx, q, k, v, gate, beta, initial, snapshots);
    struct ggml_cgraph * graph = ggml_new_graph_custom(ctx, 64, false);
    ggml_build_forward_expand(graph, gdn);
    void * storage = fg_standalone_graph_alloc(graph);
    assert(storage);

    struct ggml_tensor * inputs[] = { qk_storage, k, v, gate, beta, initial };
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
    for (int seq = 0; seq < seqs; ++seq) {
        for (int head = 0; head < heads; ++head) {
            const int task = seq * heads + head;
            float expected[square];
            memcpy(expected, (float *) initial->data + task * square, sizeof expected);
            for (int token = 0; token < tokens; ++token) {
                const int input = (seq * tokens + token) * heads + head;
                const float * query = queries + token * (width + 1);
                const float * key = keys + token * width;
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
                    assert(fabsf(actual[input * width + row] - score / sqrtf(width)) < 1e-6f);
                }
                const int slot = tokens - 1 - token;
                if (slot < snapshots) {
                    const float * actual_state = actual + scores + slot * state_size + task * square;
                    for (int i = 0; i < square; ++i) assert(fabsf(actual_state[i] - expected[i]) < 1e-6f);
                }
            }
        }
    }
    printf("GDN %s gate matches reference, including snapshots\n", column_gate ? "column" : "scalar");
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
    check_gdn_snapshots(false);
    check_gdn_snapshots(true);
    check_quantized_cache_attention();
    check_quantized_matrix(GGML_TYPE_Q4_K, false);
    check_quantized_matrix(GGML_TYPE_Q6_K, true);
    if (argc == 2) check_model_step(argv[1]);
    if (argc == 3 && strcmp(argv[1], "--chunked") == 0) check_chunked_generation(argv[2]);
}
