#include "build_chain.c"
#include "fn_ids.h"

int64_t gate_dispatch_c(int64_t op, const float * lv, float * p, int64_t * id, int64_t n,
                        float f, int64_t k, float f2, float f3) {
    gg_slice s0 = { (void *) lv, n }, s1 = { (void *) p, n }, s2 = { (void *) id, n };
    return FG_DISPATCH(op, s0, s1, s2, n, f, k, f2, f3, 0);
}
