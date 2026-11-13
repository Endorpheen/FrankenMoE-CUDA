// Verbatim copy of the timestep embedding kernel from the read-only source tree,
// because it is defined inside tsembd.cu (not a header). Source:
// /home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml/src/ggml-cuda/tsembd.cu
// sha256 54109ff13fb88f2f2bfd3bb0d91a11833f347a3a2e129963cf103d41108abe82
// Only the kernel name changed (timestep_embedding_f32 ->
// timestep_embedding_f32_ref); the body is unedited.
static __global__ void timestep_embedding_f32_ref(const float * timesteps, float * dst, const int nb1, const int dim, const int max_period) {
    // blockIDx.y: idx of timesteps->ne[0]
    // blockIDx.x: idx of ((dim + 1) / 2) / BLOCK_SIZE
    int i = blockIdx.y;
    int j = threadIdx.x + blockIdx.x * blockDim.x;
    float * embed_data = (float *)((char *)dst +  i*nb1);

    int half = dim / 2;
    if (dim % 2 != 0 && j == half) {
        embed_data[2 * half] = 0.f;
    }

    if (j >= half) {
        return;
    }

    float timestep = timesteps[i];
    float freq = (float)expf(-logf(max_period) * j / half);
    float arg = timestep * freq;
    embed_data[j] = cosf(arg);
    embed_data[j + half] = sinf(arg);
}
