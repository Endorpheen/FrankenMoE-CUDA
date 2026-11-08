// Native-vs-Goose graph fixture for worker-1.
// Builds real ggml tensors, runs the native forward graph builder, records a
// canonical trace and FNV-1a hash, then runs the Goose entries.
//
// Exact exported entry ABI (provided by generated C):
//   int64_t gt_entry(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
//   int64_t gs_entry(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
// There is no lookup oracle and no PENDING fallback. Missing generated C is a
// link error. The parent emitter derives internal IDs from generated C.
//
// Source anchors:
//   native traversal: inputs/ggml-native-core.c:7258-7365
//   graph ABI:        inputs/ggml-impl.h:238-359
//   tensor ABI:       inputs/ggml.h:685-717
//
// Ownership: the Goose graph is allocated by gs_entry through the arena
// adapter and is owned by ctx. The view is written into local view_storage
// and has no allocation and no release.

#include "graph-bridge.h"
#include "ggml.h"
#include "ggml-impl.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int64_t gt_entry(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
extern int64_t gs_entry(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);

struct trace_row {
    const char * name;
    int op;
    int flags;
    int is_leaf;
    int32_t use_count;
};

static uint64_t fnv1a(const void * data, size_t n, uint64_t h) {
    const unsigned char * p = (const unsigned char *) data;
    for (size_t i = 0; i < n; ++i) {
        h ^= (uint64_t) p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static uint64_t trace_hash(struct trace_row * rows, int n) {
    for (int i = 1; i < n; ++i) {
        struct trace_row key = rows[i];
        int j = i - 1;
        while (j >= 0) {
            int cmp = strcmp(rows[j].name, key.name);
            if (cmp < 0 || (cmp == 0 && rows[j].op <= key.op)) {
                break;
            }
            rows[j + 1] = rows[j];
            --j;
        }
        rows[j + 1] = key;
    }
    uint64_t h = 1469598103934665603ULL;
    for (int i = 0; i < n; ++i) {
        h = fnv1a(rows[i].name, strlen(rows[i].name) + 1, h);
        h = fnv1a(&rows[i].op, sizeof(rows[i].op), h);
        h = fnv1a(&rows[i].flags, sizeof(rows[i].flags), h);
        h = fnv1a(&rows[i].is_leaf, sizeof(rows[i].is_leaf), h);
        h = fnv1a(&rows[i].use_count, sizeof(rows[i].use_count), h);
    }
    return h;
}

static int collect_native_rows(const struct ggml_cgraph * g, struct trace_row * rows, int cap) {
    int n = 0;
    for (int i = 0; i < g->n_leafs && n < cap; ++i) {
        struct ggml_tensor * t = g->leafs[i];
        size_t pos = ggml_hash_find(&g->visited_hash_set, t);
        rows[n].name = t->name;
        rows[n].op = (int) t->op;
        rows[n].flags = t->flags;
        rows[n].is_leaf = 1;
        rows[n].use_count = pos != GGML_HASHSET_FULL ? g->use_counts[pos] : -1;
        ++n;
    }
    for (int i = 0; i < g->n_nodes && n < cap; ++i) {
        struct ggml_tensor * t = g->nodes[i];
        size_t pos = ggml_hash_find(&g->visited_hash_set, t);
        rows[n].name = t->name;
        rows[n].op = (int) t->op;
        rows[n].flags = t->flags;
        rows[n].is_leaf = 0;
        rows[n].use_count = pos != GGML_HASHSET_FULL ? g->use_counts[pos] : -1;
        ++n;
    }
    return n;
}

static void print_rows(const char * tag, const struct trace_row * rows, int n) {
    for (int i = 0; i < n; ++i) {
        printf("%s row=%d name=%s op=%d flags=%d leaf=%d uses=%d\n",
               tag, i, rows[i].name, rows[i].op, rows[i].flags,
               rows[i].is_leaf, rows[i].use_count);
    }
}

static int collect_goose_rows(int64_t g, struct trace_row * rows, int cap) {
    int n = 0;
    int64_t n_leafs = fg153_gt_graph_get(g, 2, 0);
    int64_t n_nodes = fg153_gt_graph_get(g, 1, 0);
    for (int64_t i = 0; i < n_leafs && n < cap; ++i) {
        int64_t t = fg153_gt_graph_get(g, 4, i);
        int64_t pos = fg153_gt_hash_find(g, t);
        rows[n].name = ggml_get_name((struct ggml_tensor *) (intptr_t) t);
        rows[n].op = (int) fg153_gt_tensor_get(t, 0, 0);
        rows[n].flags = (int) fg153_gt_tensor_get(t, 1, 0);
        rows[n].is_leaf = 1;
        rows[n].use_count = (int32_t) fg153_gt_graph_get(g, 5, pos);
        ++n;
    }
    for (int64_t i = 0; i < n_nodes && n < cap; ++i) {
        int64_t t = fg153_gt_graph_get(g, 3, i);
        int64_t pos = fg153_gt_hash_find(g, t);
        rows[n].name = ggml_get_name((struct ggml_tensor *) (intptr_t) t);
        rows[n].op = (int) fg153_gt_tensor_get(t, 0, 0);
        rows[n].flags = (int) fg153_gt_tensor_get(t, 1, 0);
        rows[n].is_leaf = 0;
        rows[n].use_count = (int32_t) fg153_gt_graph_get(g, 5, pos);
        ++n;
    }
    return n;
}

int main(void) {
    struct ggml_init_params params = {
        .mem_size = 16 * 1024 * 1024,
        .mem_buffer = NULL,
        .no_alloc = false,
    };
    struct ggml_context * ctx = ggml_init(params);
    if (!ctx) {
        fprintf(stderr, "ggml_init failed\n");
        return 1;
    }

    struct ggml_tensor * x = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 8);
    struct ggml_tensor * w = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 8);
    ggml_set_name(x, "x");
    ggml_set_name(w, "w");
    struct ggml_tensor * a = ggml_add(ctx, x, w);
    ggml_set_name(a, "a");
    struct ggml_tensor * b = ggml_mul(ctx, a, x);
    ggml_set_name(b, "b");
    struct ggml_tensor * out = ggml_add(ctx, b, a);
    ggml_set_name(out, "out");

    struct ggml_cgraph * native = ggml_new_graph(ctx);
    ggml_build_forward_expand(native, out);

    struct trace_row native_rows[64];
    int n_native = collect_native_rows(native, native_rows, 64);
    print_rows("NATIVE", native_rows, n_native);
    uint64_t native_hash = trace_hash(native_rows, n_native);
    printf("NATIVE n_nodes=%d n_leafs=%d hash=%016llx\n",
           native->n_nodes, native->n_leafs, (unsigned long long) native_hash);

    // Storage: arena-backed Graph owned by ctx.
    int64_t g = gs_entry(0, (int64_t) (intptr_t) ctx, (int64_t) native->size, 0, 0, 0, 0);
    if (!g) {
        fprintf(stderr, "gs_entry new_graph failed\n");
        ggml_free(ctx);
        return 1;
    }
    printf("GOOSE-GRAPH size=%lld hash_size=%lld\n",
           (long long) fg153_gs_graph_get(g, 0, 0), (long long) fg153_gs_graph_get(g, 9, 0));

    int64_t n_new = gt_entry(0, g, (int64_t) (intptr_t) out, 0, 0, 0, 0);
    if (n_new < 0) {
        fprintf(stderr, "gt_entry expand failed\n");
        ggml_free(ctx);
        return 1;
    }

    struct trace_row goose_rows[64];
    int n_goose = collect_goose_rows(g, goose_rows, 64);
    print_rows("GOOSE", goose_rows, n_goose);
    uint64_t goose_hash = trace_hash(goose_rows, n_goose);
    printf("GOOSE n_nodes=%lld n_leafs=%lld hash=%016llx\n",
           (long long) fg153_gt_graph_get(g, 1, 0), (long long) fg153_gt_graph_get(g, 2, 0),
           (unsigned long long) goose_hash);

    // View: caller-owned storage, no allocation and no release.
    struct ggml_cgraph view_storage;
    memset(&view_storage, 0, sizeof(view_storage));
    int64_t view = gs_entry(4, (int64_t) (intptr_t) &view_storage,
                            (int64_t) (intptr_t) native, 1, 3, 0, 0);
    int64_t view_ok = view != 0;
    if (view_ok) {
        view_ok = fg153_gs_graph_get(view, 0, 0) == 0 &&
                  fg153_gs_graph_get(view, 1, 0) == 2 &&
                  fg153_gs_graph_get(view, 2, 0) == 0 &&
                  fg153_gs_graph_get(view, 15, 0) == 0 &&
                  fg153_gs_graph_get(view, 16, 0) == 0 &&
                  fg153_gs_graph_get(view, 18, 0) == 0;
        printf("GOOSE-VIEW n_nodes=%lld n_leafs=%lld grads=%lld grad_accs=%lld leafs=%lld ok=%lld\n",
               (long long) fg153_gs_graph_get(view, 1, 0), (long long) fg153_gs_graph_get(view, 2, 0),
               (long long) fg153_gs_graph_get(view, 15, 0), (long long) fg153_gs_graph_get(view, 16, 0),
               (long long) fg153_gs_graph_get(view, 18, 0), (long long) view_ok);
    }

    int same = n_native == n_goose && native_hash == goose_hash && view_ok;
    printf("COMPARE same=%d native_hash=%016llx goose_hash=%016llx\n",
           same, (unsigned long long) native_hash, (unsigned long long) goose_hash);

    ggml_free(ctx);
    return same ? 0 : 1;
}
