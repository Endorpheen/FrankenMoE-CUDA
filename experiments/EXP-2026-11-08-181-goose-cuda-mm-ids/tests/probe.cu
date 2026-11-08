// Isolates the two primitives the MM IDs helper depends on, so their behaviour can
// be compared with CUDA semantics outside the kernel:
//   shfl_up_sync(0xFFFFFFFF, v, off, 32)  for off in {8,16,24}
//   the 8-wide xor ladder used as warp_reduce_any<8>
// Prints per-lane values for a known input pattern.
#include <cstdio>
#include <cuda_runtime.h>

static __global__ void probe(int * out_any, int * out_up) {
    const int lane = threadIdx.x;
    // Pattern: lanes 3 and 12 "used", all others zero.
    const int used = (lane == 3 || lane == 12) ? 1 : 0;
    // 8-wide xor ladder, exactly as the kernel does it.
    int a = used;
    a = __shfl_xor_sync(0xFFFFFFFF, a, 4, 8);
    a = a | a;
    a = __shfl_xor_sync(0xFFFFFFFF, a, 2, 8);
    a = a | a;
    a = __shfl_xor_sync(0xFFFFFFFF, a, 1, 8);
    a = a | a;
    out_any[lane] = a;
    // shfl_up at three offsets over the whole warp.
    out_up[lane*3+0] = __shfl_up_sync(0xFFFFFFFF, a, 8, 32);
    out_up[lane*3+1] = __shfl_up_sync(0xFFFFFFFF, a, 16, 32);
    out_up[lane*3+2] = __shfl_up_sync(0xFFFFFFFF, a, 24, 32);
}

int main() {
    int * d_any = nullptr, * d_up = nullptr;
    int h_any[32], h_up[32*3];
    cudaMalloc(&d_any, sizeof(h_any)); cudaMalloc(&d_up, sizeof(h_up));
    probe<<<1,32>>>(d_any, d_up);
    if (cudaDeviceSynchronize() != cudaSuccess) { std::fprintf(stderr, "launch failed\n"); return 1; }
    cudaMemcpy(h_any, d_any, sizeof(h_any), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_up, d_up, sizeof(h_up), cudaMemcpyDeviceToHost);
    std::printf("any8:"); for (int i = 0; i < 32; ++i) std::printf(" %d", h_any[i]); std::printf("\n");
    std::printf("shfl_up off=8 :"); for (int i = 0; i < 32; ++i) std::printf(" %d", h_up[i*3+0]); std::printf("\n");
    std::printf("shfl_up off=16:"); for (int i = 0; i < 32; ++i) std::printf(" %d", h_up[i*3+1]); std::printf("\n");
    std::printf("shfl_up off=24:"); for (int i = 0; i < 32; ++i) std::printf(" %d", h_up[i*3+2]); std::printf("\n");
    cudaFree(d_any); cudaFree(d_up);
    return 0;
}
