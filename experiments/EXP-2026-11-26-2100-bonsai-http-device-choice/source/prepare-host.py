"""Expose Bonsai resident choice and its full-logit recovery to Goose."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one native host anchor: {old!r}")
    return text.replace(old, new)


base, output, old_host, new_host = map(Path, sys.argv[1:])
host = base.read_text()
host = replace(host, "FFN_DOWN_Q8_PANEL, KERNEL_COUNT",
               "FFN_DOWN_Q8_PANEL, CHOICE_TILES, CHOICE_FINAL, KERNEL_COUNT")
host = replace(host, "CUfunction kernel[KERNEL_COUNT];",
               "CUfunction kernel[KERNEL_COUNT];\n    CUdeviceptr choice_mask, choice_tiles, choice_result;")
host = replace(host, '"bonsai_pq2_mmq_int8_down_n128_spread",',
               '"bonsai_pq2_mmq_int8_down_n128_spread", "bonsai_choice_tiles", "bonsai_choice_final",')
host = replace(host, 'static model *bonsai_open(', '''struct token_choice {
    uint32_t token;
    float logit;
    uint32_t invalid;
};
_Static_assert(sizeof(struct token_choice) == 12, "CUDA token choice ABI");
enum { CHOICE_JOBS = (VOCAB + 1023) / 1024, CHOICE_MASK_WORDS = (VOCAB + 31) / 32 };
_Static_assert(CHOICE_JOBS <= 256, "one final block covers all partials");

static int choice_open(model *m) {
    uint32_t tokens[CHOICE_MASK_WORDS] = {0};
    for (int64_t j = 0; j < m->eog_count; ++j) {
        const uint32_t id = (uint32_t)m->eog[j];
        if (id >= VOCAB) return -1;
        tokens[id / 32] |= 1U << (id % 32);
    }
    const size_t partial_bytes = CHOICE_JOBS * sizeof(struct token_choice);
    const size_t bytes = sizeof(tokens) + partial_bytes + sizeof(struct token_choice);
    if (check(bonsai_cuda.fn_cuMemAlloc(&m->choice_mask, bytes), "choice workspace") ||
        check(bonsai_cuda.fn_cuMemcpyHtoD(m->choice_mask, tokens, sizeof(tokens)), "choice EOG mask")) return -1;
    m->choice_tiles = m->choice_mask + sizeof(tokens);
    m->choice_result = m->choice_tiles + partial_bytes;
    return 0;
}

static int choose_device(model *m, int ignore_eos, struct token_choice *result) {
    CUdeviceptr logits = m->b[19], mask = ignore_eos ? m->choice_mask : 0;
    unsigned width = VOCAB, jobs = CHOICE_JOBS;
    return launch(m, CHOICE_TILES, jobs, 1, 1, 256,
                  (void *[]){ &logits, &mask, &m->choice_tiles, &width, &jobs }) ||
           launch(m, CHOICE_FINAL, 1, 1, 1, 256,
                  (void *[]){ &m->choice_tiles, &m->choice_result, &jobs }) ||
           check(bonsai_cuda.fn_cuMemcpyDtoH(result, m->choice_result, sizeof(*result)), "token choice");
}

static model *bonsai_open(''')
host = replace(host, '    if (loaded != 0) goto error;\n    size_t available',
               '''    if (loaded != 0) goto error;
    const char *device_choice = getenv("BONSAI_HTTP_DEVICE_CHOICE");
    if (device_choice && !strcmp(device_choice, "1") && choice_open(m) != 0) goto error;
    size_t available''')
host = replace(host, 'static int evaluate_batch(model *m, const unsigned *tokens, unsigned count, float *logits) {',
               '''static int evaluate_batch_output(model *m, const unsigned *tokens, unsigned count,
                                 float *logits, int device_only) {''')
host = replace(host, 'tokens[0], pos, logits != NULL);','tokens[0], pos, logits != NULL || device_only);')
host = replace(host, '''static double seconds(void) {''', '''static int evaluate_batch(model *m, const unsigned *tokens, unsigned count, float *logits) {
    return evaluate_batch_output(m, tokens, count, logits, 0);
}

static double seconds(void) {''')
host += '''
int bonsai_session_choice_available(const struct bonsai_session *session, const int64_t *eog, int64_t n) {
    if (!session || !session->runtime->choice_mask || !eog || n != session->runtime->eog_count) return 0;
    const model *m = session->runtime;
    for (int64_t i = 0; i < n; ++i) {
        if (eog[i] != m->eog[i]) return 0;
    }
    return 1;
}

int64_t bonsai_session_choose_next(struct bonsai_session *session, uint32_t id, int ignore_eos) {
    if (!session || session->needs_reset || !session->runtime->choice_mask || id >= VOCAB) return -1;
    model *m = session->runtime;
    if (m->position >= m->context ||
        check(bonsai_cuda.fn_cuCtxPushCurrent(m->cuda), "bind Bonsai choice context")) return -1;
    m->in_decode = 1;
    struct token_choice choice;
    const int status = evaluate_batch_output(m, &id, 1, NULL, 1) ||
                       choose_device(m, ignore_eos, &choice);
    m->in_decode = 0;
    if (finish_session_call(session, status)) return -1;
    return choice.invalid || choice.token >= VOCAB ? -2 : (int64_t)choice.token;
}

int bonsai_session_read_choice_logits(struct bonsai_session *session, float *logits) {
    if (!session || session->needs_reset || !logits) return -1;
    model *m = session->runtime;
    if (check(bonsai_cuda.fn_cuCtxPushCurrent(m->cuda), "bind Bonsai fallback context")) return -1;
    return finish_session_call(session, check(
        bonsai_cuda.fn_cuMemcpyDtoH(logits, m->b[19], VOCAB * sizeof(float)), "fallback logits"));
}
'''
output.write_text(host)

model_host = old_host.read_text()
model_host += '''
extern int bonsai_session_choice_available(const struct bonsai_session *, const int64_t *, int64_t);
extern int64_t bonsai_session_choose_next(struct bonsai_session *, uint32_t, int);
extern int bonsai_session_read_choice_logits(struct bonsai_session *, float *);

uint8_t fg_bonsai_choice_available(sl_i64 eog) {
    return H.bonsai && bonsai_session_choice_available(H.bonsai, eog.data, eog.len);
}

int64_t fg_bonsai_choose_next(int32_t token, sl_f32 output, uint8_t ignore_eos) {
    if (!H.bonsai || token < 0 || !output.data || output.len != H.n_tok) return -1;
    const int64_t id = bonsai_session_choose_next(H.bonsai, (uint32_t)token, ignore_eos != 0);
    if (id != -2) return id;
    const size_t bytes = (size_t)output.len * sizeof(float);
    float *safe = malloc(bytes);
    if (!safe) return -1;
    const int status = bonsai_session_read_choice_logits(H.bonsai, safe);
    if (!status) memcpy(output.data, safe, bytes);
    free(safe);
    return status ? -1 : -2;
}
'''
new_host.write_text(model_host)
