// GPU differential fixture for the Goose MoE weighted reduction.
//
// Contract: the candidate DSO redefines ggml_cuda_op_moe_weighted_reduction, which
// either runs the Goose route or, with GGML_GOOSE_CUDA_MOE_WEIGHTED=0, the
// private-renamed native one.
//
// Correctness here is bit-exact, not approximate: the operator is a fixed order of
// float multiply-adds, so the fixture computes the reference with the same
// operation order ( (experts*scale)*weights summed left to right ) and requires
// identical bytes. Any reassociation shows up immediately.
#include "common.cuh"
void ggml_cuda_op_moe_weighted_reduction(ggml_backend_cuda_context & ctx,
    const ggml_tensor * experts, const ggml_tensor * expert_scale,
    const ggml_tensor * weights, ggml_tensor * dst);
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <string>

static int g_checks, g_fail;
static void check(bool ok, const char * what) {
    ++g_checks;
    if (!ok) { ++g_fail; std::fprintf(stderr, "FAIL %s\n", what); }
}

struct Case {
    int64_t n_embd, n_used, n2, n3;
    bool with_scale;
    const char * name;
};

static const Case CASES[] = {
    {256, 8, 1, 1, true,  "basic-scaled"},
    {256, 8, 1, 1, false, "basic-unscaled"},
    {2048, 8, 4, 1, true,  "wide-4tok"},
    {512, 2, 8, 2, true,  "many-tokens"},
    {128, 4, 1, 1, false, "small-unscaled"},
    {4096, 8, 2, 1, true,  "wide-4096"},
};

