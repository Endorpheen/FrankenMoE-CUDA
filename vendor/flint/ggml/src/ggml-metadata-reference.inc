const char * ggml_status_to_string(enum ggml_status status) {
    switch (status) {
        case GGML_STATUS_ALLOC_FAILED: return "GGML status: error (failed to allocate memory)";
        case GGML_STATUS_FAILED:       return "GGML status: error (operation failed)";
        case GGML_STATUS_SUCCESS:      return "GGML status: success";
        case GGML_STATUS_ABORTED:      return "GGML status: warning (operation aborted)";
    }

    return "GGML status: unknown";
}

int64_t ggml_blck_size(enum ggml_type type) {
    assert(type >= 0);
    assert(type < GGML_TYPE_COUNT);
    return type_traits[type].blck_size;
}

size_t ggml_type_size(enum ggml_type type) {
    assert(type >= 0);
    assert(type < GGML_TYPE_COUNT);
    return type_traits[type].type_size;
}

size_t ggml_row_size(enum ggml_type type, int64_t ne) {
    assert(type >= 0);
    assert(type < GGML_TYPE_COUNT);
    assert(ne % ggml_blck_size(type) == 0);
    return ggml_type_size(type)*ne/ggml_blck_size(type);
}

double ggml_type_sizef(enum ggml_type type) {
    assert(type >= 0);
    assert(type < GGML_TYPE_COUNT);
    return ((double)(type_traits[type].type_size))/type_traits[type].blck_size;
}

const char * ggml_type_name(enum ggml_type type) {
    assert(type >= 0);
    assert(type < GGML_TYPE_COUNT);
    return type_traits[type].type_name;
}

bool ggml_is_quantized(enum ggml_type type) {
    assert(type >= 0);
    assert(type < GGML_TYPE_COUNT);
    return type_traits[type].is_quantized;
}

const char * ggml_op_name(enum ggml_op op) {
    return GGML_OP_NAME[op];
}

const char * ggml_unary_op_name(enum ggml_unary_op op) {
    return GGML_UNARY_OP_NAME[op];
}

const char * ggml_glu_op_name(enum ggml_glu_op op) {
    return GGML_GLU_OP_NAME[op];
}
