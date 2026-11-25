int64_t bonsai_gdn_defer_state_swap(uint64_t context) {
    const model *m = (model *)(uintptr_t)context;
    return m->txn.phase == TXN_RUNNING;
}

static int launch_transaction_conv(model *m, const CUdeviceptr p[8]) {
    const unsigned i = m->txn.next_layer;
    if (i >= m->txn.layers || !m->txn.prefix[i][0][0]) return -1;
    CUdeviceptr args[] = {p[0], p[1], p[2], p[3], m->txn.prefix[i][0][0], m->txn.prefix[i][1][0]};
    return launch(m, VERIFY_CONV, 40, 1, 1, 256,
                  (void *[]){&args[0], &args[1], &args[2], &args[3], &args[4], &args[5]});
}

static int launch_transaction_gdn(model *m, const CUdeviceptr p[8]) {
    const unsigned i = m->txn.next_layer;
    if (i >= m->txn.layers || !m->txn.prefix[i][0][1]) return -1;
    const unsigned layer = m->txn.layer_ids[i];
    if ((CUdeviceptr)m->states[2 * layer + 1] != p[5]) return -1;
    const CUdeviceptr middle[] = {p[5], m->txn.prefix[i][0][1], m->txn.prefix[i][1][1], p[5]};
    for (unsigned t = 0; t < VERIFY_ROWS; ++t) {
        const CUdeviceptr args[8] = {
            p[0] + (size_t)t * 4096 * sizeof(float), p[1] + (size_t)t * 4096 * sizeof(float),
            p[2] + (size_t)t * 10240 * sizeof(float), p[3] + (size_t)t * 48 * sizeof(float),
            p[4] + (size_t)t * 48 * sizeof(float), middle[t],
            p[6] + (size_t)t * 6144 * sizeof(float), middle[t + 1],
        };
        if (gdn(m, args, 1)) return -1;
    }
    ++m->txn.next_layer;
    return 0;
}

static int commit_target(model *m, unsigned kept) {
    if (m->txn.phase != TXN_READY || kept == 0 || kept > VERIFY_ROWS) return -1;
    if (kept < VERIFY_ROWS) {
        for (unsigned i = 0; i < m->txn.layers; ++i) {
            const unsigned layer = m->txn.layer_ids[i];
            for (unsigned plane = 0; plane < 2; ++plane) {
                CUdeviceptr *candidate = &m->txn.prefix[i][kept - 1][plane];
                const CUdeviceptr old = (CUdeviceptr)m->states[2 * layer + plane];
                m->states[2 * layer + plane] = (int64_t)*candidate;
                *candidate = old;
            }
        }
    }
    // Attention rows at and beyond this cursor are invisible and overwritten on the next step.
    m->position = m->txn.start + kept;
    m->txn.phase = TXN_IDLE;
    return 0;
}
