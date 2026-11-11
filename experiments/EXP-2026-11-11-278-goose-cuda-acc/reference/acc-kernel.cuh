// Verbatim copy of the acc kernel from the read-only source tree, because it is
// defined inside acc.cu (not a header). Source:
// /home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml/src/ggml-cuda/acc.cu
// sha256 9463f70b9edc9749d3f06331640120d99fd75af072c46c5fd1ee40d6148ffd3d
// Only the kernel name changed (acc_f32 -> acc_f32_ref); the body is unedited.
static __global__ void acc_f32_ref(const float * x, const float * y, float * dst, const int64_t ne,
        const int64_t ne10, const int64_t ne11, const int64_t ne12, const int64_t ne13,
        const int64_t s11, const int64_t s12, const int64_t s13, const int64_t offset) {
    const int64_t i = blockDim.x * blockIdx.x + threadIdx.x;

    if (i >= ne) {
        return;
    }

    int64_t src1_idx = i - offset;

    int64_t tmp = src1_idx;
    const int64_t i13 = tmp / s13;
    tmp -= i13 * s13;
    const int64_t i12 = tmp / s12;
    tmp -= i12 * s12;
    const int64_t i11 = tmp / s11;
    tmp -= i11 * s11;
    const int64_t i10 = tmp;

    float val = x[i];
    if (src1_idx >= 0 && i10 < ne10 && i11 < ne11 && i12 < ne12 && i13 < ne13) {
        val += y[((i13*ne12 + i12) * ne11 + i11) * ne10 + i10];
    }
    dst[i] = val;
}
