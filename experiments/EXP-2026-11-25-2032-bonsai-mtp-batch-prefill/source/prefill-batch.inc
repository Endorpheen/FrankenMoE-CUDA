int64_t bonsai_mtp_concat_launch(uint64_t context, int64_t embeddings, int64_t hidden, int64_t joined) {
    model *m = (model *)(uintptr_t)context;
    CUdeviceptr e = embeddings, h = hidden, dst = joined;
    return launch(m, DRAFT_CONCAT, 20 * m->count, 1, 1, 256, (void *[]){ &e, &h, &dst });
}

int64_t bonsai_mtp_mat_batch(uint64_t context, int64_t weights, int64_t input,
                             int64_t output, int64_t width, int64_t rows) {
    model *m = (model *)(uintptr_t)context;
    if (width <= 0 || width % 32 || rows <= 0 || rows % 4) return -1;
    CUdeviceptr w = weights, x = input, y = output;
    unsigned blocks = (unsigned)width / 32, nrows = (unsigned)rows, tokens = m->count;
    return launch(m, DRAFT_MAT_BATCH, nrows/4, (tokens+3)/4, 1, 128,
                  (void *[]){ &w, &x, &y, &blocks, &nrows, &tokens });
}

static int draft_prefill_block(model *m, const unsigned *tokens, unsigned start,
                               unsigned count, CUdeviceptr previous) {
    enum { DRAFT_BATCH_ROWS = 128 };
    if (count < 2 || count > DRAFT_BATCH_ROWS) return -1;
    const CUdeviceptr states = m->b[13];
    if (save_conditioning(states, previous) ||
        check(bonsai_cuda.fn_cuMemcpyDtoDAsync(states + DRAFT_HIDDEN_BYTES,
            (CUdeviceptr)m->b[0] + (size_t)(start - (m->position - m->count)) * DRAFT_HIDDEN_BYTES,
            (size_t)(count - 1) * DRAFT_HIDDEN_BYTES, NULL), "shifted prompt hidden rows")) return -1;
    int32_t positions[4 * DRAFT_BATCH_ROWS];
    for (unsigned row = 0; row < count; ++row) {
        positions[row] = positions[count+row] = positions[2*count+row] = (int32_t)(start + row);
        positions[3*count+row] = 0;
    }
    const unsigned target_count = m->count;
    const CUdeviceptr target_rope_k = m->rope_k;
    m->rope_k = m->draft_rope_k;
    m->count = count;
    const int rc = rope_config(m->rope_k, 4, count) ||
        check(bonsai_cuda.fn_cuMemcpyHtoD(m->positions, positions, 16 * count), "draft positions") ||
        check(bonsai_cuda.fn_cuMemcpyHtoD(m->token_ids, tokens, 4 * count), "draft token ids") ||
        bonsai_mtp_kv_batch((uintptr_t)m, slice(m->mtp_layer, 15), slice(m->draft_kv, 2),
            slice(m->mtp, 4), slice(m->globals, 3), slice(m->b, 24), tokens[0], start);
    m->count = target_count;
    m->rope_k = target_rope_k;
    return rc;
}

static int draft_prefill(model *m, const unsigned *ids, unsigned rows) {
    if (!m->draft_batch || rows < 2) return draft_catchup(m, ids, rows, 0);
    const unsigned start = m->position - rows;
    const CUdeviceptr hidden = m->b[0];
    for (unsigned at = 0; at < rows;) {
        const unsigned remaining = rows - at;
        const unsigned count = remaining > 128 ? 128 : remaining;
        const CUdeviceptr previous = at ? hidden + (size_t)(at - 1) * DRAFT_HIDDEN_BYTES : m->draft_carry;
        const int rc = count == 1 ? draft_step(m, ids[at], start + at, previous, NULL) :
            draft_prefill_block(m, ids + at, start + at, count, previous);
        if (rc) return -1;
        at += count;
    }
    return save_conditioning(m->draft_carry, hidden + (size_t)(rows - 1) * DRAFT_HIDDEN_BYTES);
}

static int dump_draft_cache(model *m, const char *path) {
    const size_t bytes = (size_t)m->position * KV_BYTES;
    uint8_t *host = malloc(bytes);
    FILE *file = fopen(path, "wb");
    int status = !host || !file;
    for (unsigned plane = 0; plane < 2 && !status; ++plane) {
        status = check(bonsai_cuda.fn_cuMemcpyDtoH(host, m->draft_kv[plane], bytes), "draft cache capture") ||
                 fwrite(host, bytes, 1, file) != 1;
    }
    free(host);
    if (file && fclose(file) != 0) status = 1;
    return status;
}
