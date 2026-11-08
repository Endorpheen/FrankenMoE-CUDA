// Unit checks for worker-1 graph modules.
// ABI source: inputs/ggml-impl.h:211-359, inputs/ggml.h:661-717.
//
// Exact exported entry ABI from generated C:
//   int64_t gt_entry(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
//   int64_t gs_entry(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
//   int64_t hp_entry(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
// No lookup oracle, no PENDING, no numeric IDs. Missing generated C is a link
// error. The parent emitter derives internal IDs from generated C.
//
// Ownership: graphs come from the arena adapter and are owned by ctx; views
// are written into caller-owned struct ggml_cgraph storage.

#include "graph-bridge.h"
#include "ggml.h"
#include "ggml-impl.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern int64_t gt_entry(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
extern int64_t gs_entry(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
extern int64_t hp_entry(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);

static int g_fail = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        g_fail = 1; \
    } \
} while (0)

static void test_constants(void) {
    CHECK(fg153_gt_const(60) == GGML_CGRAPH_EVAL_ORDER_LEFT_TO_RIGHT);
    CHECK(fg153_gt_const(61) == GGML_CGRAPH_EVAL_ORDER_RIGHT_TO_LEFT);
    CHECK(fg153_gt_const(62) == (int64_t) GGML_HASHSET_FULL);
    CHECK(fg153_gs_const(70) == GGML_OP_OPT_STEP_ADAMW);
    CHECK(fg153_gs_const(71) == GGML_TYPE_F32);
    CHECK(fg153_gs_const(80) == (int64_t) sizeof(struct ggml_cgraph));
    CHECK(fg153_gs_const(82) == (int64_t) sizeof(void *));
    CHECK(fg153_gs_const(84) == (int64_t) sizeof(int32_t));
    CHECK(fg153_hp_full() == (int64_t) GGML_HASHSET_FULL);
    CHECK(fg153_hp_already() == (int64_t) GGML_HASHSET_ALREADY_EXISTS);
}

static void test_hash_size(void) {
    CHECK(fg153_gs_hash_size(8) == 11);
    CHECK(fg153_gs_hash_size(32) == 37);
    CHECK(fg153_gs_hash_size(4096) == 4099);
    CHECK(fg153_gs_bitset_bytes(37) == (int64_t) (ggml_bitset_size(37) * sizeof(ggml_bitset_t)));
}

static void test_storage_guards(struct ggml_context * ctx) {
    CHECK(gs_entry(0, 0, 16, 0, 0, 0, 0) == 0); // null ctx: no graph

    int64_t g = gs_entry(0, (int64_t) (intptr_t) ctx, 4, 0, 0, 0, 0);
    CHECK(g != 0);
    CHECK(fg153_gs_graph_get(g, 0, 0) == 4);
    CHECK(fg153_gs_graph_get(g, 1, 0) == 0);
    CHECK(fg153_gs_graph_get(g, 2, 0) == 0);
    CHECK(fg153_gs_graph_get(g, 6, 0) == GGML_CGRAPH_EVAL_ORDER_LEFT_TO_RIGHT);
    CHECK(fg153_gs_graph_get(g, 9, 0) == 11);
    CHECK(fg153_gs_graph_get(g, 15, 0) == 0);
    CHECK(fg153_gs_graph_get(g, 16, 0) == 0);
    CHECK(fg153_gs_graph_get(g, 17, 0) != 0);

    // Null/out-of-range arrays must not dereference.
    CHECK(fg153_gs_graph_get(g, 10, 0) == 0);
    CHECK(fg153_gs_graph_get(g, 11, 0) == 0);
    CHECK(fg153_gs_graph_get(g, 3, 99) == 0);
    CHECK(fg153_gs_graph_get(g, 5, 99) == 0);

    // clear path writes n_leafs through key 2.
    fg153_gt_graph_put(g, 2, 0, 7);
    CHECK(fg153_gt_graph_get(g, 2, 0) == 7);
    fg153_gt_graph_put(g, 2, 0, 0);
    CHECK(fg153_gt_graph_get(g, 2, 0) == 0);

    int64_t fake_node = 0x1000;
    fg153_gs_graph_put(g, 3, 0, fake_node);
    CHECK(fg153_gs_graph_get(g, 3, 0) == fake_node);

    struct ggml_cgraph view_storage;
    memset(&view_storage, 0, sizeof(view_storage));
    int64_t v = gs_entry(4, (int64_t) (intptr_t) &view_storage, g, 0, 1, 0, 0);
    CHECK(v == (int64_t) (intptr_t) &view_storage);
    CHECK(fg153_gs_graph_get(v, 0, 0) == 0);
    CHECK(fg153_gs_graph_get(v, 1, 0) == 1);
    CHECK(fg153_gs_graph_get(v, 2, 0) == 0);
    CHECK(fg153_gs_graph_get(v, 3, 0) == fake_node);
    CHECK(fg153_gs_graph_get(v, 4, 0) == 0);   // null leafs array
    CHECK(fg153_gs_graph_get(v, 18, 0) == 0);  // leafs base pointer, not leafs[0]
    CHECK(fg153_gs_graph_get(v, 10, 0) == 0);  // null grads
    CHECK(fg153_gs_graph_get(v, 11, 0) == 0);  // null grad_accs
    CHECK(fg153_gs_graph_get(v, 15, 0) == 0);
    CHECK(fg153_gs_graph_get(v, 16, 0) == 0);

    CHECK(gs_entry(4, 0, g, 0, 1, 0, 0) == 0); // null out
    CHECK(gs_entry(4, (int64_t) (intptr_t) &view_storage, 0, 0, 1, 0, 0) == 0); // null src
    CHECK(fg153_gt_array_get(0, 0) == 0);
}

