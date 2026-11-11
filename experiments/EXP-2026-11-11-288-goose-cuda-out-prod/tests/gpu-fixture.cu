// Differential fixture for the Goose out-prod pointer kernel.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <cstdint>
#include <vector>

static __device__ __forceinline__ void fg288d_st_u64(uint64_t p,uint64_t o,uint64_t v){ *(uint64_t*)((char*)(uintptr_t)p+o)=v; }
static __device__ __forceinline__ void fg288d_fail(int64_t){ asm("trap;"); }
#include "out-prod-kernel.cuh"
#include "device-functions-generated.cuh"
#include "entries.h"

static __global__ void goose_kernel(const float* s0,const float* s1,float* d,
        const float** pa,const float** pb,float** pc,
        int64_t ne2,int64_t ne3,int64_t dps2,int64_t dps3,
        int64_t s02,int64_t s03,int64_t s12,int64_t s13,int64_t s2,int64_t s3){
    FG288D_CB_kernel((uint64_t)(uintptr_t)s0,(uint64_t)(uintptr_t)s1,(uint64_t)(uintptr_t)d,
        (uint64_t)(uintptr_t)pa,(uint64_t)(uintptr_t)pb,(uint64_t)(uintptr_t)pc,
        ne2,ne3,dps2,dps3,s02,s03,s12,s13,s2,s3,
        (int64_t)blockIdx.x,(int64_t)blockIdx.y,(int64_t)blockDim.x,(int64_t)blockDim.y,
        (int64_t)threadIdx.x,(int64_t)threadIdx.y,0);
}

static int checks, total_fails, launch_fail;

struct Case { int64_t ne2, ne3, dps2, dps3; };

int main(void){
    int ndev=0; cudaDeviceProp props{};
    if (cudaGetDeviceCount(&ndev)!=cudaSuccess || ndev<1){ fprintf(stderr,"no CUDA device\n"); return 77; }
    cudaSetDevice(0); cudaGetDeviceProperties(&props,0);
    const Case cases[6]={{1,1,1,1},{8,1,1,1},{8,1,4,1},{1,5,1,1},{37,19,3,2},{64,7,1,7}};
    for (int c=0;c<6;++c){
        const Case& k=cases[c];
        const int64_t s02=7,s03=11,s12=5,s13=13,s2=3,s3=17;
        const size_t src0n=(size_t)(k.ne2*k.ne3*4+4096);
        const size_t src1n=(size_t)(k.ne2+k.ne3+4096);
        const size_t dstn=(size_t)(k.ne2*k.ne3+4096);
        float *s0=nullptr,*s1=nullptr,*d=nullptr;
        const float **pa=nullptr,**pb=nullptr; float **pc=nullptr;
        const float **qa=nullptr,**qb=nullptr; float **qc=nullptr;
        cudaMalloc(&s0,src0n*4);cudaMalloc(&s1,src1n*4);cudaMalloc(&d,dstn*4);
        cudaMalloc(&pa,k.ne2*k.ne3*8);cudaMalloc(&pb,k.ne2*k.ne3*8);cudaMalloc(&pc,k.ne2*k.ne3*8);
        cudaMalloc(&qa,k.ne2*k.ne3*8);cudaMalloc(&qb,k.ne2*k.ne3*8);cudaMalloc(&qc,k.ne2*k.ne3*8);
        cudaMemset(pa,0,k.ne2*k.ne3*8);cudaMemset(pb,0,k.ne2*k.ne3*8);cudaMemset(pc,0,k.ne2*k.ne3*8);
        cudaMemset(qa,0,k.ne2*k.ne3*8);cudaMemset(qb,0,k.ne2*k.ne3*8);cudaMemset(qc,0,k.ne2*k.ne3*8);
        dim3 block(16,16), grid((unsigned)((k.ne2+15)/16),(unsigned)((k.ne3+15)/16));
        k_compute_out_prod_ptrs_ref<<<grid,block>>>(s0,s1,d,pa,pb,pc,k.ne2,k.ne3,k.dps2,k.dps3,
            (size_t)s02,(size_t)s03,(size_t)s12,(size_t)s13,(size_t)s2,(size_t)s3);
        goose_kernel<<<grid,block>>>(s0,s1,d,qa,qb,qc,k.ne2,k.ne3,k.dps2,k.dps3,s02,s03,s12,s13,s2,s3);
        cudaError_t e=cudaDeviceSynchronize();
        if (e!=cudaSuccess){ printf("LAUNCH FAIL c=%d: %s\n",c,cudaGetErrorString(e)); ++launch_fail; }
        std::vector<uint64_t> a(k.ne2*k.ne3),b(k.ne2*k.ne3);
        for (int arr=0;arr<3;++arr){
            const void* src = arr==0?(const void*)pa:arr==1?(const void*)pb:(const void*)pc;
            const void* dst = arr==0?(const void*)qa:arr==1?(const void*)qb:(const void*)qc;
            cudaMemcpy(a.data(),src,a.size()*8,cudaMemcpyDeviceToHost);
            cudaMemcpy(b.data(),dst,b.size()*8,cudaMemcpyDeviceToHost);
            for (size_t i=0;i<a.size();++i){ ++checks; if (a[i]!=b[i]){ ++total_fails; if(total_fails<4) printf("FAIL c=%d arr=%d i=%zu nat=%llx goo=%llx\n",c,arr,i,(unsigned long long)a[i],(unsigned long long)b[i]); } }
        }
        cudaFree(s0);cudaFree(s1);cudaFree(d);
        cudaFree(pa);cudaFree(pb);cudaFree(pc);cudaFree(qa);cudaFree(qb);cudaFree(qc);
    }
    printf("FG288D_GPU_FIXTURE pointers=%d failures=%d launch_fail=%d\n",checks,total_fails,launch_fail);
    return (total_fails||launch_fail)?1:0;
}
