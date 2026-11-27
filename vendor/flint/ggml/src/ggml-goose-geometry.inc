extern int64_t fg143_geometry_product(ggml_shape_slice shape, int64_t first);
extern uint8_t fg143_geometry_unit_from(ggml_shape_slice shape, int64_t first);
extern int64_t fg143_geometry_rank(ggml_shape_slice shape);
extern uint8_t fg143_geometry_empty(ggml_shape_slice shape);
extern uint8_t fg143_geometry_equal(ggml_shape_slice a, ggml_shape_slice b);
extern uint8_t fg143_geometry_repeat(ggml_shape_slice a, ggml_shape_slice b);
extern int64_t goose_tensor_span(int64_t kind, ggml_shape_slice layout);
extern uint8_t fg143_geometry_contiguous(int64_t kind, ggml_shape_slice layout, int64_t loose);
extern uint8_t fg143_geometry_rows(int64_t kind, int64_t width, int64_t stride);

static ggml_shape_slice ggml_shape(const struct ggml_tensor *tensor) {
    return (ggml_shape_slice){tensor->ne, GGML_MAX_DIMS};
}

static void ggml_load_layout(const struct ggml_tensor *tensor, int64_t layout[8]) {
    _Static_assert(GGML_MAX_DIMS == 4 && sizeof(tensor->ne) == 4 * sizeof(int64_t) &&
        sizeof(tensor->nb) == 4 * sizeof(int64_t), "Goose tensor geometry ABI");
    memcpy(layout, tensor->ne, sizeof(tensor->ne));
    memcpy(layout + 4, tensor->nb, sizeof(tensor->nb));
}

int64_t ggml_nelements(const struct ggml_tensor *tensor) { return fg143_geometry_product(ggml_shape(tensor), 0); }
int64_t ggml_nrows(const struct ggml_tensor *tensor) { return fg143_geometry_product(ggml_shape(tensor), 1); }
int ggml_n_dims(const struct ggml_tensor *tensor) { return (int)fg143_geometry_rank(ggml_shape(tensor)); }

bool ggml_is_scalar(const struct ggml_tensor *tensor) { return fg143_geometry_unit_from(ggml_shape(tensor), 0); }
bool ggml_is_vector(const struct ggml_tensor *tensor) { return fg143_geometry_unit_from(ggml_shape(tensor), 1); }
bool ggml_is_matrix(const struct ggml_tensor *tensor) { return fg143_geometry_unit_from(ggml_shape(tensor), 2); }
bool ggml_is_3d(const struct ggml_tensor *tensor) { return fg143_geometry_unit_from(ggml_shape(tensor), 3); }
bool ggml_is_empty(const struct ggml_tensor *tensor) { return fg143_geometry_empty(ggml_shape(tensor)); }

size_t ggml_nbytes(const struct ggml_tensor *tensor) {
    int64_t layout[8];
    ggml_load_layout(tensor, layout);
    const int64_t bytes = goose_tensor_span(tensor->type, (ggml_shape_slice){layout, 8});
    GGML_ASSERT(bytes >= 0);
    return (size_t)bytes;
}

static bool ggml_goose_contiguous(const struct ggml_tensor *tensor, int64_t loose) {
    int64_t layout[8];
    ggml_load_layout(tensor, layout);
    return fg143_geometry_contiguous(tensor->type, (ggml_shape_slice){layout, 8}, loose);
}

bool ggml_is_contiguous(const struct ggml_tensor *tensor) { return ggml_goose_contiguous(tensor, 0); }
bool ggml_is_contiguous_0(const struct ggml_tensor *tensor) { return ggml_goose_contiguous(tensor, 0); }
bool ggml_is_contiguous_1(const struct ggml_tensor *tensor) { return ggml_goose_contiguous(tensor, 1); }
bool ggml_is_contiguous_rows(const struct ggml_tensor *tensor) {
    return fg143_geometry_rows(tensor->type, tensor->ne[0], (int64_t)tensor->nb[0]);
}

bool ggml_are_same_shape(const struct ggml_tensor *a, const struct ggml_tensor *b) {
    return fg143_geometry_equal(ggml_shape(a), ggml_shape(b));
}

bool ggml_are_same_stride(const struct ggml_tensor *a, const struct ggml_tensor *b) {
    return memcmp(a->nb, b->nb, sizeof(a->nb)) == 0;
}

bool ggml_can_repeat(const struct ggml_tensor *a, const struct ggml_tensor *b) {
    return fg143_geometry_repeat(ggml_shape(a), ggml_shape(b));
}
