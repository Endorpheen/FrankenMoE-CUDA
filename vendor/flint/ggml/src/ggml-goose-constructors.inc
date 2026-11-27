extern int64_t fg143_construct_elementwise(int64_t context, int64_t first, int64_t second,
    int64_t op, int64_t predicate, ggml_shape_slice params);
extern int64_t fg143_construct_row_sum(int64_t context, int64_t input, int64_t type, int64_t op);

static struct ggml_tensor * ggml_goose_elementwise(struct ggml_context *ctx,
        struct ggml_tensor *first, struct ggml_tensor *second, enum ggml_op op,
        int64_t predicate, const float *parameters, size_t count) {
    int64_t bits[2];
    for (size_t i = 0; i < count; ++i) {
        int32_t word;
        memcpy(&word, &parameters[i], sizeof(word));
        bits[i] = word;
    }
    return (void *)(intptr_t)fg143_construct_elementwise((intptr_t)ctx, (intptr_t)first, (intptr_t)second,
        op, predicate, (ggml_shape_slice){bits, (int64_t)count});
}

#define GOOSE_GGML_BINARY(name, op) \
    struct ggml_tensor * name(struct ggml_context *ctx, struct ggml_tensor *a, struct ggml_tensor *b) { \
        return ggml_goose_elementwise(ctx, a, b, op, 4, NULL, 0); \
    }
GOOSE_GGML_BINARY(ggml_add, GGML_OP_ADD)
GOOSE_GGML_BINARY(ggml_sub, GGML_OP_SUB)
GOOSE_GGML_BINARY(ggml_mul, GGML_OP_MUL)
GOOSE_GGML_BINARY(ggml_div, GGML_OP_DIV)
#undef GOOSE_GGML_BINARY

#define GOOSE_GGML_UNARY(name, op) \
    struct ggml_tensor * name(struct ggml_context *ctx, struct ggml_tensor *a) { \
        return ggml_goose_elementwise(ctx, a, NULL, op, 0, NULL, 0); \
    }
GOOSE_GGML_UNARY(ggml_sqr, GGML_OP_SQR)
GOOSE_GGML_UNARY(ggml_sqrt, GGML_OP_SQRT)
GOOSE_GGML_UNARY(ggml_log, GGML_OP_LOG)
#undef GOOSE_GGML_UNARY

#define GOOSE_GGML_NORM(name, op) \
    struct ggml_tensor * name(struct ggml_context *ctx, struct ggml_tensor *a, float eps) { \
        return ggml_goose_elementwise(ctx, a, NULL, op, 0, &eps, 1); \
    }
GOOSE_GGML_NORM(ggml_norm, GGML_OP_NORM)
GOOSE_GGML_NORM(ggml_rms_norm, GGML_OP_RMS_NORM)
GOOSE_GGML_NORM(ggml_l2_norm, GGML_OP_L2_NORM)
#undef GOOSE_GGML_NORM

struct ggml_tensor * ggml_scale_bias(struct ggml_context *ctx, struct ggml_tensor *a, float scale, float bias) {
    const float params[] = {scale, bias};
    return ggml_goose_elementwise(ctx, a, NULL, GGML_OP_SCALE, 2, params, 2);
}

struct ggml_tensor * ggml_scale(struct ggml_context *ctx, struct ggml_tensor *a, float scale) {
    return ggml_scale_bias(ctx, a, scale, 0.0f);
}

struct ggml_tensor * ggml_sum_rows(struct ggml_context *ctx, struct ggml_tensor *a) {
    return (void *)(intptr_t)fg143_construct_row_sum((intptr_t)ctx, (intptr_t)a, a->type, GGML_OP_SUM_ROWS);
}

extern int64_t fg143_construct_reshape(int64_t context, int64_t input, ggml_shape_slice shape, int64_t op);
extern int64_t fg143_construct_view(int64_t context, int64_t input, ggml_shape_slice shape,
    ggml_layout_slice strides, uint64_t offset, int64_t op);
extern int64_t fg143_construct_axis_map(int64_t context, int64_t input, ggml_shape_slice axes,
    int64_t op, uint8_t transpose);

