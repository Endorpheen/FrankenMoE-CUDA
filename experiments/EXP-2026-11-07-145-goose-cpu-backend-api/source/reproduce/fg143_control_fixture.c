#include "ggml.h"
#include "ggml-cpu.h"
#include "ggml-impl.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *output;
static void sample(enum ggml_op op, int variant, int sub, int requested) {
    struct ggml_init_params init = {2*1024*1024, NULL, false};
    struct ggml_context *ctx = ggml_init(init);
    if (!ctx) abort();
    enum ggml_type x = GGML_TYPE_F32, y = GGML_TYPE_F32, d = GGML_TYPE_F32;
    if (variant == 1) x = GGML_TYPE_F16, y = GGML_TYPE_BF16;
    if (variant == 2) x = GGML_TYPE_BF16, y = GGML_TYPE_F16;
    if (variant == 3) x = GGML_TYPE_Q4_K;
    if (variant == 4) d = GGML_TYPE_Q4_K;
    if (variant == 5) x = GGML_TYPE_Q6_K;
    if (variant == 6) x = GGML_TYPE_I32;
    if (variant == 7) x = GGML_TYPE_F32, y = GGML_TYPE_I32;
    struct ggml_tensor *a = ggml_new_tensor_4d(ctx, x, 256, 8, 1, 1);
    struct ggml_tensor *b = ggml_new_tensor_4d(ctx, y, 256, 8, 1, 1);
    struct ggml_tensor *ids = ggml_new_tensor_2d(ctx, GGML_TYPE_I32, 4, 3);
    struct ggml_tensor *t = ggml_new_tensor_4d(ctx, d, 256, 8, 1, 1);
    t->op = op;
    t->src[0] = a; t->src[1] = b; t->src[2] = ids;
    memset(t->op_params, 0, sizeof(t->op_params));
    ggml_set_op_params_i32(t, 0, sub);
    if (op == GGML_OP_MAP_CUSTOM1) {struct ggml_map_custom1_op_params p = {0}; p.n_tasks = sub; memcpy(t->op_params, &p, sizeof(p));}
    if (op == GGML_OP_MAP_CUSTOM2) {struct ggml_map_custom2_op_params p = {0}; p.n_tasks = sub; memcpy(t->op_params, &p, sizeof(p));}
    if (op == GGML_OP_MAP_CUSTOM3) {struct ggml_map_custom3_op_params p = {0}; p.n_tasks = sub; memcpy(t->op_params, &p, sizeof(p));}
    if (op == GGML_OP_CUSTOM) {struct ggml_custom_op_params p = {0}; p.n_tasks = sub; memcpy(t->op_params, &p, sizeof(p));}
    struct ggml_cgraph *g = ggml_new_graph(ctx);
    ggml_build_forward_expand(g, t);
    struct ggml_cplan plan = ggml_graph_plan(g, requested, NULL);
    uint64_t values[2] = {(uint64_t)plan.n_threads, (uint64_t)plan.work_size};
    if (fwrite(values, sizeof(values), 1, output) != 1) abort();
    printf("{\"op\":%d,\"variant\":%d,\"sub\":%d,\"requested\":%d,\"threads\":%d,\"work_size\":%zu}\n", op, variant, sub, requested, plan.n_threads, plan.work_size);
    fflush(stdout);
    ggml_free(ctx);
}
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    output = fopen(argv[1], "wb");
    if (!output) return 2;
    int threads[] = {-1, 0, 1, 3, 16};
    for (unsigned ti = 0; ti < sizeof(threads)/sizeof(*threads); ++ti) {
        for (int op = 0; op < GGML_OP_COUNT; ++op) {
            if (op == GGML_OP_UNARY) {for (int sub = 0; sub < GGML_UNARY_OP_COUNT; ++sub) sample(op, 0, sub, threads[ti]);}
            else if (op == GGML_OP_GLU) {for (int sub = 0; sub < GGML_GLU_OP_COUNT; ++sub) sample(op, 0, sub, threads[ti]);}
            else if (op == GGML_OP_MAP_CUSTOM1 || op == GGML_OP_MAP_CUSTOM2 || op == GGML_OP_MAP_CUSTOM3 || op == GGML_OP_CUSTOM) {
                sample(op, 0, GGML_N_TASKS_MAX, threads[ti]); sample(op, 0, 1, threads[ti]); sample(op, 0, 2, threads[ti]);
            } else sample(op, 0, op == GGML_OP_GATED_DELTA_NET ? 3 : 0, threads[ti]);
        }
        enum ggml_op typed[] = {GGML_OP_CPY, GGML_OP_DUP, GGML_OP_ADD, GGML_OP_ACC, GGML_OP_OUT_PROD, GGML_OP_SET_ROWS, GGML_OP_MUL_MAT, GGML_OP_MUL_MAT_ID};
        for (unsigned op = 0; op < sizeof(typed)/sizeof(*typed); ++op) for (int variant = 1; variant < 8; ++variant) sample(typed[op], variant, 0, threads[ti]);
    }
    fclose(output);
    return 0;
}
