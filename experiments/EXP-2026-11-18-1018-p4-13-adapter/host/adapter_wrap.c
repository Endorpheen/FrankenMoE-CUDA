#include <stdint.h>
#define gg_stacks (*fg_adapter_stacks())
#define main fg_adapter_unused_main
#include "build_adapter.c"
#undef main
#undef gg_stacks
static gg_stack (*fg_adapter_stacks(void))[GG_STACK_COUNT] {
    static _Thread_local gg_stack s[GG_STACK_COUNT];
    return &s;
}
int64_t fg_adapter_dispatch(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d) {
    return gg_function_1(op, a, b, c, d, 0);
}
