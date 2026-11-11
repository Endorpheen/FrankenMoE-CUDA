// Verbatim copy of the softmax params struct and the cooperative kernels from the
// read-only source tree. Source:
// /home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml/src/ggml-cuda/softmax.cu
// sha256 ac288d3d091ceab46e56b05e298b893f37f6129a7e3b36703c3244675d384976
// Only the wrapper name changed (soft_max_f32_parallelize_cols -> ..._ref); the
// bodies are unedited. Lines 25-46, 146-250 and 308-325 of the source.
struct soft_max_params {

    int64_t nheads;
    uint32_t n_head_log2;
    int64_t ncols;
    int64_t nrows_x;
    int64_t nrows_y;
    int64_t ne00;
    int64_t ne01;
    int64_t ne02;
    int64_t ne03;
    int64_t nb11;
    int64_t nb12;
    int64_t nb13;

    int64_t ne12;
    int64_t ne13;
    float scale;
    float max_bias;
    float m0;
    float m1;
};

static __device__ void soft_max_f32_parallelize_cols_single_row(const float * __restrict__ x,
                                                                float * __restrict__ dst,
                                                                float * __restrict__ tmp_maxs,
                                                                float * __restrict__ tmp_sums,
                                                                float * shared_vals_max,
                                                                float * shared_vals_sum,
                                                                const soft_max_params p) {
    namespace cg = cooperative_groups;

    const cg::grid_group g = cg::this_grid();

    const int tid               = threadIdx.x;
    const int col_start         = blockIdx.x * blockDim.x + tid;
    const int n_elem_per_thread = 4;

    float     local_vals[n_elem_per_thread] = { -INFINITY, -INFINITY, -INFINITY, -INFINITY };
    float     local_max                     = -INFINITY;
    const int step_size                     = gridDim.x * blockDim.x;

    // Compute thread-local max
    for (int col = col_start; col < p.ncols;) {
#pragma unroll
        for (int i = 0; i < n_elem_per_thread; i++) {
            const int idx = col + i * step_size;
            local_vals[i] = idx < p.ncols ? x[idx] : -INFINITY;
        }
#pragma unroll
        for (int i = 0; i < n_elem_per_thread; i++) {
            local_max = fmaxf(local_max, local_vals[i]);
        }
        col += step_size * n_elem_per_thread;
    }

    // Compute CTA-level max
    local_max = block_reduce<block_reduce_method::MAX>(local_max, shared_vals_max);

    // Store CTA-level max to GMEM
    if (tid == 0) {
        tmp_maxs[blockIdx.x] = local_max;
    }
    g.sync();

    // Compute compute global max from CTA-level maxs
    assert(gridDim.x < blockDim.x);  // currently we only support this case
    if (tid < gridDim.x) {
        local_max = tmp_maxs[tid];
    } else {
        local_max = -INFINITY;
    }
    local_max = block_reduce<block_reduce_method::MAX>(local_max, shared_vals_max);

    // Compute softmax dividends, accumulate divisor
    float tmp_expf = 0.0f;
    for (int col = col_start; col < p.ncols;) {
#pragma unroll
        for (int i = 0; i < n_elem_per_thread; i++) {
            const int idx = col + i * step_size;
            local_vals[i] = idx < p.ncols ? x[idx] : -INFINITY;
        }
#pragma unroll
        for (int i = 0; i < n_elem_per_thread; i++) {
            const int idx = col + i * step_size;
            if (idx < p.ncols) {
                const float tmp = expf(local_vals[i] - local_max);
                tmp_expf += tmp;
                dst[idx] = tmp;
            }
        }
        col += step_size * n_elem_per_thread;
    }

    // Reduce divisor within CTA
    tmp_expf = block_reduce<block_reduce_method::SUM>(tmp_expf, shared_vals_sum);

    // Store CTA-level sum to GMEM
    if (tid == 0) {
        tmp_sums[blockIdx.x] = tmp_expf;
    }
    g.sync();

    // Compute global sum from CTA-level sums
    if (tid < gridDim.x) {
        tmp_expf = tmp_sums[tid];
    } else {
        tmp_expf = 0.0f;
    }
    tmp_expf = block_reduce<block_reduce_method::SUM>(tmp_expf, shared_vals_sum);

    // Divide dividend by global sum + store data
    for (int col = col_start; col < p.ncols;) {
#pragma unroll
        for (int i = 0; i < n_elem_per_thread; i++) {
            const int idx = col + i * step_size;
            local_vals[i] = idx < p.ncols ? dst[idx] : -INFINITY;
        }
#pragma unroll
        for (int i = 0; i < n_elem_per_thread; i++) {
            const int idx = col + i * step_size;
            if (idx < p.ncols) {
                dst[idx] = local_vals[i] / tmp_expf;
            }
        }
        col += step_size * n_elem_per_thread;
    }
}

__launch_bounds__(8*WARP_SIZE, 1) static __global__ void soft_max_f32_parallelize_cols_ref(const float * __restrict__ x,
                                                     float * __restrict__ dst,
                                                     float * __restrict__ tmp_maxs,
                                                     float * __restrict__ tmp_sums,
                                                     const soft_max_params p)
// We loop over all instead of parallelizing across gridDim.y as cooperative groups
// currently only support synchronizing the complete grid if not launched as a cluster group
// (which requires CC > 9.0)
// https://docs.nvidia.com/cuda/cuda-programming-guide/05-appendices/device-callable-apis.html#grid-synchronization
// https://docs.nvidia.com/cuda/cuda-programming-guide/05-appendices/device-callable-apis.html#class-cluster-group
{
    __shared__ float shared_vals[2][32];

    for (int rowx = 0; rowx < p.ne01 * p.ne02 * p.ne03; rowx++) {
        soft_max_f32_parallelize_cols_single_row(x + int64_t(rowx) * p.ncols, dst + int64_t(rowx) * p.ncols, tmp_maxs,
                                                 tmp_sums, shared_vals[0], shared_vals[1], p);
    }
}
