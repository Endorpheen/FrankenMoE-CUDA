#include <cstdio>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <vector>
#include <random>
#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
#include "ggml-backend-impl.h"
typedef struct { const uint8_t *data; int64_t len; } fg_cpu_simd_bytes;
extern "C" {
int32_t fg_cpu_simd_row_q4_k_q8_k_x8(fg_cpu_simd_bytes w, fg_cpu_simd_bytes a, int32_t nb, int32_t nc, float * out);
void ggml_gemv_q4_K_8x8_q8_K(int n, float *s, size_t bs, const void *vx, const void *vy, int nr, int nc);
void quantize_row_q4_K(const float *x, void *y, int64_t k);
void quantize_row_q8_K(const float *x, void *y, int64_t k);
}
ggml_backend_buffer_type_t ggml_backend_cpu_repack_buffer_type(void);
int main(int argc, char ** argv) {
    const int n = argc > 1 ? atoi(argv[1]) : 2048;
    const int nc = argc > 2 ? atoi(argv[2]) : 512;
    const int reps = argc > 3 ? atoi(argv[3]) : 2000;
    const int nb = n/256;
    ggml_init_params ip = { 32u*1024*1024, nullptr, false };
    std::mt19937 rng(7); std::uniform_real_distribution<float> d(-1,1);
    std::vector<float> w((size_t)nc*n), act(n);
    for (auto &v : w) v = d(rng); for (auto &v : act) v = d(rng);
    size_t rb = ggml_row_size(GGML_TYPE_Q4_K, n), qb = ggml_row_size(GGML_TYPE_Q8_K, n);
    std::vector<uint8_t> q4((size_t)nc*rb), q8(qb);
    for (int r=0;r<nc;++r) quantize_row_q4_K(w.data()+(size_t)r*n, q4.data()+(size_t)r*rb, n);
    quantize_row_q8_K(act.data(), q8.data(), n);
    ggml_context * c = ggml_init(ip);
    ggml_tensor * t = ggml_new_tensor_3d(c, GGML_TYPE_Q4_K, n, nc, 1);
    ggml_backend_buffer_type_t buft = ggml_backend_cpu_repack_buffer_type();
    ggml_backend_buffer_t buf = ggml_backend_buft_alloc_buffer(buft, ggml_backend_buft_get_alloc_size(buft, t));
    ggml_backend_buffer_init_tensor(buf, t);
    t->buffer = buf; t->data = (char*)ggml_backend_buffer_get_base(buf);
    buf->iface.set_tensor(buf, t, q4.data(), 0, (size_t)nc*rb);
    const void * inter = ggml_backend_buffer_get_base(buf);
    std::vector<float> o1(nc), o2(nc);
    auto t0 = std::chrono::steady_clock::now();
    for (int i=0;i<reps;++i) ggml_gemv_q4_K_8x8_q8_K(n, o1.data(), 0, inter, q8.data(), 1, nc);
    auto t1 = std::chrono::steady_clock::now();
    for (int i=0;i<reps;++i) fg_cpu_simd_row_q4_k_q8_k_x8({(const uint8_t*)inter,(int64_t)nc*nb*144},{q8.data(),(int64_t)nb*292},nb,nc,o2.data());
    auto t2 = std::chrono::steady_clock::now();
    double a = std::chrono::duration<double,std::micro>(t1-t0).count()/reps;
    double b = std::chrono::duration<double,std::micro>(t2-t1).count()/reps;
    int diff=0; for (int i=0;i<nc;++i) if (memcmp(&o1[i],&o2[i],4)) diff++;
    printf("n=%d nc=%d nb=%d reps=%d native=%.1f us goose=%.1f us ratio=%.2fx diffs=%d\n", n,nc,nb,reps,a,b,b/a,diff);
    return 0;
}
