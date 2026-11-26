#ifndef GIGA_MODEL_BINDINGS_H
#define GIGA_MODEL_BINDINGS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { GIGA_TENSOR_COUNT = 1036, GIGA_LAYER_COUNT = 33 };

struct ggml_context;

struct giga_graph_config {
    float tuning[5];
    float routed_limits[32];
    float shared_limits[33];
    uint64_t factors[33];
    float rope[6];
    float index_rope[6];
    int32_t original_context;
    bool normalize;
};

struct giga_bindings {
    struct ggml_context *descriptors;
    uint64_t weights[GIGA_TENSOR_COUNT];
    int64_t offsets[GIGA_LAYER_COUNT + 1];
    int64_t kinds[GIGA_LAYER_COUNT];
    struct giga_graph_config config;
};

int giga_bind_model(struct giga_bindings *out, const void *mapping, size_t bytes);
void giga_unbind_model(struct giga_bindings *bindings);
#endif