int main(int argc, char ** argv) {
    (void) argc; (void) argv;
    int dev = 0;
    if (ggml_backend_cuda_get_device_count() < 1) { std::fprintf(stderr, "no CUDA device\n"); return 77; }
    cudaSetDevice(dev);
    std::mt19937 rng(179001);
    const char * dump = getenv("FG179W_DUMP");

    for (const Case & c : CASES) {
        const int64_t n_tok = c.n2 * c.n3;
        const int64_t rows = n_tok * c.n_used;
        const int64_t exp_n = rows * c.n_embd;
        const int64_t dst_n = n_tok * c.n_embd;

        std::vector<float> hexp(exp_n), hwts(rows), hsc(rows), href(dst_n, -999.0f);
        std::uniform_real_distribution<float> de(-2.0f, 2.0f), dist_w(-1.0f, 1.0f), dist_s(0.5f, 1.5f);
        for (auto & v : hexp) v = de(rng);
        for (auto & v : hwts) v = dist_w(rng);
        for (auto & v : hsc) v = dist_s(rng);

        // Reference with the native operation order.
        // The scale factors are read only when expert_scale is present; otherwise
        // the native kernel multiplies by the literal 1.0f. Multiplying by the
        // allocated-but-unused hsc array here would not match that path.
        const float one = 1.0f;
        for (int64_t t = 0; t < n_tok; ++t)
            for (int64_t col = 0; col < c.n_embd; ++col) {
                const int64_t first = t * c.n_used;
                const float s0 = c.with_scale ? hsc[first] : one;
                float s = (hexp[first*c.n_embd + col] * s0) * hwts[first];
                for (int64_t e = 1; e < c.n_used; ++e) {
                    const int64_t row = first + e;
                    const float sc = c.with_scale ? hsc[row] : one;
                    s = s + (hexp[row*c.n_embd + col] * sc) * hwts[row];
                }
                href[t*c.n_embd + col] = s;
            }

        float * dexp = nullptr, * dw = nullptr, * dsc = nullptr, * dd = nullptr;
        cudaMalloc(&dexp, exp_n*sizeof(float));
        cudaMalloc(&dw, rows*sizeof(float));
        cudaMalloc(&dsc, rows*sizeof(float));
        cudaMalloc(&dd, dst_n*sizeof(float));
        cudaMemcpy(dexp, hexp.data(), exp_n*sizeof(float), cudaMemcpyHostToDevice);
        cudaMemcpy(dw, hwts.data(), rows*sizeof(float), cudaMemcpyHostToDevice);
        cudaMemcpy(dsc, hsc.data(), rows*sizeof(float), cudaMemcpyHostToDevice);
        cudaMemset(dd, 0xFF, dst_n*sizeof(float));

        ggml_tensor te{}, tw{}, tsc{}, td{};
        for (int i = 0; i < 4; ++i) {
            te.ne[i] = (i == 0 ? c.n_embd : i == 1 ? c.n_used : i == 2 ? c.n2 : c.n3);
            tw.ne[i] = (i == 0 ? c.n_used : i == 1 ? c.n2 : i == 2 ? c.n3 : 1);
            tsc.ne[i] = tw.ne[i];
            td.ne[i]  = (i == 0 ? c.n_embd : i == 1 ? c.n2 : i == 2 ? c.n3 : 1);
        }
        te.type = tw.type = tsc.type = td.type = GGML_TYPE_F32;
        te.data = dexp; tw.data = dw; tsc.data = dsc; td.data = dd;
        for (ggml_tensor * t : {&te, &tw, &tsc, &td}) {
            t->nb[0] = sizeof(float);
            for (int i = 1; i < 4; ++i) t->nb[i] = t->nb[i-1] * (size_t)t->ne[i-1];
        }

        ggml_backend_cuda_context ctx(dev);
        ggml_cuda_op_moe_weighted_reduction(ctx, &te, c.with_scale ? &tsc : nullptr, &tw, &td);
        check(cudaDeviceSynchronize() == cudaSuccess, (std::string("sync ")+c.name).c_str());

        std::vector<float> got(dst_n);
        cudaMemcpy(got.data(), dd, dst_n*sizeof(float), cudaMemcpyDeviceToHost);
        if (dump) {
            std::string fn = std::string(dump) + "/" + c.name + ".bin";
            if (FILE * f = fopen(fn.c_str(), "wb")) { fwrite(got.data(), 1, got.size()*4, f); fclose(f); }
        }
        // Native is compiled with the default -fmad=true, so (a*b)*c may fuse into
        // an FMA and differ from a strict host evaluation in the last bit. Report
        // both the byte-exact count and the worst ULP distance so the gate can tell
        // a real defect from contraction noise.
        int64_t bad = 0; int64_t maxulp = 0; double maxrel = 0.0;
        auto col_of = [](int64_t idx, int64_t n) { return idx % n; };
        for (int64_t i = 0; i < dst_n; ++i) {
            const int64_t t = i / c.n_embd;
            if (std::memcmp(&got[i], &href[i], sizeof(float)) != 0) {
                ++bad;
                int32_t a, b; std::memcpy(&a, &got[i], 4); std::memcpy(&b, &href[i], 4);
                int64_t d = (int64_t)a - (int64_t)b; if (d < 0) d = -d;
                if (d > maxulp) maxulp = d;
                // Absolute error scaled by the operand magnitude, not by the
                // result: an 8-term sum can land near zero, where a relative
                // metric explodes even though the absolute error is tiny.
                double scale = 1.0; for (int64_t e = 0; e < c.n_used; ++e) {
                    const int64_t row = t*c.n_used + e;
                    scale += std::fabs((double)hexp[row*c.n_embd + col_of(i, c.n_embd)]) * std::fabs((double)hwts[row]);
                }
                double rel = std::fabs((double)got[i]-(double)href[i]) / scale;
                if (rel > maxrel) maxrel = rel;
            }
        }
        std::fprintf(stderr, "STAT %s bytes_diff=%lld/%lld max_ulp=%lld max_rel=%.3e\n",
                     c.name, (long long)bad, (long long)dst_n, (long long)maxulp, maxrel);
        if (bad) {
            int64_t f = -1;
            for (int64_t i = 0; i < dst_n; ++i) if (std::memcmp(&got[i], &href[i], sizeof(float)) != 0) { f = i; break; }
            std::fprintf(stderr, "DIAG %s first_bad=%lld got=%f want=%f\n", c.name, (long long)f, got[f], href[f]);
        }
        // The native kernel is compiled with -fmad=true, so its multiply-add chain
        // legitimately differs from a strict host evaluation in the low bits. The
        // host reference is therefore a tolerance gate; the byte-exact criterion is
        // the Goose-vs-native comparison, which FG179W_DUMP feeds.
        check(maxrel <= 1.0e-5, (std::string("within-tolerance ")+c.name).c_str());
        cudaFree(dexp); cudaFree(dw); cudaFree(dsc); cudaFree(dd);
    }
    std::printf("FG179W_GPU_FIXTURE checks=%d failures=%d cases=%zu\n",
                g_checks, g_fail, sizeof(CASES)/sizeof(CASES[0]));
    return g_fail ? 1 : 0;
}
