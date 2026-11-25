static inline __attribute__((always_inline)) void mvq_three(
    __global const uchar *weights, __global const uchar *input,
    __global float *output, uint width, uint rows, uint grouping) {
    const uint thread = local_id(), lane = thread & 31;
    const uint start = (work_group() * 4 + thread / 32) * grouping;
    if (start >= rows) return;
    const uint groups = width / 32, blocks = width / 128;
    float values[2][3] = {{0}};
    for (uint i = lane; i < groups; i += 32) {
        const __global uchar *a[3];
        float dx[3];
        #pragma unroll
        for (uint t = 0; t < 3; ++t) {
            a[t] = input + ((size_t)t * groups + i) * 36;
            dx[t] = mvq_half(*(__global const ushort *)a[t]);
        }
        #pragma unroll
        for (uint r = 0; r < grouping; ++r) {
            const uint row = start + r < rows ? start + r : rows - 1;
            const __global uchar *w = weights + ((size_t)row * blocks + i/4) * 34;
            const __global ushort *pairs = (__global const ushort *)(w + 2 + (i % 4) * 8);
            const float dw = mvq_half(*(__global const ushort *)w);
            int dot[3] = {0, 0, 0};
            const uint map = 0x020100ff;
            #pragma unroll
            for (uint j = 0; j < 4; ++j) {
                const uint code = pairs[j];
                const uint even = mvq_permute(map, map, code & 0x3333);
                const uint odd = mvq_permute(map, map, (code >> 2) & 0x3333);
                const uint w0 = mvq_permute(even, odd, 0x5140);
                const uint w1 = mvq_permute(even, odd, 0x7362);
                #pragma unroll
                for (uint t = 0; t < 3; ++t) {
                    const __global uint *q = (__global const uint *)(a[t] + 4);
                    __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(dot[t]) : "r"(w0), "r"(q[2*j]));
                    __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(dot[t]) : "r"(w1), "r"(q[2*j+1]));
                }
            }
            #pragma unroll
            for (uint t = 0; t < 3; ++t) values[r][t] += (dw * dx[t]) * (float)dot[t];
        }
    }
    #pragma unroll
    for (uint r = 0; r < grouping; ++r) {
        #pragma unroll
        for (uint t = 0; t < 3; ++t) {
            for (uint bit = 16; bit; bit >>= 1) values[r][t] += flash_shuffle(values[r][t], bit);
            if (lane == 0 && start + r < rows) output[(size_t)t * rows + start + r] = values[r][t];
        }
    }
}

#define MVQ_VERIFY(N) \
__kernel void bonsai_pq2_mvq_q8_three_##N(__global const uchar *weights, __global const uchar *input, \
    __global float *output, uint width, uint rows) { \
    mvq_three(weights, input, output, width, rows, N); \
}
MVQ_VERIFY(1)
MVQ_VERIFY(2)
