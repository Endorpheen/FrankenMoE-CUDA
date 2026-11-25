enum checkpoint_op { SAVE, RESTORE, COMPARE };

struct checkpoint {
    uint8_t *data;
    size_t bytes;
    unsigned position;
};

static size_t state_bytes(model *m, unsigned layer, unsigned plane, unsigned rows) {
    return m->bound[layer * 15] == 0 ? (size_t)rows * KV_BYTES :
        sizeof(float) * (plane ? GDN_STATE : GDN_HISTORY);
}

static int checkpoint_init(model *m, struct checkpoint *saved, unsigned rows) {
    for (unsigned layer = 0; layer < LAYERS; ++layer) {
        for (unsigned plane = 0; plane < 2; ++plane) saved->bytes += state_bytes(m, layer, plane, rows);
    }
    saved->data = malloc(saved->bytes);
    return saved->data ? 0 : -1;
}

static int checkpoint_io(model *m, struct checkpoint *saved, unsigned rows, enum checkpoint_op op,
                         uint8_t *scratch) {
    uint8_t *host = saved->data;
    int differing_planes = 0;
    if (op == SAVE) saved->position = m->position;
    for (unsigned layer = 0; layer < LAYERS; ++layer) {
        for (unsigned plane = 0; plane < 2; ++plane) {
            // GDN state rotates through scratch. Always resolve the currently owning pointer.
            const CUdeviceptr ptr = m->states[layer * 2 + plane];
            const size_t bytes = state_bytes(m, layer, plane, rows);
            if (op == RESTORE) {
                if (check(bonsai_cuda.fn_cuMemcpyHtoD(ptr, host, bytes), "restore target state")) return -1;
            } else {
                if (check(bonsai_cuda.fn_cuMemcpyDtoH(op == SAVE ? host : scratch, ptr, bytes),
                          "read target state")) return -1;
                if (op == COMPARE && memcmp(scratch, host, bytes) != 0) ++differing_planes;
            }
            host += bytes;
        }
    }
    if (op == RESTORE) m->position = saved->position;
    return m->position == saved->position ? differing_planes : -1;
}

static int greedy_id(model *m, const float *logits, int ignore_eos) {
    int best = -1;
    for (unsigned token = 0; token < VOCAB; ++token) {
        if (!isfinite(logits[token])) return -1;
        if (ignore_eos) {
            int eog = 0;
            for (int64_t j = 0; j < m->eog_count; ++j) eog |= m->eog[j] == token;
            if (eog) continue;
        }
        if (best < 0 || logits[token] > logits[best]) best = (int)token;
    }
    return best;
}

