static inline __attribute__((always_inline)) void bonsai_pq2_rows(
    __global const uchar *restrict matrix, __global const float *restrict input,
    __global float *restrict output, uint width, uint rows, uint row_group) {
    const uint thread = local_id();
    const uint lane = thread & 31;
    const uint first = (work_group() * 4 + thread / 32) * row_group;
    if (first >= rows) return;
    const uint blocks = width / 128;
    const size_t stride = (size_t)blocks * 34;
    const uint shift = (lane & 3u) * 2u;
    float sums[4] = {0};
    __global const uchar *row[4];
    #pragma unroll
    for (uint j = 0; j < row_group; ++j) {
        const uint index = first + j < rows ? first + j : rows - 1;
        row[j] = matrix + index * stride;
    }
    const __global float *x = input + lane;
    for (uint block = 0; block < blocks; ++block, x += 128) {
        const float x0 = x[0], x1 = x[32], x2 = x[64], x3 = x[96];
        uint codes[4][4];
        float scales[4];
        #pragma unroll
        for (uint j = 0; j < row_group; ++j) {
            const __global uchar *q = row[j] + block * 34;
            const ushort bits = *(__global const ushort *)q;
            __asm__("cvt.f32.f16 %0, %1;" : "=f"(scales[j]) : "h"(bits));
            const __global uchar *packed = q + 2 + lane / 4;
            codes[j][0] = packed[0];
            codes[j][1] = packed[8];
            codes[j][2] = packed[16];
            codes[j][3] = packed[24];
        }
        #pragma unroll
        for (uint j = 0; j < row_group; ++j) {
            float sum = sums[j];
            const float scale = scales[j];
            sum = bonsai_ordered_fma(codes[j][0], shift, scale, x0, sum);
            sum = bonsai_ordered_fma(codes[j][1], shift, scale, x1, sum);
            sum = bonsai_ordered_fma(codes[j][2], shift, scale, x2, sum);
            sums[j] = bonsai_ordered_fma(codes[j][3], shift, scale, x3, sum);
        }
    }
    #pragma unroll
    for (uint j = 0; j < row_group; ++j) {
        float sum = sums[j];
        for (int bit = 16; bit > 0; bit /= 2) {
            float peer;
            __asm__("shfl.sync.bfly.b32 %0, %1, %2, 31, -1;"
                : "=f"(peer) : "f"(sum), "r"(bit));
            sum += peer;
        }
        if (lane == 0 && first + j < rows) output[first + j] = sum;
    }
}

#define ROWS(N) \
__kernel void bonsai_pq2_matvec_rows##N(__global const uchar *matrix, \
    __global const float *input, __global float *output, uint width, uint rows) { \
    bonsai_pq2_rows(matrix, input, output, width, rows, N); \
}
ROWS(2)
ROWS(4)
