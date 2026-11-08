/* Canonical CONTEXT / BATCH / UBATCH typed C API (APX-019/030, worker5).
 *
 * Opaque int64_t handles only. Never cast a canonical handle to a C++ class.
 * Storage is built on the worker8 canonical record/vector primitives.
 *
 * Ownership:
 *  - fgc_ctx_new / fgc_batch_new / fgc_ubatch_new return owned handles.
 *  - fgc_batch_free / fgc_ubatch_free release only the module's own records and
 *    vectors; they do not free caller-provided bytes or handles stored by value.
 *  - fgc_ubatch_view_build materializes pointers owned by the ubatch record and
 *    valid until fgc_ubatch_free or the next view build on that ubatch.
 */
#ifndef FGC_CANONICAL_CONTEXT_H
#define FGC_CANONICAL_CONTEXT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t fgc_ctx_h;
typedef int64_t fgc_batch_h;
typedef int64_t fgc_ub_h;

/* Record kinds owned by this module. */
enum {
    FGC_KIND_CTX   = 0x43545831,  /* "CTX1" */
    FGC_KIND_BATCH = 0x42415431,  /* "BAT1" */
    FGC_KIND_UB    = 0x55425431   /* "UBT1" */
};

/* Context record keys. */
enum {
    FGC_CTX_N_EMBD        = 0,
    FGC_CTX_N_EMBD_STATE  = 1,
    FGC_CTX_N_POS         = 2,
    FGC_CTX_N_SEQ_MAX     = 3,
    FGC_CTX_N_VOCAB       = 4,
    FGC_CTX_N_TOKENS_MAX  = 5,
    FGC_CTX_ALLOW_MIXED   = 6
};

/* Batch record keys. */
enum {
    FGC_B_CTX          = 0,
    FGC_B_N_TOKENS     = 1,
    FGC_B_N_EMBD       = 2,
    FGC_B_N_EMBD_STATE = 3,
    FGC_B_N_POS        = 4,
    FGC_B_N_SEQ_MAX    = 5,
    FGC_B_N_VOCAB      = 6,
    FGC_B_ALLOW_MIXED  = 7,
    FGC_B_N_OUTPUTS    = 8,
    FGC_B_N_USED       = 9,
    FGC_B_HAS_TOKEN    = 10,
    FGC_B_HAS_EMBD     = 11,
    FGC_B_HAS_STATE    = 12,
    FGC_B_MIXED        = 13,
    FGC_B_V_IS_EMBD    = 20,
    FGC_B_V_TOKEN      = 21,
    FGC_B_V_EMBD       = 22,
    FGC_B_V_STATE      = 23,
    FGC_B_V_POS        = 24,
    FGC_B_V_NSQ        = 25,
    FGC_B_V_SEQFLAT    = 26,
    FGC_B_V_SEQOFF     = 27,
    FGC_B_V_OUTPUT     = 28,
    FGC_B_V_DECISION   = 29,
    FGC_B_V_USED       = 30,
    FGC_B_V_OUTIDS     = 31,
    FGC_B_V_HAS_EMBD   = 32,
    FGC_B_V_HAS_STATE  = 33
};

/* Ubatch record keys (arrays are records too, same vec keys). */
enum {
    FGC_U_N_TOKENS      = 0,
    FGC_U_N_SEQ_TOKENS  = 1,
    FGC_U_N_SEQS        = 2,
    FGC_U_N_SEQS_UNQ    = 3,
    FGC_U_N_POS         = 4,
    FGC_U_EQUAL         = 5,
    FGC_U_FLAGS         = 6,
    FGC_U_V_TOKEN       = 20,
    FGC_U_V_EMBD        = 21,
    FGC_U_V_STATE       = 22,
    FGC_U_V_POS         = 23,
    FGC_U_V_NSQ         = 24,
    FGC_U_V_SEQFLAT     = 25,
    FGC_U_V_SEQOFF      = 26,
    FGC_U_V_SEQUNQ      = 27,
    FGC_U_V_SEQIDX      = 28,
    FGC_U_V_OUTPUT      = 29,
    FGC_U_V_TYPE        = 30,
    FGC_U_V_DECISION    = 31,
    FGC_U_V_BIDX        = 32,
    FGC_U_V_SEQPTR      = 33   /* internal: pointer-fixup array (i64 slots) */
};

/* Return status: 0 success, negative error. Handles are never 0 on success. */
fgc_ctx_h   fgc_ctx_new(int64_t n_embd, int64_t n_embd_state, int64_t n_pos_per_embd,
                        int64_t n_seq_max, int64_t n_vocab, int64_t allow_mixed);
int32_t     fgc_ctx_free(fgc_ctx_h ctx);

fgc_batch_h fgc_batch_new(fgc_ctx_h ctx);
int32_t     fgc_batch_free(fgc_batch_h batch);
int32_t     fgc_batch_clear(fgc_batch_h batch);

int32_t     fgc_batch_add_token(fgc_batch_h batch, int64_t seq_id);
int32_t     fgc_batch_add_seq(fgc_batch_h batch, int64_t idx, int64_t seq_id);
int32_t     fgc_batch_set_token_id(fgc_batch_h batch, int64_t idx, int64_t token_id);
int32_t     fgc_batch_set_token_embd(fgc_batch_h batch, int64_t idx, int64_t src_ptr, int64_t n_rows, int64_t n_embd);
int32_t     fgc_batch_set_token_state(fgc_batch_h batch, int64_t idx, int64_t src_ptr, int64_t n_rows, int64_t n_embd);
int32_t     fgc_batch_set_token_pos(fgc_batch_h batch, int64_t idx, int64_t pos_ptr);
int32_t     fgc_batch_set_output(fgc_batch_h batch, int64_t idx, int64_t value);
int32_t     fgc_batch_set_decision(fgc_batch_h batch, int64_t idx, int64_t order);

/* Reinitialize a fresh batch from a context: clear first, then only the
 * reserve metadata (n_pos, n_seqs, token/seq skeleton). No full import. */
fgc_ub_h    fgc_ubatch_reserve(fgc_batch_h fresh_batch, int64_t n_seq_tokens, int64_t n_seqs);
fgc_ub_h    fgc_ubatch_from_batch(fgc_batch_h batch);
int32_t     fgc_ubatch_free(fgc_ub_h ub);

int32_t     fgc_ubatch_as_batch(fgc_ub_h ub, fgc_batch_h out_batch); /* metadata+arrays copy */

typedef struct fgc_ubatch_view {
    int64_t n_tokens, n_seq_tokens, n_seqs, n_seqs_unq, n_pos;
    int32_t equal;
    const int32_t * token;
    const float   * embd;
    const float   * embd_state;
    const int32_t * pos;
    const int32_t * n_seq_id;
    const int32_t * const * seq_id;
    const int32_t * seq_id_unq;
    const int32_t * seq_idx;
    const int8_t  * output;
    const int8_t  * type;
    const int32_t * decision_order;
    const int32_t * batch_idxs;
} fgc_ubatch_view;

int32_t fgc_ubatch_view_build(fgc_ub_h ub, fgc_ubatch_view * out);

/* Diagnostics for local checks: number of entries executed by this module. */
uint64_t fgc_ctx_entries(void);

#ifdef __cplusplus
}
#endif
#endif
