#include "goose-unit.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

extern void fg143_cpu_init(int argc, char ** argv);
extern int fg_cuda_activation_graph_begin(bool planned);
extern int fg_cuda_activation_prepare(uint64_t host, int64_t bytes, bool fresh, bool overwrite,
    int64_t step, int64_t deadline);
extern int fg_cuda_activation_graph_end(bool success, bool all, uint64_t host, int64_t bytes,
    uint64_t * hits, uint64_t * skipped, uint64_t * expired);
extern int64_t fg_cuda_begin(uint64_t input, int64_t input_bytes, uint64_t output, int64_t output_bytes);
extern int64_t fg_cuda_plane(uint64_t weights, int64_t weight_bytes, int64_t row_stride,
    int64_t extent, int64_t kind, int64_t rows, sl_i64 jobs);
extern int64_t fg_cuda_finish(uint64_t output, int64_t output_bytes);

static void check_matrices(int iterations, int start, bool planned) {
    enum { rows = 16, width = 2 };
    float input[width] = {0};
    float weight[rows][width];
    float output[rows] = {0};
    int64_t job[2];
    const uint64_t target = (uintptr_t) output;
    if (planned) assert(fg_cuda_activation_graph_begin(true) == 0);
    for (int step = 0; step < iterations; ++step) {
        input[0] = (float) (start + step);
        input[1] = 2.0f;
        for (int row = 0; row < rows; ++row) {
            weight[row][0] = 1.0f;
            weight[row][1] = (float) row;
        }
        job[0] = 0;
        job[1] = 0;
        assert(fg_cuda_activation_prepare(target, sizeof output, true, true, step, iterations) == 0);
        assert(fg_cuda_begin((uintptr_t) input, sizeof input, target, sizeof output) == 1);
        assert(fg_cuda_plane((uintptr_t) weight, sizeof weight, sizeof weight[0], width, 0,
            rows, (sl_i64) {job, 2}) == 0);
        assert(fg_cuda_finish(target, sizeof output) == 0);
    }
    if (planned) {
        uint64_t hits, skipped, expired;
        assert(fg_cuda_activation_graph_end(true, false, target, sizeof output, &hits, &skipped, &expired) == 0);
    }
    for (int row = 0; row < rows; ++row) assert(output[row] == (float) (start + iterations - 1 + 2 * row));
}

int main(int argc, char ** argv) {
    goose_init(argc, argv);
    fg143_cpu_init(argc, argv);
    check_matrices(4097, 1, true);
    check_matrices(7, 17, true);
    check_matrices(1, 21, false);
    puts("Packet storage saturation and reuse passed");
}
