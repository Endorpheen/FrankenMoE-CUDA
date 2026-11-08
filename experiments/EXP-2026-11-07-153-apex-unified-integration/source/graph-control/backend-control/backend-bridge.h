#pragma once
// APX016/015 backend-control raw bridge.
// ABI sources: inputs/ggml-backend-impl.h (iface layouts), inputs/ggml.h
// (tensor), inputs/active-ggml-backend.cpp (function anchors).
//
// The service exposes only raw typed callback access and field/byte/alloc
// primitives. Branch, loop, error and lifetime policy is in
// backend-control.goose. Scheduler and buffer functions stay parent-owned.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Backend iface codes:
// 0=get_name, 2=set_tensor_async, 3=get_tensor_async, 9=graph_compute,
// 10=synchronize, 12=event_record, 13=event_wait, 14=graph_optimize.
int64_t fg153_bc_backend_has(int64_t backend, int64_t code);
int64_t fg153_bc_backend_call(int64_t backend, int64_t code,
                              int64_t a, int64_t b, int64_t c, int64_t d, int64_t e);
int64_t fg153_bc_backend_device(int64_t backend);
int64_t fg153_bc_backend_name(int64_t backend);

// Device iface codes:
// 0=supports_op, 1=offload_op, 2=event_new, 3=event_free,
// 4=event_synchronize, 5=supports_buft.
int64_t fg153_bc_dev_has(int64_t device, int64_t code);
int64_t fg153_bc_dev_call(int64_t device, int64_t code,
                          int64_t a, int64_t b, int64_t c, int64_t d, int64_t e);

// Buffer iface codes:
// 0=cpy_tensor, 1=set_tensor, 2=get_tensor, 5=memset_tensor.
int64_t fg153_bc_buffer_has(int64_t buffer, int64_t code);
int64_t fg153_bc_buffer_call(int64_t buffer, int64_t code,
                             int64_t a, int64_t b, int64_t c, int64_t d, int64_t e);

// Tensor fields: 0=data, 1=buffer, 2=view_src, 3=view_offs, 4=op,
// 5=flags, 6=name pointer, 7=src[i], 8=op_params pointer, 9=type.
int64_t fg153_bc_tensor_get(int64_t tensor, int64_t code, int64_t i);
int64_t fg153_bc_tensor_buffer(int64_t tensor); // effective buffer: view_src ? view_src->buffer : buffer
int64_t fg153_bc_tensor_nbytes(int64_t tensor);
int64_t fg153_bc_same_layout(int64_t a, int64_t b);
int64_t fg153_bc_buffer_is_host(int64_t buffer);

// Event field: 0=device.
int64_t fg153_bc_event_get(int64_t event, int64_t code);

// OS byte/allocation primitives.
int64_t fg153_bc_alloc(int64_t bytes);
void    fg153_bc_free_mem(int64_t ptr);

// Failure hook shared with the graph cohort style.
void fg153_bc_fail(int64_t code);

#ifdef __cplusplus
}
#endif
