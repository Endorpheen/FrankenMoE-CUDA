"""Write Bonsai's single-token logits into the caller buffer after graph evaluation."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one host anchor: {old!r}")
    return text.replace(old, new)


backend, model_host, out = map(Path, sys.argv[1:])
native = backend.read_text()
native += '''
int bonsai_session_step_direct_logits(struct bonsai_session *session, uint32_t token, float *logits) {
    if (!session || session->needs_reset || !logits || token >= VOCAB) return -1;
    model *m = session->runtime;
    if (m->position >= m->context ||
        check(bonsai_cuda.fn_cuCtxPushCurrent(m->cuda), "bind Bonsai decode context")) return -1;
    m->in_decode = 1;
    int status = evaluate_batch_output(m, &token, 1, NULL, 1);
    // The destination is the caller's restored slice. There is no Goose reentry
    // between this readback and the return to the outer generation loop.
    if (!status) status = check(bonsai_cuda.fn_cuMemcpyDtoH(logits, m->b[19],
                                     VOCAB * sizeof(float)), "direct host logits");
    m->in_decode = 0;
    return finish_session_call(session, status);
}
'''
(out / "backend.c").write_text(native)

host = model_host.read_text()
host = replace(host, 'int64_t fg_q35_generation_eval(sl_i32 tokens, sl_f32 output, uint8_t prefill) {',
               '''extern int bonsai_session_step_direct_logits(struct bonsai_session *, uint32_t, float *);

static int bonsai_direct_logits_enabled(void) {
    const char *enabled = getenv("BONSAI_HTTP_DIRECT_LOGITS");
    return H.bonsai && enabled && !strcmp(enabled, "1");
}

int64_t fg_q35_generation_eval(sl_i32 tokens, sl_f32 output, uint8_t prefill) {''')
host = replace(host, '    const size_t token_bytes = (size_t) tokens.len * sizeof(int32_t);',
               '''    if (!prefill && tokens.len == 1 && output.len == H.n_tok && output.data &&
            bonsai_direct_logits_enabled()) {
        const int status = bonsai_session_step_direct_logits(H.bonsai,
            (uint32_t)tokens.data[0], output.data);
        return status ? -1 : output.len;
    }
    const size_t token_bytes = (size_t) tokens.len * sizeof(int32_t);''')
host = replace(host, '    if (id != -2) return id;\n    const size_t bytes',
               '''    if (id != -2) return id;
    if (bonsai_direct_logits_enabled()) {
        return bonsai_session_read_choice_logits(H.bonsai, output.data) ? -1 : -2;
    }
    const size_t bytes''')
(out / "model-host.c").write_text(host)
