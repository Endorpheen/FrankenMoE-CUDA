// Bit-exact gate: Goose tiled Q4_K kernel vs the native tiled op.
#include "ggml.h"
#include "ggml-cpu.h"
#include "ggml-cpu/ggml-cpu-impl.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <random>
#include <vector>
#include <chrono>

extern "C" void quantize_row_q4_K_ref(const float * x, void * y, int64_t k);
extern "C" void quantize_row_q8_K_ref(const float * x, void * y, int64_t k);

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

static void fill_q4k(ggml_tensor * t, std::mt19937 & rng) {
    std::vector<float> row(t->ne[0]);
    std::normal_distribution<float> nd(0.0f, 0.05f);
    for (int64_t r = 0; r < t->ne[1]; r++) {
        for (int64_t i = 0; i < t->ne[0]; i++) row[i] = nd(rng);
        quantize_row_q4_K_ref(row.data(), (char *) t->data + r * t->nb[1], t->ne[0]);
    }
}

static void fill_q8k(ggml_tensor * t, std::mt19937 & rng) {
    std::vector<float> row(t->ne[0]);
    std::normal_distribution<float> nd(0.0f, 0.6f);
    for (int64_t r = 0; r < t->ne[1]; r++) {
        for (int64_t i = 0; i < t->ne[0]; i++) row[i] = nd(rng);
        quantize_row_q8_K_ref(row.data(), (char *) t->data + r * t->nb[1], t->ne[0]);
    }
}

int main(int argc, char ** argv) {
    const int64_t K = argc > 4 ? atoll(argv[4]) : 2048;
    const int64_t R = argc > 5 ? atoll(argv[5]) : 512;
    const int64_t SLOTS = 4;
    const int64_t C = argc > 3 ? atoll(argv[3]) : 8;
    const int nth = argc > 1 ? atoi(argv[1]) : 1;
    const int iters = argc > 2 ? atoi(argv[2]) : 20;
    ggml_init_params ip = { 64u * 1024u * 1024u, nullptr, false };
    ggml_context * ctx = ggml_init(ip);

    ggml_tensor * src0 = ggml_new_tensor_3d(ctx, GGML_TYPE_Q4_K, K, R, 1);
    ggml_tensor * src1 = ggml_new_tensor_2d(ctx, GGML_TYPE_Q8_K, K, C);
    ggml_tensor * dst  = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, R, C, 1);

    dst->src[0] = src0;
    dst->src[1] = src1;

    std::mt19937 rng(1234);
    fill_q4k(src0, rng);
    fill_q8k(src1, rng);

    std::vector<int32_t> routes(2 * C);
    for (int m = 0; m < C; m++) { routes[2 * m] = (int32_t) m; routes[2 * m + 1] = 0; }

    const size_t ws_size = 512u * 1024u;
    uint8_t * wdata_raw = (uint8_t *) aligned_alloc(64, ws_size * nth);
    uint8_t * acc_raw = (uint8_t *) aligned_alloc(64, ws_size * nth);
    std::memset(wdata_raw, 0, ws_size * nth);
    std::memset(acc_raw, 0, ws_size * nth);
    std::vector<float> ref((size_t) ggml_nelements(dst)), got((size_t) ggml_nelements(dst));

    ggml_compute_params params{};
    params.ith = 0;
    params.nth = nth;
    params.wdata = wdata_raw;
    params.wsize = ws_size * nth;
    params.threadpool = nullptr;
    params.use_ref = false;

    int64_t compared = 0, differ = 0;
    for (int it = 0; it < iters; it++) {
        memset(dst->data, 0, ggml_nbytes(dst));
        std::memset(wdata_raw, 0, ws_size * nth);
        for (int t = 0; t < nth; t++) {
            params.ith = t;
            ggml_compute_forward_mul_mat_id_tiled(&params, dst, 0, C, routes.data(), (char *) wdata_raw);
        }
        std::memcpy(ref.data(), dst->data, ggml_nbytes(dst));

        memset(dst->data, 0, ggml_nbytes(dst));
        for (int t = 0; t < nth; t++) {
            fg_args a{ (const uint8_t *) src0->data, src0->ne[0], src0->ne[1], src0->nb[1],
                       (const uint8_t *) src1->data, src1->nb[1], routes.data(), C,
                       dst->nb[0], dst->nb[1], dst->nb[2], (float *) dst->data, t, nth };
            fg_cpu_simd_mmid_tiled_q4_k(&a, acc_raw);
        }
        std::memcpy(got.data(), dst->data, ggml_nbytes(dst));

        for (size_t i = 0; i < ref.size(); i++) {
            compared++;
            uint32_t a, b;
            std::memcpy(&a, &ref[i], 4); std::memcpy(&b, &got[i], 4);
            if (a != b) differ++;
        }
    }
    printf("nth=%d iters=%d compared=%lld differ=%lld\n", nth, iters, (long long) compared, (long long) differ);
    {
        size_t rz = 0, gz = 0;
        for (size_t i = 0; i < ref.size(); i++) { if (ref[i] != 0.0f) rz++; if (got[i] != 0.0f) gz++; }
        printf("nonzero ref=%zu got=%zu\n", rz, gz);
        int shown = 0;
        printf("ref nz:");
        for (size_t i = 0; i < ref.size() && shown < 8; i++) if (ref[i] != 0.0f) { printf(" %zu", i); shown++; }
        printf("\ngot nz:");
        shown = 0;
        for (size_t i = 0; i < got.size() && shown < 8; i++) if (got[i] != 0.0f) { printf(" %zu", i); shown++; }
        printf("\n");
    }
    if (differ) {
        for (size_t i = 0; i < ref.size(); i++) {
            uint32_t a, b; std::memcpy(&a, &ref[i], 4); std::memcpy(&b, &got[i], 4);
            if (a != b) { printf("first diff at %zu ref=%.9g got=%.9g\n", i, ref[i], got[i]); break; }
        }
    }
    free(wdata_raw);
    free(acc_raw);
    ggml_free(ctx);
    return differ ? 1 : 0;
}
