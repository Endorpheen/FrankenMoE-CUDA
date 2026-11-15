// Graph-execution gate: the Goose builder creates a node, a ggml backend runs it.
//
// This is a TEST, not a runtime adapter: it holds no runtime logic and nothing
// links it into the shipped host. It proves two things:
//
//   1. the graph builder's decisions reach a computed result;
//   2. the compute goes through the backend abstraction, not through a bare
//      ggml_graph_compute on a CPU context. That abstraction is the seam a GPU
//      backend plugs into, so proving it on CPU is the prerequisite for wiring
//      CUDA without changing the Goose side.
//
// Only public ggml-backend API is used: no struct field is touched, so the gate
// cannot drift from ggml's internals the way a hand-rolled buffer manager would.
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"

extern int64_t gb_common(int64_t p);
// The composed unit refuses to answer before its one initialiser has run.
extern void fg_goose_host_ensure_init(void);

int main(void) {
    fg_goose_host_ensure_init();

    // no_alloc: the backend owns the tensor memory, which is what makes this a
    // backend test rather than a context test.
    struct ggml_init_params gp = { 4 * 1024 * 1024, NULL, /*.no_alloc =*/ true };
    struct ggml_context * gc = ggml_init(gp);
    if (gc == NULL) {
        printf("FG_GRAPH_EXEC init failed\n");
        return 2;
    }

    ggml_backend_t backend = ggml_backend_cpu_init();
    if (backend == NULL) {
        printf("FG_GRAPH_EXEC backend init failed\n");
        ggml_free(gc);
        return 2;
    }

    struct ggml_tensor * a = ggml_new_tensor_1d(gc, GGML_TYPE_F32, 4);
    struct ggml_tensor * b = ggml_new_tensor_1d(gc, GGML_TYPE_F32, 4);
    if (!ggml_backend_alloc_ctx_tensors(gc, backend)) {
        printf("FG_GRAPH_EXEC backend alloc failed\n");
        ggml_backend_free(backend);
        ggml_free(gc);
        return 2;
    }

    const float ad[4] = { 1.f, 2.f, 3.f, 4.f };
    const float bd[4] = { 10.f, 20.f, 30.f, 40.f };
    ggml_backend_tensor_set(a, ad, 0, sizeof ad);
    ggml_backend_tensor_set(b, bd, 0, sizeof bd);

    static const char * names[] = { "ADD", "MUL" };
    static const int ops[] = { GGML_OP_ADD, GGML_OP_MUL };
    int failures = 0;

    for (int k = 0; k < 2; ++k) {
        // mode 0: the result is a fresh tensor shaped like `a`; the module chooses
        // mode, op and operands, this file only packs them.
        uint64_t args[8] = { 0, (uint64_t) (uintptr_t) gc, (uint64_t) (uintptr_t) a,
                             (uint64_t) (uintptr_t) b, 0, (uint64_t) ops[k], 0, 0 };
        struct ggml_tensor * r =
            (struct ggml_tensor *) (uintptr_t) gb_common((int64_t) (uintptr_t) args);
        if (r == NULL) {
            printf("FG_GRAPH_EXEC %s builder returned null\n", names[k]);
            ++failures;
            continue;
        }
        // The builder created this node after the backend allocation, so ask the
        // backend for the result buffer too.
        ggml_backend_alloc_ctx_tensors(gc, backend);

        struct ggml_cgraph * gf = ggml_new_graph(gc);
        ggml_build_forward_expand(gf, r);
        const enum ggml_status st = ggml_backend_graph_compute(backend, gf);

        float got[4] = { 0, 0, 0, 0 };
        ggml_backend_tensor_get(r, got, 0, sizeof got);

        int ok = st == GGML_STATUS_SUCCESS;
        for (int i = 0; ok && i < 4; ++i) {
            const float want = ops[k] == GGML_OP_ADD ? ad[i] + bd[i] : ad[i] * bd[i];
            if (got[i] != want) ok = 0;
        }
        printf("FG_GRAPH_EXEC %s backend=cpu status=%d result=[%.0f %.0f %.0f %.0f] %s\n",
               names[k], (int) st, got[0], got[1], got[2], got[3], ok ? "PASS" : "FAIL");
        if (!ok) ++failures;
    }

    ggml_backend_free(backend);
    ggml_free(gc);
    return failures == 0 ? 0 : 1;
}
