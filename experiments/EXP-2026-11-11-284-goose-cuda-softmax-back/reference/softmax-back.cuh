// Verbatim copy of soft_max_back_f32 from the read-only source tree. Source:
// /home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml/src/ggml-cuda/softmax.cu
// sha256 ac288d3d091ceab46e56b05e298b893f37f6129a7e3b36703c3244675d384976
// Only the kernel name changed (soft_max_back_f32 -> soft_max_back_f32_ref); the
// body is unedited. Lines 256-276 of the source.
static __global__ void soft_max_back_f32_ref(
        const float * grad, const float * dstf, float * dst, const int ncols, const float scale) {
    const int tid  = threadIdx.x;
    const int rowx = blockIdx.x;

    grad += int64_t(rowx)*ncols;
    dstf += int64_t(rowx)*ncols;
    dst  += int64_t(rowx)*ncols;

    float dgf_dot = 0.0f; // dot product of dst from forward pass and gradients

    for (int col = tid; col < ncols; col += WARP_SIZE) {
        dgf_dot += dstf[col]*grad[col];
    }

    dgf_dot = warp_reduce_sum(dgf_dot);

    for (int col = tid; col < ncols; col += WARP_SIZE) {
        dst[col] = scale * (grad[col] - dgf_dot) * dstf[col];
    }
}