static int probe_target(model *m, const float *initial, int ignore_eos) {
    const unsigned start = m->position;
    const unsigned snapshot_rows = start + VERIFY_ROWS;
    const size_t logit_bytes = VOCAB * sizeof(float);
    if (m->ubatch < VERIFY_ROWS || m->context < snapshot_rows) return -1;

    struct checkpoint before = {0}, accepted = {0}, complete = {0};
    const size_t kv_plane = (size_t)snapshot_rows * KV_BYTES;
    const size_t recurrent_plane = sizeof(float) * GDN_STATE;
    uint8_t *scratch = malloc(kv_plane > recurrent_plane ? kv_plane : recurrent_plane);
    float *serial = malloc(VERIFY_ROWS * logit_bytes);
    float *batched = malloc(VERIFY_ROWS * logit_bytes);
    float *batch_prefix = malloc(2 * logit_bytes);
    unsigned tokens[VERIFY_ROWS + 1];
    const int first = greedy_id(m, initial, ignore_eos);
    int status = -1;
    if (first < 0 || !scratch || !serial || !batched || !batch_prefix ||
        checkpoint_init(m, &before, snapshot_rows) ||
        checkpoint_init(m, &accepted, start + 2) ||
        checkpoint_init(m, &complete, snapshot_rows)) goto done;

    tokens[0] = (unsigned)first;
    if (checkpoint_io(m, &before, snapshot_rows, SAVE, NULL)) goto done;
    for (unsigned row = 0; row < VERIFY_ROWS; ++row) {
        float *output = serial + (size_t)row * VOCAB;
        if (evaluate_batch(m, &tokens[row], 1, output)) goto done;
        const int next = greedy_id(m, output, ignore_eos);
        if (next < 0) goto done;
        tokens[row + 1] = (unsigned)next;
        if (row == 1 && checkpoint_io(m, &accepted, start + 2, SAVE, NULL)) goto done;
    }

    if (checkpoint_io(m, &complete, snapshot_rows, SAVE, NULL) ||
        checkpoint_io(m, &before, snapshot_rows, RESTORE, NULL) ||
        evaluate_batch(m, tokens, VERIFY_ROWS, batched)) goto done;
    const int different = checkpoint_io(m, &complete, snapshot_rows, COMPARE, scratch);
    if (different < 0) goto done;
    memcpy(batch_prefix, batched, 2 * logit_bytes);
    for (unsigned row = 0; row < VERIFY_ROWS; ++row) {
        const float *ref = serial + (size_t)row * VOCAB;
        const float *candidate = batched + (size_t)row * VOCAB;
        const int next = greedy_id(m, candidate, ignore_eos);
        double squared = 0, norm = 0, max_abs = 0;
        if (next < 0) goto done;
        for (unsigned id = 0; id < VOCAB; ++id) {
            const double delta = (double)candidate[id] - ref[id];
            squared += delta * delta;
            norm += (double)ref[id] * ref[id];
            if (fabs(delta) > max_abs) max_abs = fabs(delta);
        }
        printf("{\"prompt\":%u,\"row\":%u,\"input\":%u,\"serial_next\":%u,"
               "\"batch_next\":%d,\"relative_l2\":%.9g,\"max_abs\":%.9g}\n",
               start, row, tokens[row], tokens[row + 1], next, sqrt(squared / norm), max_abs);
    }

    const unsigned wrong = (tokens[2] + 1) % VOCAB;
    const unsigned proposals[VERIFY_ROWS] = {tokens[0], tokens[1], wrong};
    if (checkpoint_io(m, &before, snapshot_rows, RESTORE, NULL) ||
        evaluate_batch(m, proposals, VERIFY_ROWS, batched)) goto done;
    if (memcmp(batch_prefix, batched, 2 * logit_bytes) != 0) {
        fputs("Changing the final token affected earlier batch logits\n", stderr);
        goto done;
    }
    printf("{\"prompt\":%u,\"all_accepted_differing_state_planes\":%d,"
           "\"state_planes\":%u,\"causal_exact_logit_rows\":2}\n", start, different, LAYERS * 2u);
    const int accepted_id = greedy_id(m, batched, ignore_eos);
    const int reject_id = greedy_id(m, batched + VOCAB, ignore_eos);
    if (accepted_id != (int)tokens[1] || reject_id < 0 || reject_id == (int)wrong) {
        fprintf(stderr, "Unexpected decision for forced mismatch: pass=%d reject=%d\n", accepted_id, reject_id);
        goto done;
    }

    if (checkpoint_io(m, &before, snapshot_rows, RESTORE, NULL) ||
        evaluate_batch(m, &tokens[0], 1, batched) ||
        evaluate_batch(m, &tokens[1], 1, batched) ||
        memcmp(batched, serial + VOCAB, logit_bytes) != 0 ||
        checkpoint_io(m, &accepted, start + 2, COMPARE, scratch) ||
        evaluate_batch(m, &tokens[2], 1, batched) ||
        memcmp(batched, serial + 2 * VOCAB, logit_bytes) != 0) goto done;
    printf("{\"prompt\":%u,\"forced_rejected_id\":%u,\"batch_replacement_id\":%d,"
           "\"serial_replacement_id\":%u,\"committed_position\":%u,\"restored_bytes\":%zu,"
           "\"compared_state_bytes\":%zu,\"exact_replay_logit_rows\":2}\n",
           start, wrong, reject_id, tokens[2], accepted.position, before.bytes, accepted.bytes);
    status = 0;
done:
    free(before.data);
    free(accepted.data);
    free(complete.data);
    free(batch_prefix);
    free(scratch);
    free(serial);
    free(batched);
    return status;
}
