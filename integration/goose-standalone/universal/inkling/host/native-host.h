struct native_attention_inputs {
    float *q, *k, *v, *relative, *sinks, *gates;
};

extern void fg_i2_native_workspace(struct decoder *d, int count, struct native_attention_inputs *args);
extern void fg_i2_native_finish(const struct native_attention_inputs *args, int count, float *output);

_Static_assert(offsetof(struct decoder, flash_floats) == 887104 && offsetof(struct decoder, flash_workspace) == 887112 &&
    offsetof(struct decoder, kh) == 887120 && offsetof(struct decoder, vh) == 887128 &&
    offsetof(struct native_attention_inputs, q) == 0 && offsetof(struct native_attention_inputs, k) == 8 &&
    offsetof(struct native_attention_inputs, v) == 16 && offsetof(struct native_attention_inputs, relative) == 24 &&
    offsetof(struct native_attention_inputs, sinks) == 32 && offsetof(struct native_attention_inputs, gates) == 40,
    "Goose native attention row ABI");
extern void fg_i2_prepare_native_row(struct decoder *d, const struct native_attention_inputs *args,
    const int64_t *aux, int slot, int position, int index, bool sliding, float *projected, int packed);

extern void fg_i2_native_submit(struct decoder *d, const struct native_attention_inputs *args,
    const int64_t *aux, int slot, int begin, int count, bool sliding, float *out);
extern void fg_i2_native_project(struct decoder *d, const struct native_attention_inputs *args,
    const int64_t *aux, int slot, int begin, int count, bool sliding, int64_t weight_id, float *out);

static bool use_resident_attention_projection(void) {
    const char *setting = getenv("I2_ATTENTION_RESIDENT");
    return !setting || strcmp(setting, "0") != 0;
}

static bool use_device_attention_input(void) {
    const char *setting = getenv("I2_ATTENTION_DEVICE_INPUT");
    return use_resident_attention_projection() && (!setting || strcmp(setting, "0") != 0);
}

extern void fg_i2_host_bind_attention(struct decoder *d, const int64_t *core, const int64_t *aux,
    int slot, int begin, int count, bool sliding, struct inkling_attention_graph_call *call);
extern void fg_i2_native_resident(struct decoder *d, const int64_t *core, const int64_t *aux,
    int slot, int begin, int count, bool sliding, const float *q, const float *k, const float *v, float *out);
#define native_attention_resident fg_i2_native_resident

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
