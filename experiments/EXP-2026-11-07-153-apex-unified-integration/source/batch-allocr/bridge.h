/* Bridge contract for the Goose llama_batch_allocr port.
 * PROPOSAL ONLY for Codex. Not compiled, not integrated.
 *
 * Storage-only layer: typed data store plus raw float row copy. It never
 * implements split policy, sequence-set comparison, tail retention,
 * validation decisions or ordering: those live in batch-allocr.goose.
 *
 * Lifecycle: exactly one fg153_ba_state per llama_batch_allocr instance,
 * created by the constructor hook and destroyed by the destructor hook.
 * All handles are pointers into that owner (state, temp vector, ubatch).
 * There is no global registry: no cross-instance handles, no cross-thread
 * races, no stale handles beyond the owner's lifetime.
 *
 * Future native-free cleanup: replace this arena with Goose-owned storage
 * once Goose has float/vector types; the boundary (not the policy) is what
 * remains here.
 */
#ifndef FG153_BATCH_ALLOCR_BRIDGE_H
#define FG153_BATCH_ALLOCR_BRIDGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fg153_ba_state fg153_ba_state;

/* Per-instance lifetime. fg153_ba_state_free releases every scratch vector
 * and ubatch buffer owned by the state. No handle from a freed state is valid. */
fg153_ba_state * fg153_ba_state_new(void);
void             fg153_ba_state_free(fg153_ba_state * s);

/* Typed input view. Owned by the caller (the allocr hook); the state never
 * frees it. Rows are float; all decisions read integers. */
typedef struct fg153_ba_input {
    int64_t n_tok, n_embd, n_embd_inp, n_embd_state;
    int64_t n_seq_max, n_vocab, n_pos, allow_mixed;
    const int64_t * tok_id;
    const int64_t * tok_has_embd;
    const int64_t * tok_has_state;
    const int64_t * tok_output;
    const int64_t * tok_decision;
    const int64_t * tok_embd_off;
    const int64_t * tok_state_off;
    const int64_t * tok_pos;          // [i * n_pos + j]
    const int64_t * tok_seq_off;      // [i]
    const int64_t * tok_seq_flat;     // [tok_seq_off[i] + s]
    const float   * embd;             // [n_tok * n_embd]
    const float   * state;            // [n_tok * n_embd_state]
    int64_t has_mem;
    const int64_t * mem_min;          // [seq] or NULL
    const int64_t * mem_max;
} fg153_ba_input;

/* Scalar fields, integers vectors, sets, maps. */
int64_t fg153_ba_get(int64_t a, int64_t k, int64_t i);
void    fg153_ba_put(int64_t a, int64_t k, int64_t i, int64_t v);
int64_t fg153_ba_call(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d);

/* Raw float row copy inside one state; used only for embedding/state rows. */
int64_t fg153_ba_rcopy(int64_t a, int64_t src_kind, int64_t src_row, int64_t dst_row);

/* Diagnostic sink; never mutates state. */
void fg153_ba_witness(int64_t mode);

/* ---- built ubatch readout (for conversion to real llama_ubatch) ----
 * keys for n(): 0 n_tokens, 1 n_seqs, 2 n_seq_tokens, 3 equal, 4 seqid_count,
 *               5 nsq_size, 6 sqidx_size
 * keys for i(): 0 token, 1 pos, 2 n_seq_id, 3 output, 4 type, 5 decision,
 *               6 batch_idx, 7 seq_id_unq, 8 seq_idx, 9 seq_id flat
 */
int64_t      fg153_ba_ub_n(int64_t h, int64_t key);
int64_t      fg153_ba_ub_i(int64_t h, int64_t key, int64_t idx);
const float * fg153_ba_ub_embd(int64_t h, int64_t * n);
const float * fg153_ba_ub_state(int64_t h, int64_t * n);
void         fg153_ba_ub_release(int64_t h);

/* Raw float import for native batch arrays that live outside the state. */
int64_t fg153_ba_import_embd(int64_t s, int64_t src_ptr, int64_t n_floats);
int64_t fg153_ba_import_state(int64_t s, int64_t src_ptr, int64_t n_floats);

/* Raw float view of the state (for materializing the native batch arrays). */
const float * fg153_ba_state_embd(int64_t s, int64_t * n);
const float * fg153_ba_state_state(int64_t s, int64_t * n);

#ifdef __cplusplus
}
#endif
#endif /* FG153_BATCH_ALLOCR_BRIDGE_H */
