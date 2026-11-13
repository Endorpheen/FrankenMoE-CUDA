// Verbatim copy of the arange kernel from the read-only source tree, because it
// is defined inside arange.cu (not a header). Source:
// /home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml/src/ggml-cuda/arange.cu
// sha256 e9bef1d5940009034d8bc8ca4185dd204191c20d1a0682a9bdccf4f953c57a5e
// Only the kernel name changed (arange_f32 -> arange_f32_ref) so the fixture can
// call it next to the Goose implementation; the body is unedited.
static __global__ void arange_f32_ref(float * dst, const int ne0, const float start, const float step) {
    // blockIDx.x: idx of ne0 / BLOCK_SIZE
    int nidx = threadIdx.x + blockIdx.x * blockDim.x;
    if (nidx >= ne0) {
        return;
    }
    dst[nidx] = start + step * nidx;
}
