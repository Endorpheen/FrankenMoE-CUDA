#ifndef INKLING_NATIVE_KV_H
#define INKLING_NATIVE_KV_H

#include <stdbool.h>
#include <stdint.h>

enum {
    INKLING_VISITS = 48, INKLING_CONTEXT_LIMIT = 1048576,
    INKLING_WINDOW = 1024, INKLING_PREFILL_LIMIT = 4096
};

enum inkling_kv_format { INKLING_KV_F16, INKLING_KV_Q4_0 };

struct inkling_attention_batch {
    int slot, capacity, first, count;
    bool sliding;
    enum inkling_kv_format format;
    const float *q, *k, *v, *relative, *bias, *sinks;
    float *output;
};

struct inkling_attention_projection {
    const float *gate;
    const uint8_t *weights;
    int64_t weight_bytes;
    int rows;
};

struct inkling_attention_input_projections {
    const float *input[3];
    const uint8_t *weights[8];
    const float *q_clip, *r_clip, *key_kernel, *value_kernel;
};

enum { INKLING_ATTENTION_GRAPH = 0x49324147 };
struct inkling_attention_graph_call {
    struct inkling_attention_batch batch;
    struct inkling_attention_input_projections inputs;
    struct inkling_attention_projection output;
};

int inkling_attention_graph_execute(const struct inkling_attention_graph_call *call);
int inkling_attention_execute_resident(const struct inkling_attention_batch *batch,
    const struct inkling_attention_input_projections *inputs,
    const struct inkling_attention_projection *output);

int inkling_attention_execute(const struct inkling_attention_batch *batch);
int inkling_attention_execute_project(const struct inkling_attention_batch *batch,
                                     const struct inkling_attention_projection *projection);
int inkling_attention_reset(void);
void inkling_attention_report(void);
#endif
