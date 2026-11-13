// Verbatim copy of the CUDA count_equal kernel from the read-only source tree,
// because it is defined in count-equal.cu (not a header). Source:
// /home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml/src/ggml-cuda/count-equal.cu
// sha256 5f180b3022437f78a80f524d1eabf7ae94cbf060e220a645d5747db451b2f7f8
// The body below is copied without edits; only this comment was added.
template <typename T>
static __global__ void count_equal_ref(const T * __restrict__ x, const T * __restrict__ y, int64_t * __restrict__ dst, const int64_t dk, const int64_t k) {
    const int64_t i0 = (int64_t) blockIdx.x*dk;
    const int64_t i1 = min(i0 + dk, k);

    int nequal = 0;

    for (int64_t i = i0 + threadIdx.x; i < i1; i += WARP_SIZE) {
        const T xi = x[i];
        const T yi = y[i];
        nequal += xi == yi;
    }

    nequal = warp_reduce_sum(nequal);

    if (threadIdx.x != 0) {
        return;
    }

    atomicAdd((int *) dst, nequal);
}
