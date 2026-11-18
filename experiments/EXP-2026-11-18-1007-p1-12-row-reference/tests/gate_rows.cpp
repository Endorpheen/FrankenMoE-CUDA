// Compare the delivered Goose row object with the kernels the live Apex runtime executes.
// Reference sources are the shipped libggml-cpu.so symbols, not a re-implementation.
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
int32_t fg_cpu_simd_row_q4_k_q8_k(fg_cpu_simd_bytes a, fg_cpu_simd_bytes b, int32_t nb, float *value);
int32_t fg_cpu_simd_row_q6_k_q8_k(fg_cpu_simd_bytes a, fg_cpu_simd_bytes b, int32_t nb, float *value);
void ggml_vec_dot_q4_K_q8_K(int n, float *s, size_t bs, const void *vx, size_t bx, const void *vy, size_t by, int nrc);
void ggml_vec_dot_q6_K_q8_K(int n, float *s, size_t bs, const void *vx, size_t bx, const void *vy, size_t by, int nrc);
void ggml_gemv_q4_K_8x8_q8_K(int n, float *s, size_t bs, const void *vx, const void *vy, int nr, int nc);
void quantize_row_q4_K(const float *x, void *y, int64_t k);
void quantize_row_q6_K(const float *x, void *y, int64_t k);
void quantize_row_q8_K(const float *x, void *y, int64_t k);
}
ggml_backend_buffer_type_t ggml_backend_cpu_repack_buffer_type(void);

static uint32_t bits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

