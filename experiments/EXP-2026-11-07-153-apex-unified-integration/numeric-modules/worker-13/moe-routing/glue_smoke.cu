// glue_smoke.cu - compile-only check of the default-on MoE bridge header.
#include "moe-routing/fg153-moe-cuda.h"
void fg153_glue_smoke_sort(const float *x, int *d, cudaStream_t st) {
    (void) fg153_goose_argsort_launch(x, d, 256, 4, 1, st);
    (void) fg153_goose_top_k_launch(x, d, 256, 4, 8, st);
}
void fg153_glue_smoke_reduce(const float *e, const float *s, const float *w,
                             float *d, cudaStream_t st) {
    (void) fg153_goose_moe_reduction_launch(e, s, w, d, 512, 4, 8, st);
}
