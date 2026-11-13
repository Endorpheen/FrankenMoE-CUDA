// Verbatim copy of the add_id kernel from the read-only source tree, because it
// is defined inside add-id.cu (not a header). Source:
// /home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml/src/ggml-cuda/add-id.cu
// sha256 f696adbcf54d0fda380fc56009c0d68009cacdbc0d226d1c27f999f3881b4840
// Only the kernel name changed (add_id_kernel -> add_id_kernel_ref); the body is
// unedited.
static __global__ void add_id_kernel_ref(
        const float * src0, const float * src1, const int32_t * src2, float * dst,
        int64_t ne0, int64_t ne1,
        size_t nb01, size_t nb02,
        size_t nb11,
        size_t nb21
    ) {

    const int64_t i1 = blockIdx.x;
    const int64_t i2 = blockIdx.y;

    const int i11 = *(const int32_t *) ((const char *) src2 + i1*sizeof(int32_t) + i2*nb21);

    const size_t nb1 = ne0 * sizeof(float);
    const size_t nb2 = ne1 * nb1;

    float * dst_row = (float *)((char *)dst + i1*nb1 + i2*nb2);
    const float * src0_row = (const float *)((const char *)src0 +  i1*nb01 + i2*nb02);
    const float * src1_row = (const float *)((const char *)src1 + i11*nb11);

    for (int64_t i0 = threadIdx.x; i0 < ne0; i0 += blockDim.x) {
        dst_row[i0] = src0_row[i0] + src1_row[i0];
    }
}
