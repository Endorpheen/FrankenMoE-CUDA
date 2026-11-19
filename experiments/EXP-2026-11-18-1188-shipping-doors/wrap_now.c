/* The dispatcher this harness declared, provided by the door that replaced it.
 *
 * The original gate_dispatch_c lived in a run-local scratch file that the storage rule correctly did
 * not commit. Its signature is identical to fg_sampler_chain_call, which the current library exports,
 * so the accepted measurements can be re-run without rewriting what was measured. The op numbers are
 * the harness's own: 0 = typical through the module's internal path, 1 = top-p.
 */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int64_t fg_sampler_typical_call(float * lv, float * p, int64_t * id, float * scr,
                                int64_t n, float typical_p, int64_t min_keep);
int64_t fg_sampler_top_p_call(float * lv, float * p, int64_t * id, int64_t n,
                              float top_p, int64_t min_keep);
int64_t fg_sampler_xtc_call(float * lv, float * p, int64_t * id, int64_t n,
                            float probability, float threshold, int64_t min_keep, float chance);

/* The dispatcher reaches the doors this harness measured, not the chain's internal ops: the chain's
 * op 1 is sm_apply_top_p, which takes no id array and filters logits in place, while the accepted
 * measurement went through the top-p door. Routing through the chain first produced 898 mismatches
 * out of 900 - a wrong reconstruction, not a defect in the port. */
int64_t gate_dispatch_c(int64_t op, const float * lv, float * p, int64_t * id, int64_t n,
                        float f, int64_t k, float f2, float f3) {
    if (op == 0) {
        float scr[64];
        if (n > 64) { return -2; }
        return fg_sampler_typical_call((float *) lv, p, id, scr, n, f, k);
    }
    if (op == 1) {
        return fg_sampler_top_p_call((float *) lv, p, id, n, f, k);
    }
    if (op == 2) {
        return fg_sampler_xtc_call((float *) lv, p, id, n, f, f2, k, f3);
    }
    return -1;
}

typedef struct { int64_t * data; int64_t len; } gate_sl_i64;

int64_t fg_dry_rep_limit_entry(gate_sl_i64, gate_sl_i64, gate_sl_i64, gate_sl_i64, gate_sl_i64,
                               int64_t, int64_t);

/* The DRY arm's contract is narrower than the sampler: it returns the repetition limit for the
 * current token, which fg_dry_rep_limit_entry provides directly. b_tail holds four breakers per
 * match, the same shape the shim builds. */
int64_t gate_dry_c(const int64_t * last, const int64_t * bh, const int64_t * bts,
                   const int64_t * btl, const int64_t * btail, int64_t n, int64_t nb) {
    gate_sl_i64 s0 = { (int64_t *) last, n }, s1 = { (int64_t *) bh, nb };
    gate_sl_i64 s2 = { (int64_t *) bts, nb }, s3 = { (int64_t *) btl, nb };
    gate_sl_i64 s4 = { (int64_t *) btail, nb * 4 };
    return fg_dry_rep_limit_entry(s0, s1, s2, s3, s4, n, nb);
}

#ifdef __cplusplus
}
#endif