int64_t fg143_ctor_view_storage(int64_t context, int64_t source, ggml_shape_slice shape, uint64_t offset) {
    struct ggml_tensor *input = (void *)(intptr_t)source;
    return (intptr_t)ggml_new_tensor_impl((void *)(intptr_t)context, input->type,
        (int)shape.len, shape.data, input, offset);
}

void fg143_ctor_geometry(int64_t address, ggml_layout_slice geometry, uint8_t write) {
    struct ggml_tensor *tensor = (void *)(intptr_t)address;
    for (int i = 0; i < GGML_MAX_DIMS; ++i) {
        if (write) {
            tensor->ne[i] = (int64_t)geometry.data[i];
            tensor->nb[i] = (size_t)geometry.data[4 + i];
        } else {
            geometry.data[i] = (uint64_t)tensor->ne[i];
            geometry.data[4 + i] = tensor->nb[i];
        }
    }
}

void fg143_ctor_caption(int64_t address, int64_t source, int64_t kind) {
    const char *suffix[] = {"reshaped", "view", "permuted", "transposed"};
    const struct ggml_tensor *input = (const void *)(intptr_t)source;
    ggml_format_name((void *)(intptr_t)address, "%s (%s)", input->name, suffix[kind]);
}

struct ggml_tensor * ggml_reshape_2d(struct ggml_context *ctx, struct ggml_tensor *a, int64_t n0, int64_t n1) {
    const int64_t shape[] = {n0, n1};
    return (void *)(intptr_t)fg143_construct_reshape((intptr_t)ctx, (intptr_t)a,
        (ggml_shape_slice){shape, 2}, GGML_OP_RESHAPE);
}

struct ggml_tensor * ggml_reshape_3d(struct ggml_context *ctx, struct ggml_tensor *a, int64_t n0, int64_t n1, int64_t n2) {
    const int64_t shape[] = {n0, n1, n2};
    return (void *)(intptr_t)fg143_construct_reshape((intptr_t)ctx, (intptr_t)a,
        (ggml_shape_slice){shape, 3}, GGML_OP_RESHAPE);
}

struct ggml_tensor * ggml_reshape_4d(struct ggml_context *ctx, struct ggml_tensor *a,
        int64_t n0, int64_t n1, int64_t n2, int64_t n3) {
    const int64_t shape[] = {n0, n1, n2, n3};
    return (void *)(intptr_t)fg143_construct_reshape((intptr_t)ctx, (intptr_t)a,
        (ggml_shape_slice){shape, 4}, GGML_OP_RESHAPE);
}

struct ggml_tensor * ggml_view_1d(struct ggml_context *ctx, struct ggml_tensor *a, int64_t n0, size_t offset) {
    return (void *)(intptr_t)fg143_construct_view((intptr_t)ctx, (intptr_t)a,
        (ggml_shape_slice){&n0, 1}, (ggml_layout_slice){0}, offset, GGML_OP_VIEW);
}

struct ggml_tensor * ggml_view_2d(struct ggml_context *ctx, struct ggml_tensor *a,
        int64_t n0, int64_t n1, size_t s1, size_t offset) {
    const int64_t shape[] = {n0, n1};
    uint64_t strides[] = {s1};
    return (void *)(intptr_t)fg143_construct_view((intptr_t)ctx, (intptr_t)a,
        (ggml_shape_slice){shape, 2}, (ggml_layout_slice){strides, 1}, offset, GGML_OP_VIEW);
}

struct ggml_tensor * ggml_view_3d(struct ggml_context *ctx, struct ggml_tensor *a,
        int64_t n0, int64_t n1, int64_t n2, size_t s1, size_t s2, size_t offset) {
    const int64_t shape[] = {n0, n1, n2};
    uint64_t strides[] = {s1, s2};
    return (void *)(intptr_t)fg143_construct_view((intptr_t)ctx, (intptr_t)a,
        (ggml_shape_slice){shape, 3}, (ggml_layout_slice){strides, 2}, offset, GGML_OP_VIEW);
}

struct ggml_tensor * ggml_view_4d(struct ggml_context *ctx, struct ggml_tensor *a,
        int64_t n0, int64_t n1, int64_t n2, int64_t n3, size_t s1, size_t s2, size_t s3, size_t offset) {
    const int64_t shape[] = {n0, n1, n2, n3};
    uint64_t strides[] = {s1, s2, s3};
    return (void *)(intptr_t)fg143_construct_view((intptr_t)ctx, (intptr_t)a,
        (ggml_shape_slice){shape, 4}, (ggml_layout_slice){strides, 3}, offset, GGML_OP_VIEW);
}

