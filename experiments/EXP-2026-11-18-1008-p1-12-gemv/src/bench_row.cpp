#include <cstdio>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <vector>
#include <random>
#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
typedef struct { const void *data; int64_t len; } bytes_t;
extern "C" {
int32_t fg_cpu_simd_row_q4_k_q8_k(bytes_t a, bytes_t b, int32_t nb, float *v);
void ggml_vec_dot_q4_K_q8_K(int n, float *s, size_t bs, const void *vx, size_t bx, const void *vy, size_t by, int nrc);
void quantize_row_q4_K(const float *x, void *y, int64_t k);
void quantize_row_q8_K(const float *x, void *y, int64_t k);
}
int main(int argc, char **argv) {
    int n = argc>1?atoi(argv[1]):2048, reps = argc>2?atoi(argv[2]):20000;
    ggml_init_params ip = { 4u*1024*1024, nullptr, false }; ggml_init(ip);
    ggml_backend_t cpu = ggml_backend_cpu_init();
    std::mt19937 rng(3); std::uniform_real_distribution<float> d(-1,1);
    std::vector<float> w(n), act(n); for(auto&v:w)v=d(rng); for(auto&v:act)v=d(rng);
    std::vector<uint8_t> q4(ggml_row_size(GGML_TYPE_Q4_K,n)), q8(ggml_row_size(GGML_TYPE_Q8_K,n));
    quantize_row_q4_K(w.data(), q4.data(), n); quantize_row_q8_K(act.data(), q8.data(), n);
    volatile float r=0,g=0;
    auto t0=std::chrono::steady_clock::now();
    for(int i=0;i<reps;++i) ggml_vec_dot_q4_K_q8_K(n,(float*)&r,0,q4.data(),0,q8.data(),0,1);
    auto t1=std::chrono::steady_clock::now();
    for(int i=0;i<reps;++i) fg_cpu_simd_row_q4_k_q8_k((bytes_t){q4.data(),(int64_t)q4.size()},(bytes_t){q8.data(),(int64_t)q8.size()},(int32_t)(n/256),(float*)&g);
    auto t2=std::chrono::steady_clock::now();
    double a=std::chrono::duration<double,std::micro>(t1-t0).count()/reps, b=std::chrono::duration<double,std::micro>(t2-t1).count()/reps;
    float rr=r, gg=g; printf("n=%d vec_dot=%.3f us goose_row=%.3f us ratio=%.1fx ref=%.9g goose=%.9g bits_equal=%d\n",n,a,b,b/a,(double)rr,(double)gg,memcmp(&rr,&gg,4)==0);
    ggml_backend_free(cpu);
    return 0;
}
