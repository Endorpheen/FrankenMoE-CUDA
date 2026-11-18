// Forwards the differential harness to the SHIPPING doors, one door per stage.
#include <stdint.h>
#include <string.h>
extern int64_t fg_typical_entry(const void*, int64_t, const void*, int64_t, const void*, int64_t,
                                const void*, int64_t, int64_t, float, int64_t);
extern int64_t fg_sampler_top_p_call(float *, float *, int64_t *, int64_t, float, int64_t);
extern int64_t fg_sampler_xtc_call(float *, float *, int64_t *, int64_t, float, float, int64_t, float);
extern int64_t fg_sampler_dry_call(const int64_t *, int64_t *, const int64_t *, const int64_t *,
                                   const int64_t *, const int64_t *, float *, const int64_t *,
                                   int64_t, int64_t, int64_t, int64_t, float, float);
int64_t gate_dispatch_c(int64_t op, const float * lv, float * p, int64_t * id, int64_t n,
                        float f, int64_t k, float f2, float f3) {
    static float scr[8192];
    if (op == 0) return fg_typical_entry(lv, n, p, n, id, n, scr, n, n, f, k);
    if (op == 1) return fg_sampler_top_p_call((float *) lv, p, id, n, f, k);
    if (op == 2) return fg_sampler_xtc_call((float *) lv, p, id, n, f, f2, k, f3);
    return -1;
}
extern int64_t fg_dry_rep_limit_entry(const void *, int64_t, const void *, int64_t, const void *, int64_t,
                                      const void *, int64_t, const void *, int64_t, int64_t, int64_t);
int64_t gate_dry_c(const int64_t * last, const int64_t * bh, const int64_t * bts,
                   const int64_t * btl, const int64_t * btail, int64_t nn, int64_t nb) {
    return fg_dry_rep_limit_entry(last, nn, bh, nb, bts, nb, btl, nb, btail, nb * 4, nn, nb);
}
int64_t gate_dry_unused(const int64_t * last, const int64_t * bh, const int64_t * bts,
                        const int64_t * btl, const int64_t * btail, int64_t nn, int64_t nb) {
    static int64_t rc[4096];
    static float lv[4096];
    static int64_t id[4096];
    memset(rc, 0, sizeof(rc));
    for (int64_t i = 0; i < nn && i < 4096; ++i) { lv[i] = 0.0f; id[i] = i; }
    return fg_sampler_dry_call(last, rc, bh, bts, btl, btail, lv, id, nn, nn, 1, nb, 1.0f, 0.0f);
}
