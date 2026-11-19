#include "blocks.h"

#define gg_stacks (*fg_cpu_simd_stacks())
#define main fg_cpu_simd_unused_example_main
#include "gemv_smoke.c"
#undef main
#undef gg_stacks

static gg_stack (*fg_cpu_simd_stacks(void))[GG_STACK_COUNT] {
    static _Thread_local gg_stack stacks[GG_STACK_COUNT];
    return &stacks;
}

FG_CPU_SIMD_API int32_t fg_cpu_simd_row_q4_k_q8_k_x8(fg_cpu_simd_bytes w, fg_cpu_simd_bytes a,
                                                     int32_t nb, int32_t nc, float * out) {
    if (!w.data || !a.data || !out || nb <= 0 || nc <= 0 || (nc % 8) != 0) return -1;
    if (w.len != (int64_t) nc * nb * 144 || a.len != (int64_t) nb * 292) return -1;
    const uint8_t ok = gg_function_1((gg_slice){ (void *) w.data, w.len },
                                     (gg_slice){ (void *) a.data, a.len },
                                     nb, nc, (gg_slice){ out, nc }, 0);
    return ok ? 0 : -1;
}
