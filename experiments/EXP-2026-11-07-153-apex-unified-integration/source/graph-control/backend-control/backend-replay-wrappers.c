// Exact-signature wrappers for in-scope APX016/015 methods.
// Each wrapper dispatches to bc_entry mode; no policy here.
#include "ggml.h"
#include "ggml-backend.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int fg153_bc_enabled(void) {
    const char * v = getenv("GGML_GOOSE_BACKEND_CONTROL");
    return !v || strcmp(v, "0") != 0;
}

extern int64_t bc_entry(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);

#ifdef __cplusplus
extern "C" {
#endif

void fg153_bc_ggml_backend_tensor_set(struct ggml_tensor * tensor, const void * data, size_t offset, size_t size) {
    (void) bc_entry(0, (int64_t) (intptr_t) tensor, (int64_t) (intptr_t) data,
                    (int64_t) offset, (int64_t) size, 0, 0, 0);
}

void fg153_bc_ggml_backend_tensor_get(const struct ggml_tensor * tensor, void * data, size_t offset, size_t size) {
    (void) bc_entry(1, (int64_t) (intptr_t) tensor, (int64_t) (intptr_t) data,
                    (int64_t) offset, (int64_t) size, 0, 0, 0);
}

void fg153_bc_ggml_backend_tensor_set_async(ggml_backend_t backend, struct ggml_tensor * tensor,
                                             const void * data, size_t offset, size_t size) {
    (void) bc_entry(2, (int64_t) (intptr_t) backend, (int64_t) (intptr_t) tensor,
                    (int64_t) (intptr_t) data, (int64_t) offset, (int64_t) size, 0, 0);
}

void fg153_bc_ggml_backend_tensor_get_async(ggml_backend_t backend, const struct ggml_tensor * tensor,
                                             void * data, size_t offset, size_t size) {
    (void) bc_entry(3, (int64_t) (intptr_t) backend, (int64_t) (intptr_t) tensor,
                    (int64_t) (intptr_t) data, (int64_t) offset, (int64_t) size, 0, 0);
}

void fg153_bc_ggml_backend_tensor_memset(struct ggml_tensor * tensor, uint8_t value, size_t offset, size_t size) {
    (void) bc_entry(4, (int64_t) (intptr_t) tensor, (int64_t) value,
                    (int64_t) offset, (int64_t) size, 0, 0, 0);
}

void fg153_bc_ggml_backend_tensor_copy(const struct ggml_tensor * src, struct ggml_tensor * dst) {
    (void) bc_entry(5, (int64_t) (intptr_t) src, (int64_t) (intptr_t) dst, 0, 0, 0, 0, 0);
}

void fg153_bc_ggml_backend_synchronize(ggml_backend_t backend) {
    (void) bc_entry(6, (int64_t) (intptr_t) backend, 0, 0, 0, 0, 0, 0);
}

enum ggml_status fg153_bc_ggml_backend_graph_compute(ggml_backend_t backend, struct ggml_cgraph * cgraph) {
    return (enum ggml_status) bc_entry(7, (int64_t) (intptr_t) backend,
                                       (int64_t) (intptr_t) cgraph, 0, 0, 0, 0, 0);
}

enum ggml_status fg153_bc_ggml_backend_graph_compute_async(ggml_backend_t backend, struct ggml_cgraph * cgraph) {
    return (enum ggml_status) bc_entry(8, (int64_t) (intptr_t) backend,
                                       (int64_t) (intptr_t) cgraph, 0, 0, 0, 0, 0);
}

bool fg153_bc_ggml_backend_supports_op(ggml_backend_t backend, const struct ggml_tensor * op) {
    return bc_entry(9, (int64_t) (intptr_t) backend, (int64_t) (intptr_t) op, 0, 0, 0, 0, 0) != 0;
}

bool fg153_bc_ggml_backend_supports_buft(ggml_backend_t backend, ggml_backend_buffer_type_t buft) {
    return bc_entry(10, (int64_t) (intptr_t) backend, (int64_t) (intptr_t) buft, 0, 0, 0, 0, 0) != 0;
}

bool fg153_bc_ggml_backend_offload_op(ggml_backend_t backend, const struct ggml_tensor * op) {
    return bc_entry(11, (int64_t) (intptr_t) backend, (int64_t) (intptr_t) op, 0, 0, 0, 0, 0) != 0;
}

void fg153_bc_ggml_backend_graph_optimize(ggml_backend_t backend, struct ggml_cgraph * cgraph,
                                          struct ggml_backend_graph_optimize_params * params) {
    (void) bc_entry(12, (int64_t) (intptr_t) backend, (int64_t) (intptr_t) cgraph,
                    (int64_t) (intptr_t) params, 0, 0, 0, 0);
}

ggml_backend_event_t fg153_bc_ggml_backend_event_new(ggml_backend_dev_t device) {
    return (ggml_backend_event_t) (intptr_t) bc_entry(13, (int64_t) (intptr_t) device, 0, 0, 0, 0, 0, 0);
}

void fg153_bc_ggml_backend_event_free(ggml_backend_event_t event) {
    (void) bc_entry(14, (int64_t) (intptr_t) event, 0, 0, 0, 0, 0, 0);
}

void fg153_bc_ggml_backend_event_record(ggml_backend_event_t event, ggml_backend_t backend) {
    (void) bc_entry(15, (int64_t) (intptr_t) event, (int64_t) (intptr_t) backend, 0, 0, 0, 0, 0);
}

void fg153_bc_ggml_backend_event_synchronize(ggml_backend_event_t event) {
    (void) bc_entry(16, (int64_t) (intptr_t) event, 0, 0, 0, 0, 0, 0);
}

void fg153_bc_ggml_backend_event_wait(ggml_backend_t backend, ggml_backend_event_t event) {
    (void) bc_entry(17, (int64_t) (intptr_t) backend, (int64_t) (intptr_t) event, 0, 0, 0, 0, 0);
}

const char * fg153_bc_ggml_backend_name(ggml_backend_t backend) {
    return (const char *) (intptr_t) bc_entry(18, (int64_t) (intptr_t) backend, 0, 0, 0, 0, 0, 0);
}

#ifdef __cplusplus
}
#endif
