#include "ggml.h"
#include <pthread.h>

_Static_assert(sizeof(void *) == 8 && GGML_MAX_NAME == 64 &&
    offsetof(struct ggml_tensor, type) == 0 &&
    offsetof(struct ggml_tensor, ne) == 16 &&
    offsetof(struct ggml_tensor, nb) == 48 &&
    offsetof(struct ggml_tensor, op) == 80 &&
    offsetof(struct ggml_tensor, op_params) == 84 &&
    offsetof(struct ggml_tensor, src) == 152 &&
    offsetof(struct ggml_tensor, view_src) == 232 &&
    offsetof(struct ggml_tensor, view_offs) == 240 &&
    offsetof(struct ggml_tensor, data) == 248 &&
    offsetof(struct ggml_tensor, name) == 256 &&
    GGML_TYPE_F32 == 0 && GGML_TYPE_I32 == 26 && GGML_OP_CUSTOM == 95,
    "Goose graph bindings ABI");

_Static_assert(sizeof(pthread_mutex_t) <= sizeof(uint64_t[8]) &&
    _Alignof(pthread_mutex_t) <= _Alignof(uint64_t), "Goose GGML lock ABI");

_Static_assert(sizeof(struct ggml_init_params) == 24 &&
    offsetof(struct ggml_init_params, mem_size) == 0 &&
    offsetof(struct ggml_init_params, mem_buffer) == 8 &&
    offsetof(struct ggml_init_params, no_alloc) == 16 &&
    GGML_OP_MUL_MAT == 29 && GGML_OP_MUL_MAT_ID == 30 &&
    GGML_UNARY_OP_TANH == 4 && GGML_UNARY_OP_RELU == 6 &&
    GGML_UNARY_OP_SIGMOID == 7 && GGML_UNARY_OP_SILU == 10 &&
    GGML_UNARY_OP_SOFTPLUS == 15, "Goose arena entry ABI");
