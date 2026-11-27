static size_t ggml_graph_layout(size_t nodes, bool grads, uint64_t offsets[8]) {
    const size_t hash_size = ggml_hash_size(nodes * 2);
    const size_t bitset_bytes = ggml_bitset_size(hash_size) * sizeof(ggml_bitset_t);
    const size_t width = sizeof(struct ggml_tensor *);
    offsets[0] = GGML_PAD(sizeof(struct ggml_cgraph), width);
    offsets[1] = offsets[0] + nodes * width;
    offsets[2] = offsets[1] + nodes * width;
    offsets[3] = GGML_PAD(offsets[2] + hash_size * sizeof(int32_t), width);
    const size_t hash_end = offsets[3] + hash_size * width;
    offsets[4] = grads ? hash_end : 0;
    offsets[5] = grads ? hash_end + hash_size * width : 0;
    offsets[6] = grads ? hash_end + 2 * hash_size * width : hash_end;
    offsets[7] = offsets[6] + bitset_bytes;
    return hash_size;
}

size_t ggml_graph_overhead_custom(size_t size, bool grads) {
    uint64_t offsets[8];
    ggml_graph_layout(size, grads, offsets);
    return GGML_OBJECT_SIZE + GGML_PAD(offsets[7], GGML_MEM_ALIGN);
}

struct ggml_cgraph * ggml_new_graph_custom(struct ggml_context * ctx, size_t size, bool grads) {
    uint64_t offsets[8];
    const size_t hash_size = ggml_graph_layout(size, grads, offsets);
    struct ggml_object *obj = ggml_new_object(ctx, GGML_OBJECT_TYPE_GRAPH, offsets[7]);
    char *base = (char *)ctx->mem_buffer + obj->offs;
    struct ggml_cgraph *cgraph = (void *)base;

    *cgraph = (struct ggml_cgraph) {
        /*.size         =*/ size,
        /*.n_nodes      =*/ 0,
        /*.n_leafs      =*/ 0,
        /*.nodes        =*/ (void *)(base + offsets[0]),
        /*.grads        =*/ grads ? (void *)(base + offsets[4]) : NULL,
        /*.grad_accs    =*/ grads ? (void *)(base + offsets[5]) : NULL,
        /*.leafs        =*/ (void *)(base + offsets[1]),
        /*.use_counts   =*/ (void *)(base + offsets[2]),
        /*.hash_table   =*/ { hash_size, (void *)(base + offsets[6]), (void *)(base + offsets[3]) },
        /*.order        =*/ GGML_CGRAPH_EVAL_ORDER_LEFT_TO_RIGHT,
        /*.uid          =*/ 0,
    };

    ggml_hash_set_reset(&cgraph->visited_hash_set);
    if (grads) {
        memset(cgraph->grads,     0, hash_size*sizeof(struct ggml_tensor *));
        memset(cgraph->grad_accs, 0, hash_size*sizeof(struct ggml_tensor *));
    }

    return cgraph;
}

struct ggml_tensor * ggml_set_zero(struct ggml_tensor * tensor) {
    if (ggml_is_empty(tensor)) {
        return tensor;
    }
    if (tensor->buffer) {
        ggml_backend_tensor_memset(tensor, 0, 0, ggml_nbytes(tensor));
        return tensor;
    }
    GGML_ASSERT(tensor->data);
    memset(tensor->data, 0, ggml_nbytes(tensor));
    return tensor;
}

void ggml_graph_clear(struct ggml_cgraph * cgraph) {
    cgraph->n_leafs = 0;
    cgraph->n_nodes = 0;
    ggml_hash_set_reset(&cgraph->visited_hash_set);
}

void ggml_set_input(struct ggml_tensor * tensor) {
    GGML_ASSERT(tensor->op == GGML_OP_NONE);
    tensor->flags |= GGML_TENSOR_FLAG_INPUT;
}

void ggml_set_output(struct ggml_tensor * tensor) {
    for (struct ggml_tensor * cur = tensor; cur != NULL; cur = cur->view_src) {
        cur->flags |= GGML_TENSOR_FLAG_OUTPUT;
    }
}
