// C shim for the Goose tiled Q4_K MoE kernel. It only unpacks the published
// ABI struct into slices and calls the emitted function; all math stays in Goose.
#include <stdint.h>
#include <string.h>

#define gg_stacks (*fg_mmid_stacks())
#define main fg_mmid_unused_example_main
#include "mmid_smoke.c"
#undef main
#undef gg_stacks

static gg_stack (*fg_mmid_stacks(void))[GG_STACK_COUNT] {
    static _Thread_local gg_stack stacks[GG_STACK_COUNT];
    return &stacks;
}

typedef struct {
    const uint8_t * src0;
    int64_t ne00;
    int64_t ne01;
    int64_t nb01;
    const uint8_t * act;
    int64_t nbw1;
    const int32_t * routes;
    int64_t cne1;
    int64_t nb0;
    int64_t nb1;
    int64_t nb2;
    float * dst;
    int32_t ith;
    int32_t nth;
} fg_mmid_tiled_q4_k_args;

#define FG_MMID_API __attribute__((visibility("default")))

FG_MMID_API int32_t fg_cpu_simd_mmid_tiled_q4_k(const fg_mmid_tiled_q4_k_args * a, uint8_t * scratch) {
    if (!a || !a->src0 || !a->act || !a->routes || !a->dst || !scratch) {
        return -1;
    }
    if (a->ne00 <= 0 || a->ne01 <= 0 || a->cne1 <= 0 || a->nth <= 0 || a->ith < 0 || a->ith >= a->nth) {
        return -1;
    }
    gg_slice s0 = { (void *) a->src0, a->nb01 * a->ne01 };
    gg_slice s1 = { (void *) a->act, a->nbw1 * a->cne1 };
    gg_slice s2 = { (void *) a->routes, 2 * a->cne1 };
    // The routed dst rows are sparse: the slice must cover the highest
    // (route0*nb1 + route1*nb2) plus the last weight row, not a dense block.
    int64_t dst_bytes = a->nb0 * a->ne01;
    for (int64_t m = 0; m < a->cne1; m++) {
        const int64_t here = (int64_t) a->routes[2 * m] * a->nb1 + (int64_t) a->routes[2 * m + 1] * a->nb2 +
                             (a->ne01 - 1) * a->nb0 + 4;
        if (here > dst_bytes) {
            dst_bytes = here;
        }
    }
    gg_slice s3 = { (void *) a->dst, dst_bytes };
    gg_slice s4 = { (void *) (scratch + (size_t) a->ith * (512 * 1024)), 16384 };
    gg_slice s5 = { (void *) (scratch + (size_t) a->ith * (512 * 1024) + 65536), 256 };
    gg_function_1(s0, a->ne00, a->ne01, a->nb01, s1, a->nbw1, s2, a->cne1,
                  a->nb0, a->nb1, a->nb2, s3, a->ith, a->nth, s4, s5, 0);
    return 0;
}
