#include "goose-unit.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern void fg143_cpu_init(int argc, char ** argv);
extern int fg_cuda_activation_graph_begin(bool planned);
extern int fg_cuda_activation_prepare(uint64_t host, int64_t bytes, bool fresh, bool overwrite,
    int64_t step, int64_t deadline);
extern int fg_cuda_activation_graph_end(bool success, bool all, uint64_t host, int64_t bytes,
    uint64_t * hits, uint64_t * skipped, uint64_t * expired);
extern int64_t fg_cuda_load_experts(sl_i64 groups, sl_i64 layers, int64_t bytes, int64_t words32, int64_t words64);
extern void fg_cuda_expert_pin_register(sl_i64 spans, int64_t bytes);
extern int64_t fg_cuda_prepare_experts(uint64_t tensor, int64_t bytes, sl_i64 active, sl_i64 offsets);
extern int64_t fg_cuda_expert_batch(int64_t weight_bytes, int64_t row_stride, int64_t extent,
    int64_t kind, int64_t rows, sl_i64 jobs, int64_t shared_rows, int64_t input_stride);
extern int64_t fg_cuda_begin(uint64_t input, int64_t input_bytes, uint64_t output, int64_t output_bytes);
extern int64_t fg_cuda_finish(uint64_t output, int64_t output_bytes);
extern void fg_cuda_expert_stats(sl_i64 stats);

enum { layers = 2, projections = 3, experts = 9, rows = 16, input_width = 2, rounds = 4 };
typedef union {
    float data[rows][input_width];
    unsigned char span[512 * 1024];
} Expert;
typedef Expert Tensor[experts];

static float marker(int layer, int plane, int expert) {
    return (float) (1 + layer * 50 + plane * 10 + expert);
}

int main(int argc, char ** argv) {
    goose_init(argc, argv);
    fg143_cpu_init(argc, argv);
    const size_t page = (size_t) sysconf(_SC_PAGESIZE);
    const size_t bytes = layers * projections * sizeof(Tensor);
    const size_t pinned = ((bytes + page - 1) / page) * page;
    const bool mixed = argc == 2 && strcmp(argv[1], "mixed") == 0;
    const bool pageable = argc == 2 && strcmp(argv[1], "pageable") == 0;
    const size_t registered = pageable ? 0 : mixed ? sizeof(Tensor) : pinned;
    Tensor * weights = aligned_alloc(page, pinned);
    assert(weights);
    memset(weights, 0, pinned);

    int64_t metadata[layers][4];
    for (int l = 0; l < layers; ++l) {
        metadata[l][0] = 0;
        for (int p = 0; p < projections; ++p) {
            Tensor * tensor = &weights[l * projections + p];
            metadata[l][p + 1] = (int64_t) (uintptr_t) tensor;
            for (int e = 0; e < experts; ++e) {
                for (int r = 0; r < rows; ++r) {
                    (*tensor)[e].data[r][0] = marker(l, p, e);
                    (*tensor)[e].data[r][1] = (float) r;
                }
            }
        }
    }
    int64_t group[] = {sizeof(Expert), sizeof(Expert), sizeof(Expert), experts,
        layers, experts, 0, layers * experts, layers};
    const int64_t bank = (int64_t) experts * projections * sizeof(Expert);
    assert(fg_cuda_load_experts((sl_i64) {group, 9}, (sl_i64) {&metadata[0][0], 4 * layers},
        bank, layers * experts + 4 * experts + 4 + 5 * experts, layers + experts + 5) == bank);
    if (registered != 0) {
        int64_t spans[] = {(int64_t) (uintptr_t) weights, (int64_t) registered};
        fg_cuda_expert_pin_register((sl_i64) {spans, 2}, (int64_t) registered);
    }

    float output[rounds][projections][experts][rows];
    const float input[input_width] = {1.0f, 2.0f};
    int64_t heads[experts];
    for (int e = 0; e < experts; ++e) heads[e] = e;
    assert(fg_cuda_activation_graph_begin(true) == 0);
    for (int round = 0; round < rounds; ++round) {
        const int l = round % layers;
        for (int slot = 0; slot < projections; ++slot) {
            const int p = (round + slot) % projections;
            const int64_t step = round * projections + slot;
            const uint64_t target = (uintptr_t) output[round][p];
            const int64_t out_bytes = sizeof output[round][p];
            assert(fg_cuda_activation_prepare(target, out_bytes, true, true, step, rounds * projections) == 0);
            assert(fg_cuda_begin((uintptr_t) input, sizeof input, target, out_bytes) == 1);
            int64_t offsets[experts], jobs[3 * experts];
            assert(fg_cuda_prepare_experts((uint64_t) metadata[l][p + 1], sizeof(Expert),
                (sl_i64) {heads, experts}, (sl_i64) {offsets, experts}) == 1);
            for (int e = 0; e < experts; ++e) {
                jobs[3 * e] = 0;
                jobs[3 * e + 1] = e * rows * (int64_t) sizeof(float);
                jobs[3 * e + 2] = offsets[e];
            }
            assert(fg_cuda_expert_batch(sizeof(Expert), input_width * sizeof(float), input_width, 0,
                rows, (sl_i64) {jobs, 3 * experts}, 0, 0) == 0);
            assert(fg_cuda_finish(target, out_bytes) == 0);
        }
    }
    uint64_t hits, skipped, expired;
    assert(fg_cuda_activation_graph_end(true, true, 0, 0, &hits, &skipped, &expired) == 0);
    for (int round = 0; round < rounds; ++round) {
        for (int p = 0; p < projections; ++p) {
            for (int e = 0; e < experts; ++e) {
                for (int r = 0; r < rows; ++r) {
                    assert(output[round][p][e][r] == marker(round % layers, p, e) + 2 * r);
                }
            }
        }
    }
    int64_t stats[6] = {0};
    fg_cuda_expert_stats((sl_i64) {stats, 6});
    assert(stats[1] == rounds * experts && stats[2] == (rounds - 1) * experts);
    assert(stats[4] == (int64_t) registered && stats[5] == (registered != 0));
    puts("Expert projection staging and cross-layer eviction passed");
    // The CUDA context owns the registration until its exit handler runs.
}
