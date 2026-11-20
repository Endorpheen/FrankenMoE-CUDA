#include "goose-unit.h"
#include "ggml.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

extern void fg143_cpu_init(int argc, char ** argv);
extern int64_t fg_cuda_load_experts(sl_i64 groups, sl_i64 layers, int64_t bytes, int64_t words32, int64_t words64);
extern int64_t fg_cuda_expert_upload(int64_t offset, uint64_t source, int64_t bytes);
extern int64_t fg_cuda_expert_batch(int64_t weight_bytes, int64_t row_stride, int64_t extent,
    int64_t kind, int64_t rows, sl_i64 jobs, int64_t shared_rows, int64_t input_stride);
extern int64_t fg_cuda_begin(uint64_t input, int64_t input_bytes, uint64_t output, int64_t output_bytes);
extern int64_t fg_cuda_finish(uint64_t output, int64_t output_bytes);
extern uint64_t fg_cuda_take_mmq(void);

int main(int argc, char ** argv) {
    assert(argc == 3);
    const bool shared_pack = atoi(argv[2]) != 0;
    const int groups[] = {1, 7, 8, 9, 16, 24, 31, 32, 33, 65};
    enum { experts = sizeof groups / sizeof *groups, rows = 512, width = 2048, input_rows = 39 };
    const int64_t stride = ggml_row_size(GGML_TYPE_Q4_K, width);
    const int64_t expert_bytes = rows * stride;
    int routes = 0;
    for (int e = 0; e < experts; ++e) routes += groups[e];
    const size_t input_bytes = input_rows * width * sizeof(float);
    const size_t output_bytes = (size_t) routes * rows * sizeof(float);
    float * input = malloc(input_bytes);
    float * output = malloc(output_bytes);
    unsigned char * weight = malloc((size_t) expert_bytes);
    int64_t * jobs = malloc((size_t) routes * 3 * sizeof *jobs);
    assert(input && output && weight && jobs);
    for (int i = 0; i < input_rows * width; ++i) {
        input[i] = (float) ((13 * i + i / 123) % 131 - 65) / 86.0f;
    }

    goose_init(argc, argv);
    fg143_cpu_init(argc, argv);
    // The test uploads expert planes directly, so no SLRU metadata is needed.
    int64_t empty = 0;
    sl_i64 unused = {&empty, 1};
    assert(fg_cuda_load_experts(unused, unused, experts * expert_bytes, 1, 1) == experts * expert_bytes);
    assert(fg_cuda_begin((uintptr_t) input, input_bytes, (uintptr_t) output, output_bytes) == 1);
    const struct ggml_type_traits * traits = ggml_get_type_traits(GGML_TYPE_Q4_K);
    int route = 0;
    for (int e = 0; e < experts; ++e) {
        for (int i = 0; i < rows; ++i) {
            float row[width];
            for (int j = 0; j < width; ++j) {
                row[j] = (float) ((17 * e + 3 * i + 37 * j + j / 17) % 97 - 48) / 53.0f;
            }
            traits->from_float_ref(row, weight + stride * i, width);
        }
        assert(fg_cuda_expert_upload(e * expert_bytes, (uintptr_t) weight, expert_bytes) == 0);
        for (int j = 0; j < groups[e]; ++j, ++route) {
            jobs[route * 3] = ((11 * route + e) % input_rows) * width * (int64_t) sizeof(float);
            jobs[route * 3 + 1] = (routes - 1 - route) * rows * (int64_t) sizeof(float);
            jobs[route * 3 + 2] = e * expert_bytes;
        }
    }
    assert(fg_cuda_expert_batch(expert_bytes, stride, width / 256, GGML_TYPE_Q4_K, rows,
        (sl_i64) {jobs, 3 * routes}, shared_pack ? input_rows : 0, width * sizeof(float)) == 0);
    assert(fg_cuda_finish((uintptr_t) output, output_bytes) == 0);
    assert(fg_cuda_take_mmq() == 1);
    FILE * file = fopen(argv[1], "wb");
    assert(file);
    assert(fwrite(output, 1, output_bytes, file) == output_bytes);
    assert(fclose(file) == 0);
    printf("%d routed Q4_K columns, %zu output bytes\n", routes, output_bytes);
    free(jobs);
    free(weight);
    free(output);
    free(input);
}
