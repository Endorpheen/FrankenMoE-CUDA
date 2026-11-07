#include <cuda_runtime.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "goose_leaves.cuh"

template<int OP, bool GOOSE>
__global__ void probe(const float *x, float *y, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    if constexpr (OP == 0) {
        y[i] = GOOSE ? gg_function_1(x[i], 0) : x[i] / (1.0f + expf(-x[i]));
    } else {
        y[i] = GOOSE ? gg_function_3(x[i], 0.125f, -0.75f, 0) : 0.125f * x[i] - 0.75f;
    }
}

static void check(cudaError_t e) {
    if (e != cudaSuccess) { fprintf(stderr, "%s\n", cudaGetErrorString(e)); exit(2); }
}

template<int OP, bool GOOSE>
static float timed(const float *x, float *y, int n) {
    cudaEvent_t a, b;
    check(cudaEventCreate(&a)); check(cudaEventCreate(&b));
    check(cudaEventRecord(a));
    for (int r = 0; r < 100; ++r) probe<OP, GOOSE><<<(n+255)/256,256>>>(x,y,n);
    check(cudaEventRecord(b)); check(cudaEventSynchronize(b));
    float ms = 0; check(cudaEventElapsedTime(&ms,a,b));
    check(cudaEventDestroy(a)); check(cudaEventDestroy(b));
    return ms / 100;
}

template<int OP>
static void run(int n) {
    std::vector<float> x(n), a(n), b(n);
    for (int i=0;i<n;++i) x[i] = ((i * 31337ULL % 200003) - 100001.0f) / 1024.0f;
    float *dx, *da, *db;
    check(cudaMalloc(&dx,n*4)); check(cudaMalloc(&da,n*4)); check(cudaMalloc(&db,n*4));
    check(cudaMemcpy(dx,x.data(),n*4,cudaMemcpyHostToDevice));
    probe<OP,false><<<(n+255)/256,256>>>(dx,da,n);
    probe<OP,true><<<(n+255)/256,256>>>(dx,db,n);
    check(cudaMemcpy(a.data(),da,n*4,cudaMemcpyDeviceToHost));
    check(cudaMemcpy(b.data(),db,n*4,cudaMemcpyDeviceToHost));
    int different=0;
    for(int i=0;i<n;++i) if(memcmp(&a[i],&b[i],4)) ++different;
    if(different) { fprintf(stderr,"FAIL op=%d n=%d different=%d\n",OP,n,different);exit(1); }
    for(int r=0;r<3;++r) {
        float original, goose;
        if(r&1) {goose=timed<OP,true>(dx,db,n);original=timed<OP,false>(dx,da,n);}
        else {original=timed<OP,false>(dx,da,n);goose=timed<OP,true>(dx,db,n);}
        printf("{\"op\":%d,\"n\":%d,\"repeat\":%d,\"original_ms\":%.9g,\"goose_ms\":%.9g,\"bitwise_equal\":true,\"preliminary\":true}\n",OP,n,r,original,goose);
    }
    check(cudaFree(dx));check(cudaFree(da));check(cudaFree(db));
}
int main() {
    for(int n: {1,4093,16384,1048576}) {run<0>(n);run<1>(n);}
    return 0;
}
