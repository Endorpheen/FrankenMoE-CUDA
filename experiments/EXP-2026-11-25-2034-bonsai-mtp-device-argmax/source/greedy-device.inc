static int run_greedy_mtp(model *m, float *initial, unsigned limit, int ignore_eos,
                          unsigned *generated, float *host_logits) {
    unsigned proposed = 0, accepted = 0, blocks = 0, histogram[VERIFY_ROWS] = {0};
    double draft_time = 0, target_time = 0, catchup_time = 0;
    struct token_choice next = choose_host(m, initial, ignore_eos);
    while (*generated < limit) {
        if (invalid_choice(next)) return -1;
        const unsigned remaining = limit - *generated, pos = m->position, anchor = next.token;
        struct token_choice choices[VERIFY_ROWS];
        if (remaining <= VERIFY_ROWS || pos > m->context - VERIFY_ROWS || is_eog(m, anchor)) {
            emit_token((*generated)++, anchor, next.logit);
            if (is_eog(m, anchor) || *generated == limit) break;
            if (evaluate_target(m, &anchor, 1, host_logits, 1)) return -1;
            if (host_logits) {
                next = choose_host(m, host_logits, ignore_eos);
            } else {
                if (choose_device(m, 1, ignore_eos, choices)) return -1;
                next = choices[0];
            }
            continue;
        }

        unsigned tokens[VERIFY_ROWS] = {anchor};
        const double draft_start = seconds();
        for (unsigned i = 0; i < VERIFY_ROWS - 1; ++i) {
            const CUdeviceptr hidden = i == 0 ? m->draft_carry : m->draft_chain;
            if (draft_forward(m, tokens[i], pos + i, hidden, host_logits, 1)) return -1;
            if (host_logits) {
                choices[0] = choose_host(m, host_logits, ignore_eos);
            } else if (choose_device(m, 1, ignore_eos, choices)) {
                return -1;
            }
            if (invalid_choice(choices[0])) return -1;
            tokens[i + 1] = choices[0].token;
            if (i == 0 && save_conditioning(m->draft_chain, m->b[1])) return -1;
        }
        draft_time += seconds() - draft_start;

        const double target_start = seconds();
        if (evaluate_target(m, tokens, VERIFY_ROWS, host_logits, 1)) return -1;
        if (!host_logits && choose_device(m, VERIFY_ROWS, ignore_eos, choices)) return -1;
        unsigned kept = 1;
        int stop = 0;
        while (kept < VERIFY_ROWS) {
            if (host_logits) choices[kept - 1] = choose_host(m, host_logits + (size_t)(kept - 1) * VOCAB, ignore_eos);
            const struct token_choice match = choices[kept - 1];
            if (invalid_choice(match)) return -1;
            if (match.token != tokens[kept]) break;
            ++kept;
            if (is_eog(m, match.token)) { stop = 1; break; }
        }
        if (commit_target(m, kept)) return -1;
        target_time += seconds() - target_start;
        ++blocks;
        proposed += VERIFY_ROWS - 1;
        accepted += kept - 1;
        ++histogram[kept - 1];

        for (unsigned i = 0; i < kept; ++i) {
            const struct token_choice choice = i ? choices[i - 1] : next;
            emit_token((*generated)++, tokens[i], choice.logit);
        }
        if (stop) break;
        if (host_logits && kept == VERIFY_ROWS) {
            choices[kept - 1] = choose_host(m, host_logits + (size_t)(kept - 1) * VOCAB, ignore_eos);
        }
        next = choices[kept - 1];
        const double catchup_start = seconds();
        if (draft_catchup(m, tokens, kept, 1) ||
            check(bonsai_cuda.fn_cuCtxSynchronize(), "draft catch-up timing boundary")) return -1;
        catchup_time += seconds() - catchup_start;
    }
    fprintf(stderr, "mtp_blocks=%u proposed=%u accepted=%u commit_1=%u commit_2=%u commit_3=%u "
            "draft_ms=%.3f target_ms=%.3f catchup_ms=%.3f\n", blocks, proposed, accepted,
            histogram[0], histogram[1], histogram[2], 1000 * draft_time, 1000 * target_time, 1000 * catchup_time);
    return 0;
}


static int greedy_mtp(model *m, float *initial, unsigned limit, int ignore_eos, unsigned *generated) {
    float *host = NULL;
    if (!m->device_choice && !(host = malloc((size_t)VERIFY_ROWS * VOCAB * sizeof(*host)))) return -1;
    const int status = run_greedy_mtp(m, initial, limit, ignore_eos, generated, host);
    free(host);
    return status;
}
