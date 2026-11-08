#pragma once
// Declarations inserted by backend-control replay.py.
#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-backend-impl.h"

#ifdef __cplusplus
extern "C" {
#endif

int fg153_bc_enabled(void); // default-on; GGML_GOOSE_BACKEND_CONTROL=0 disables

void fg153_bc_ggml_backend_tensor_set(struct ggml_tensor *, const void *, size_t, size_t);
void fg153_bc_ggml_backend_tensor_get(const struct ggml_tensor *, void *, size_t, size_t);
void fg153_bc_ggml_backend_tensor_set_async(ggml_backend_t, struct ggml_tensor *, const void *, size_t, size_t);
void fg153_bc_ggml_backend_tensor_get_async(ggml_backend_t, const struct ggml_tensor *, void *, size_t, size_t);
void fg153_bc_ggml_backend_tensor_memset(struct ggml_tensor *, uint8_t, size_t, size_t);
void fg153_bc_ggml_backend_tensor_copy(const struct ggml_tensor *, struct ggml_tensor *);
void fg153_bc_ggml_backend_synchronize(ggml_backend_t);
enum ggml_status fg153_bc_ggml_backend_graph_compute(ggml_backend_t, struct ggml_cgraph *);
enum ggml_status fg153_bc_ggml_backend_graph_compute_async(ggml_backend_t, struct ggml_cgraph *);
bool fg153_bc_ggml_backend_supports_op(ggml_backend_t, const struct ggml_tensor *);
bool fg153_bc_ggml_backend_supports_buft(ggml_backend_t, ggml_backend_buffer_type_t);
bool fg153_bc_ggml_backend_offload_op(ggml_backend_t, const struct ggml_tensor *);
void fg153_bc_ggml_backend_graph_optimize(ggml_backend_t, struct ggml_cgraph *, struct ggml_backend_graph_optimize_params *);
ggml_backend_event_t fg153_bc_ggml_backend_event_new(ggml_backend_dev_t);
void fg153_bc_ggml_backend_event_free(ggml_backend_event_t);
void fg153_bc_ggml_backend_event_record(ggml_backend_event_t, ggml_backend_t);
void fg153_bc_ggml_backend_event_synchronize(ggml_backend_event_t);
void fg153_bc_ggml_backend_event_wait(ggml_backend_t, ggml_backend_event_t);
const char * fg153_bc_ggml_backend_name(ggml_backend_t);

#ifdef __cplusplus
}
#endif
