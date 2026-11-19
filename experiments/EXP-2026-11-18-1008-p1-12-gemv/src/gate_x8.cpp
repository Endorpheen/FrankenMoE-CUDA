// Differential gate: the Goose x8 entry against the live native gemv.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <random>
#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
#include "ggml-backend-impl.h"

typedef struct { const uint8_t *data; int64_t len; } fg_cpu_simd_bytes;
extern "C" {
int32_t fg_cpu_simd_row_q4_k_q8_k_x8(fg_cpu_simd_bytes w, fg_cpu_simd_bytes a,
                                     int32_t nb, int32_t nc, float * out);
void ggml_vec_dot_q4_K_q8_K(int n, float *s, size_t bs, const void *vx, size_t bx, const void *vy, size_t by, int nrc);
void ggml_gemv_q4_K_8x8_q8_K(int n, float *s, size_t bs, const void *vx, const void *vy, int nr, int nc);
void quantize_row_q4_K(const float *x, void *y, int64_t k);
void quantize_row_q8_K(const float *x, void *y, int64_t k);
}
ggml_backend_buffer_type_t ggml_backend_cpu_repack_buffer_type(void);

static uint32_t bits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

int main(int argc, char ** argv) {
    const int iters = argc > 1 ? atoi(argv[1]) : 8;
    const int nlist[] = {512, 2048, 8192};
    const int nclist[] = {8, 16, 64, 512};

    ggml_init_params ip = { 64u*1024*1024, nullptr, false };
    ggml_context * ctx = ggml_init(ip);
    std::mt19937 rng(999);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    printf("iters=%d\n", iters);
    long total = 0, equal = 0, differ = 0;
    double maxrel = 0.0; long maxulp = 0;

    for (int n : nlist) {
        for (int nc : nclist) {
            const int nb = n / 256;
            const size_t row_bytes = ggml_row_size(GGML_TYPE_Q4_K, n);
            const size_t q8_bytes  = ggml_row_size(GGML_TYPE_Q8_K, n);
            std::vector<float> w((size_t) nc * n), act(n);
            std::vector<uint8_t> q4((size_t) nc * row_bytes), q8(q8_bytes);

            long le = 0, ld = 0; long lulp = 0; double lrel = 0.0;
            for (int it = 0; it < iters; ++it) {
                for (auto & v : w) v = dist(rng);
                for (auto & v : act) v = dist(rng);
                for (int r = 0; r < nc; ++r)
                    quantize_row_q4_K(w.data() + (size_t) r*n, q4.data() + (size_t) r*row_bytes, n);
                quantize_row_q8_K(act.data(), q8.data(), n);

                ggml_context * c = ggml_init(ip);
                ggml_tensor * t = ggml_new_tensor_3d(c, GGML_TYPE_Q4_K, n, nc, 1);
                ggml_backend_buffer_type_t buft = ggml_backend_cpu_repack_buffer_type();
                ggml_backend_buffer_t buf = ggml_backend_buft_alloc_buffer(buft, ggml_backend_buft_get_alloc_size(buft, t));
                if (!buf) { printf("FATAL alloc\n"); return 2; }
                ggml_backend_buffer_init_tensor(buf, t);
                t->buffer = buf; t->data = (char *) ggml_backend_buffer_get_base(buf);
                buf->iface.set_tensor(buf, t, q4.data(), 0, (size_t) nc * row_bytes);
                const void * inter = ggml_backend_buffer_get_base(buf);
                if (t->nb[1] != row_bytes) {
                    printf("LAYOUT nb1=%zu expected %zu\n", (size_t) t->nb[1], row_bytes);
                    return 2;
                }

                std::vector<float> ref(nc), go(nc);
                ggml_gemv_q4_K_8x8_q8_K(n, ref.data(), 0, inter, q8.data(), 1, nc);
                int32_t rc = fg_cpu_simd_row_q4_k_q8_k_x8(
                    { (const uint8_t *) inter, (int64_t) nc * nb * 144 },
                    { q8.data(), (int64_t) nb * 292 }, nb, nc, go.data());
                if (rc != 0) { printf("FATAL goose rc=%d\n", rc); return 2; }

                for (int i = 0; i < nc; ++i) {
                    total++;
                    if (bits(ref[i]) == bits(go[i])) { equal++; le++; }
                    else {
                        differ++; ld++;
                        double rel = fabs((double) ref[i] - (double) go[i]) /
                                     (fabs((double) ref[i]) > 1e-30 ? fabs((double) ref[i]) : 1.0);
                        if (rel > lrel) lrel = rel;
                        if (rel > maxrel) maxrel = rel;
                        int64_t a = (int32_t) bits(ref[i]);
                        int64_t b = (int32_t) bits(go[i]);
                        long ulp = labs(a > b ? a - b : b - a);
                        if (ulp > lulp) lulp = ulp;
                        if (ulp > maxulp) maxulp = ulp;
                    }
                }
                ggml_backend_buffer_free(buf);
                ggml_free(c);
            }
            printf("n=%5d nc=%4d nb=%2d : %ld cases, %ld equal, %ld differ, max ulp %ld, max rel %.3e\n",
                   n, nc, nb, le+ld, le, ld, lulp, lrel);
        }
    }
    printf("\nTOTAL %ld cases, %ld equal, %ld differ, max ulp %ld, max rel %.3e\n",
           total, equal, differ, maxulp, maxrel);
    ggml_free(ctx);
    return differ == 0 ? 0 : 1;
}