struct ggml_tensor * ggml_permute(struct ggml_context *ctx, struct ggml_tensor *a, int ax0, int ax1, int ax2, int ax3) {
    const int64_t axes[] = {ax0, ax1, ax2, ax3};
    return (void *)(intptr_t)fg143_construct_axis_map((intptr_t)ctx, (intptr_t)a,
        (ggml_shape_slice){axes, 4}, GGML_OP_PERMUTE, false);
}

struct ggml_tensor * ggml_transpose(struct ggml_context *ctx, struct ggml_tensor *a) {
    const int64_t axes[] = {1, 0, 2, 3};
    return (void *)(intptr_t)fg143_construct_axis_map((intptr_t)ctx, (intptr_t)a,
        (ggml_shape_slice){axes, 4}, GGML_OP_TRANSPOSE, true);
}

extern int64_t fg143_construct_product(int64_t context, int64_t first, int64_t second,
    int64_t ids, int64_t output_type, int64_t id_type, int64_t op);
extern int64_t fg143_construct_copy(int64_t context, int64_t input, int64_t target,
    int64_t kind, int64_t op, int64_t mode);

struct ggml_tensor * ggml_mul_mat(struct ggml_context *ctx, struct ggml_tensor *a, struct ggml_tensor *b) {
    return (void *)(intptr_t)fg143_construct_product((intptr_t)ctx, (intptr_t)a, (intptr_t)b, 0,
        GGML_TYPE_F32, GGML_TYPE_I32, GGML_OP_MUL_MAT);
}

struct ggml_tensor * ggml_mul_mat_id(struct ggml_context *ctx, struct ggml_tensor *a,
        struct ggml_tensor *b, struct ggml_tensor *ids) {
    return (void *)(intptr_t)fg143_construct_product((intptr_t)ctx, (intptr_t)a, (intptr_t)b, (intptr_t)ids,
        GGML_TYPE_F32, GGML_TYPE_I32, GGML_OP_MUL_MAT_ID);
}

struct ggml_tensor * ggml_cpy(struct ggml_context *ctx, struct ggml_tensor *a, struct ggml_tensor *b) {
    return (void *)(intptr_t)fg143_construct_copy((intptr_t)ctx, (intptr_t)a, (intptr_t)b, 0, GGML_OP_CPY, 0);
}

struct ggml_tensor * ggml_cast(struct ggml_context *ctx, struct ggml_tensor *a, enum ggml_type type) {
    return (void *)(intptr_t)fg143_construct_copy((intptr_t)ctx, (intptr_t)a, 0, type, GGML_OP_CPY, 1);
}

struct ggml_tensor * ggml_cont(struct ggml_context *ctx, struct ggml_tensor *a) {
    return (void *)(intptr_t)fg143_construct_copy((intptr_t)ctx, (intptr_t)a, 0, 0, GGML_OP_CONT, 2);
}

extern int64_t fg143_construct_flash_attention(int64_t context, int64_t q, int64_t k, int64_t v, int64_t mask,
    ggml_shape_slice params, int64_t output_type, int64_t mask_type, int64_t op);
extern int64_t fg143_construct_rope(int64_t context, int64_t input, int64_t positions, int64_t factors,
    ggml_shape_slice config, ggml_shape_slice scales, ggml_shape_slice sections);

static void ggml_copy_float_words(int64_t *out, const float *values, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        int32_t word;
        memcpy(&word, &values[i], sizeof(word));
        out[i] = word;
    }
}

struct ggml_tensor * ggml_flash_attn_ext(struct ggml_context *ctx, struct ggml_tensor *q,
        struct ggml_tensor *k, struct ggml_tensor *v, struct ggml_tensor *mask,
        float scale, float max_bias, float logit_softcap) {
    const float values[] = {scale, max_bias, logit_softcap};
    int64_t params[3];
    ggml_copy_float_words(params, values, 3);
    return (void *)(intptr_t)fg143_construct_flash_attention((intptr_t)ctx, (intptr_t)q,
        (intptr_t)k, (intptr_t)v, (intptr_t)mask, (ggml_shape_slice){params, 3},
        GGML_TYPE_F32, GGML_TYPE_F16, GGML_OP_FLASH_ATTN_EXT);
}

