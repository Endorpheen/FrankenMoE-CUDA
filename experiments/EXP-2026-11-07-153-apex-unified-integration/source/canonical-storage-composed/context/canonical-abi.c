/* Canonical CONTEXT/BATCH/UBATCH C ABI over the generated Goose entry.
 * Handles are opaque int64 and are never cast to C++ classes.
 * PROPOSAL ONLY; local checks compile it with the generated entry. */
#include "canonical-context.h"
#include <stdatomic.h>

extern int64_t fgc_goose_entry(int64_t mode, int64_t a, int64_t b, int64_t c,
                               int64_t d, int64_t e, int64_t f);

static atomic_uint_fast64_t g_entries = 0;

#define CALL0(m) (atomic_fetch_add(&g_entries, 1), fgc_goose_entry((m), 0, 0, 0, 0, 0, 0))
#define CALL6(m,a,b,c,d,e,f) (atomic_fetch_add(&g_entries, 1), fgc_goose_entry((m),(a),(b),(c),(d),(e),(f)))

fgc_ctx_h fgc_ctx_new(int64_t n_embd, int64_t n_embd_state, int64_t n_pos_per_embd,
                      int64_t n_seq_max, int64_t n_vocab, int64_t allow_mixed) {
    return CALL6(0, n_embd, n_embd_state, n_pos_per_embd, n_seq_max, n_vocab, allow_mixed);
}
int32_t fgc_ctx_free(fgc_ctx_h ctx) { return (int32_t) CALL6(1, ctx, 0, 0, 0, 0, 0); }

fgc_batch_h fgc_batch_new(fgc_ctx_h ctx) { return CALL6(2, ctx, 0, 0, 0, 0, 0); }
int32_t fgc_batch_free(fgc_batch_h b) { return (int32_t) CALL6(3, b, 0, 0, 0, 0, 0); }
int32_t fgc_batch_clear(fgc_batch_h b) { return (int32_t) CALL6(4, b, 0, 0, 0, 0, 0); }

int32_t fgc_batch_add_token(fgc_batch_h b, int64_t seq_id) { return (int32_t) CALL6(5, b, seq_id, 0, 0, 0, 0); }
int32_t fgc_batch_add_seq(fgc_batch_h b, int64_t idx, int64_t seq_id) { return (int32_t) CALL6(6, b, idx, seq_id, 0, 0, 0); }
int32_t fgc_batch_set_token_id(fgc_batch_h b, int64_t idx, int64_t id) { return (int32_t) CALL6(7, b, idx, id, 0, 0, 0); }
int32_t fgc_batch_set_token_embd(fgc_batch_h b, int64_t idx, int64_t src, int64_t rows, int64_t cols) {
    return (int32_t) CALL6(8, b, idx, src, rows, cols, 0);
}
int32_t fgc_batch_set_token_state(fgc_batch_h b, int64_t idx, int64_t src, int64_t rows, int64_t cols) {
    return (int32_t) CALL6(9, b, idx, src, rows, cols, 0);
}
int32_t fgc_batch_set_token_pos(fgc_batch_h b, int64_t idx, int64_t ptr) { return (int32_t) CALL6(10, b, idx, ptr, 0, 0, 0); }
int32_t fgc_batch_set_output(fgc_batch_h b, int64_t idx, int64_t v) { return (int32_t) CALL6(11, b, idx, v, 0, 0, 0); }
int32_t fgc_batch_set_decision(fgc_batch_h b, int64_t idx, int64_t v) { return (int32_t) CALL6(12, b, idx, v, 0, 0, 0); }

fgc_ub_h fgc_ubatch_reserve(fgc_batch_h fresh_batch, int64_t n_seq_tokens, int64_t n_seqs) {
    return CALL6(13, fresh_batch, n_seq_tokens, n_seqs, 0, 0, 0);
}
fgc_ub_h fgc_ubatch_from_batch(fgc_batch_h b) { return CALL6(14, b, 0, 0, 0, 0, 0); }
int32_t fgc_ubatch_free(fgc_ub_h u) { return (int32_t) CALL6(15, u, 0, 0, 0, 0, 0); }
int32_t fgc_ubatch_as_batch(fgc_ub_h u, fgc_batch_h b) { return (int32_t) CALL6(17, u, b, 0, 0, 0, 0); }
int32_t fgc_ubatch_view_build(fgc_ub_h u, fgc_ubatch_view * out) {
    return (int32_t) CALL6(16, u, (int64_t) (intptr_t) out, 0, 0, 0, 0);
}
uint64_t fgc_ctx_entries(void) { return (uint64_t) atomic_load(&g_entries); }
