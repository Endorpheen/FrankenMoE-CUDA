// C wrapper exposing the Goose adapter dispatch to the host test.
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
int64_t fg_adapter_allocate(int64_t buft, uint64_t size) { return gg_function_2(buft, size, 0); }
int64_t fg_adapter_release(int64_t h) { return gg_function_5(h, 0); }
int64_t fg_adapter_reset(int64_t h) { return gg_function_7(h, 0); }