struct ggml_tensor * ggml_rope_multi(struct ggml_context *ctx, struct ggml_tensor *a,
        struct ggml_tensor *positions, struct ggml_tensor *factors, int n_dims,
        int sections[GGML_MROPE_SECTIONS], int mode, int n_ctx_orig,
        float freq_base, float freq_scale, float ext_factor, float attn_factor,
        float beta_fast, float beta_slow) {
    const int64_t config[] = {n_dims, mode, n_ctx_orig, GGML_ROPE_TYPE_MROPE,
        GGML_TYPE_I32, GGML_TYPE_F32, GGML_OP_ROPE};
    const float values[] = {freq_base, freq_scale, ext_factor, attn_factor, beta_fast, beta_slow};
    int64_t words[6], axes[GGML_MROPE_SECTIONS];
    ggml_copy_float_words(words, values, 6);
    if (sections && (mode & GGML_ROPE_TYPE_MROPE)) {
        for (int i = 0; i < GGML_MROPE_SECTIONS; ++i) axes[i] = sections[i];
    } else {
        sections = NULL;
    }
    return (void *)(intptr_t)fg143_construct_rope((intptr_t)ctx, (intptr_t)a, (intptr_t)positions,
        (intptr_t)factors, (ggml_shape_slice){config, 7}, (ggml_shape_slice){words, 6},
        (ggml_shape_slice){axes, sections ? GGML_MROPE_SECTIONS : 0});
}

struct ggml_tensor * ggml_rope_ext(struct ggml_context *ctx, struct ggml_tensor *a,
        struct ggml_tensor *positions, struct ggml_tensor *factors, int n_dims, int mode,
        int n_ctx_orig, float freq_base, float freq_scale, float ext_factor, float attn_factor,
        float beta_fast, float beta_slow) {
    return ggml_rope_multi(ctx, a, positions, factors, n_dims, NULL, mode, n_ctx_orig,
        freq_base, freq_scale, ext_factor, attn_factor, beta_fast, beta_slow);
}

extern int64_t fg143_construct_rows(int64_t context, int64_t a, int64_t b, int64_t ids,
    ggml_shape_slice types, int64_t op);
extern int64_t fg143_construct_glu(int64_t context, int64_t a, int64_t b, int64_t op,
    int64_t subtype, uint8_t swapped, ggml_shape_slice limit);
extern int64_t fg143_construct_tile(int64_t context, int64_t a, int64_t b, int64_t axis,
    ggml_shape_slice shape, int64_t op);

static const int64_t ggml_rows_types[] = {GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_I32, GGML_TYPE_I64};

struct ggml_tensor * ggml_get_rows(struct ggml_context *ctx, struct ggml_tensor *a, struct ggml_tensor *b) {
    return (void *)(intptr_t)fg143_construct_rows((intptr_t)ctx, (intptr_t)a, (intptr_t)b, 0,
        (ggml_shape_slice){ggml_rows_types, 4}, GGML_OP_GET_ROWS);
}

struct ggml_tensor * ggml_set_rows(struct ggml_context *ctx, struct ggml_tensor *a,
        struct ggml_tensor *b, struct ggml_tensor *ids) {
    return (void *)(intptr_t)fg143_construct_rows((intptr_t)ctx, (intptr_t)a, (intptr_t)b, (intptr_t)ids,
        (ggml_shape_slice){ggml_rows_types, 4}, GGML_OP_SET_ROWS);
}

struct ggml_tensor * ggml_glu(struct ggml_context *ctx, struct ggml_tensor *a,
        enum ggml_glu_op op, bool swapped) {
    return (void *)(intptr_t)fg143_construct_glu((intptr_t)ctx, (intptr_t)a, 0,
        GGML_OP_GLU, op, swapped, (ggml_shape_slice){0});
}

struct ggml_tensor * ggml_swiglu(struct ggml_context *ctx, struct ggml_tensor *a) {
    return ggml_glu(ctx, a, GGML_GLU_OP_SWIGLU, false);
}

