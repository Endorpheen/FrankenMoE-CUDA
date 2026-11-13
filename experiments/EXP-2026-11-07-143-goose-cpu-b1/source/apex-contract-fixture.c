#include "ggml.h"
#include "ggml-cpu.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *output;
static void evaluate(const char *name, struct ggml_context *ctx, struct ggml_tensor *t, int threads) {
    struct ggml_cgraph *graph = ggml_new_graph(ctx);
    ggml_build_forward_expand(graph, t);
    if (ggml_graph_compute_with_ctx(ctx, graph, threads) != GGML_STATUS_SUCCESS) abort();
    size_t bytes = ggml_nbytes(t);
    if (fwrite(t->data, 1, bytes, output) != bytes) abort();
    printf("{\"name\":\"%s\",\"type\":0,\"bytes\":%zu,\"threads\":%d}\n", name, bytes, threads);
    fflush(stdout);
}
static struct ggml_context *context(void) {
    struct ggml_init_params params = {32*1024*1024, NULL, false};
    struct ggml_context *ctx = ggml_init(params);
    if (!ctx) abort();
    return ctx;
}
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    FILE *input = fopen(argv[1], "rb");
    output = fopen(argv[2], "wb");
    uint32_t header[3];
    if (!input || !output || fread(header, sizeof(header), 1, input) != 1) abort();
    const int threads_list[] = {1, 8, 16};
    for (int ti = 0; ti < 3; ++ti) {
        int threads = threads_list[ti];
        struct ggml_context *ctx = context();
        struct ggml_tensor *weights = ggml_new_tensor_2d(ctx, header[0], header[1], header[2]);
        if (fread(weights->data, 1, ggml_nbytes(weights), input) != ggml_nbytes(weights)) abort();
        struct ggml_tensor *ids = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, header[2]*2);
        for (uint32_t i = 0; i < header[2]*2; ++i) ((int32_t *)ids->data)[i] = (int32_t)((i*17)%header[2]);
        struct ggml_tensor *t = ggml_get_rows(ctx, weights, ids);
        char name[100];
        snprintf(name, sizeof(name), "apex-real-embedding-q4k-t%d", threads);
        evaluate(name, ctx, t, threads);
        ggml_free(ctx);
        fseek(input, sizeof(header), SEEK_SET);
        for (int op = 0; op < GGML_GLU_OP_COUNT; ++op) {
            for (int layout = 0; layout < 3; ++layout) {
                ctx = context();
                const int width = 2048;
                struct ggml_tensor *a = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, layout ? width*2 : width, 8, 3);
                struct ggml_tensor *b = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, width, 8, 3);
                for (int64_t i = 0; i < ggml_nelements(a); ++i) ((float *)a->data)[i] = sinf((float)(i%997)*.37f)*60.f;
                for (int64_t i = 0; i < ggml_nelements(b); ++i) ((float *)b->data)[i] = cosf((float)(i%733)*.19f)*1.7f;
                if (layout) t = ggml_glu(ctx, a, op, layout == 2);
                else if (op == GGML_GLU_OP_SWIGLU_CLAMP) t = ggml_swiglu_clamp(ctx, a, b, 7.f);
                else if (op == GGML_GLU_OP_SWIGLU_OAI) t = ggml_swiglu_oai(ctx, a, b, 1.702f, 7.f);
                else t = ggml_glu_split(ctx, a, b, op);
                snprintf(name, sizeof(name), "apex-glu-op%d-layout%d-wide-values-t%d", op, layout, threads);
                evaluate(name, ctx, t, threads);
                ggml_free(ctx);
            }
        }
    }
    fclose(input);
    fclose(output);
    return 0;
}
