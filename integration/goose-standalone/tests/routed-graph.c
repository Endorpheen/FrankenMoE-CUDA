#include "goose-unit.h"
#include "graph-execute.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

extern int64_t fg_q35_moe_norm_node(int64_t, int64_t, int64_t, int64_t);
extern int64_t fg_q35_moe_weighted_node(int64_t, int64_t, int64_t);
extern int64_t fg_cuda_load_experts(sl_i64, sl_i64, int64_t, int64_t, int64_t);
extern void fg_cuda_expert_stats(sl_i64);

enum { input_width = 512, hidden_width = 256, experts = 256, slots = 8, rounds = 6 };

static int64_t handle(void * p) { return (int64_t) (uintptr_t) p; }

static void fill_quantized(struct ggml_tensor * t, int projection) {
    const struct ggml_type_traits * traits = ggml_get_type_traits(t->type);
    float row[input_width];
    for (int e = 0; e < experts; ++e) {
        for (int j = 0; j < t->ne[0]; ++j) {
            row[j] = (float) (((e + projection * 23 + j * 13) % 97) - 48) / 193.0f;
        }
        for (int r = 0; r < t->ne[1]; ++r) {
            traits->from_float_ref(row, (uint8_t *) t->data + e * t->nb[2] + r * t->nb[1], t->ne[0]);
        }
    }
}

int main(int argc, char ** argv) {
    assert(argc == 3);
    goose_init(argc, argv);
    enum ggml_type down_type = atoi(argv[2]) == 4 ? GGML_TYPE_Q4_K : GGML_TYPE_Q6_K;
    struct ggml_context * ctx = ggml_init((struct ggml_init_params){1024 * 1024, NULL, true});
    assert(ctx);
    struct ggml_tensor * raw = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, input_width, 1);
    struct ggml_tensor * logits = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, experts, 1);
    struct ggml_tensor * up_w = ggml_new_tensor_3d(ctx, GGML_TYPE_Q4_K, input_width, hidden_width, experts);
    struct ggml_tensor * gate_w = ggml_dup_tensor(ctx, up_w);
    struct ggml_tensor * down_w = ggml_new_tensor_3d(ctx, down_type, hidden_width, input_width, experts);
    struct ggml_tensor * input = ggml_scale(ctx, raw, 1.0f);
    struct ggml_tensor * probabilities = ggml_soft_max(ctx, logits);
    struct ggml_tensor * ids = ggml_top_k(ctx, probabilities, slots);
    struct ggml_tensor * up = ggml_mul_mat_id(ctx, up_w, input, ids);
    struct ggml_tensor * gate = ggml_mul_mat_id(ctx, gate_w, input, ids);
    struct ggml_tensor * activated = ggml_swiglu_split(ctx, gate, up);
    struct ggml_tensor * down = ggml_mul_mat_id(ctx, down_w, activated, ids);
    struct ggml_tensor * normalized = (void *) (uintptr_t) fg_q35_moe_norm_node(
        handle(ctx), handle(probabilities), handle(ids), 1065353216);
    struct ggml_tensor * weighted = (void *) (uintptr_t) fg_q35_moe_weighted_node(
        handle(ctx), handle(down), handle(normalized));
    struct ggml_tensor * out = ggml_add(ctx, weighted, input);
    struct ggml_cgraph * graph = ggml_new_graph_custom(ctx, 64, false);
    ggml_build_forward_expand(graph, out);
    struct ggml_tensor * external[] = {raw, logits, up_w, gate_w, down_w};
    for (size_t i = 0; i < sizeof external / sizeof *external; ++i) {
        external[i]->data = calloc(1, ggml_nbytes(external[i]));
        assert(external[i]->data);
    }
    struct fg_standalone_graph_plan * plan = fg_standalone_graph_plan_alloc(graph, out);
    assert(plan);

    fill_quantized(up_w, 0);
    fill_quantized(gate_w, 1);
    fill_quantized(down_w, 2);
    int64_t layer[] = {0, handle(up_w->data), handle(gate_w->data), handle(down_w->data)};
    int64_t group[] = {up_w->nb[2], gate_w->nb[2], down_w->nb[2], experts, 1, slots, 0, experts, 1};
    const int64_t bytes = slots * (group[0] + group[1] + group[2]);
    assert(fg_cuda_load_experts((sl_i64){group, 9}, (sl_i64){layer, 4}, bytes,
        experts + 4 * slots + 4 + 5 * experts, 1 + experts + 5) == bytes);

    FILE * stream = fopen(argv[1], "wb");
    assert(stream);
    for (int i = 0; i < rounds; ++i) {
        for (int j = 0; j < input_width; ++j) ((float *) raw->data)[j] = (float) (((i + j) % 29) - 14) / 29.0f;
        for (int e = 0; e < experts; ++e) {
            ((float *) logits->data)[e] = e / slots == i ? 2.0f + (float) (e % slots) * 0.125f : -2.0f;
        }
        assert(fg_standalone_graph_plan_compute(plan) == GGML_STATUS_SUCCESS);
        assert(fwrite(out->data, 1, ggml_nbytes(out), stream) == ggml_nbytes(out));
    }
    int64_t stats[6] = {0};
    fg_cuda_expert_stats((sl_i64){stats, 6});
    assert(stats[1] == rounds * slots && stats[2] == (rounds - 1) * slots);
    printf("%lld fills and %lld evictions\n", (long long) stats[1], (long long) stats[2]);
    assert(fclose(stream) == 0);
    fg_standalone_graph_plan_free(plan);
    for (size_t i = 0; i < sizeof external / sizeof *external; ++i) free(external[i]->data);
    ggml_free(ctx);
}
