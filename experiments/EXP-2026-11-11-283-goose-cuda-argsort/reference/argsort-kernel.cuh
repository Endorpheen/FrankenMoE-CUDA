// Verbatim copy of ggml_cuda_swap, bitonic_step and k_argsort_f32_i32 from the
// read-only source tree. Source:
// /home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml/src/ggml-cuda/argsort.cu
// sha256 ebf2448dc3b259745d048ea440bab1afc7bcd81b88e498149c292c09eca19e3c
// Only the kernel name changed (k_argsort_f32_i32 -> k_argsort_f32_i32_ref); the
// bodies are unedited. Lines 161-228 of the source.

// Bitonic sort implementation
template<typename T>
static inline __device__ void ggml_cuda_swap(T & a, T & b) {
    T tmp = a;
    a = b;
    b = tmp;
}

// One compare-exchange of the bitonic network at (k, j) for column col.
template<ggml_sort_order order>
static inline __device__ void bitonic_step(const float * x_row, int * dst_row, const int ncols, const int col, const int k, const int j) {
    const int ixj = col ^ j;
    if (ixj <= col) {
        return;
    }
    if ((col & k) == 0) {
        if (dst_row[col] >= ncols ||
            (dst_row[ixj] < ncols && (order == GGML_SORT_ORDER_ASC ?
                x_row[dst_row[col]] > x_row[dst_row[ixj]] :
                x_row[dst_row[col]] < x_row[dst_row[ixj]]))
        ) {
            ggml_cuda_swap(dst_row[col], dst_row[ixj]);
        }
    } else {
        if (dst_row[ixj] >= ncols ||
            (dst_row[col] < ncols && (order == GGML_SORT_ORDER_ASC ?
                x_row[dst_row[col]] < x_row[dst_row[ixj]] :
                x_row[dst_row[col]] > x_row[dst_row[ixj]]))
        ) {
            ggml_cuda_swap(dst_row[col], dst_row[ixj]);
        }
    }
}

// Bitonic sort of one row per block. Each thread owns the columns
// threadIdx.x + i * blockDim.x, so rows wider than the block (up to the
// shared memory limit) sort with several columns per thread. Every
// (k, j) stage runs all owned columns before the barrier; a pair
// (col, col ^ j) is exchanged by the owner of its lower index only.
template<ggml_sort_order order>
static __global__ void k_argsort_f32_i32_ref(const float * x, int * dst, const int ncols, int ncols_pad) {
    const int row = blockIdx.x;

    const float * x_row = x + row * ncols;
    extern __shared__ int dst_row[];

    // initialize indices
    for (int col = threadIdx.x; col < ncols_pad; col += blockDim.x) {
        dst_row[col] = col;
    }

    __syncthreads();

    for (int k = 2; k <= ncols_pad; k *= 2) {
        for (int j = k / 2; j > 0; j /= 2) {
            for (int col = threadIdx.x; col < ncols_pad; col += blockDim.x) {
                bitonic_step<order>(x_row, dst_row, ncols, col, k, j);
            }
            __syncthreads();
        }
    }

    // copy the result to dst without the padding
    for (int col = threadIdx.x; col < ncols; col += blockDim.x) {
        dst[row * ncols + col] = dst_row[col];
    }
}
