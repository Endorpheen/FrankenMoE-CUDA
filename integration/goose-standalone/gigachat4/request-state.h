#ifndef GIGA_REQUEST_STATE_H
#define GIGA_REQUEST_STATE_H

#include "ggml.h"
#include "model-bindings.h"

struct giga_request {
    struct ggml_context *descriptors;
    uint64_t state[32 * 4];
    uint64_t draft[3];
    int64_t capacity;
    int64_t committed;
    int64_t draft_committed;
    bool device_state;
};

struct giga_step {
    struct ggml_tensor *tokens;
    struct ggml_tensor *positions;
    struct ggml_tensor *rows;
    struct ggml_tensor *mask;
    struct ggml_tensor *output_ids;
    int64_t base;
    int64_t count;
    int64_t output_count;
    int64_t cells;
    bool draft;
    bool draft_catchup;
};

struct giga_step_output {
    struct ggml_tensor *hidden;
    struct ggml_tensor *logits;
};

int giga_request_init(struct giga_request *request, const struct giga_bindings *model,
                      int64_t capacity);
void giga_request_free(struct giga_request *request);
void giga_request_reset(struct giga_request *request);
int giga_step_prepare(const struct giga_request *request, struct ggml_context *ctx,
                      int64_t count, int64_t output_count, bool draft, struct giga_step *step);
int giga_step_upload(const struct giga_step *step, const int32_t *tokens);

int giga_step_graph(struct giga_request *request, struct giga_bindings *model,
                    struct ggml_context *ctx, struct ggml_cgraph *graph,
                    const struct giga_step *step, struct ggml_tensor *draft_hidden,
                    const struct giga_graph_config *config, struct giga_step_output *output);
int giga_step_commit(struct giga_request *request, const struct giga_step *step);

#endif