struct ggml_tensor * ggml_swiglu_swapped(struct ggml_context *ctx, struct ggml_tensor *a) {
    return ggml_glu(ctx, a, GGML_GLU_OP_SWIGLU, true);
}

struct ggml_tensor * ggml_swiglu_split(struct ggml_context *ctx, struct ggml_tensor *a, struct ggml_tensor *b) {
    return (void *)(intptr_t)fg143_construct_glu((intptr_t)ctx, (intptr_t)a, (intptr_t)b,
        GGML_OP_GLU, GGML_GLU_OP_SWIGLU, false, (ggml_shape_slice){0});
}

struct ggml_tensor * ggml_swiglu_clamp(struct ggml_context *ctx, struct ggml_tensor *a,
        struct ggml_tensor *b, float limit) {
    int64_t word;
    ggml_copy_float_words(&word, &limit, 1);
    return (void *)(intptr_t)fg143_construct_glu((intptr_t)ctx, (intptr_t)a, (intptr_t)b,
        GGML_OP_GLU, GGML_GLU_OP_SWIGLU_CLAMP, false, (ggml_shape_slice){&word, 1});
}

struct ggml_tensor * ggml_repeat_4d(struct ggml_context *ctx, struct ggml_tensor *a,
        int64_t n0, int64_t n1, int64_t n2, int64_t n3) {
    const int64_t shape[] = {n0, n1, n2, n3};
    return (void *)(intptr_t)fg143_construct_tile((intptr_t)ctx, (intptr_t)a, 0, 0,
        (ggml_shape_slice){shape, 4}, GGML_OP_REPEAT);
}

struct ggml_tensor * ggml_concat(struct ggml_context *ctx, struct ggml_tensor *a,
        struct ggml_tensor *b, int axis) {
    return (void *)(intptr_t)fg143_construct_tile((intptr_t)ctx, (intptr_t)a, (intptr_t)b, axis,
        (ggml_shape_slice){0}, GGML_OP_CONCAT);
}

struct ggml_tensor * ggml_unary(struct ggml_context *ctx, struct ggml_tensor *a, enum ggml_unary_op op) {
    const int64_t value = op;
    return (void *)(intptr_t)fg143_construct_elementwise((intptr_t)ctx, (intptr_t)a,
        0, GGML_OP_UNARY, 1, (ggml_shape_slice){&value, 1});
}

extern int64_t fg143_construct_ssm_conv(int64_t context, int64_t input, int64_t kernel,
    int64_t type, int64_t op);
extern int64_t fg143_construct_ssm_scan(int64_t context, ggml_shape_slice inputs,
    int64_t snapshots, int64_t type, int64_t ids_type, int64_t op);
extern int64_t fg143_construct_gated_delta(int64_t context, ggml_shape_slice inputs,
    int64_t snapshots, int64_t type, int64_t op);
extern int64_t fg143_construct_indexer(int64_t context, ggml_shape_slice inputs, int64_t type,
    int64_t mask_type, int64_t op);

struct ggml_tensor * ggml_ssm_conv(struct ggml_context *ctx, struct ggml_tensor *x,
        struct ggml_tensor *kernel) {
    return (void *)(intptr_t)fg143_construct_ssm_conv((intptr_t)ctx, (intptr_t)x,
        (intptr_t)kernel, GGML_TYPE_F32, GGML_OP_SSM_CONV);
}

struct ggml_tensor * ggml_ssm_scan(struct ggml_context *ctx, struct ggml_tensor *state,
        struct ggml_tensor *x, struct ggml_tensor *dt, struct ggml_tensor *a,
        struct ggml_tensor *b, struct ggml_tensor *c, struct ggml_tensor *ids, int64_t snapshots) {
    const int64_t inputs[] = {(intptr_t)state, (intptr_t)x, (intptr_t)dt, (intptr_t)a,
        (intptr_t)b, (intptr_t)c, (intptr_t)ids};
    return (void *)(intptr_t)fg143_construct_ssm_scan((intptr_t)ctx, (ggml_shape_slice){inputs, 7},
        snapshots, GGML_TYPE_F32, GGML_TYPE_I32, GGML_OP_SSM_SCAN);
}

