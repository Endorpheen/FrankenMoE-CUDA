/* bridge.h - exact extern contract for the recurrent Goose modules.
 * Proposal for Codex; not verified by a build here.
 *
 * Symbols are grouped per Goose module. services.cpp implements all of them
 * (fg153_lc is a Goose-to-Goose seam and is bound to the emitted cells entry).
 *
 * All handles are opaque int64_t. Nothing here exposes C struct offsets;
 * every field is reached through a named accessor in services.cpp.
 */
#ifndef FG153_RECURRENT_BRIDGE_H
#define FG153_RECURRENT_BRIDGE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int fg153_recurrent_enabled(void);

/* ---- cells module: worker-3/recurrent-cells.goose ---- */
int64_t fg153_rg(int64_t p, int64_t k, int64_t i);          /* typed field read  */
void    fg153_rw(int64_t p, int64_t k, int64_t i, int64_t v); /* typed field write */
int64_t fg153_rs(int64_t op, int64_t a, int64_t b, int64_t c); /* container+prim  */
void    fg153_rc(int64_t mode);                              /* witness sink      */

/* ---- lifecycle module: worker-3/recurrent-lifecycle.goose ---- */
int64_t fg153_lg(int64_t p, int64_t k, int64_t i);
void    fg153_lp(int64_t p, int64_t k, int64_t i, int64_t v);
int64_t fg153_ls(int64_t op, int64_t a, int64_t b, int64_t c);
int64_t fg153_lc(int64_t op, int64_t a, int64_t b, int64_t c);
void    fg153_lw(int64_t mode);

/* ---- state module: worker-3/recurrent-state.goose ---- */
int64_t fg153_sg(int64_t p, int64_t k, int64_t i);
void    fg153_sp(int64_t p, int64_t k, int64_t i, int64_t v);
int64_t fg153_ss(int64_t op, int64_t a, int64_t b, int64_t c);
int64_t fg153_sc(int64_t op, int64_t a, int64_t b, int64_t c);
void    fg153_sw(int64_t mode);

/* harness helpers (fixture/replay only) */
int64_t fg153_box_new(int64_t bytes);
void    fg153_box_free(int64_t h);
/* atomic Goose liveness counters: family 0=cells,1=lifecycle,2=state */
uint64_t fg153_recurrent_witness(int64_t family, int64_t mode);
void     fg153_recurrent_witness_reset(void);

/* =====================================================================
 * RAW KEY SCHEMAS (shared by fg153_rg/lg/sg and rw/lp/sp)
 * =====================================================================
 * mem projection handle (opened by fg153_projection_open):
 *   0 size          4 n_seq_max      7 empty(derived, override-able)
 *   1 used          5 n_rs_seq       70 r_l[il]
 *   2 head          6 rs_z           71 s_l[il]
 *   3 n                              72 p_l[il]
 *   10 pos[i]       11 src[i]        80 buf[k]
 *   12 src0[i]      13 tail[i]       81 buft[k]
 *   14 seq_set[i]   15 rs_idx[seq]   83 ctx[k]   84 buf_count
 *   Key 7 is derived from raw ctxs_bufs emptiness (never the owned is_empty()).
 *
 * seq_set handle (opaque std::set<llama_seq_id>*, only passed to fg153_rs):
 *   used by cells container ops 1..5,7,10. Never dereferenced by Goose.
 *
 * context box keys:
 *   0 status  1 mem  2 i_next  3 ubatch_list  4 ubatch_count  5 is_full
 *
 * descriptor box (find_slot): 20 n_seqs, 21 n_seq_tokens,
 *   22 primary_seq[s], 23 last_pos[s]
 * prepare batch box: 24 count, 25 desc[k] (cells owns 30..35 internally)
 *
 * generic box/list: 10 item[i], 11 count, 12 agg[k]
 * create cfg: 0 size 1 n_seq_max 2 n_rs_seq 3 empty
 * tensor cfg: 0 type 1 ne0 2 n_rows
 * split_equal cfg: 0 n_ubatch 1 equal_seqs 2 tail
 * io tensor cfg: 0 offset 1 size
 * ===================================================================== */

#ifdef __cplusplus
}
/* typed projection helpers (C++ only; never used by the C ABI above) */
struct llama_memory_recurrent;
struct fg153_recurrent_projection;
fg153_recurrent_projection * fg153_projection_open(llama_memory_recurrent * m,
                                                   const void * filter, void * model);
void fg153_projection_close(fg153_recurrent_projection * p);
#endif
#endif /* FG153_RECURRENT_BRIDGE_H */
