#include "session.h"

struct bonsai_session {
    model *runtime;
    int needs_reset;
};

struct bonsai_session *bonsai_session_open(const char *gguf, const char *cubin,
    const char *frozen_ptx, unsigned context, unsigned ubatch) {
    if (!gguf || !cubin || !frozen_ptx || ubatch == 0 || ubatch > UBATCH_MAX) return NULL;
    struct bonsai_session *session = calloc(1, sizeof(*session));
    if (!session) return NULL;
    goose_init(0, NULL);
    session->runtime = bonsai_open(gguf, cubin, frozen_ptx, context, ubatch);
    if (!session->runtime) {
        free(session);
        return NULL;
    }
    CUcontext current;
    if (check(cuCtxPopCurrent(&current), "detach Bonsai context")) {
        bonsai_session_close(session);
        return NULL;
    }
    return session;
}

void bonsai_session_close(struct bonsai_session *session) {
    if (!session) return;
    (void) check(cuCtxDestroy(session->runtime->cuda), "destroy Bonsai context");
    free(session->runtime);
    free(session);
}

static int finish_session_call(struct bonsai_session *session, int status) {
    CUcontext current;
    if (check(cuCtxPopCurrent(&current), "restore caller context")) status = -1;
    session->needs_reset = status != 0;
    return status;
}

int bonsai_session_reset(struct bonsai_session *session) {
    if (!session) return -1;
    model *m = session->runtime;
    if (check(cuCtxPushCurrent(m->cuda), "bind Bonsai context")) return -1;
    int status = 0;
    for (unsigned layer = 0; layer < LAYERS && !status; ++layer) {
        if (m->bound[15 * layer] == 0) continue;
        status = check(cuMemsetD8(m->states[2 * layer], 0, 4 * GDN_HISTORY), "reset convolution");
        if (!status) {
            status = check(cuMemsetD8(m->states[2 * layer + 1], 0, 4 * GDN_STATE), "reset GDN");
        }
    }
    if (!status) status = check(cuMemsetD8(m->b[23], 0, 4 * GDN_STATE), "reset GDN scratch");
    if (!status) {
        /* Attention writes new rows before reading only the active prefix. */
        m->position = 0;
        m->count = 0;
        m->packed_source = 0;
    }
    return finish_session_call(session, status);
}

int bonsai_session_step(struct bonsai_session *session, const uint32_t *tokens,
    unsigned count, float logits[BONSAI_SESSION_VOCAB]) {
    if (!session || session->needs_reset || !tokens || !count) return -1;
    model *m = session->runtime;
    if (count > m->context - m->position) return -1;
    for (unsigned i = 0; i < count; ++i) {
        if (tokens[i] >= VOCAB) return -1;
    }
    if (check(cuCtxPushCurrent(m->cuda), "bind Bonsai context")) return -1;
    int status = 0;
    for (unsigned at = 0; at < count && !status;) {
        const unsigned remaining = count - at;
        const unsigned available = remaining - (logits && remaining > 1);
        const unsigned batch = available < m->ubatch ? available : m->ubatch;
        status = evaluate_batch(m, tokens + at, batch, remaining == 1 ? logits : NULL);
        at += batch;
    }
    return finish_session_call(session, status);
}

int bonsai_session_is_eog(const struct bonsai_session *session, uint32_t token) {
    if (!session) return 0;
    for (int64_t i = 0; i < session->runtime->eog_count; ++i) {
        if (session->runtime->eog[i] == token) return 1;
    }
    return 0;
}