int main(int argc, char ** argv) {
    if (argc > 1 && strcmp(argv[1], "decide") == 0) {
        ggml_init_params dp = { 256u*1024*1024, nullptr, false };
        ggml_context * c = ggml_init(dp);
        ggml_backend_t cpu = ggml_backend_cpu_init();
        ggml_backend_dev_t dev = ggml_backend_get_device(cpu);
        ggml_backend_dev_props props; ggml_backend_dev_get_props(dev, &props);
        printf("device: %s\n", props.name);
        ggml_backend_buffer_type_t b0 = ggml_backend_cpu_buffer_type();
        ggml_backend_buffer_type_t br = ggml_backend_cpu_repack_buffer_type();
        printf("cpu buft: %s | repack buft: %s\n", ggml_backend_buft_name(b0), ggml_backend_buft_name(br));
        struct probe_case { const char * name; enum ggml_type type; int ne0, ne1, ne2; } cases[] = {
            {"q4_K gate/up expert", GGML_TYPE_Q4_K, 2048, 512, 256},
            {"q6_K down expert",    GGML_TYPE_Q6_K,  512, 2048, 256},
        };
        for (auto & cs : cases) {
            ggml_tensor * w = ggml_new_tensor_3d(c, cs.type, cs.ne0, cs.ne1, cs.ne2);
            ggml_tensor * b = ggml_new_tensor_3d(c, GGML_TYPE_F32, cs.ne0, 4, 1);
            ggml_tensor * ids = ggml_new_tensor_2d(c, GGML_TYPE_I32, 4, 1);
            ggml_tensor * op = ggml_mul_mat_id(c, w, b, ids);
            ggml_backend_buffer_t dummy = ggml_backend_buft_alloc_buffer(br, 0);
            w->buffer = dummy;
            bool sup = ggml_backend_dev_supports_op(dev, op);
            w->buffer = nullptr;
            ggml_backend_buffer_free(dummy);
            printf("supports_op(repack, %s) = %s\n", cs.name, sup ? "true" : "false");
        }
        ggml_backend_free(cpu);
        ggml_free(c);
        return 0;
    }

    const int n = argc > 1 ? atoi(argv[1]) : 2048;
    const int rows = 8;
    const int iters = argc > 2 ? atoi(argv[2]) : 32;

    printf("n=%d rows=%d iterations=%d\n", n, rows, iters);



    ggml_init_params ip = { /*mem_size*/ 64u*1024*1024, /*mem_buffer*/ nullptr, /*no_alloc*/ false };
    ggml_context * ctx = ggml_init(ip);

    if (argc > 3 && strcmp(argv[3], "probe") == 0) {
        size_t wb = ggml_row_size(GGML_TYPE_Q4_K, n);
        size_t q8b = ggml_row_size(GGML_TYPE_Q8_K, n);
        std::vector<float> pw((size_t) rows * n, 0.0f), pa(n, 1.0f);
        for (int i = 0; i < n; ++i) pw[i] = 1.0f;          // only row 0 nonzero
        std::vector<uint8_t> pq4((size_t) rows * wb), pq8(q8b);
        for (int r = 0; r < rows; ++r) quantize_row_q4_K(pw.data() + (size_t) r*n, pq4.data() + (size_t) r*wb, n);
        quantize_row_q8_K(pa.data(), pq8.data(), n);
        ggml_context * pc = ggml_init(ip);
        ggml_tensor * pt = ggml_new_tensor_3d(pc, GGML_TYPE_Q4_K, n, rows, 1);
        ggml_backend_buffer_type_t pbuft = ggml_backend_cpu_repack_buffer_type();
        ggml_backend_buffer_t pbuf = ggml_backend_buft_alloc_buffer(pbuft, ggml_backend_buft_get_alloc_size(pbuft, pt));
        ggml_backend_buffer_init_tensor(pbuf, pt);
        pt->buffer = pbuf;
        pt->data   = (char *) ggml_backend_buffer_get_base(pbuf);
        pbuf->iface.set_tensor(pbuf, pt, pq4.data(), 0, (size_t) rows * wb);
        std::vector<float> pgv(rows);
        ggml_gemv_q4_K_8x8_q8_K(n, pgv.data(), 0, ggml_backend_buffer_get_base(pbuf), pq8.data(), 1, rows);
        float pr, pg;
        ggml_vec_dot_q4_K_q8_K(n, &pr, 0, pq4.data(), 0, pq8.data(), 0, 1);
        fg_cpu_simd_row_q4_k_q8_k({pq4.data(), (int64_t) wb}, {pq8.data(), (int64_t) q8b}, (int32_t)(n/256), &pg);
        printf("PROBE vec_dot(row0)=%.9g goose_row(row0)=%.9g\n", pr, pg);
        for (int c = 0; c < rows; ++c) printf("PROBE gemv col %d = %.9g\n", c, pgv[c]);
        ggml_backend_buffer_free(pbuf); ggml_free(pc);
        ggml_free(ctx);
        return 0;
    }

    size_t w_row_bytes  = ggml_row_size(GGML_TYPE_Q4_K, n);
    size_t q6_row_bytes = ggml_row_size(GGML_TYPE_Q6_K, n);
    size_t q8_row_bytes = ggml_row_size(GGML_TYPE_Q8_K, n);
    printf("row bytes: q4_K=%zu q6_K=%zu q8_K=%zu\n", w_row_bytes, q6_row_bytes, q8_row_bytes);

    std::vector<float> w((size_t) rows * n), act(n);
    std::vector<uint8_t> q4((size_t) rows * w_row_bytes), q6((size_t) rows * q6_row_bytes), q8(q8_row_bytes);
    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    long q4_ge_vs_v[4] = {0,0,0,0};
    long q4_ge_vs_g[4] = {0,0,0,0};
    long q4_v_vs_g[4]  = {0,0,0,0};
    long q6_v_vs_g[4]  = {0,0,0,0};
    float g_q4_maxrel = 0.0f, g_q4_maxulp = 0.0f;

    for (int it = 0; it < iters; ++it) {
        for (int r = 0; r < rows; ++r)
            for (int i = 0; i < n; ++i) w[(size_t) r*n + i] = dist(rng);
        for (int i = 0; i < n; ++i) act[i] = dist(rng);

        for (int r = 0; r < rows; ++r)
            quantize_row_q4_K(w.data() + (size_t) r*n, q4.data() + (size_t) r*w_row_bytes, n);
        quantize_row_q8_K(act.data(), q8.data(), n);

        ggml_context * rctx = ggml_init(ip);
        ggml_tensor * t = ggml_new_tensor_3d(rctx, GGML_TYPE_Q4_K, n, rows, 1);
        ggml_backend_buffer_type_t buft = ggml_backend_cpu_repack_buffer_type();
        size_t bsize = ggml_backend_buft_get_alloc_size(buft, t);
        ggml_backend_buffer_t buf = ggml_backend_buft_alloc_buffer(buft, bsize);
        if (!buf) { printf("FATAL: repack buffer alloc failed\n"); return 2; }
        ggml_backend_buffer_init_tensor(buf, t);
        t->buffer = buf;
        t->data   = (char *) ggml_backend_buffer_get_base(buf);
        buf->iface.set_tensor(buf, t, q4.data(), 0, (size_t) rows * w_row_bytes);
        const void * inter = ggml_backend_buffer_get_base(buf);

        std::vector<float> gv(rows), refv(rows), gow(rows);
        ggml_gemv_q4_K_8x8_q8_K(n, gv.data(), 0, inter, q8.data(), 1, rows);

        for (int r = 0; r < rows; ++r) {
            ggml_vec_dot_q4_K_q8_K(n, &refv[r], 0, q4.data() + (size_t) r*w_row_bytes, 0, q8.data(), 0, 1);
            int32_t rc = fg_cpu_simd_row_q4_k_q8_k({q4.data() + (size_t) r*w_row_bytes, (int64_t) w_row_bytes},
                                                   {q8.data(), (int64_t) q8_row_bytes}, (int32_t)(n/256), &gow[r]);
            if (rc != 0) { printf("FATAL: goose row rc=%d\n", rc); return 2; }
        }

        for (int r = 0; r < rows; ++r) {
            // same-index column, and best match across columns
            int match = -1;
            for (int c = 0; c < rows; ++c) if (bits(gv[c]) == bits(refv[r])) { match = c; break; }
            q4_ge_vs_v[3]++;
            if (match >= 0) q4_ge_vs_v[0]++; else q4_ge_vs_v[1]++;

            long best = -1;
            for (int c = 0; c < rows; ++c) {
                long d = labs((long) bits(gv[c]) - (long) bits(refv[r]));
                if (best < 0 || d < best) best = d;
            }
            if (best > q4_v_vs_g[2]) q4_v_vs_g[2] = best;
            {
                float a = refv[r], b = gv[r];
                float rel = fabsf(a - b) / (fabsf(a) > 1e-30f ? fabsf(a) : 1.0f);
                if (rel > g_q4_maxrel) g_q4_maxrel = rel;
                float ulp = 0.0f;
                uint32_t ua = bits(a), ub = bits(b);
                int64_t ia = (int32_t) ua, ib = (int32_t) ub;
                ulp = fabsf((float)(ia > ib ? ia - ib : ib - ia));
                if (ulp > g_q4_maxulp) g_q4_maxulp = ulp;
            }

            q4_ge_vs_g[3]++;
            if (bits(gv[r]) == bits(gow[r])) q4_ge_vs_g[0]++; else {
                q4_ge_vs_g[1]++;
                long d = labs((long) bits(gv[r]) - (long) bits(gow[r]));
                if (d > q4_ge_vs_g[2]) q4_ge_vs_g[2] = d;
            }
            q4_v_vs_g[3]++;
            if (bits(refv[r]) == bits(gow[r])) q4_v_vs_g[0]++; else q4_v_vs_g[1]++;
        }

        ggml_backend_buffer_free(buf);
        ggml_free(rctx);
    }

    printf("\nQ4_K cases: %ld\n", q4_ge_vs_v[3]);
    printf("  gemv column == vec_dot row : %ld equal, %ld unmatched\n", q4_ge_vs_v[0], q4_ge_vs_v[1]);
    printf("  gemv vs goose row object   : %ld equal, %ld differ, max raw-bit delta %ld\n", q4_ge_vs_g[0], q4_ge_vs_g[1], q4_ge_vs_g[2]);
    printf("  vec_dot vs goose row object: %ld equal, %ld differ\n", q4_v_vs_g[0], q4_v_vs_g[1]);
    printf("  gemv vs vec_dot            : max relative %.3e, max ulp %.0f\n", g_q4_maxrel, g_q4_maxulp);

    for (int it = 0; it < iters; ++it) {
        for (int r = 0; r < rows; ++r)
            for (int i = 0; i < n; ++i) w[(size_t) r*n + i] = dist(rng);
        for (int i = 0; i < n; ++i) act[i] = dist(rng);
        for (int r = 0; r < rows; ++r)
            quantize_row_q6_K(w.data() + (size_t) r*n, q6.data() + (size_t) r*q6_row_bytes, n);
        quantize_row_q8_K(act.data(), q8.data(), n);
        for (int r = 0; r < rows; ++r) {
            float ref, go;
            ggml_vec_dot_q6_K_q8_K(n, &ref, 0, q6.data() + (size_t) r*q6_row_bytes, 0, q8.data(), 0, 1);
            int32_t rc = fg_cpu_simd_row_q6_k_q8_k({q6.data() + (size_t) r*q6_row_bytes, (int64_t) q6_row_bytes},
                                                   {q8.data(), (int64_t) q8_row_bytes}, (int32_t)(n/256), &go);
            if (rc != 0) { printf("FATAL: goose q6 row rc=%d\n", rc); return 2; }
            q6_v_vs_g[3]++;
            if (bits(ref) == bits(go)) q6_v_vs_g[0]++; else q6_v_vs_g[1]++;
        }
    }
    printf("\nQ6_K cases: %ld\n", q6_v_vs_g[3]);
    printf("  vec_dot vs goose row object: %ld equal, %ld differ\n", q6_v_vs_g[0], q6_v_vs_g[1]);

    ggml_free(ctx);
    return 0;
}
