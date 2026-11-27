extern int64_t i2_prepare_native(sl_f32 projected, sl_f32 q_clip, sl_f32 r_clip,
    sl_f32 key_kernel, sl_f32 value_kernel, int64_t position, uint8_t sliding,
    sl_f32 key_history, sl_f32 value_history, sl_f32 q, sl_f32 r, sl_f32 k, sl_f32 v);
extern int64_t i2_finish_native(sl_f32 gate, sl_f32 output);

struct native_attention_inputs {
    float *q, *k, *v, *relative, *sinks, *gates;
};

static struct native_attention_inputs native_attention_inputs(struct decoder *d, int count) {
    const size_t floats = (size_t)count * (2 * attn_width + 2 * key_width + 384 + 24);
    if (d->flash_floats < floats) {
        float *storage = realloc(d->flash_workspace, floats * sizeof(float));
        assert(storage);
        d->flash_workspace = storage;
        d->flash_floats = floats;
    }
    float *q = d->flash_workspace;
    float *k = q + count * attn_width;
    float *v = k + count * key_width;
    float *r = v + count * key_width;
    float *s = r + count * 384;
    float *g = s + count * 24;
    return (struct native_attention_inputs){q, k, v, r, s, g};
}

static void native_attention_prepare(struct decoder *d, struct native_attention_inputs args,
    const int64_t *aux, int slot, int position, int index, bool sliding, float *projected, int packed) {
    assert(i2_prepare_native(vec(projected, packed), coefficients(d, aux[4], 3072),
        coefficients(d, aux[7], 384), coefficients(d, aux[0], 4096),
        coefficients(d, aux[1], 4096), position, sliding,
        vec(d->kh + slot * 3 * key_width, 3 * key_width), vec(d->vh + slot * 3 * key_width, 3 * key_width),
        vec(args.q + index * attn_width, attn_width), vec(args.relative + index * 384, 384),
        vec(args.k + index * key_width, key_width), vec(args.v + index * key_width, key_width)) == 1);
    memcpy(args.sinks + index * 24, projected + 5504, 24 * sizeof(float));
    memcpy(args.gates + index * attn_width, projected + 5528, attn_width * sizeof(float));
}

static void native_attention_submit(struct decoder *d, struct native_attention_inputs args,
    const int64_t *aux, int slot, int begin, int count, bool sliding, float *out) {
    const struct inkling_attention_batch request = {
        .slot = slot, .capacity = d->capacity, .first = begin, .count = count, .sliding = sliding,
        .format = d->format, .q = args.q, .k = args.k, .v = args.v, .relative = args.relative, .sinks = args.sinks,
        .bias = coefficients(d, aux[6], 16384).data, .output = out,
    };
    const double start = now();
    assert(inkling_attention_execute(&request) == 0);
    call_seconds[0] += now() - start;
    ++call_count[0];
    for (int i = 0; i < count; ++i) {
        assert(i2_finish_native(vec(args.gates + i * attn_width, attn_width),
                                vec(out + i * attn_width, attn_width)) == attn_width);
    }
}

static bool use_resident_attention_projection(void) {
    const char *setting = getenv("I2_ATTENTION_RESIDENT");
    return !setting || strcmp(setting, "0") != 0;
}

static void native_attention_project(struct decoder *d, struct native_attention_inputs args,
    const int64_t *aux, int slot, int begin, int count, bool sliding, weight weights, float *out) {
    assert(weights.kind == GGML_TYPE_Q8_0 && weights.shape[0] == attn_width &&
           weights.shape[1] == model_width && weights.shape[2] == 1 && weights.shape[3] == 1);
    const struct inkling_attention_batch batch = {
        .slot = slot, .capacity = d->capacity, .first = begin, .count = count, .sliding = sliding,
        .format = d->format, .q = args.q, .k = args.k, .v = args.v, .relative = args.relative, .sinks = args.sinks,
        .bias = coefficients(d, aux[6], 16384).data, .output = out,
    };
    const struct inkling_attention_projection projection = {
        .gate = args.gates, .weights = weights.data,
        .weight_bytes = model_width * ggml_row_size(GGML_TYPE_Q8_0, attn_width),
        .rows = model_width,
    };
    const double start = now();
    assert(inkling_attention_execute_project(&batch, &projection) == 0);
    call_seconds[0] += now() - start;
    ++call_count[0];
}

static bool use_device_attention_input(void) {
    const char *setting = getenv("I2_ATTENTION_DEVICE_INPUT");
    return use_resident_attention_projection() && (!setting || strcmp(setting, "0") != 0);
}

static struct inkling_attention_graph_call bind_attention_graph(struct decoder *d, const int64_t *core, const int64_t *aux,
    int slot, int begin, int count, bool sliding) {
    const int64_t ids[] = {core[0], core[1], core[2], aux[5], aux[8], core[4], aux[15], aux[16]};
    const int widths[] = {3072, 1024, 1024, 384, 24, 3072, 32, 32};
    struct inkling_attention_input_projections inputs = {
        .q_clip = coefficients(d, aux[4], 3072).data,
        .r_clip = coefficients(d, aux[7], 384).data,
        .key_kernel = coefficients(d, aux[0], 4096).data,
        .value_kernel = coefficients(d, aux[1], 4096).data,
    };
    for (int i = 0; i < (sliding ? 6 : 8); ++i) {
        weight w = named(d, ids[i]);
        assert(w.kind == GGML_TYPE_Q8_0 && w.shape[0] == model_width &&
               w.shape[1] == widths[i] && w.shape[2] == 1 && w.shape[3] == 1);
        inputs.weights[i] = w.data;
    }
    weight w = named(d, core[3]);
    assert(w.kind == GGML_TYPE_Q8_0 && w.shape[0] == attn_width &&
           w.shape[1] == model_width && w.shape[2] == 1 && w.shape[3] == 1);
    const struct inkling_attention_projection projection = {
        .weights = w.data, .weight_bytes = model_width * ggml_row_size(GGML_TYPE_Q8_0, attn_width),
        .rows = model_width,
    };
    const struct inkling_attention_batch batch = {
        .slot = slot, .capacity = d->capacity, .first = begin, .count = count,
        .sliding = sliding, .format = d->format, .bias = coefficients(d, aux[6], 16384).data,
    };
    return (struct inkling_attention_graph_call){batch, inputs, projection};
}

static void native_attention_resident(struct decoder *d, const int64_t *core, const int64_t *aux,
    int slot, int begin, int count, bool sliding, const float *q, const float *k, const float *v, float *out) {
    struct inkling_attention_graph_call call = bind_attention_graph(d, core, aux, slot, begin, count, sliding);
    call.inputs.input[0] = q;
    call.inputs.input[1] = k;
    call.inputs.input[2] = v;
    call.batch.output = out;
    const double start = now();

    assert(inkling_attention_execute_resident(&call.batch, &call.inputs, &call.output) == 0);
    call_seconds[0] += now() - start;
    ++call_count[0];
}

static int reset_inkling_attention(struct decoder *d) {
    if (inkling_attention_reset() != 0) return -1;
    memset(d->kh, 0, logical * 3 * key_width * sizeof(float));
    memset(d->vh, 0, logical * 3 * key_width * sizeof(float));
    memset(d->ha, 0, logical * 3 * model_width * sizeof(float));
    memset(d->hf, 0, logical * 3 * model_width * sizeof(float));
    return 0;
}
