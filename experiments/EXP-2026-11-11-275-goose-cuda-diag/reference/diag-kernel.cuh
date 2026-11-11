// Verbatim copy of the diag kernel from the read-only source tree, because it is
// defined inside diag.cu (not a header). Source:
// /home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml/src/ggml-cuda/diag.cu
// sha256 9d683fe8ba8e1fec233ebb7f6a4a9ed06979cb944a046277265aebeb6d795299
// Only the kernel name changed (diag_kernel -> diag_kernel_ref); the body is
// unedited.
template <typename T>
static __global__ void diag_kernel_ref(T * __restrict__ dst,
                                       const T * __restrict__ src,
                                       const int64_t ne0,
                                       const int64_t ne1,
                                       const int64_t ne2,
                                       const int64_t ne3,
                                       const int64_t total_elements) {
    const int64_t global_idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (global_idx >= total_elements) {
        return;
    }

    const int64_t i0 = global_idx % ne0;
    const int64_t i1 = (global_idx / ne0) % ne1;
    const int64_t i2 = (global_idx / (ne0 * ne1)) % ne2;
    const int64_t i3 = global_idx / (ne0 * ne1 * ne2);

    const int64_t dst_idx = ((i3 * ne2 + i2) * ne1 + i1) * ne0 + i0;

    if (i0 == i1) {
        const int64_t batch_idx = i3 * ne2 + i2;
        const int64_t src_idx   = batch_idx * ne0 + i0;
        dst[dst_idx]            = src[src_idx];
    } else {
        dst[dst_idx] = ggml_cuda_cast<T>(0);
    }
    GGML_UNUSED_VARS(ne3);
}
