#include "scale.cuh"
#include "/home/end0/AI/LlamaServer/franken-goose-live-20261107/generated/goose_leaves.cuh"

#if FG_GOOSE_TRACE
#include <atomic>
#include <thread>
#include <chrono>
#include <stdio.h>
static __device__ unsigned long long fg_scale_device_calls;
static std::atomic<bool> fg_scale_sampling{true};
static std::thread *fg_scale_thread = nullptr;
__attribute__((constructor)) static void fg_scale_start() {
 fg_scale_thread = new std::thread([] {
  while(fg_scale_sampling.load()) {
   std::this_thread::sleep_for(std::chrono::milliseconds(250));
   if(!fg_scale_sampling.load()) break;
   FILE *f=fopen("/home/end0/AI/LlamaServer/franken-goose-live-20261107/results/read-scale-counter.request","r");
   if(!f) continue;
   fclose(f);remove("/home/end0/AI/LlamaServer/franken-goose-live-20261107/results/read-scale-counter.request");
   unsigned long long n=0;cudaError_t e=cudaMemcpyFromSymbol(&n,fg_scale_device_calls,sizeof(n));
   fprintf(stderr,"FG_SELFHOST_scale_DEVICE_KERNELS=%llu CUDA_STATUS=%d\n",n,(int)e);fflush(stderr);
  }
 });
}
__attribute__((destructor)) static void fg_scale_stop() {fg_scale_sampling.store(false);if(fg_scale_thread){fg_scale_thread->join();delete fg_scale_thread;}}
#endif
static __device__ __forceinline__ float fg_scale_witness(float x, float scale, float bias) {
#if FG_GOOSE_TRACE
 if(threadIdx.x==0&&blockIdx.x==0&&blockIdx.y==0&&blockIdx.z==0)atomicAdd(&fg_scale_device_calls,1ULL);
#endif
 return gg_function_3(x, scale, bias, 0);
}

#include "convert.cuh"

#define MAX_GRIDDIM_X 0x7FFFFFFF

template <typename T>
static __global__ void scale_f32(const T * x, T * dst, const float scale, const float bias, const int64_t nelements) {
    ggml_cuda_pdl_lc();
    int64_t tid = (int64_t)blockIdx.x * (int64_t)blockDim.x + (int64_t)threadIdx.x;
    int64_t stride = (int64_t)blockDim.x * (int64_t)gridDim.x;

    ggml_cuda_pdl_sync();
    for (int64_t i = tid; i < nelements; i += stride) {
        dst[i] = ggml_cuda_cast<T>(fg_scale_witness(ggml_cuda_cast<float>(x[i]), scale, bias));
    }
}

template <typename T>
static void scale_f32_cuda(const T * x, T * dst, const float scale, const float bias, const int64_t nelements, cudaStream_t stream) {
    const int64_t num_blocks = (nelements + CUDA_SCALE_BLOCK_SIZE - 1) / CUDA_SCALE_BLOCK_SIZE;
    const ggml_cuda_kernel_launch_params launch_params = ggml_cuda_kernel_launch_params(MIN(MAX_GRIDDIM_X, num_blocks), CUDA_SCALE_BLOCK_SIZE, 0, stream);
    ggml_cuda_kernel_launch(scale_f32<T>, launch_params, x, dst, scale, bias, nelements);
}

void ggml_cuda_op_scale(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {
    const ggml_tensor * src0 = dst->src[0];
    cudaStream_t stream = ctx.stream();

    GGML_ASSERT(src0->type == GGML_TYPE_F32 || src0->type == GGML_TYPE_BF16);
    GGML_ASSERT(dst->type == src0->type);

    float scale;
    float bias;
    memcpy(&scale, (float *) dst->op_params + 0, sizeof(float));
    memcpy(&bias,  (float *) dst->op_params + 1, sizeof(float));

    if (src0->type == GGML_TYPE_BF16) {
        scale_f32_cuda((const nv_bfloat16 *) src0->data, (nv_bfloat16 *) dst->data, scale, bias, ggml_nelements(src0), stream);
    } else {
        scale_f32_cuda((const float *) src0->data, (float *) dst->data, scale, bias, ggml_nelements(src0), stream);
    }
}
