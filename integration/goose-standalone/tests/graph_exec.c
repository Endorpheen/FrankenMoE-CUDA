// Graph-execution gate: the Goose builder creates a node, ggml runs it.
//
// This is a TEST, not a runtime adapter: it holds no runtime logic and nothing
// links it into the shipped host. It exists to prove that the graph builder's
// decisions reach a computed result, and it is the evidence EXP681 records.
//
// Why two ops: one agreeing case could be a coincidence of a node that happens to
// be initialised to the right values. ADD and MUL together make that implausible.
//
// The module chooses mode, op, operands and result shape; this file only packs
// the argument array the module reads through fg158_arg and fills the inputs.
#include <stdint.h>
#include <stdio.h>

#include "ggml.h"

extern int64_t gb_common(int64_t p);
// The composed unit refuses to answer before its one initialiser has run.
extern void fg_goose_host_ensure_init(void);

int main(void) {
    fg_goose_host_ensure_init();

    struct ggml_init_params gp = { 4 * 1024 * 1024, NULL, /*.no_alloc =*/ false };
    struct ggml_context * gc = ggml_init(gp);
    if (gc == NULL) {
        printf("FG_GRAPH_EXEC init failed\n");
        return 2;
    }

    struct ggml_tensor * a = ggml_new_tensor_1d(gc, GGML_TYPE_F32, 4);
    struct ggml_tensor * b = ggml_new_tensor_1d(gc, GGML_TYPE_F32, 4);
    float * ad = (float *) a->data;
    float * bd = (float *) b->data;
    for (int i = 0; i < 4; ++i) {
        ad[i] = (float) (i + 1);
        bd[i] = (float) (10 * (i + 1));
    }

    static const int ops[] = { GGML_OP_ADD, GGML_OP_MUL };
    int failures = 0;

    for (int k = 0; k < 2; ++k) {
        // mode 0: the result is a fresh tensor shaped like `a`; op and operands
        // are the module's arguments, not this file's choice.
        uint64_t args[8] = { 0, (uint64_t) (uintptr_t) gc, (uint64_t) (uintptr_t) a,
                             (uint64_t) (uintptr_t) b, 0, (uint64_t) ops[k], 0, 0 };
        struct ggml_tensor * r =
            (struct ggml_tensor *) (uintptr_t) gb_common((int64_t) (uintptr_t) args);
        if (r == NULL) {
            printf("FG_GRAPH_EXEC op=%d builder returned null\n", ops[k]);
            ++failures;
            continue;
        }

        struct ggml_cgraph * gf = ggml_new_graph(gc);
        ggml_build_forward_expand(gf, r);
        const int rc = ggml_graph_compute_with_ctx(gc, gf, 1);

        const float * rd = (const float *) r->data;
        int ok = rd != NULL;
        for (int i = 0; ok && i < 4; ++i) {
            const float want = ops[k] == GGML_OP_ADD ? ad[i] + bd[i] : ad[i] * bd[i];
            if (rd[i] != want) ok = 0;
        }
        printf("FG_GRAPH_EXEC op=%d rc=%d result=[%.0f %.0f %.0f %.0f] %s\n",
               (int) r->op, rc, rd ? rd[0] : 0.f, rd ? rd[1] : 0.f,
               rd ? rd[2] : 0.f, rd ? rd[3] : 0.f, ok ? "PASS" : "FAIL");
        if (!ok) ++failures;
    }

    ggml_free(gc);
    return failures == 0 ? 0 : 1;
}
