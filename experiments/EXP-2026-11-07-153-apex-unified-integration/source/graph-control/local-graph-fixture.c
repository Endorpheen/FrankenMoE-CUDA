// LOCAL executable native-vs-Goose graph fixture.
// Uses real ggml contexts, tensors and native graphs. Creation mode is
// skipped because ggml_new_object is private and local-check has no ggml.c;
// traversal, view, copy, clear and hash are real.
#include "graph-bridge.h"
#include "goose-replay-wrappers.h"
#include "ggml.h"
#include "ggml-impl.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern int64_t gt_entry(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
extern int64_t gs_entry(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
extern int64_t hp_entry(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);

static int32_t uses(const struct ggml_cgraph * g, struct ggml_tensor * t) {
    size_t p = ggml_hash_find(&g->visited_hash_set, t);
    return p == GGML_HASHSET_FULL ? -1 : g->use_counts[p];
}

int main(void) {
    int ok = 1;
    struct ggml_init_params p = { .mem_size = 8 * 1024 * 1024, .mem_buffer = NULL, .no_alloc = false };
    struct ggml_context * ctx = ggml_init(p);
    if (!ctx) return 1;

    struct ggml_tensor * x = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 4);
    struct ggml_tensor * w = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 4);
    struct ggml_tensor * a = ggml_add(ctx, x, w);
    struct ggml_tensor * b = ggml_mul(ctx, a, x);
    struct ggml_tensor * out = ggml_add(ctx, b, a);
    ggml_set_name(x, "x"); ggml_set_name(w, "w"); ggml_set_name(a, "a");
    ggml_set_name(b, "b"); ggml_set_name(out, "out");

    struct ggml_cgraph * ref = ggml_new_graph(ctx);
    ggml_build_forward_expand(ref, out);

    struct ggml_cgraph * goose = ggml_new_graph(ctx);
    int64_t n_new = gt_entry(0, (int64_t) (intptr_t) goose, (int64_t) (intptr_t) out, 0, 0, 0, 0);
    if (n_new != ref->n_nodes) { fprintf(stderr, "FAIL traversal n_new=%lld ref=%d\n", (long long) n_new, ref->n_nodes); ok = 0; }
    if (goose->n_nodes != ref->n_nodes || goose->n_leafs != ref->n_leafs) {
        fprintf(stderr, "FAIL traversal counts %d/%d vs %d/%d\n", goose->n_nodes, goose->n_leafs, ref->n_nodes, ref->n_leafs);
        ok = 0;
    }
    for (int i = 0; i < ref->n_nodes; ++i) {
        if (goose->nodes[i] != ref->nodes[i]) { fprintf(stderr, "FAIL node %d order\n", i); ok = 0; }
        if (uses(goose, goose->nodes[i]) != uses(ref, ref->nodes[i])) { fprintf(stderr, "FAIL node %d uses\n", i); ok = 0; }
    }
    for (int i = 0; i < ref->n_leafs; ++i) {
        if (goose->leafs[i] != ref->leafs[i]) { fprintf(stderr, "FAIL leaf %d order\n", i); ok = 0; }
        if (uses(goose, goose->leafs[i]) != uses(ref, ref->leafs[i])) { fprintf(stderr, "FAIL leaf %d uses\n", i); ok = 0; }
    }

    struct ggml_cgraph * replay_graph = ggml_new_graph(ctx);
    fg153_goose_gt_build_forward_expand(replay_graph, out);
    if (replay_graph->n_nodes != ref->n_nodes || replay_graph->n_leafs != ref->n_leafs) {
        fprintf(stderr, "FAIL replay expand counts\n"); ok = 0;
    }
    for (int i = 0; i < ref->n_nodes; ++i) {
        if (replay_graph->nodes[i] != ref->nodes[i]) { fprintf(stderr, "FAIL replay node %d\n", i); ok = 0; }
    }
    struct ggml_cgraph replay_view = fg153_goose_gs_graph_view(ref, 1, 3);
    if (replay_view.n_nodes != 2 || replay_view.n_leafs != 0 || replay_view.grads != NULL) {
        fprintf(stderr, "FAIL replay view\n"); ok = 0;
    }

    struct ggml_cgraph view;
    memset(&view, 0, sizeof(view));
    int64_t v = gs_entry(4, (int64_t) (intptr_t) &view, (int64_t) (intptr_t) ref, 1, 3, 0, 0);
    if (v != (int64_t) (intptr_t) &view || view.size != 0 || view.n_nodes != 2 ||
        view.n_leafs != 0 || view.leafs != NULL || view.grads != NULL || view.grad_accs != NULL) {
        fprintf(stderr, "FAIL storage view\n"); ok = 0;
    }

    struct ggml_cgraph * cpy = ggml_new_graph(ctx);
    gs_entry(5, (int64_t) (intptr_t) ref, (int64_t) (intptr_t) cpy, 0, 0, 0, 0);
    if (cpy->n_nodes != ref->n_nodes || cpy->n_leafs != ref->n_leafs || cpy->order != ref->order) {
        fprintf(stderr, "FAIL storage copy counts\n"); ok = 0;
    }
    for (int i = 0; i < ref->n_nodes; ++i) {
        if (cpy->nodes[i] != ref->nodes[i] || uses(cpy, cpy->nodes[i]) != uses(ref, ref->nodes[i])) {
            fprintf(stderr, "FAIL copy node %d\n", i); ok = 0;
        }
    }
    for (int i = 0; i < ref->n_leafs; ++i) {
        if (cpy->leafs[i] != ref->leafs[i] || uses(cpy, cpy->leafs[i]) != uses(ref, ref->leafs[i])) {
            fprintf(stderr, "FAIL copy leaf %d\n", i); ok = 0;
        }
    }

    fg153_goose_gs_graph_clear(cpy);
    if (cpy->n_nodes != 0 || cpy->n_leafs != 0) { fprintf(stderr, "FAIL clear counts\n"); ok = 0; }
    for (size_t i = 0; i < cpy->visited_hash_set.size; ++i) {
        if (ggml_bitset_get(cpy->visited_hash_set.used, i)) { fprintf(stderr, "FAIL clear used %zu\n", i); ok = 0; break; }
    }

    struct ggml_cgraph * hg = ggml_new_graph(ctx);
    int64_t hs = fg153_gs_hash_set((int64_t) (intptr_t) hg);
    int64_t pos = hp_entry(2, hs, (int64_t) (intptr_t) out, 0, 0, 0, 0);
    if (pos < 0) { fprintf(stderr, "FAIL hash insert pos\n"); ok = 0; }
    if (hp_entry(3, hs, (int64_t) (intptr_t) out, 0, 0, 0, 0) != 1) { fprintf(stderr, "FAIL hash contains\n"); ok = 0; }
    if (hp_entry(0, hs, (int64_t) (intptr_t) out, 0, 0, 0, 0) != pos) { fprintf(stderr, "FAIL hash find\n"); ok = 0; }
    if (hp_entry(1, hs, (int64_t) (intptr_t) out, 0, 0, 0, 0) != -2) { fprintf(stderr, "FAIL hash duplicate\n"); ok = 0; }

    if (gs_entry(0, (int64_t) (intptr_t) ctx, 16, 0, 0, 0, 0) == 0) {
        fprintf(stderr, "FAIL real arena creation unavailable\n"); ok = 0;
    }

    ggml_free(ctx);
    printf("RESULT %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
