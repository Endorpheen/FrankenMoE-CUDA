// Differential fixture for the Goose snake (F32/F16/BF16) and softcap.
#include "common.cuh"
#include "convert.cuh"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>

static __device__ __forceinline__ float fg286d_ld_f32(uint64_t p,uint64_t o){ return *(const float*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ float fg286d_ld_half(uint64_t p,uint64_t o){ return __half2float(*(const half*)((const char*)(uintptr_t)p+o)); }
static __device__ __forceinline__ float fg286d_ld_bf16(uint64_t p,uint64_t o){ return __bfloat162float(*(const nv_bfloat16*)((const char*)(uintptr_t)p+o)); }
static __device__ __forceinline__ void  fg286d_st_f32(uint64_t p,uint64_t o,float v){ *(float*)((char*)(uintptr_t)p+o) = v; }
static __device__ __forceinline__ void  fg286d_st_half(uint64_t p,uint64_t o,float v){ *(half*)((char*)(uintptr_t)p+o) = __float2half(v); }
static __device__ __forceinline__ void  fg286d_st_bf16(uint64_t p,uint64_t o,float v){ *(nv_bfloat16*)((char*)(uintptr_t)p+o) = __float2bfloat16(v); }
static __device__ __forceinline__ float fg286d_sinf(float x){ return sinf(x); }
static __device__ __forceinline__ float fg286d_tanhf(float x){ return tanhf(x); }
static __device__ __forceinline__ void  fg286d_fail(int64_t){ asm("trap;"); }
// The reference headers keep GGML_ASSERT active, which references ggml_abort;
// the fixture does not link ggml-base, so provide the abort hook here.
extern "C" void ggml_abort(const char * file, int line, const char * fmt, ...) {
    fprintf(stderr, "ggml_abort %s:%d %s\n", file, line, fmt);
    abort();
}
#include "snake-softcap.cuh"
#include "device-functions-generated.cuh"
#include "entries.h"

static __global__ void goose_snake_f32(const float * x,const float * a,const float * ib,float * dst,int total,int Tlen){
    FG286D_CB_snake_f32((uint64_t)(uintptr_t)x,(uint64_t)(uintptr_t)a,(uint64_t)(uintptr_t)ib,(uint64_t)(uintptr_t)dst,
        (int64_t)total,(int64_t)Tlen,(int64_t)(blockIdx.x*blockDim.x+threadIdx.x),0);
}
static __global__ void goose_snake_f16(const half * x,const float * a,const float * ib,half * dst,int total,int Tlen){
    FG286D_CB_snake_f16((uint64_t)(uintptr_t)x,(uint64_t)(uintptr_t)a,(uint64_t)(uintptr_t)ib,(uint64_t)(uintptr_t)dst,
        (int64_t)total,(int64_t)Tlen,(int64_t)(blockIdx.x*blockDim.x+threadIdx.x),0);
}
static __global__ void goose_snake_bf16(const nv_bfloat16 * x,const float * a,const float * ib,nv_bfloat16 * dst,int total,int Tlen){
    FG286D_CB_snake_bf16((uint64_t)(uintptr_t)x,(uint64_t)(uintptr_t)a,(uint64_t)(uintptr_t)ib,(uint64_t)(uintptr_t)dst,
        (int64_t)total,(int64_t)Tlen,(int64_t)(blockIdx.x*blockDim.x+threadIdx.x),0);
}
static __global__ void goose_softcap(const float * x,float * dst,float scale,float cap,int k){
    FG286D_CB_softcap((uint64_t)(uintptr_t)x,(uint64_t)(uintptr_t)dst,scale,cap,(int64_t)k,
        (int64_t)(blockIdx.x*blockDim.x+threadIdx.x),0);
}

static int checks, total_fails, launch_fail;

int main(void){
    int ndev=0;
    if (cudaGetDeviceCount(&ndev)!=cudaSuccess || ndev<1){ fprintf(stderr,"no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937_64 rng(286011);
    const int Ts[4]={1,3,64,1000};
    const int Cs[3]={1,4,8};
    for (int ti=0;ti<4;++ti) for (int ci=0;ci<3;++ci){
        const int Tlen=Ts[ti], C=Cs[ci], total=Tlen*C;
        std::vector<float> hx(total), ha(C+1), hb(C+1);
        for (auto & v:hx) v=(float)((int64_t)(rng()%2001)-1000)*0.002f;
        for (auto & v:ha) v=(float)((int64_t)(rng()%2001)-1000)*0.001f;
        for (auto & v:hb) v=0.5f+(float)(rng()%1001)*0.001f;
        std::vector<half> hxh(total); std::vector<nv_bfloat16> hxb(total);
        for (int i=0;i<total;++i){ hxh[i]=__float2half(hx[i]); hxb[i]=__float2bfloat16(hx[i]); }
        int nb=(total+255)/256;
        const uint3 fd=init_fastdiv_values((uint64_t)Tlen);
        { // F32
            float *dx=nullptr,*da=nullptr,*db=nullptr,*dn=nullptr,*dg=nullptr;
            cudaMalloc(&dx,total*4);cudaMalloc(&da,(C+1)*4);cudaMalloc(&db,(C+1)*4);cudaMalloc(&dn,total*4);cudaMalloc(&dg,total*4);
            cudaMemcpy(dx,hx.data(),total*4,cudaMemcpyHostToDevice);
            cudaMemcpy(da,ha.data(),(C+1)*4,cudaMemcpyHostToDevice);
            cudaMemcpy(db,hb.data(),(C+1)*4,cudaMemcpyHostToDevice);
            cudaMemset(dn,0xFF,total*4);cudaMemset(dg,0xFF,total*4);
            snake_kernel_ref<float><<<nb,256>>>(dx,da,db,dn,total,fd);
            goose_snake_f32<<<nb,256>>>(dx,da,db,dg,total,Tlen);
            if (cudaDeviceSynchronize()!=cudaSuccess){ printf("LAUNCH FAIL snake f32\n"); ++launch_fail; }
            std::vector<float> a(total),b(total);
            cudaMemcpy(a.data(),dn,total*4,cudaMemcpyDeviceToHost);cudaMemcpy(b.data(),dg,total*4,cudaMemcpyDeviceToHost);
            for (int i=0;i<total;++i){ ++checks; if (memcmp(&a[i],&b[i],4)){ ++total_fails; if(total_fails==1) printf("FIRST FAIL snake_f32 T=%d C=%d i=%d\n",Tlen,C,i); } }
            cudaFree(dx);cudaFree(da);cudaFree(db);cudaFree(dn);cudaFree(dg);
        }
        { // F16
            half *dx=nullptr,*dn=nullptr,*dg=nullptr; float *da=nullptr,*db=nullptr;
            cudaMalloc(&dx,total*2);cudaMalloc(&da,(C+1)*4);cudaMalloc(&db,(C+1)*4);cudaMalloc(&dn,total*2);cudaMalloc(&dg,total*2);
            cudaMemcpy(dx,hxh.data(),total*2,cudaMemcpyHostToDevice);
            cudaMemcpy(da,ha.data(),(C+1)*4,cudaMemcpyHostToDevice);
            cudaMemcpy(db,hb.data(),(C+1)*4,cudaMemcpyHostToDevice);
            cudaMemset(dn,0xFF,total*2);cudaMemset(dg,0xFF,total*2);
            snake_kernel_ref<half><<<nb,256>>>(dx,da,db,dn,total,fd);
            goose_snake_f16<<<nb,256>>>(dx,da,db,dg,total,Tlen);
            if (cudaDeviceSynchronize()!=cudaSuccess){ printf("LAUNCH FAIL snake f16\n"); ++launch_fail; }
            std::vector<uint16_t> a(total),b(total);
            cudaMemcpy(a.data(),dn,total*2,cudaMemcpyDeviceToHost);cudaMemcpy(b.data(),dg,total*2,cudaMemcpyDeviceToHost);
            for (int i=0;i<total;++i){ ++checks; if (a[i]!=b[i]){ ++total_fails; if(total_fails==1) printf("FIRST FAIL snake_f16 T=%d C=%d i=%d nat=%04x goo=%04x\n",Tlen,C,i,a[i],b[i]); } }
            cudaFree(dx);cudaFree(da);cudaFree(db);cudaFree(dn);cudaFree(dg);
        }
        { // BF16
            nv_bfloat16 *dx=nullptr,*dn=nullptr,*dg=nullptr; float *da=nullptr,*db=nullptr;
            cudaMalloc(&dx,total*2);cudaMalloc(&da,(C+1)*4);cudaMalloc(&db,(C+1)*4);cudaMalloc(&dn,total*2);cudaMalloc(&dg,total*2);
            cudaMemcpy(dx,hxb.data(),total*2,cudaMemcpyHostToDevice);
            cudaMemcpy(da,ha.data(),(C+1)*4,cudaMemcpyHostToDevice);
            cudaMemcpy(db,hb.data(),(C+1)*4,cudaMemcpyHostToDevice);
            cudaMemset(dn,0xFF,total*2);cudaMemset(dg,0xFF,total*2);
            snake_kernel_ref<nv_bfloat16><<<nb,256>>>(dx,da,db,dn,total,fd);
            goose_snake_bf16<<<nb,256>>>(dx,da,db,dg,total,Tlen);
            if (cudaDeviceSynchronize()!=cudaSuccess){ printf("LAUNCH FAIL snake bf16\n"); ++launch_fail; }
            std::vector<uint16_t> a(total),b(total);
            cudaMemcpy(a.data(),dn,total*2,cudaMemcpyDeviceToHost);cudaMemcpy(b.data(),dg,total*2,cudaMemcpyDeviceToHost);
            for (int i=0;i<total;++i){ ++checks; if (a[i]!=b[i]){ ++total_fails; if(total_fails==1) printf("FIRST FAIL snake_bf16 T=%d C=%d i=%d nat=%04x goo=%04x\n",Tlen,C,i,a[i],b[i]); } }
            cudaFree(dx);cudaFree(da);cudaFree(db);cudaFree(dn);cudaFree(dg);
        }
    }
    const int ks[5]={1,255,256,257,10000};
    const float scales[2]={0.5f,-1.2f};
    const float caps[2]={30.0f,0.7f};
    for (int ki=0;ki<5;++ki) for (int si=0;si<2;++si) for (int cpi=0;cpi<2;++cpi){
        const int k=ks[ki];
        std::vector<float> hx((size_t)k);
        for (auto & v:hx) v=(float)((int64_t)(rng()%2001)-1000)*0.005f;
        float *dx=nullptr,*dn=nullptr,*dg=nullptr;
        cudaMalloc(&dx,(size_t)k*4);cudaMalloc(&dn,(size_t)k*4);cudaMalloc(&dg,(size_t)k*4);
        cudaMemcpy(dx,hx.data(),(size_t)k*4,cudaMemcpyHostToDevice);
        cudaMemset(dn,0xFF,(size_t)k*4);cudaMemset(dg,0xFF,(size_t)k*4);
        int nb=(k+255)/256;
        softcap_f32_ref<<<nb,256>>>(dx,dn,scales[si],caps[cpi],k);
        goose_softcap<<<nb,256>>>(dx,dg,scales[si],caps[cpi],k);
        if (cudaDeviceSynchronize()!=cudaSuccess){ printf("LAUNCH FAIL softcap\n"); ++launch_fail; }
        std::vector<float> a((size_t)k),b((size_t)k);
        cudaMemcpy(a.data(),dn,(size_t)k*4,cudaMemcpyDeviceToHost);cudaMemcpy(b.data(),dg,(size_t)k*4,cudaMemcpyDeviceToHost);
        for (int i=0;i<k;++i){ ++checks; if (memcmp(&a[(size_t)i],&b[(size_t)i],4)){ ++total_fails; if(total_fails==1) printf("FIRST FAIL softcap k=%d s=%g cap=%g i=%d nat=%a goo=%a\n",k,scales[si],caps[cpi],i,a[(size_t)i],b[(size_t)i]); } }
        cudaFree(dx);cudaFree(dn);cudaFree(dg);
    }
    printf("FG286D_GPU_FIXTURE values=%d failures=%d launch_fail=%d\n",checks,total_fails,launch_fail);
    return (total_fails||launch_fail)?1:0;
}
