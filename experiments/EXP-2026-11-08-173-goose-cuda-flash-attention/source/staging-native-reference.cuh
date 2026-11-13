#pragma once
#include "dequantize.cuh"
template <int qk, int qr, dequantize_kernel_t dequantize_kernel, typename dst_t>
static __global__ void dequantize_block(const void * __restrict__ vx, dst_t * __restrict__ y,
        const int64_t ne00, const int64_t ne01,
        const int64_t ne0203, const uint3 ne02,
        const int64_t s01, const int64_t s02, const int64_t s03) {
    const int64_t i00 = 2 * (int64_t(blockDim.x)*blockIdx.x + threadIdx.x);

    if (i00 >= ne00) {
        return;
    }

    for (int64_t i01 = blockIdx.y; i01 < ne01; i01 += gridDim.y) {
        for (int64_t i0203 = blockIdx.z; i0203 < ne0203; i0203 += gridDim.z) {
            const uint2 dm = fast_div_modulo((uint32_t)i0203, ne02);
            const int64_t i02 = dm.y;
            const int64_t i03 = dm.x;

            const int64_t ibx0 = i03*s03 + i02*s02 + i01*s01;

            const int64_t ib = ibx0 + i00/qk; // block index
            const int64_t iqs = (i00%qk)/qr; // quant index
            const int64_t iybs = i00 - i00%qk; // y block start index
            const int64_t y_offset = qr == 1 ? 1 : qk/2;

            // dequantize
            float2 v;
            dequantize_kernel(vx, ib, iqs, v);

            const int64_t iy0 = (i0203*ne01 + i01)*ne00 + iybs + iqs;
            y[iy0 + 0]        = ggml_cuda_cast<dst_t>(v.x);
            y[iy0 + y_offset] = ggml_cuda_cast<dst_t>(v.y);
        }
    }
}
template<typename dst_t>
static __global__ void dequantize_block_q4_0(const void * __restrict__ vx, dst_t * __restrict__ yy, int nb32) {

    const int64_t i = blockIdx.x;

    // assume 32 threads
    const int64_t tid = threadIdx.x;
    const int64_t il  = tid/8;
    const int64_t ir  = tid%8;
    const int64_t ib = 8*i + ir;
    if (ib >= nb32) {
        return;
    }

    dst_t * y = yy + 256*i + 32*ir + 4*il;

    const block_q4_0 * x = (const block_q4_0 *)vx + ib;
    const float d = __half2float(x->d);
    const float dm = -8*d;

    const uint8_t * q = x->qs + 4*il;

    for (int l = 0; l < 4; ++l) {
        y[l+ 0] = ggml_cuda_cast<dst_t>(d * (q[l] & 0xF) + dm);
        y[l+16] = ggml_cuda_cast<dst_t>(d * (q[l] >>  4) + dm);
    }
}
