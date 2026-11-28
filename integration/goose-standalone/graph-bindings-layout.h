#include "ggml.h"

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
