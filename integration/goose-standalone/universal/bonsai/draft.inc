static int choose_mtp_rows(model *m, unsigned rows, int ignore_eos, struct token_choice result[VERIFY_ROWS]) {
    CUdeviceptr logits = m->b[19], mask = ignore_eos ? m->choice_mask : 0;
    unsigned width = VOCAB, jobs = CHOICE_JOBS;
    return launch(m, CHOICE_TILES, jobs, rows, 1, 256,
                  (void *[]){ &logits, &mask, &m->choice_tiles, &width, &jobs }) ||
           launch(m, CHOICE_FINAL, rows, 1, 1, 256,
                  (void *[]){ &m->choice_tiles, &m->choice_result, &jobs }) ||
           check(bonsai_cuda.fn_cuMemcpyDtoH(result, m->choice_result, rows * sizeof(*result)), "token choices");
}

int64_t bonsai_mtp_eh(uint64_t context, int64_t weights, int64_t input, int64_t output) {
    model *m = (model *)(uintptr_t)context;
    CUdeviceptr w = weights, x = input, y = output;
    return launch(m, MTP_EH, 1280, 1, 1, 128, (void *[]){ &w, &x, &y });
}

int64_t bonsai_mtp_mat(uint64_t context, int64_t weights, int64_t input, int64_t output,
                       int64_t width, int64_t rows) {
    model *m = (model *)(uintptr_t)context;
    if (m->count != 1 || width <= 0 || width % 32 || rows <= 0 || rows % 4) return -1;
    CUdeviceptr w = weights, x = input, y = output;
    unsigned blocks = (unsigned)width / 32;
    return launch(m, MTP_MAT, (unsigned)rows / 4, 1, 1, 128, (void *[]){ &w, &x, &y, &blocks });
}

static int configure_mtp_row(model *m, unsigned pos) {
    const unsigned keys = pos + 1;
    const int64_t tiles = (keys + 255) / 256;
    const int32_t positions[4] = {pos, pos, pos, 0};
    const int64_t cfg[22] = {
        256, 256, 1, 24, 4, 4, keys, 24576, 1024, KV_BYTES, 144, KV_BYTES, 144,
        0, 1024, 24576, 256, tiles, 24, 24, 24 * tiles * 258 * 4, keys,
    };
    m->count = 1;
    m->position = pos;
    m->wave = 24;
    return check(bonsai_cuda.fn_cuMemcpyHtoD(m->positions, positions, sizeof(positions)), "draft positions") ||
           check(bonsai_cuda.fn_cuMemcpyHtoD(m->attention, cfg, sizeof(cfg)), "draft attention");
}

enum { DRAFT_HIDDEN_BYTES = 5120 * sizeof(float) };

static int draft_open(model *m) {
    if (!m->device_commit || !m->verify_native || !m->verify_q8 ||
        m->q8_mode != 1 || m->ubatch < m->verify_rows) return -1;
    const size_t plane = (size_t)m->context * KV_BYTES;
    m->draft_batch = 1;
    const size_t bytes = 2 * plane + 2 * DRAFT_HIDDEN_BYTES +
        (m->draft_batch ? 27 * sizeof(int64_t) : 0);
    CUdeviceptr memory;
    if (check(bonsai_cuda.fn_cuMemAlloc(&memory, bytes), "draft KV and conditioning")) return -1;
    m->draft_kv[0] = (int64_t)memory;
    m->draft_kv[1] = (int64_t)(memory + plane);
    m->draft_carry = memory + 2 * plane;
    m->draft_chain = m->draft_carry + DRAFT_HIDDEN_BYTES;
    if (m->draft_batch) m->draft_rope_k = m->draft_chain + DRAFT_HIDDEN_BYTES;
    fprintf(stderr, "draft_device_bytes=" BONSAI_SIZE_FMT "\n", bytes);
    return check(bonsai_cuda.fn_cuMemsetD8(m->draft_carry, 0, DRAFT_HIDDEN_BYTES), "draft zero seed");
}

static int save_conditioning(CUdeviceptr out, CUdeviceptr in) {
    return check(bonsai_cuda.fn_cuMemcpyDtoDAsync(out, in, DRAFT_HIDDEN_BYTES, NULL), "draft conditioning");
}

static int draft_forward(model *m, unsigned token, unsigned pos, CUdeviceptr hidden,
                         float *logits, int want_logits) {
    if (m->txn.phase != TXN_IDLE || pos >= m->context || token >= VOCAB) return -1;
    const unsigned target_pos = m->position, target_count = m->count, target_wave = m->wave;
    const int target_decode = m->in_decode, target_graph = m->capture.enabled;
    m->capture.enabled = 0;
    const CUdeviceptr rope_q = m->rope_q, rope_k = m->rope_k;
    m->rope_q = m->verify_rope_q;
    m->rope_k = m->verify_rope_k;
    // The draft's Q4 projections bypass this flag; only its PQ2 vocabulary uses it.
    m->in_decode = want_logits && m->draft_q8_head;
    int64_t scratch[24];
    memcpy(scratch, m->b, sizeof(scratch));
    scratch[0] = (int64_t)hidden;
    int rc = configure_mtp_row(m, pos);
    if (!rc) {
        const uintptr_t ctx = (uintptr_t)m;
        const sl_i64 w = slice(m->mtp_layer, 15), s = slice(m->draft_kv, 2),
                     t = slice(m->mtp, 4), g = slice(m->globals, 3), b = slice(scratch, 24);
        rc = (int)(want_logits ? bonsai_mtp_head(ctx, w, s, t, g, b, token, pos) :
                           bonsai_mtp_kv(ctx, w, s, t, g, b, token, pos));
    }
    if (!rc && logits) rc = check(bonsai_cuda.fn_cuMemcpyDtoH(logits, m->b[19],
        VOCAB * sizeof(float)), "draft logits");
    m->rope_q = rope_q;
    m->rope_k = rope_k;
    m->in_decode = target_decode;
    m->capture.enabled = target_graph;
    m->position = target_pos;
    m->count = target_count;
    m->wave = target_wave;
    return rc;
}

static int draft_step(model *m, unsigned token, unsigned pos, CUdeviceptr hidden, float *logits) {
    return draft_forward(m, token, pos, hidden, logits, logits != NULL);
}

static int draft_catchup(model *m, const unsigned *ids, unsigned rows, unsigned already_written) {
    const unsigned start = m->position - rows;
    const CUdeviceptr output = (CUdeviceptr)m->b[0];
    for (unsigned row = already_written; row < rows; ++row) {
        const CUdeviceptr state = row == 0 ? m->draft_carry : output + (size_t)(row - 1) * DRAFT_HIDDEN_BYTES;
        if (draft_step(m, ids[row], start + row, state, NULL)) return -1;
    }
    return save_conditioning(m->draft_carry, output + (size_t)(rows - 1) * DRAFT_HIDDEN_BYTES);
}

static int is_eog(model *m, unsigned token) {
    for (int64_t j = 0; j < m->eog_count; ++j) {
        if (m->eog[j] == token) return 1;
    }
    return 0;
}

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
    const int rc = bonsai_rope_config(m->rope_k, 4, count) ||
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

