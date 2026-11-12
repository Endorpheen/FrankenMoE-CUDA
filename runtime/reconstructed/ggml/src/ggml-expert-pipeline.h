#pragma once
#include "ggml-backend.h"

enum ggml_expert_pipeline_phase {
    GGML_EXPERT_QUERY,
    GGML_EXPERT_RESERVE,
    GGML_EXPERT_BIND,
    GGML_EXPERT_BEGIN,
    GGML_EXPERT_END,
    GGML_EXPERT_CANCEL,
};

typedef bool (*ggml_expert_pipeline_fn)(ggml_backend_t backend,
    enum ggml_expert_pipeline_phase phase, const struct ggml_tensor * src,
    struct ggml_tensor * dst, size_t * value);
