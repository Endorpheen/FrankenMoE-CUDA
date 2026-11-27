#ifndef BONSAI_SESSION_H
#define BONSAI_SESSION_H

#include <stdint.h>

enum { BONSAI_SESSION_VOCAB = 248320 };
struct bonsai_session;

/* Serialize calls on a session. Device failures require reset before reuse. */
struct bonsai_session *bonsai_session_open(const char *gguf, const char *cubin,
    const char *frozen_ptx, unsigned context, unsigned ubatch);
void bonsai_session_close(struct bonsai_session *session);
int bonsai_session_reset(struct bonsai_session *session);

/* If requested, logits receives one row for the final input token. */
int bonsai_session_step(struct bonsai_session *session, const uint32_t *tokens,
    unsigned count, float logits[BONSAI_SESSION_VOCAB]);
int bonsai_session_is_eog(const struct bonsai_session *session, uint32_t token);

#endif
