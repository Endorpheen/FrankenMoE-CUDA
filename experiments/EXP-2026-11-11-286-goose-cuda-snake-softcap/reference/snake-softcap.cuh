// Verbatim copy of snake_kernel and softcap_f32 from the read-only source tree.
// Sources:
//   .../ggml/src/ggml-cuda/snake.cu  (snake_kernel, lines 8-26)
//   .../ggml/src/ggml-cuda/softcap.cu (softcap_f32, lines 3-13)
// Only the kernel names changed (_ref suffix); the bodies are unedited.
template <typename T>
static __global__ void snake_kernel_ref(
        const T     * __restrict__ x,
        const float * __restrict__ a,
        const float * __restrict__ inv_b,
        T           * __restrict__ dst,
        const int    total,
        const uint3  T_len_fastdiv) {
    const int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= total) return;

    const int c = (int) fastdiv((uint32_t) idx, T_len_fastdiv);

    const float xi = ggml_cuda_cast<float>(x[idx]);
    const float s  = sinf(a[c] * xi);
    dst[idx] = ggml_cuda_cast<T>(xi + s * s * inv_b[c]);
}

// Internal launcher with explicit x/a/inv_b/dst tensors.

static __global__ void softcap_f32_ref(const float * x, float * dst, const float scale, const float softcap, const int k) {
    ggml_cuda_pdl_lc();
    const int i = blockDim.x*blockIdx.x + threadIdx.x;

    if (i >= k) {
        return;
    }

    ggml_cuda_pdl_sync();
    dst[i] = tanhf(scale * x[i]) * softcap;
}
