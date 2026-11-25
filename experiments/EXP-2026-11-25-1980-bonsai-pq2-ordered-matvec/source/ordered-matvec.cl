static float bonsai_ordered_fma(uint byte, uint shift, float scale, float value, float sum) {
    const float weight = scale * (float)((int)((byte >> shift) & 3u) - 1);
    __asm__("fma.rn.f32 %0, %1, %2, %0;" : "+f"(sum) : "f"(weight), "f"(value));
    return sum;
}

// Each lane accumulates columns lane + 32*j in the original order. A
// 128-weight block shares one scale across four successive lane updates.
static void bonsai_ordered_matvec(__global const uchar *matrix, __global const float *input,
                                  __global float *output, uint width, uint rows, uint exchange) {
    const uint thread = local_id(), lane = thread & 31;
    const uint row = work_group() * 4 + thread / 32;
    if (row >= rows) return;

    const uint blocks = width / 128;
    const uint shift = (lane & 3u) * 2u;
    const __global uchar *q = matrix + (ulong)row * blocks * 34;
    const __global float *x = input + lane;
    float sum = 0.0f;
    for (uint block = 0; block < blocks; ++block, q += 34, x += 128) {
        float scale;
        const ushort bits = *(__global const ushort *)q;
        __asm__("cvt.f32.f16 %0, %1;" : "=f"(scale) : "h"(bits));
        uint codes[4];
        if (exchange) {
            const uint packed = q[2 + lane];
            for (uint j = 0; j < 4; ++j) {
                const uint peer = lane / 4 + 8*j;
                __asm__("shfl.sync.idx.b32 %0, %1, %2, 31, -1;"
                        : "=r"(codes[j]) : "r"(packed), "r"(peer));
            }
        } else {
            const __global uchar *packed = q + 2 + lane / 4;
            codes[0] = packed[0];
            codes[1] = packed[8];
            codes[2] = packed[16];
            codes[3] = packed[24];
        }
        sum = bonsai_ordered_fma(codes[0], shift, scale, x[0], sum);
        sum = bonsai_ordered_fma(codes[1], shift, scale, x[32], sum);
        sum = bonsai_ordered_fma(codes[2], shift, scale, x[64], sum);
        sum = bonsai_ordered_fma(codes[3], shift, scale, x[96], sum);
    }
    for (int bit = 16; bit > 0; bit /= 2) {
        float peer;
        __asm__("shfl.sync.bfly.b32 %0, %1, %2, 31, -1;" : "=f"(peer) : "f"(sum), "r"(bit));
        sum += peer;
    }
    if (lane == 0) output[row] = sum;
}

__kernel void bonsai_pq2_matvec_ordered(__global const uchar *matrix, __global const float *input,
                                       __global float *output, uint width, uint rows) {
    bonsai_ordered_matvec(matrix, input, output, width, rows, 0);
}

__kernel void bonsai_pq2_matvec_exchange(__global const uchar *matrix, __global const float *input,
                                        __global float *output, uint width, uint rows) {
    bonsai_ordered_matvec(matrix, input, output, width, rows, 1);
}