static void test_hash_primitives(struct ggml_context * ctx) {
    int64_t g = gs_entry(0, (int64_t) (intptr_t) ctx, 4, 0, 0, 0, 0);
    CHECK(g != 0);
    int64_t hs = fg153_gs_hash_set(g);
    CHECK(hs != 0);

    int64_t a = 0x1010;
    int64_t b = 0x2020;
    CHECK(fg153_gs_hash_find(g, a) != (int64_t) GGML_HASHSET_FULL);
    int64_t pa = fg153_gs_hash_insert(g, a);
    CHECK(pa >= 0 && pa < 11);
    CHECK(fg153_gs_hash_insert(g, a) == (int64_t) GGML_HASHSET_ALREADY_EXISTS);
    CHECK(fg153_gs_hash_find(g, a) == pa);
    CHECK(hp_entry(3, hs, a, 0, 0, 0, 0) == 1);

    int64_t pb = hp_entry(2, hs, b, 0, 0, 0, 0);
    CHECK(pb >= 0 && pb < 11 && pb != pa);
    CHECK(hp_entry(0, hs, b, 0, 0, 0, 0) == pb);
    CHECK(hp_entry(1, hs, b, 0, 0, 0, 0) == (int64_t) GGML_HASHSET_ALREADY_EXISTS);

    // Reset the used-bitset through the raw field; keys stay.
    fg153_gs_zero_field(g, 7, 11);
    CHECK(fg153_gs_graph_get(g, 7, pa) == 0);
    CHECK(fg153_gs_graph_get(g, 8, pa) == a);
    CHECK(hp_entry(3, hs, a, 0, 0, 0, 0) == 0);
}

static void test_names(struct ggml_context * ctx) {
    struct ggml_tensor * t = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 4);
    t->name[0] = '\0';
    fg153_gt_ensure_name((int64_t) (intptr_t) t, 1, 7);
    CHECK(strcmp(t->name, "leaf_7") == 0);
    t->name[0] = '\0';
    fg153_gt_ensure_name((int64_t) (intptr_t) t, 0, 3);
    CHECK(strcmp(t->name, "node_3") == 0);
}

static void test_goose_entries(struct ggml_context * ctx) {
    struct ggml_tensor * x = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 4);
    struct ggml_tensor * w = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 4);
    struct ggml_tensor * a = ggml_add(ctx, x, w);
    ggml_set_name(x, "ux");
    ggml_set_name(w, "uw");
    ggml_set_name(a, "ua");

    int64_t g = gs_entry(0, (int64_t) (intptr_t) ctx, 16, 0, 0, 0, 0);
    CHECK(g != 0);

    int64_t n_new = gt_entry(0, g, (int64_t) (intptr_t) a, 0, 0, 0, 0);
    CHECK(n_new == 1);
    CHECK(gt_entry(4, g, 0, 0, 0, 0, 0) == 1);
    CHECK(gt_entry(5, g, 0, 0, 0, 0, 0) == 2);

    // Typed tensor fields on real tensors.
    CHECK(fg153_gt_tensor_get((int64_t) (intptr_t) a, 0, 0) == a->op);
    CHECK(fg153_gt_tensor_get((int64_t) (intptr_t) a, 1, 0) == a->flags);
    CHECK(fg153_gt_tensor_get((int64_t) (intptr_t) a, 2, 0) == (int64_t) (intptr_t) a->src[0]);

    struct ggml_cgraph view_storage;
    memset(&view_storage, 0, sizeof(view_storage));
    int64_t v = gs_entry(4, (int64_t) (intptr_t) &view_storage, g, 0, 1, 0, 0);
    CHECK(v == (int64_t) (intptr_t) &view_storage);
    CHECK(fg153_gs_graph_get(v, 1, 0) == 1);
    CHECK(fg153_gs_graph_get(v, 18, 0) == 0);
}

int main(void) {
    test_constants();
    test_hash_size();

    struct ggml_init_params params = {
        .mem_size = 8 * 1024 * 1024,
        .mem_buffer = NULL,
        .no_alloc = false,
    };
    struct ggml_context * ctx = ggml_init(params);
    if (!ctx) {
        fprintf(stderr, "ggml_init failed\n");
        return 1;
    }
    test_storage_guards(ctx);
    test_hash_primitives(ctx);
    test_names(ctx);
    test_goose_entries(ctx);
    ggml_free(ctx);

    printf("RESULT %s\n", g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}
