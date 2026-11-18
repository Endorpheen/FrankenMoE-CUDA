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
