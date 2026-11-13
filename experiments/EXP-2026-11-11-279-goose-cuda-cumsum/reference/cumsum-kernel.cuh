// Verbatim copy of the cumsum fallback kernel from the read-only source tree.
// Source: /home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml/src/ggml-cuda/cumsum.cu
// sha256 75e37c9febac23c6fb103ac18914a154bfe34cbaadb862b177f46f63441b0d1e
// Only the kernel name changed (cumsum_kernel -> cumsum_kernel_ref); the body is
// unedited.
template<typename T>
static __global__ void cumsum_kernel_ref(
        const T * src, T * dst,
        const int64_t ne00, const int64_t ne01, const int64_t ne02, const int64_t ne03,
        const int64_t  s00, const int64_t  s01, const int64_t  s02, const int64_t  s03,
        const int64_t   s0, const int64_t   s1, const int64_t   s2, const int64_t   s3) {

    GGML_UNUSED_VARS(s00, s0);

    const int tid = threadIdx.x;
    constexpr int warp_size = ggml_cuda_get_physical_warp_size();
    const int lane = tid % warp_size;
    const int warp = tid / warp_size;
    const int warps_per_block = blockDim.x / warp_size;

    extern __shared__ float smem[];
    float *                 s_vals        = smem;
    float *                 s_warp_sums   = smem + blockDim.x;
    float *                 s_carry       = smem + blockDim.x + warps_per_block;
    float *                 s_chunk_total = s_carry + 1;

    // Initialize carry
    if (tid == 0) {
        *s_carry = 0.0f;
    }
    __syncthreads();

    const int64_t i3 = blockIdx.z;
    const int64_t i2 = blockIdx.y;
    const int64_t i1 = blockIdx.x;
    if (i3 >= ne03 || i2 >= ne02 || i1 >= ne01) {
        return;
    }

    const T * src_row = src + i1 * s01 + i2 * s02 + i3 * s03;
    T       * dst_row = dst + i1 * s1  + i2 * s2  + i3 * s3;

    // register blocking: process 4 elements per thread to hide latency
    // and reduce synchronization overhead
    constexpr int num_unroll = 4;
    T             temp[num_unroll];

    for (int64_t i = 0; i < ne00; i += num_unroll * blockDim.x) {
        int64_t idx = i + tid * num_unroll;

        // thread local sequential scan
        temp[0] = (idx < ne00 ? src_row[idx] : T(0));
#pragma unroll
        for (int64_t j = 1; j < num_unroll; j++) {
            temp[j] = temp[j - 1];
            if (idx + j < ne00) {
                temp[j] += src_row[idx + j];
            } else {
                temp[j] += 0;
            }
        }

        // last emenent is sum of all values assigned to thread
        float val = (idx < ne00) ? ggml_cuda_cast<float, T>(temp[num_unroll - 1]) : 0.0f;

        // Warp inclusive scan
        val = warp_prefix_inclusive_sum<T, warp_size>(val);
        s_vals[tid] = val;

        if (lane == warp_size - 1) {
            s_warp_sums[warp] = val;
        }
        __syncthreads();

        // Exclusive scan of warp sums (warp 0 only)
        if (warp == 0) {
            float w = (tid < warps_per_block) ? s_warp_sums[tid] : 0.0f;
            float inc = warp_prefix_inclusive_sum<T, warp_size>(w);
            if (tid < warps_per_block) {
                s_warp_sums[tid] = inc - w;   // exclusive sum
            }
            if (tid == warps_per_block - 1) {
                *s_chunk_total = inc;          // total sum of this chunk
            }
        }
        __syncthreads();

        // write back results
        float carry = *s_carry;
        // calculate sum offset for this thread
        float final_val_offset = s_vals[tid] + s_warp_sums[warp] + carry - temp[num_unroll - 1];

#pragma unroll
        for (int32_t j = 0; j < num_unroll; j++) {
            if (idx + j < ne00) {
                dst_row[idx + j] = temp[j] + ggml_cuda_cast<T, float>(final_val_offset);
            }
        }

        __syncthreads();

        // Update carry for next chunk
        if (tid == 0) {
            *s_carry += *s_chunk_total;
        }
    }
}
