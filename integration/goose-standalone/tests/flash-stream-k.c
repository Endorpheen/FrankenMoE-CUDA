#include "graph-execute.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void check(const char * path, int queries, int keys, int prefix) {
    enum { width = 256, heads = 16, kv_heads = 2 };
    assert(queries >= 1 && prefix > 0 && prefix + queries - 1 <= keys);
    struct ggml_context * ctx = ggml_init((struct ggml_init_params){1024 * 1024, NULL, true});
    assert(ctx);
    struct ggml_tensor * q_data = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, width, heads, queries);
    struct ggml_tensor * k_data = ggml_new_tensor_3d(ctx, GGML_TYPE_Q4_0, width, kv_heads, keys);
    struct ggml_tensor * v_data = ggml_dup_tensor(ctx, k_data);
    struct ggml_tensor * q = ggml_permute(ctx, q_data, 0, 2, 1, 3);
    struct ggml_tensor * k = ggml_permute(ctx, k_data, 0, 2, 1, 3);
    struct ggml_tensor * v = ggml_permute(ctx, v_data, 0, 2, 1, 3);
    struct ggml_tensor * out = ggml_flash_attn_ext(ctx, q, k, v, NULL, 0.0625f, 0.0f, 0.0f);
    out->op_params[5] = prefix;
    struct ggml_cgraph * graph = ggml_new_graph_custom(ctx, 16, false);
    ggml_build_forward_expand(graph, out);
    void * arena = fg_standalone_graph_alloc(graph);
    assert(arena);
    float * query = q_data->data;
    for (int i = 0; i < queries * heads * width; ++i) {
        query[i] = (float) ((13 * i + i / 19) % 27 - 13) / 16.0f;
    }
    const struct ggml_type_traits * q4 = ggml_get_type_traits(GGML_TYPE_Q4_0);
    for (int row = 0; row < keys * kv_heads; ++row) {
        float src[width];
        for (int i = 0; i < width; ++i) src[i] = (float) ((17 * row + i * 7) % 21 - 10) / 8.0f;
        q4->from_float_ref(src, (unsigned char *) k_data->data + row * k_data->nb[1], width);
        for (int i = 0; i < width; ++i) src[i] = (float) ((3 * row + i * 11) % 31 - 11) / 8.0f;
        q4->from_float_ref(src, (unsigned char *) v_data->data + row * v_data->nb[1], width);
    }
    assert(fg_standalone_graph_compute(graph) == GGML_STATUS_SUCCESS);
    FILE * file = fopen(path, "wb");
    assert(file);
    const size_t size = ggml_nbytes(out);
    assert(fwrite(out->data, 1, size, file) == size);
    assert(fclose(file) == 0);
    printf("%d queries, %d keys, %d causal prefix, %zu bytes\n", queries, keys, prefix, size);
    free(arena);
    ggml_free(ctx);
}

int main(int argc, char ** argv) {
    assert(argc >= 5 && (argc - 1) % 4 == 0);
    for (int i = 1; i < argc; i += 4) {
        check(argv[i], atoi(argv[i + 1]), atoi(argv[i + 2]), atoi(argv[i + 3]));
    }
}
