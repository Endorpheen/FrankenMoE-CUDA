// Speed comparison: native tiled Q4_K op vs the Goose kernel, same buffers.
#include "ggml.h"
#include "ggml-cpu.h"
#include "ggml-cpu/ggml-cpu-impl.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <random>
#include <vector>

extern "C" int32_t fg_cpu_simd_mmid_tiled_q4_k(const void * a, uint8_t * scratch);
extern "C" bool ggml_compute_forward_mul_mat_id_tiled(const struct ggml_compute_params * params,
                                                      struct ggml_tensor * dst, int64_t cur_a, int64_t cne1,
                                                      const int32_t * expert_rows, char * scratch);

struct fg_args {
    const uint8_t * src0; int64_t ne00, ne01, nb01;
    const uint8_t * act;  int64_t nbw1;
    const int32_t * routes; int64_t cne1;
    int64_t nb0, nb1, nb2;
    float * dst; int32_t ith, nth;
};

extern "C" void quantize_row_q4_K_ref(const float * x, void * y, int64_t k);
extern "C" void quantize_row_q8_K_ref(const float * x, void * y, int64_t k);

static void run(int64_t K, int64_t R, int nth, int reps, int Cs) {
    ggml_init_params ip = { 64u * 1024u * 1024u, nullptr, false };
    ggml_context * ctx = ggml_init(ip);
    ggml_tensor * src0 = ggml_new_tensor_3d(ctx, GGML_TYPE_Q4_K, K, R, 1);
    ggml_tensor * src1 = ggml_new_tensor_2d(ctx, GGML_TYPE_Q8_K, K, Cs);
    ggml_tensor * dst  = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, R, Cs, 1);
    dst->src[0] = src0;
    dst->src[1] = src1;

    std::mt19937 rng(99);
    std::normal_distribution<float> wd(0.0f, 0.05f), ad(0.0f, 0.6f);
    std::vector<float> row(K);
    for (int64_t r = 0; r < src0->ne[1]; r++) {
        for (int64_t i = 0; i < K; i++) row[i] = wd(rng);
        quantize_row_q4_K_ref(row.data(), (char *) src0->data + r * src0->nb[1], K);
    }
    for (int64_t r = 0; r < Cs; r++) {
        for (int64_t i = 0; i < K; i++) row[i] = ad(rng);
        quantize_row_q8_K_ref(row.data(), (char *) src1->data + r * src1->nb[1], K);
    }
    std::vector<int32_t> routes(2 * Cs);
    for (int m = 0; m < Cs; m++) { routes[2 * m] = m; routes[2 * m + 1] = 0; }

    const size_t ws = 512u * 1024u;
    uint8_t * wdata = (uint8_t *) aligned_alloc(64, ws * nth);
    uint8_t * acc = (uint8_t *) aligned_alloc(64, ws * nth);
    std::memset(wdata, 0, ws * nth);
    std::memset(acc, 0, ws * nth);

    ggml_compute_params params{};
    params.ith = 0; params.nth = nth; params.wdata = wdata; params.wsize = ws * nth;
    params.threadpool = nullptr; params.use_ref = false;

    for (int r = 0; r < 20; r++) {
        for (int t = 0; t < nth; t++) { params.ith = t; ggml_compute_forward_mul_mat_id_tiled(&params, dst, 0, Cs, routes.data(), (char *) wdata); }
        for (int t = 0; t < nth; t++) {
            fg_args a{ (const uint8_t *) src0->data, src0->ne[0], src0->ne[1], src0->nb[1],
                       (const uint8_t *) src1->data, src1->nb[1], routes.data(), Cs,
                       dst->nb[0], dst->nb[1], dst->nb[2], (float *) dst->data, t, nth };
            fg_cpu_simd_mmid_tiled_q4_k(&a, acc);
        }
    }

    auto t0 = std::chrono::steady_clock::now();
    for (int r = 0; r < reps; r++) {
        std::memset(wdata, 0, ws * nth);
        for (int t = 0; t < nth; t++) { params.ith = t; ggml_compute_forward_mul_mat_id_tiled(&params, dst, 0, Cs, routes.data(), (char *) wdata); }
    }
    auto t1 = std::chrono::steady_clock::now();
    for (int r = 0; r < reps; r++) {
        for (int t = 0; t < nth; t++) {
            fg_args a{ (const uint8_t *) src0->data, src0->ne[0], src0->ne[1], src0->nb[1],
                       (const uint8_t *) src1->data, src1->nb[1], routes.data(), Cs,
                       dst->nb[0], dst->nb[1], dst->nb[2], (float *) dst->data, t, nth };
            fg_cpu_simd_mmid_tiled_q4_k(&a, acc);
        }
    }
    auto t2 = std::chrono::steady_clock::now();
    double nat = std::chrono::duration<double, std::micro>(t1 - t0).count() / reps;
    double goo = std::chrono::duration<double, std::micro>(t2 - t1).count() / reps;
    printf("K=%lld R=%lld C=%d nth=%d native=%.2f us goose=%.2f us ratio=%.2fx\n",
           (long long) K, (long long) R, Cs, nth, nat, goo, goo / nat);

    free(wdata);
    free(acc);
    ggml_free(ctx);
}

int main() {
    for (int nth : { 1, 8 }) {
        run(2048, 512, nth, 300, 8);
        run(512, 2048, nth, 300, 8);
    }
    return 0;
}
