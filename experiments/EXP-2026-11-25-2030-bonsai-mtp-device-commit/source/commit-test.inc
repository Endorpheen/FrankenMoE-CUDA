static double seconds(void);

static int probe_target(model *m, const float *initial, int ignore_eos) {
    enum { CONTROL_ROWS = VERIFY_ROWS + 1 };
    const unsigned start = m->position;
    const unsigned snapshot_rows = start + CONTROL_ROWS;
    const size_t logit_bytes = VOCAB * sizeof(float);
    if (!m->device_commit || !exact_verify(m, VERIFY_ROWS) || m->ubatch < VERIFY_ROWS ||
        m->context < snapshot_rows) return -1;

    struct checkpoint before = {0}, oracle[CONTROL_ROWS] = {{0}};
    const size_t kv_plane = (size_t)snapshot_rows * KV_BYTES;
    const size_t recurrent_plane = sizeof(float) * GDN_STATE;
    uint8_t *scratch = malloc(kv_plane > recurrent_plane ? kv_plane : recurrent_plane);
    float *serial = malloc(CONTROL_ROWS * logit_bytes);
    float *batch = malloc(VERIFY_ROWS * logit_bytes);
    unsigned tokens[CONTROL_ROWS + 1];
    const int first = greedy_id(m, initial, ignore_eos);
    int status = -1;
    if (first < 0 || !scratch || !serial || !batch ||
        checkpoint_init(m, &before, snapshot_rows) ||
        checkpoint_io(m, &before, snapshot_rows, SAVE, NULL)) goto done;

    tokens[0] = (unsigned)first;
    for (unsigned row = 0; row < CONTROL_ROWS; ++row) {
        float *out = serial + (size_t)row * VOCAB;
        if (evaluate_batch(m, &tokens[row], 1, out) ||
            checkpoint_init(m, &oracle[row], start + row + 1) ||
            checkpoint_io(m, &oracle[row], start + row + 1, SAVE, NULL)) goto done;
        const int next = greedy_id(m, out, ignore_eos);
        if (next < 0) goto done;
        tokens[row + 1] = (unsigned)next;
    }

    for (unsigned kept = 1; kept <= VERIFY_ROWS; ++kept) {
        unsigned proposals[VERIFY_ROWS];
        memcpy(proposals, tokens, sizeof(proposals));
        if (kept < VERIFY_ROWS) proposals[kept] = (tokens[kept] + 1) % VOCAB;
        if (checkpoint_io(m, &before, snapshot_rows, RESTORE, NULL) ||
            check(bonsai_cuda.fn_cuCtxSynchronize(), "commit probe boundary")) goto done;

        const double begin = seconds();
        if (evaluate_batch(m, proposals, VERIFY_ROWS, batch)) goto done;
        const double evaluated = seconds();
        if (m->position != start || m->txn.phase != TXN_READY ||
            evaluate_batch(m, &tokens[0], 1, NULL) == 0 || commit_target(m, 0) == 0 ||
            memcmp(batch, serial, (size_t)kept * logit_bytes) != 0) goto done;

        for (unsigned row = 0; row < kept; ++row) {
            if (greedy_id(m, batch + (size_t)row * VOCAB, ignore_eos) != (int)tokens[row + 1]) goto done;
        }
        const double commit_start = seconds();
        if (commit_target(m, kept)) goto done;
        const double commit_end = seconds();
        if (commit_target(m, kept) == 0 ||
            checkpoint_io(m, &oracle[kept - 1], start + kept, COMPARE, scratch) ||
            evaluate_batch(m, &tokens[kept], 1, batch) ||
            checkpoint_io(m, &oracle[kept], start + kept + 1, COMPARE, scratch) ||
            memcmp(batch, serial + (size_t)kept * VOCAB, logit_bytes) != 0) goto done;
        printf("{\"prompt\":%u,\"committed\":%u,\"forced_reject\":%u,\"exact_logit_rows\":%u,"
               "\"state_bytes\":%zu,\"follow_state_bytes\":%zu,\"target_ms\":%.6f,\"commit_us\":%.3f}\n",
               start, kept, kept < VERIFY_ROWS, kept + 1, oracle[kept-1].bytes, oracle[kept].bytes,
               1000 * (evaluated - begin), 1000000 * (commit_end - commit_start));
    }
    status = 0;
done:
    for (unsigned i = 0; i < CONTROL_ROWS; ++i) free(oracle[i].data);
    free(before.data);
    free(scratch);
    free(serial);
    free(batch);
    if (status) fputs("Device commit parity failed\n", stderr);
    return status;
}
