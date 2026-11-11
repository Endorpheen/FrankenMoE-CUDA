// Verbatim copy of k_compute_out_prod_ptrs from the read-only source tree, because
// it is defined inside out-prod.cu (not a header). Source:
// /home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml/src/ggml-cuda/out-prod.cu
// sha256 c6fe3a117f327eb880f27764bae2865dd3b544cd477efed815f91116c98abb94
// Only the kernel name changed (k_compute_out_prod_ptrs -> k_compute_out_prod_ptrs_ref).
// Lines 5-25 of the source, closing brace included.
static __global__ void k_compute_out_prod_ptrs_ref(
        const float * src0_d, const float * src1_d, float * dst_d,
        const float ** ptrs_a, const float ** ptrs_b, float ** ptrs_c,
        const int64_t ne2, const int64_t ne3,
        const int64_t dps2, const int64_t dps3,
        const size_t s02, const size_t s03,
        const size_t s12, const size_t s13,
        const size_t s2,  const size_t s3) {
    const int64_t i2 = blockIdx.x*blockDim.x + threadIdx.x;
    const int64_t i3 = blockIdx.y*blockDim.y + threadIdx.y;

    if (i2 >= ne2 || i3 >= ne3) {
        return;
    }

    const int64_t idx = i3*ne2 + i2;

    ptrs_a[idx] = src0_d + (i3/dps3)*s03 + (i2/dps2)*s02;
    ptrs_b[idx] = src1_d +  i3      *s13 +  i2      *s12;
    ptrs_c[idx] = dst_d  +  i3      *s3  +  i2      *s2;
}