struct ggml_tensor * ggml_gated_delta_net(struct ggml_context *ctx, struct ggml_tensor *q,
        struct ggml_tensor *k, struct ggml_tensor *v, struct ggml_tensor *g,
        struct ggml_tensor *beta, struct ggml_tensor *state, int64_t snapshots) {
    const int64_t inputs[] = {(intptr_t)q, (intptr_t)k, (intptr_t)v, (intptr_t)g,
        (intptr_t)beta, (intptr_t)state};
    return (void *)(intptr_t)fg143_construct_gated_delta((intptr_t)ctx, (ggml_shape_slice){inputs, 6},
        snapshots, GGML_TYPE_F32, GGML_OP_GATED_DELTA_NET);
}

struct ggml_tensor * ggml_lightning_indexer(struct ggml_context *ctx, struct ggml_tensor *q,
        struct ggml_tensor *k, struct ggml_tensor *weights, struct ggml_tensor *mask) {
    const int64_t inputs[] = {(intptr_t)q, (intptr_t)k, (intptr_t)weights, (intptr_t)mask};
    return (void *)(intptr_t)fg143_construct_indexer((intptr_t)ctx, (ggml_shape_slice){inputs, 4},
        GGML_TYPE_F32, GGML_TYPE_F16, GGML_OP_LIGHTNING_INDEXER);
}

extern int64_t fg143_construct_softmax(int64_t context, int64_t input, int64_t mask,
    ggml_shape_slice params, int64_t f32_type, int64_t f16_type, int64_t op);
extern int64_t fg143_construct_fill(int64_t context, int64_t input, int64_t value,
    int64_t f32_type, int64_t f16_type, int64_t op);
extern int64_t fg143_construct_sort(int64_t context, int64_t input, int64_t limit, int64_t order,
    int64_t mode, int64_t type, int64_t op, int64_t view_op);

struct ggml_tensor * ggml_clamp(struct ggml_context *ctx, struct ggml_tensor *a, float min, float max) {
    const float params[] = {min, max};
    return ggml_goose_elementwise(ctx, a, NULL, GGML_OP_CLAMP, 0, params, 2);
}

struct ggml_tensor * ggml_soft_max_ext(struct ggml_context *ctx, struct ggml_tensor *a,
        struct ggml_tensor *mask, float scale, float max_bias) {
    const float values[] = {scale, max_bias};
    int64_t params[2];
    ggml_copy_float_words(params, values, 2);
    return (void *)(intptr_t)fg143_construct_softmax((intptr_t)ctx, (intptr_t)a, (intptr_t)mask,
        (ggml_shape_slice){params, 2}, GGML_TYPE_F32, GGML_TYPE_F16, GGML_OP_SOFT_MAX);
}

struct ggml_tensor * ggml_soft_max(struct ggml_context *ctx, struct ggml_tensor *a) {
    return ggml_soft_max_ext(ctx, a, NULL, 1.0f, 0.0f);
}

struct ggml_tensor * ggml_fill(struct ggml_context *ctx, struct ggml_tensor *a, float value) {
    int64_t word;
    ggml_copy_float_words(&word, &value, 1);
    return (void *)(intptr_t)fg143_construct_fill((intptr_t)ctx, (intptr_t)a, word,
        GGML_TYPE_F32, GGML_TYPE_F16, GGML_OP_FILL);
}

struct ggml_tensor * ggml_argsort(struct ggml_context *ctx, struct ggml_tensor *a, enum ggml_sort_order order) {
    return (void *)(intptr_t)fg143_construct_sort((intptr_t)ctx, (intptr_t)a, 0, order,
        0, GGML_TYPE_I32, GGML_OP_ARGSORT, GGML_OP_VIEW);
}

struct ggml_tensor * ggml_argsort_top_k(struct ggml_context *ctx, struct ggml_tensor *a, int k) {
    return (void *)(intptr_t)fg143_construct_sort((intptr_t)ctx, (intptr_t)a, k, GGML_SORT_ORDER_DESC,
        1, GGML_TYPE_I32, GGML_OP_ARGSORT, GGML_OP_VIEW);
}

struct ggml_tensor * ggml_top_k(struct ggml_context *ctx, struct ggml_tensor *a, int k) {
    return (void *)(intptr_t)fg143_construct_sort((intptr_t)ctx, (intptr_t)a, k, 0,
        2, GGML_TYPE_I32, GGML_OP_TOP_K, GGML_OP_VIEW);
}
