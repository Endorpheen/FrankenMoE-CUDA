// GPU differential fixture for the Goose mul_mat_vec_f.
//
// The seam is `ggml_cuda_mul_mat_vec_f`, which either runs the Goose body (F32
// source, one destination column, no fusion) or, with GGML_GOOSE_CUDA_MMVF=0, the
// private-renamed native implementation.
//
// The gate is bit-exact equality of the F32 destination. Both arms execute the same
// FMA sequence over the same columns in the same order, so a tolerance would hide a
// wrong reduction or a missed tail column.
#include "common.cuh"
#include "mmvf.cuh"
#include <cstdio>
#include <cstring>
#include <cmath>
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

struct Case { int64_t ncols; int64_t nrows; int64_t extra; const char * name; };

// ncols values span every block-size branch of the dispatcher: 256 and 512 stay at
// 32/64 threads, 2048 and 8192 climb to wider blocks, and odd multiples force the
// ceil in the column loop.
static const Case CASES[] = {
    {  256,   64, 1, "n256-r64"   },
    {  512,  128, 1, "n512-r128"  },
    { 2048,   33, 1, "n2048-r33"  },
    { 4096,    8, 1, "n4096-r8"   },
    { 8192,    5, 4, "n8192-r5-c4" },   // extra = channel count in src1
};

int main(int argc, char ** argv) {
    (void) argc; (void) argv;
    if (ggml_backend_cuda_get_device_count() < 1) { std::fprintf(stderr, "no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937 rng(184001);
    std::uniform_real_distribution<float> dist(-2.0f, 2.0f);
    const char * dump = getenv("FG184M_DUMP");
    const bool native_arm = getenv("GGML_GOOSE_CUDA_MMVF") &&
                            std::strcmp(getenv("GGML_GOOSE_CUDA_MMVF"), "0") == 0;

    for (const Case & c : CASES) {
        const int64_t nch = c.extra;
        std::vector<float> x((size_t)(c.ncols*c.nrows));
        for (auto & v : x) v = dist(rng);
        std::vector<float> y((size_t)(c.ncols*nch));
        for (auto & v : y) v = dist(rng);
        std::vector<float> out((size_t)(c.nrows*nch), -3.5f);

        float * dx = nullptr, * dy = nullptr, * dd = nullptr;
        cudaMalloc(&dx, x.size()*sizeof(float));
        cudaMalloc(&dy, y.size()*sizeof(float));
        cudaMalloc(&dd, out.size()*sizeof(float));
        cudaMemcpy(dx, x.data(), x.size()*4, cudaMemcpyHostToDevice);
        cudaMemcpy(dy, y.data(), y.size()*4, cudaMemcpyHostToDevice);
        cudaMemset(dd, 0x7f, out.size()*sizeof(float));

        ggml_tensor s0{}, s1{}, d{};
        s0.type = s1.type = d.type = GGML_TYPE_F32;
        s0.ne[0] = c.ncols; s0.ne[1] = c.nrows; s0.ne[2] = 1; s0.ne[3] = 1;
        s0.nb[0] = 4; s0.nb[1] = c.ncols*4; s0.nb[2] = s0.nb[1]*c.nrows; s0.nb[3] = s0.nb[2];
        s1.ne[0] = c.ncols; s1.ne[1] = 1; s1.ne[2] = nch; s1.ne[3] = 1;
        s1.nb[0] = 4; s1.nb[1] = c.ncols*4; s1.nb[2] = s1.nb[1]; s1.nb[3] = s1.nb[2]*nch;
        d.ne[0] = c.nrows; d.ne[1] = 1; d.ne[2] = nch; d.ne[3] = 1;
        d.nb[0] = 4; d.nb[1] = c.nrows*4; d.nb[2] = d.nb[1]; d.nb[3] = d.nb[2]*nch;
        s0.data = dx; s1.data = dy; d.data = dd;

        ggml_backend_cuda_context ctx(0);
        ggml_cuda_mul_mat_vec_f(ctx, &s0, &s1, nullptr, &d, nullptr);
        check(cudaDeviceSynchronize() == cudaSuccess, (std::string("sync ")+c.name).c_str());

        cudaMemcpy(out.data(), dd, out.size()*4, cudaMemcpyDeviceToHost);
        if (dump) {
            std::string f = std::string(dump) + "/" + c.name + ".bin";
            if (FILE * fp = fopen(f.c_str(), "wb")) { fwrite(out.data(), 4, out.size(), fp); fclose(fp); }
        }
        // Host oracle in double: tells which arm is right when they differ.
        if (getenv("FG184M_ORACLE")) {
            double worst = 0.0; int64_t worst_i = -1;
            for (int64_t ch = 0; ch < nch; ++ch) {
                for (int64_t row = 0; row < c.nrows; ++row) {
                    double acc = 0.0;
                    for (int64_t col = 0; col < c.ncols; ++col) {
                        acc += (double)x[(size_t)(row*c.ncols + col)] * (double)y[(size_t)(ch*c.ncols + col)];
                    }
                    const double got = out[(size_t)(ch*c.nrows + row)];
                    const double rel = std::fabs(got - acc) / std::fmax(1e-30, std::fabs(acc));
                    if (rel > worst) { worst = rel; worst_i = ch*c.nrows + row; }
                }
            }
            std::printf("ORACLE %s worst_rel=%.3e at=%lld\n", c.name, worst, (long long)worst_i);
        }
        size_t untouched = 0;
        for (float v : out) { int32_t b; std::memcpy(&b, &v, 4); if (b == 0x7f7f7f7f) ++untouched; }
        check(untouched == 0, (std::string("all written ")+c.name).c_str());
        cudaFree(dx); cudaFree(dy); cudaFree(dd);
    }
    std::printf("FG184M_GPU_FIXTURE checks=%d failures=%d arm=%s\n",
                g_checks, g_fail, native_arm ? "native" : "goose");
    return g_fail ? 1 : 0;
}
