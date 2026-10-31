#include <cuda_runtime.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "goose_device.cuh"
#include "launch_loop.h"

static void check(cudaError_t e) {
    if (e != cudaSuccess) { std::fprintf(stderr,"CUDA error: %s\n",cudaGetErrorString(e)); std::exit(2); }
}
static cudaStream_t stream;
static float * gates, * ups, * output;
static int elements;
static __global__ void empty_kernel() {}
static __device__ __forceinline__ float native_silu_gate(float g,float u) {
    return (g / (1.0f + expf(-g))) * u;
}
template<bool Goose> static __global__ void silu_kernel(const float *g,const float *u,float *y,int n) {
    int i=blockIdx.x*blockDim.x+threadIdx.x;
    if(i<n) y[i]=Goose ? goose_silu_gate_g(g[i],u[i]) : native_silu_gate(g[i],u[i]);
}
extern "C" void bridge_launch(int32_t kind) {
    if(kind==0) empty_kernel<<<1,1,0,stream>>>();
    else if(kind==1) silu_kernel<false><<<(elements+255)/256,256,0,stream>>>(gates,ups,output,elements);
    else silu_kernel<true><<<(elements+255)/256,256,0,stream>>>(gates,ups,output,elements);
}
static void correctness() {
    const int capacity=1<<22;
    std::vector<float> g(capacity),u(capacity),a(capacity),b(capacity);
    const float special[]={0.0f,-0.0f,1e-30f,-1e-30f,100.0f,-100.0f,INFINITY,-INFINITY,NAN};
    for(int i=0;i<capacity;++i) { g[i]=(int((uint32_t(i)*1664525u+1013904223u)%20001)-10000)*.01f; u[i]=(i%257-128)*.03125f; }
    for(unsigned i=0;i<sizeof(special)/sizeof(float);++i)g[i]=special[i];
    check(cudaMemcpyAsync(gates,g.data(),g.size()*sizeof(float),cudaMemcpyHostToDevice,stream));
    check(cudaMemcpyAsync(ups,u.data(),u.size()*sizeof(float),cudaMemcpyHostToDevice,stream));
    for(int n:{1,31,256,4096,65537,capacity}) {
        elements=n;bridge_launch(1);check(cudaMemcpyAsync(a.data(),output,n*sizeof(float),cudaMemcpyDeviceToHost,stream));
        bridge_launch(2);check(cudaMemcpyAsync(b.data(),output,n*sizeof(float),cudaMemcpyDeviceToHost,stream));
        check(cudaStreamSynchronize(stream));
        size_t mismatches=0;double max_abs=0;
        for(int i=0;i<n;++i) { if(std::memcmp(&a[i],&b[i],sizeof(float)))++mismatches; if(std::isfinite(a[i])&&std::isfinite(b[i]))max_abs=std::max(max_abs,double(std::fabs(a[i]-b[i]))); }
        std::printf("{\"kind\":\"correctness\",\"n\":%d,\"bit_mismatches\":%zu,\"max_abs\":%.9g}\n",n,mismatches,max_abs);
        if(mismatches)std::exit(3);
    }
}
static void trial(int caller,int kernel,int count,int pair) {
    check(cudaStreamSynchronize(stream));cudaEvent_t begin,end;check(cudaEventCreate(&begin));check(cudaEventCreate(&end));
    check(cudaEventRecord(begin,stream));auto t0=std::chrono::steady_clock::now();
    if(caller)goose_launch_loop(kernel,count);else for(int i=0;i<count;++i)bridge_launch(kernel);
    auto enqueue=std::chrono::steady_clock::now();check(cudaEventRecord(end,stream));check(cudaEventSynchronize(end));
    auto done=std::chrono::steady_clock::now();float gpu_ms;check(cudaEventElapsedTime(&gpu_ms,begin,end));
    double host_us=std::chrono::duration<double,std::micro>(enqueue-t0).count();double wall_us=std::chrono::duration<double,std::micro>(done-t0).count();
    std::printf("{\"kind\":\"timing\",\"caller\":\"%s\",\"kernel\":%d,\"n\":%d,\"count\":%d,\"pair\":%d,\"host_enqueue_us\":%.6f,\"wall_us\":%.6f,\"gpu_event_ms\":%.6f}\n",caller?"goose":"cpp",kernel,elements,count,pair,host_us,wall_us,gpu_ms);
    check(cudaEventDestroy(begin));check(cudaEventDestroy(end));
}
int main(int argc,char **argv) {
    goose_init(argc,argv);cudaDeviceProp prop;check(cudaGetDeviceProperties(&prop,0));
    std::printf("{\"kind\":\"device\",\"name\":\"%s\",\"sm\":%d}\n",prop.name,prop.major*10+prop.minor);
    check(cudaStreamCreateWithFlags(&stream,cudaStreamNonBlocking));
    check(cudaMalloc(&gates,(1<<22)*sizeof(float)));check(cudaMalloc(&ups,(1<<22)*sizeof(float)));check(cudaMalloc(&output,(1<<22)*sizeof(float)));
    correctness();
    if(argc==1 || std::strcmp(argv[1],"check")) {
        for(int k=0;k<3;++k) {
            elements=4096;for(int i=0;i<100;++i)bridge_launch(k);check(cudaStreamSynchronize(stream));
            for(int pair=0;pair<5;++pair) { int first=pair%2;trial(first,k,4000,pair);trial(1-first,k,4000,pair); }
        }
        elements=1<<22;for(int pair=0;pair<5;++pair){int first=pair%2;trial(0,first?2:1,100,pair);trial(0,first?1:2,100,pair);}
    }
    check(cudaGetLastError());check(cudaFree(gates));check(cudaFree(ups));check(cudaFree(output));check(cudaStreamDestroy(stream));
}
