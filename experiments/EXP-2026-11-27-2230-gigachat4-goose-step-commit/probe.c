#include "step.h"
#include <assert.h>
#include <stdio.h>

static int alloc_fail, upload_fail, compute_fail, resets, releases, wave;
uint64_t fg_standalone_graph_alloc(uint64_t graph) { return alloc_fail ? 0 : graph + 99; }
uint64_t fg_standalone_graph_plan_alloc(uint64_t graph, uint64_t output) { return fg_standalone_graph_alloc(graph); }
int32_t fg_standalone_graph_compute(uint64_t graph) { assert(!wave); return compute_fail; }
int32_t fg_standalone_graph_plan_compute(uint64_t plan) { assert(wave); return compute_fail; }
int32_t giga_step_upload(uint64_t step, uint64_t tokens) { return upload_fail; }
void fg_standalone_graph_plan_free(uint64_t plan) { ++releases; }
void fg_q35_release_graph(uint64_t arena) { ++releases; }
void fg_cuda_gigachat4_small_expert_wave(uint8_t enabled) { wave = enabled; }
void giga_request_reset(uint64_t request) { ++resets; }

int main(void) {
    goose_init(0, NULL);
    unsigned cases = 0;
    for (int planned = 0; planned <= 1; ++planned)
        for (int c = 0; c < 7; ++c) {
            int64_t cursor = 15;
            uint64_t arena = 0;
            int64_t base = c == 4 ? 16 : 15;
            int64_t count = c == 5 ? 0 : 5;
            int64_t capacity = c == 6 ? 19 : 20;
            alloc_fail = c==1; upload_fail = c==2; compute_fail = c==3;
            releases = 0; resets = 0;
            int64_t want = c == 0 ? 0 : c == 1 ? -2 : c == 2 ? -3 : -4;
            assert(giga_execute_step(1, 1, 1, 1, 1, planned, (sl_u64){&arena,1},
                                     (sl_i64){&cursor,1},base,count,capacity) == want);
            assert(cursor==(c==0?20:15));
            assert(arena==(c==0?100:0));
            assert(resets==(c>=3));
            assert(releases==(c>=2));
            assert(!wave);
            ++cases;
        }
    printf("%u scenarios passed\n",cases);
}
