struct token_choice {
    uint32_t token;
    float logit;
    uint32_t invalid;
};

_Static_assert(sizeof(struct token_choice) == 12, "CUDA token choice ABI");
enum { CHOICE_JOBS = (VOCAB + 1023) / 1024, CHOICE_MASK_WORDS = (VOCAB + 31) / 32 };
_Static_assert(CHOICE_JOBS <= 256, "one final CUDA block must cover all partials");

static int choice_open(model *m) {
    uint32_t tokens[CHOICE_MASK_WORDS] = {0};
    for (int64_t j = 0; j < m->eog_count; ++j) {
        const uint32_t id = (uint32_t)m->eog[j];
        if (id >= VOCAB) return -1;
        tokens[id / 32] |= 1U << (id % 32);
    }
    const size_t partial_bytes = (size_t)VERIFY_ROWS * CHOICE_JOBS * sizeof(struct token_choice);
    const size_t result_bytes = VERIFY_ROWS * sizeof(struct token_choice);
    const size_t bytes = sizeof(tokens) + partial_bytes + result_bytes;
    if (check(bonsai_cuda.fn_cuMemAlloc(&m->choice_mask, bytes), "choice workspace") ||
        check(bonsai_cuda.fn_cuMemcpyHtoD(m->choice_mask, tokens, sizeof(tokens)), "choice EOG mask")) return -1;
    m->choice_tiles = m->choice_mask + sizeof(tokens);
    m->choice_result = m->choice_tiles + partial_bytes;
    fprintf(stderr, "choice_device_bytes=%zu\n", bytes);
    return 0;
}

static int choose_device(model *m, unsigned rows, int ignore_eos, struct token_choice result[VERIFY_ROWS]) {
    CUdeviceptr logits = m->b[19], mask = ignore_eos ? m->choice_mask : 0;
    unsigned width = VOCAB, jobs = CHOICE_JOBS;
    return launch(m, CHOICE_TILES, jobs, rows, 1, 256,
                  (void *[]){ &logits, &mask, &m->choice_tiles, &width, &jobs }) ||
           launch(m, CHOICE_FINAL, rows, 1, 1, 256,
                  (void *[]){ &m->choice_tiles, &m->choice_result, &jobs }) ||
           check(bonsai_cuda.fn_cuMemcpyDtoH(result, m->choice_result, rows * sizeof(*result)), "token choices");
}

static struct token_choice choose_host(model *m, const float *logits, int ignore_eos) {
    const int token = greedy_id(m, logits, ignore_eos);
    return token >= 0 ? (struct token_choice){(uint32_t)token, logits[token], 0} :
                        (struct token_choice){UINT32_MAX, 0.0f, 1};
}

static int invalid_choice(struct token_choice choice) {
    return choice.invalid || choice.token >= VOCAB;
}
