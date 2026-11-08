// Executable native/Goose backend-control fixture.
// Real ggml context, tensors, CPU backend, real backend buffer and graph.
// Calls the exact fg153_bc_* wrappers (Goose entry) directly; no lookup,
// no PENDING. Deferred graph_copy/compare/plan are not exercised.
//
// Error/cancellation cleanup: every failure path jumps to cleanup and frees
// event, buffer, backend, context. Zero-size transfers and same-tensor copy
// are the cancellation-equivalent early returns from the source.

#include "backend-replay-wrappers.h"
#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-alloc.h"
#include "ggml-cpu.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int check_bytes(const void * got, const void * want, size_t n, const char * tag) {
    if (memcmp(got, want, n) != 0) {
        fprintf(stderr, "FAIL byte compare: %s\n", tag);
        return 0;
    }
    return 1;
}

int main(void) {
    int ok = 1;
    struct ggml_init_params params = {
        .mem_size = 4 * 1024 * 1024,
        .mem_buffer = NULL,
        .no_alloc = true,
    };
    struct ggml_context * ctx = ggml_init(params);
    ggml_backend_t backend = NULL;
    ggml_backend_buffer_t buffer = NULL;
    ggml_backend_event_t event = NULL;
    if (!ctx) {
        return 1;
    }

    struct ggml_tensor * src = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 8);
    struct ggml_tensor * dst = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 8);
    struct ggml_tensor * out = ggml_add(ctx, src, dst);
    ggml_set_name(src, "src");
    ggml_set_name(dst, "dst");
    ggml_set_name(out, "out");
    struct ggml_cgraph * graph = ggml_new_graph(ctx);
    ggml_build_forward_expand(graph, out);

    backend = ggml_backend_cpu_init();
    if (!backend) {
        fprintf(stderr, "FAIL backend init\n");
        ok = 0;
        goto cleanup;
    }
    buffer = ggml_backend_alloc_ctx_tensors(ctx, backend);
    if (!buffer) {
        fprintf(stderr, "FAIL buffer alloc\n");
        ok = 0;
        goto cleanup;
    }

    const size_t n = ggml_nbytes(src);
    float host[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    float got[8];
    memset(got, 0, sizeof(got));

    fg153_bc_ggml_backend_tensor_set(src, host, 0, n);
    fg153_bc_ggml_backend_tensor_get(src, got, 0, n);
    ok = ok && check_bytes(got, host, n, "set/get sync");

    fg153_bc_ggml_backend_synchronize(backend);
    fg153_bc_ggml_backend_tensor_memset(dst, 0x2a, 0, n);
    fg153_bc_ggml_backend_tensor_get(dst, got, 0, n);
    unsigned char want42[32];
    memset(want42, 0x2a, sizeof(want42));
    ok = ok && check_bytes(got, want42, n, "memset");

    fg153_bc_ggml_backend_tensor_copy(src, dst);
    fg153_bc_ggml_backend_tensor_get(dst, got, 0, n);
    ok = ok && check_bytes(got, host, n, "tensor copy");

    // Async path: CPU has no async callback, so Goose falls back to sync.
    memset(got, 0, sizeof(got));
    fg153_bc_ggml_backend_tensor_set_async(backend, src, host, 0, n);
    fg153_bc_ggml_backend_tensor_get_async(backend, src, got, 0, n);
    ok = ok && check_bytes(got, host, n, "async fallback");

    // Cancellation-equivalent early returns.
    fg153_bc_ggml_backend_tensor_set(src, host, 0, 0);
    fg153_bc_ggml_backend_tensor_get(src, got, 0, 0);
    fg153_bc_ggml_backend_tensor_memset(dst, 0xff, 0, 0);
    fg153_bc_ggml_backend_tensor_copy(src, src);

    ok = ok && fg153_bc_ggml_backend_supports_op(backend, out);
    ok = ok && fg153_bc_ggml_backend_supports_buft(backend, ggml_backend_get_default_buffer_type(backend));
    (void) fg153_bc_ggml_backend_offload_op(backend, out);

    struct ggml_backend_graph_optimize_params opt = {0};
    fg153_bc_ggml_backend_graph_optimize(backend, graph, &opt);

    enum ggml_status st = fg153_bc_ggml_backend_graph_compute_async(backend, graph);
    fg153_bc_ggml_backend_synchronize(backend);
    if (st != GGML_STATUS_SUCCESS) {
        fprintf(stderr, "FAIL graph compute async status=%d\n", (int) st);
        ok = 0;
        goto cleanup;
    }
    st = fg153_bc_ggml_backend_graph_compute(backend, graph);
    if (st != GGML_STATUS_SUCCESS) {
        fprintf(stderr, "FAIL graph compute status=%d\n", (int) st);
        ok = 0;
        goto cleanup;
    }
    fg153_bc_ggml_backend_tensor_get(out, got, 0, n);
    for (int i = 0; i < 8; ++i) {
        if (got[i] != host[i] * 2.0f) {
            fprintf(stderr, "FAIL graph result i=%d got=%f\n", i, got[i]);
            ok = 0;
            break;
        }
    }

    const char * name = fg153_bc_ggml_backend_name(backend);
    if (!name || !name[0]) {
        fprintf(stderr, "FAIL backend name\n");
        ok = 0;
    }
    if (strcmp(fg153_bc_ggml_backend_name(NULL), "NULL") != 0) {
        fprintf(stderr, "FAIL null backend name\n");
        ok = 0;
    }

    // Event branch: CPU may not expose events. NULL is the valid optional
    // path; if present, free it through the same Goose control.
    event = fg153_bc_ggml_backend_event_new(ggml_backend_get_device(backend));
    if (event) {
        fg153_bc_ggml_backend_event_record(event, backend);
        fg153_bc_ggml_backend_event_wait(backend, event);
        fg153_bc_ggml_backend_event_synchronize(event);
    }
    if (fg153_bc_ggml_backend_event_new(NULL) != NULL) {
        fprintf(stderr, "FAIL event_new(NULL)\n");
        ok = 0;
    }

cleanup:
    if (event) {
        fg153_bc_ggml_backend_event_free(event);
    }
    if (buffer) {
        ggml_backend_buffer_free(buffer);
    }
    if (backend) {
        ggml_backend_free(backend);
    }
    ggml_free(ctx);
    printf("RESULT %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
