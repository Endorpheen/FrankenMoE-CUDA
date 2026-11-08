// glue_smoke.cu - compile-only check of the default-on bridge header.
#include "fg153-goose-cuda.h"
void fg153_glue_smoke_gdn(const float *q, const float *k, const float *v,
                          const float *g, const float *b, const float *s,
                          float *d, float *o, cudaStream_t st) {
    (void) fg153_goose_gdn_launch(q, k, v, g, b, s, d, o,
        128, 4, 8, 2, 1, 128, 4096, 1, 128, 4096, 1, 4, 32,
        4, 1, 0.08838834764831845f, 0, 1, false, false, st);
}
void fg153_glue_smoke_conv(const float *x, const float *w, const float *b,
                           float *y, cudaStream_t st) {
    (void) fg153_goose_ssm_conv_launch(x, w, b, y, 4, 128, 40, 1,
        40, 5120, 4, 1, 128, 5120, true, st);
}
