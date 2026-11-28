int64_t bonsai_ffn_prefix_launch(uint64_t context, int64_t hidden, int64_t branch,
    int64_t weight, int64_t signs, int64_t residual, int64_t owner) {
    (void)context; (void)hidden; (void)branch; (void)weight;
    (void)signs; (void)residual; (void)owner;
    return 0;
}

void bonsai_session_mtp_prepare(struct bonsai_session *session, int allowed) {
    if (session && !session->needs_reset) {
        model *m = session->runtime;
        m->mtp_request = m->mtp_enabled && allowed;
    }
}

static int verify_mtp_block(model *m, uint32_t anchor, int ignore_eos) {
    unsigned tokens[VERIFY_ROWS] = {anchor};
    struct token_choice choices[VERIFY_ROWS];
    const unsigned start = m->position;
    for (unsigned i = 0; i + 1 < m->verify_rows; ++i) {
        const CUdeviceptr hidden = i == 0 ? m->draft_carry : m->draft_chain;
        if (draft_forward(m, tokens[i], start + i, hidden, NULL, 1) ||
            choose_mtp_rows(m, 1, ignore_eos, choices) ||
            choices[0].invalid || choices[0].token >= VOCAB) return -1;
        tokens[i + 1] = choices[0].token;
        if (i + 2 < m->verify_rows && save_conditioning(m->draft_chain, m->b[1])) return -1;
    }

    m->in_decode = 1;
    const int status = evaluate_batch_output(m, tokens, m->verify_rows, NULL, 1) ||
                       choose_mtp_rows(m, m->verify_rows, ignore_eos, choices);
    m->in_decode = 0;
    if (status) return -1;

    unsigned kept = 1;
    int stop = 0;
    while (kept < m->verify_rows) {
        const struct token_choice choice = choices[kept - 1];
        if (choice.invalid || choice.token >= VOCAB) return -1;
        if (choice.token != tokens[kept]) break;
        if (!ignore_eos && is_eog(m, tokens[kept])) {
            ++kept;
            stop = 1;
            break;
        }
        ++kept;
    }
    const struct token_choice next = choices[kept - 1];
    if (!stop && (next.invalid || next.token >= VOCAB)) return -1;
    if (commit_target(m, kept) || (!stop && draft_catchup(m, tokens, kept, 1))) return -1;
    for (unsigned i = 1; i < kept; ++i) m->mtp_pending[i - 1] = tokens[i];
    m->mtp_next = 0;
    m->mtp_end = kept - 1;
    if (!stop) m->mtp_pending[m->mtp_end++] = next.token;
    fprintf(stderr, "[bonsai-mtp] proposed=%u accepted=%u\n", m->verify_rows - 1, kept - 1);
    return 0;
}

int64_t bonsai_session_mtp_next(struct bonsai_session *session, uint32_t token,
                                int ignore_eos, int64_t remaining) {
    if (!session || session->needs_reset || token >= VOCAB || remaining <= 0) return -1;
    model *m = session->runtime;
    if (m->mtp_next < m->mtp_end) {
        if (m->mtp_next == 0 || token != m->mtp_pending[m->mtp_next - 1]) return -1;
        return m->mtp_pending[m->mtp_next++];
    }
    if (!m->mtp_request || remaining < m->verify_rows ||
        m->verify_rows > m->context - m->position) {
        return bonsai_session_choose_next(session, token, ignore_eos);
    }
    if (check(bonsai_cuda.fn_cuCtxPushCurrent(m->cuda), "bind Bonsai MTP context")) return -1;
    const int status = verify_mtp_block(m, token, ignore_eos);
    if (fg_bonsai_session_finish(session, status)) return -1;
    return m->mtp_pending[m->mtp_next++];
}
