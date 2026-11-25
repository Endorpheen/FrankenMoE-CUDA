enum { DRAFT_HIDDEN_BYTES = 5120 * sizeof(float) };

static int draft_open(model *m) {
    if (!m->device_commit || !m->verify_native || !m->verify_q8 ||
        m->q8_mode != 1 || m->ubatch < VERIFY_ROWS) return -1;
    const size_t plane = (size_t)m->context * KV_BYTES;
    const size_t bytes = 2 * plane + 2 * DRAFT_HIDDEN_BYTES;
    CUdeviceptr memory;
    if (check(bonsai_cuda.fn_cuMemAlloc(&memory, bytes), "draft KV and conditioning")) return -1;
    m->draft_kv[0] = (int64_t)memory;
    m->draft_kv[1] = (int64_t)(memory + plane);
    m->draft_carry = memory + 2 * plane;
    m->draft_chain = m->draft_carry + DRAFT_HIDDEN_BYTES;
    fprintf(stderr, "draft_device_bytes=%zu\n", bytes);
    return check(bonsai_cuda.fn_cuMemsetD8(m->draft_carry, 0, DRAFT_HIDDEN_BYTES), "draft zero seed");
}

static int save_conditioning(CUdeviceptr out, CUdeviceptr in) {
    return check(bonsai_cuda.fn_cuMemcpyDtoDAsync(out, in, DRAFT_HIDDEN_BYTES, NULL), "draft conditioning");
}

static int draft_step(model *m, unsigned token, unsigned pos, CUdeviceptr hidden, float *logits) {
    if (m->txn.phase != TXN_IDLE || pos >= m->context || token >= VOCAB) return -1;
    const unsigned target_pos = m->position, target_count = m->count, target_wave = m->wave;
    const int target_decode = m->in_decode;
    const CUdeviceptr rope_q = m->rope_q, rope_k = m->rope_k;
    m->rope_q = m->verify_rope_q;
    m->rope_k = m->verify_rope_k;
    m->in_decode = 0;
    int64_t scratch[24];
    memcpy(scratch, m->b, sizeof(scratch));
    scratch[0] = (int64_t)hidden;
    int rc = configure_mtp_row(m, pos);
    if (!rc) {
        const uintptr_t ctx = (uintptr_t)m;
        const sl_i64 w = slice(m->mtp_layer, 15), s = slice(m->draft_kv, 2),
                     t = slice(m->mtp, 4), g = slice(m->globals, 3), b = slice(scratch, 24);
        rc = (int)(logits ? bonsai_mtp_head(ctx, w, s, t, g, b, token, pos) :
                           bonsai_mtp_kv(ctx, w, s, t, g, b, token, pos));
    }
    if (!rc && logits) rc = check(bonsai_cuda.fn_cuMemcpyDtoH(logits, m->b[19],
        VOCAB * sizeof(float)), "draft logits");
    m->rope_q = rope_q;
    m->rope_k = rope_k;
    m->in_decode = target_decode;
    m->position = target_pos;
    m->count = target_count;
    m->wave = target_wave;
    return rc;
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

static void emit_token(unsigned step, unsigned token, float value) {
    printf("{\"step\":%u,\"token\":%u,\"logit\":%.9g}\n", step, token, value);
}

static int greedy_mtp(model *m, float *next_logits, unsigned limit, int ignore_eos, unsigned *generated) {
    const size_t logit_bytes = VOCAB * sizeof(float);
    float *result = malloc(VERIFY_ROWS * logit_bytes);
    if (!result) return -1;
    unsigned proposed = 0, accepted = 0, blocks = 0, histogram[VERIFY_ROWS] = {0};
    double draft_time = 0, target_time = 0, catchup_time = 0;
    int status = -1;
    while (*generated < limit) {
        const unsigned remaining = limit - *generated, pos = m->position;
        const int next = greedy_id(m, next_logits, ignore_eos);
        if (next < 0) goto done;
        const unsigned anchor = (unsigned)next;
        if (remaining <= VERIFY_ROWS || pos > m->context - VERIFY_ROWS || is_eog(m, anchor)) {
            emit_token((*generated)++, anchor, next_logits[anchor]);
            if (is_eog(m, anchor) || *generated == limit) break;
            if (evaluate_batch(m, &anchor, 1, next_logits)) goto done;
            continue;
        }

        unsigned tokens[VERIFY_ROWS] = {anchor};
        const double draft_start = seconds();
        for (unsigned i = 0; i < VERIFY_ROWS - 1; ++i) {
            const CUdeviceptr hidden = i == 0 ? m->draft_carry : m->draft_chain;
            if (draft_step(m, tokens[i], pos + i, hidden, result)) goto done;
            const int guessed = greedy_id(m, result, ignore_eos);
            if (guessed < 0) goto done;
            tokens[i + 1] = (unsigned)guessed;
            if (i == 0 && save_conditioning(m->draft_chain, m->b[1])) goto done;
        }
        draft_time += seconds() - draft_start;

        const double target_start = seconds();
        if (evaluate_batch(m, tokens, VERIFY_ROWS, result)) goto done;
        unsigned kept = 1;
        int stop = 0;
        while (kept < VERIFY_ROWS) {
            const int match = greedy_id(m, result + (size_t)(kept - 1) * VOCAB, ignore_eos);
            if (match < 0) goto done;
            if ((unsigned)match != tokens[kept]) break;
            ++kept;
            if (is_eog(m, (unsigned)match)) { stop = 1; break; }
        }
        if (commit_target(m, kept)) goto done;
        target_time += seconds() - target_start;
        ++blocks;
        proposed += VERIFY_ROWS - 1;
        accepted += kept - 1;
        ++histogram[kept - 1];

        for (unsigned i = 0; i < kept; ++i) {
            const float *source = i ? result + (size_t)(i - 1) * VOCAB : next_logits;
            emit_token((*generated)++, tokens[i], source[tokens[i]]);
        }
        if (stop) break;
        memcpy(next_logits, result + (size_t)(kept - 1) * VOCAB, logit_bytes);
        const double catchup_start = seconds();
        if (draft_catchup(m, tokens, kept, 1) ||
            check(bonsai_cuda.fn_cuCtxSynchronize(), "draft catch-up timing boundary")) goto done;
        catchup_time += seconds() - catchup_start;
    }
    fprintf(stderr, "mtp_blocks=%u proposed=%u accepted=%u commit_1=%u commit_2=%u commit_3=%u "
            "draft_ms=%.3f target_ms=%.3f catchup_ms=%.3f\n", blocks, proposed, accepted,
            histogram[0], histogram[1], histogram[2], 1000 * draft_time, 1000 * target_time, 1000 * catchup_time);
    status = 0;
done:
    free(result);
    return status;
}
