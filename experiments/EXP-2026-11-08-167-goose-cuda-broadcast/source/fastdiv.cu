#include "binbcast.cuh"
#include "convert.cuh"
#include "projection.cuh"
#include "device-generated.cuh"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <limits>
extern "C" void fg167_probe_magic(fg167_plan*,int64_t,uint64_t);
static void ck(cudaError_t e){if(e!=cudaSuccess){fprintf(stderr,"CUDA %s\n",cudaGetErrorString(e));std::exit(2);}}
static __global__ void compare(const fg167_plan*p,const uint32_t*ns,uint32_t*a,uint32_t*b,int count){int i=blockIdx.x*blockDim.x+threadIdx.x;if(i>=count)return;int id=i/128;auto*q=p+id;a[i]=fg167d_gg_function_7((int64_t)(uintptr_t)q,ns[i],0,0);b[i]=fastdiv(ns[i],make_uint3(q->magic[0][0],q->magic[0][1],q->magic[0][2]));}
int main(){static const uint32_t ds[]={1,2,3,7,31,32,33,65535,65536,65537,2147483647U,2147483648U,2147483649U,4294967295U};constexpr int nd=sizeof(ds)/sizeof(ds[0]),count=nd*128;std::vector<fg167_plan>p(nd);std::vector<uint32_t>ns(count),goose(count),native(count);for(int i=0;i<nd;i++){fg167_probe_magic(&p[i],0,ds[i]);auto ref=init_fastdiv_values(ds[i]);if(p[i].magic[0][0]!=ref.x||p[i].magic[0][1]!=ref.y||p[i].magic[0][2]!=ref.z){fprintf(stderr,"FAIL host magic d=%u\n",ds[i]);return 3;}for(int j=0;j<128;j++){uint32_t v=uint32_t(j)*3133731337U+uint32_t(i)*1729U;if(j==0)v=0;if(j==1)v=1;if(j==2)v=ds[i]-1;if(j==3)v=ds[i];if(j==4)v=UINT32_MAX;if(j==5)v=2147483648U;ns[i*128+j]=v;}}fg167_plan*dp;uint32_t*dn,*dg,*dr;ck(cudaMalloc(&dp,p.size()*sizeof(p[0])));ck(cudaMalloc(&dn,count*4));ck(cudaMalloc(&dg,count*4));ck(cudaMalloc(&dr,count*4));ck(cudaMemcpy(dp,p.data(),p.size()*sizeof(p[0]),cudaMemcpyHostToDevice));ck(cudaMemcpy(dn,ns.data(),count*4,cudaMemcpyHostToDevice));compare<<<(count+127)/128,128>>>(dp,dn,dg,dr,count);ck(cudaGetLastError());ck(cudaMemcpy(goose.data(),dg,count*4,cudaMemcpyDeviceToHost));ck(cudaMemcpy(native.data(),dr,count*4,cudaMemcpyDeviceToHost));size_t mismatches=0;for(int i=0;i<count;i++){uint32_t expected=ns[i]/ds[i/128];if(goose[i]!=expected){fprintf(stderr,"FAIL n=%u d=%u expected=%u goose=%u\n",ns[i],ds[i/128],expected,goose[i]);return 4;}mismatches+=native[i]!=expected;}if(!mismatches)return 5;ck(cudaFree(dp));ck(cudaFree(dn));ck(cudaFree(dg));ck(cudaFree(dr));printf("PASS real_GPU_uint32_division cases=%d host_magic_exact=1 goose_exact=1 reproduced_native_overflow_or_shift_mismatches=%zu\n",count,mismatches);}
