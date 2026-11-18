#include <stdint.h>
#define gg_stacks (*stacks_tl())
#define main chain_unused_main
#include "build_chain.c"
#undef main
#undef gg_stacks
static gg_stack (*stacks_tl(void))[GG_STACK_COUNT] {
    static _Thread_local gg_stack s[GG_STACK_COUNT];
    return &s;
}
int64_t gate_typical_c(const float * lv, float * p, int64_t * id, int64_t n, float tp, int64_t mk) {
    gg_slice s0 = { (void *) lv, n }, s1 = { (void *) p, n }, s2 = { (void *) id, n };
    return gg_function_1(s0, s1, s2, n, tp, mk, 0);
}
int64_t gate_sort_c(float * lv, int64_t * id, int64_t n) {
    gg_slice s0 = { (void *) lv, n }, s1 = { (void *) id, n };
    return gg_function_8(s0, s1, n, 0);
}
int64_t gate_top_p_c(const float * lv, float * p, int64_t * id, int64_t n, float tp, int64_t mk) {
    gg_slice s0 = { (void *) lv, n }, s1 = { (void *) p, n }, s2 = { (void *) id, n };
    return gg_function_2(s0, s1, s2, n, tp, mk, 0);
}
int64_t gate_dispatch_c(int64_t op, const float * lv, float * p, int64_t * id, int64_t n,
                        float f, int64_t k, float f2, float f3) {
    gg_slice s0 = { (void *) lv, n }, s1 = { (void *) p, n }, s2 = { (void *) id, n };
    return gg_function_17(op, s0, s1, s2, n, f, k, f2, f3, 0);
}
int64_t gate_dry_c(const int64_t * last, const int64_t * bh, const int64_t * bts,
                   const int64_t * btl, const int64_t * btail, int64_t n, int64_t nb) {
    gg_slice s0 = { (void *) last, n }, s1 = { (void *) bh, nb }, s2 = { (void *) bts, nb };
    gg_slice s3 = { (void *) btl, nb }, s4 = { (void *) btail, nb * 4 };
    return gg_function_20(s0, s1, s2, s3, s4, n, nb, 0);
}
int64_t gate_dry2_c(const int64_t * last, int64_t * rc, int64_t n, int64_t rep_limit) {
    gg_slice s0 = { (void *) last, n }, s1 = { (void *) rc, n };
    return gg_function_22(s0, s1, n, rep_limit, 0);
}
