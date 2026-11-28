struct native_attention_inputs {
    float *q, *k, *v, *relative, *sinks, *gates;
};

_Static_assert(offsetof(struct decoder, flash_floats) == 887104 && offsetof(struct decoder, flash_workspace) == 887112 &&
    offsetof(struct decoder, kh) == 887120 && offsetof(struct decoder, vh) == 887128 &&
    offsetof(struct native_attention_inputs, q) == 0 && offsetof(struct native_attention_inputs, k) == 8 &&
    offsetof(struct native_attention_inputs, v) == 16 && offsetof(struct native_attention_inputs, relative) == 24 &&
    offsetof(struct native_attention_inputs, sinks) == 32 && offsetof(struct native_attention_inputs, gates) == 40,
    "Goose native attention row ABI");

_Static_assert(sizeof(struct inkling_attention_batch) == 80 &&
    offsetof(struct inkling_attention_batch, sliding) == 16 && offsetof(struct inkling_attention_batch, format) == 20 &&
    offsetof(struct inkling_attention_batch, q) == 24 && offsetof(struct inkling_attention_batch, k) == 32 &&
    offsetof(struct inkling_attention_batch, v) == 40 && offsetof(struct inkling_attention_batch, relative) == 48 &&
    offsetof(struct inkling_attention_batch, bias) == 56 && offsetof(struct inkling_attention_batch, sinks) == 64 &&
    offsetof(struct inkling_attention_batch, output) == 72 &&
    offsetof(struct inkling_attention_graph_call, inputs) == 80 && offsetof(struct inkling_attention_graph_call, output) == 200 &&
    offsetof(struct inkling_attention_input_projections, weights) == 24 &&
    offsetof(struct inkling_attention_input_projections, q_clip) == 88 &&
    offsetof(struct inkling_attention_input_projections, r_clip) == 96 &&
    offsetof(struct inkling_attention_input_projections, key_kernel) == 104 &&
    offsetof(struct inkling_attention_input_projections, value_kernel) == 112 &&
    sizeof(struct inkling_attention_projection) == 32 && offsetof(struct inkling_attention_projection, weights) == 8 &&
    offsetof(struct inkling_attention_projection, weight_bytes) == 16 &&
    offsetof(struct inkling_attention_projection, rows) == 24, "Goose native attention binding ABI");
