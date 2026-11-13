// Single-block probe: run the Goose mul_mat_vec_q over exactly one 256-wide block and
// one row, so the whole result is one vec_dot_q4_K_q8_1 call. Comparing against native
// on that shape isolates the scale/min unpack and the q4/q8 offsets from any launch
// geometry question.
//
// Usage: probe <dump_prefix>   writes goose.bin and native.bin next to the prefix.
#include "common.cuh"
#include "mmvq.cuh"
#include "quantize.cuh"
#include "ggml-backend.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>

int main(int argc, char ** argv) {
    if (ggml_backend_cuda_get_device_count() < 1) return 77;
    cudaSetDevice(0);
    const int64_t ncols = 256, nrows = 1, nch = 1;
    std::mt19937 rng(185009);
    std::uniform_real_distribution<float> dist(-1.f, 1.f);

    std::vector<uint8_t> x((size_t)(ncols/256)*144);
    for (auto & v : x) v = (uint8_t)rng();
    for (size_t b = 0; b < x.size()/144; ++b) {
        half d = __float2half(0.02f), dm = __float2half(0.01f);
        std::memcpy(x.data()+b*144+0, &d, 2);
        std::memcpy(x.data()+b*144+2, &dm, 2);
    }
    std::vector<float> y(ncols);
    for (auto & v : y) v = dist(rng);
    std::vector<float> out(1, 0.f);

    uint8_t * dx=nullptr; float * dy=nullptr, * dd=nullptr;
    cudaMalloc(&dx, x.size()); cudaMalloc(&dy, y.size()*4); cudaMalloc(&dd, 4);
    cudaMemcpy(dx, x.data(), x.size(), cudaMemcpyHostToDevice);
    cudaMemcpy(dy, y.data(), y.size()*4, cudaMemcpyHostToDevice);

    ggml_tensor s0{}, s1{}, d{};
    s0.type=GGML_TYPE_Q4_K; s1.type=d.type=GGML_TYPE_F32;
    s0.ne[0]=ncols; s0.ne[1]=nrows; s0.ne[2]=1; s0.ne[3]=1;
    s0.nb[0]=144; s0.nb[1]=x.size(); s0.nb[2]=s0.nb[1]; s0.nb[3]=s0.nb[2];
    s1.ne[0]=ncols; s1.ne[1]=1; s1.ne[2]=nch; s1.ne[3]=1;
    s1.nb[0]=4; s1.nb[1]=ncols*4; s1.nb[2]=s1.nb[1]; s1.nb[3]=s1.nb[2];
    d.ne[0]=nrows; d.ne[1]=1; d.ne[2]=nch; d.ne[3]=1;
    d.nb[0]=4; d.nb[1]=nrows*4; d.nb[2]=d.nb[1]; d.nb[3]=d.nb[2];
    s0.data=dx; s1.data=dy; d.data=dd;
    ggml_backend_buffer_type_t buft = ggml_backend_cuda_buffer_type(0);
    ggml_backend_buffer_t bx=ggml_backend_buft_alloc_buffer(buft,x.size());
    ggml_backend_buffer_t by=ggml_backend_buft_alloc_buffer(buft,y.size()*4);
    ggml_backend_buffer_t bd=ggml_backend_buft_alloc_buffer(buft,4);
    s0.buffer=bx; s1.buffer=by; d.buffer=bd;

    ggml_backend_cuda_context ctx(0);
    ggml_cuda_mul_mat_vec_q(ctx, &s0, &s1, nullptr, &d, nullptr);
    cudaDeviceSynchronize();
    cudaMemcpy(out.data(), dd, 4, cudaMemcpyDeviceToHost);

    // Float reference from the same bytes.
    const uint8_t * p = x.data();
    half dh, dmh; std::memcpy(&dh,p,2); std::memcpy(&dmh,p+2,2);
    const float dsc=__half2float(dh), dmin=__half2float(dmh);
    const uint8_t * sc=p+4; const uint8_t * qs=p+16;
    double acc=0.0;
    for (int jj=0;jj<8;++jj){
        const int is = jj;
        uint8_t s,m;
        if (is < 4){ s = sc[is] & 63; m = sc[is+4] & 63; }
        else { s = (sc[is+4] & 0xF) | ((sc[is-4] >> 6) << 4); m = (sc[is+4] >> 4) | ((sc[is] >> 6) << 4); }
        const uint8_t * q = qs + (jj/2)*32;
        for (int i=0;i<32;++i){
            const int qv = (jj%2==0) ? (q[i] & 0xF) : (q[i] >> 4);
            acc += (double)(dsc*s*qv - dmin*m) * (double)y[jj*32+i];
        }
    }
    const bool nat = getenv("GGML_GOOSE_CUDA_MMVQ_Q4K") &&
                     !strcmp(getenv("GGML_GOOSE_CUDA_MMVQ_Q4K"),"0");
    printf("PROBE arm=%s kernel=%.9g float_ref=%.9g rel=%.3e\n",
           nat?"native":"goose", out[0], (float)acc,
           fabs(out[0]-acc)/fmax(1e-30,fabs(acc)));
    return 0;
}
