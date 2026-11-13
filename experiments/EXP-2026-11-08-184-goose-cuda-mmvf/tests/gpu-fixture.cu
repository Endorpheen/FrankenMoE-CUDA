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

struct Case { int64_t ncols; int64_t nrows; int64_t extra; int64_t samples; int padded; int overlap; int bcast; const char * name; };

// ncols values span every block-size branch of the dispatcher: 256 and 512 stay at
// 32/64 threads, 2048 and 8192 climb to wider blocks, and odd multiples force the
// ceil in the column loop.
static const Case CASES[] = {
    {  256,   64, 1, 1, 0, 0, 0, "n256-r64"   },
    {  512,  128, 1, 1, 0, 0, 0, "n512-r128"  },
    { 2048,   33, 1, 1, 0, 0, 0, "n2048-r33"  },
    { 4096,    8, 1, 1, 0, 0, 0, "n4096-r8"   },
    { 8192,    5, 4, 1, 0, 0, 0, "n8192-r5-c4" },   // extra = channel count in src1
    // One destination column with the batch on dimension 3.
    // src0 carries ONE sample while src1 and dst carry two: the broadcast the harness builds
    // from bs=[1,1] with nr=[*,2].
    {  256,   16, 1, 2, 1, 1, 1, "n256-r16-s2-bcast"     },
    {  256,   16, 1, 2, 1, 0, 1, "n256-r16-s2-bcast-pad" },
    {    4,   16, 1, 2, 1, 1, 1, "n4-r16-s2-bcast"       },
    {    4,   16, 1, 2, 0, 0, 1, "n4-r16-s2-bcast-c"     },
    {    4,   16, 1, 2, 0, 0, 0, "n4-r16-s2"   },
    {    4,   16, 3, 2, 0, 0, 0, "n4-r16-c3-s2" },
    {    4,   16, 1, 1, 0, 0, 0, "n4-r16"      },
    {  256,   16, 1, 2, 0, 0, 0, "n256-r16-s2" },
    {  256,    4, 1, 2, 0, 0, 0, "n256-r4-s2"  },
    // Same shape, but laid out the way test-backend-ops builds it: the sources are views
    // into a base tensor twice as wide and as tall, so nb[1] and nb[2] carry padding.
    {  256,   16, 1, 2, 1, 0, 0, "n256-r16-s2-pad" },
    {  256,    4, 1, 2, 1, 0, 0, "n256-r4-s2-pad"  },
    // Exactly how the harness builds it: one base allocation, both sources are views into
    // it at offsets 0 and k*type_size, so they overlap.
    // The harness's bs=[3,2]: channels and samples both greater than one.
    {  256,   16, 3, 2, 1, 1, 0, "n256-r16-c3-s2-ovl" },
    {  256,   16, 3, 2, 1, 0, 0, "n256-r16-c3-s2-pad" },
    {  256,   16, 3, 2, 0, 0, 0, "n256-r16-c3-s2"     },
    {  256,   16, 1, 2, 1, 1, 0, "n256-r16-s2-ovl" },
    {  256,    4, 1, 2, 1, 1, 0, "n256-r4-s2-ovl"  },
};

int main(int argc, char ** argv) {
    (void) argc; (void) argv;
    if (ggml_backend_cuda_get_device_count() < 1) { std::fprintf(stderr, "no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937 rng(184001);
    const char * scale = getenv("FG184M_WIDE");
    std::uniform_real_distribution<float> dist(-2.0f, 2.0f);
    std::uniform_real_distribution<float> wide(-1.0e4f, 1.0e4f);
    const char * dump = getenv("FG184M_DUMP");
    const bool native_arm = getenv("GGML_GOOSE_CUDA_MMVF") &&
                            std::strcmp(getenv("GGML_GOOSE_CUDA_MMVF"), "0") == 0;

    for (const Case & c : CASES) {
        const int64_t nch = c.extra;
        const int64_t ns_buf = c.samples;
        // The padded layout leaves gaps, so the buffer has to be sized from the strides
        // rather than from the element count.
        const int64_t base_rows_b = c.nrows > 1 ? c.nrows : 1;
        const int64_t nb1_b = (c.padded ? 2*c.ncols : c.ncols) * 4;
        const int64_t nb2_b = (c.padded ? 2*c.ncols*base_rows_b : c.ncols*c.nrows) * 4;
        const int64_t nb3_b = c.padded ? nb2_b*ns_buf : nb2_b;
        const size_t xsz = (size_t)((ns_buf-1)*nb3_b/4 + (c.nrows-1)*nb1_b/4 + c.ncols + 1);
        const size_t ysz = (size_t)((ns_buf-1)*nb3_b/4 + (nch-1)*(c.padded ? nb2_b : nb1_b)/4 + c.ncols + 1);
        const size_t osz = (size_t)(c.nrows*nch*ns_buf);
        std::vector<float> x(xsz);
        for (auto & v : x) v = scale ? wide(rng) : dist(rng);
        std::vector<float> y(ysz);
        for (auto & v : y) v = scale ? wide(rng) : dist(rng);
        std::vector<float> out(osz, -3.5f);

        float * dx = nullptr, * dy = nullptr, * dd = nullptr;
        cudaMalloc(&dx, x.size()*sizeof(float));
        cudaMalloc(&dy, y.size()*sizeof(float));
        cudaMalloc(&dd, out.size()*sizeof(float));
        cudaMemcpy(dx, x.data(), x.size()*4, cudaMemcpyHostToDevice);
        if (c.overlap) cudaMemcpy(dy, y.data(), std::min(y.size(), x.size())*4, cudaMemcpyHostToDevice);
        cudaMemcpy(dy, y.data(), y.size()*4, cudaMemcpyHostToDevice);
        cudaMemset(dd, 0x7f, out.size()*sizeof(float));

        ggml_tensor s0{}, s1{}, d{};
        s0.type = s1.type = d.type = GGML_TYPE_F32;
        const int64_t ns = c.samples;
        // Padded layout: both sources are views into [2*ncols, max(nrows, n), 1, ns], so the
        // row pitch is 2*ncols and the channel pitch is the base row count.
        const int64_t base_rows = c.nrows > 1 ? c.nrows : 1;
        const int64_t nb1 = (c.padded ? 2*c.ncols : c.ncols) * 4;
        const int64_t nb2 = (c.padded ? 2*c.ncols*base_rows : c.ncols*c.nrows) * 4;
        s0.ne[0] = c.ncols; s0.ne[1] = c.nrows; s0.ne[2] = 1; s0.ne[3] = c.bcast ? 1 : ns;
        s0.nb[0] = 4; s0.nb[1] = nb1; s0.nb[2] = nb2; s0.nb[3] = nb2;
        s1.ne[0] = c.ncols; s1.ne[1] = 1; s1.ne[2] = nch; s1.ne[3] = ns;
        s1.nb[0] = 4; s1.nb[1] = nb1; s1.nb[2] = c.padded ? nb2 : nb1;
        s1.nb[3] = s1.nb[2];
        d.ne[0] = c.nrows; d.ne[1] = 1; d.ne[2] = nch; d.ne[3] = ns;
        d.nb[0] = 4; d.nb[1] = d.ne[0]*4; d.nb[2] = d.nb[1]; d.nb[3] = d.nb[2]*nch;
        // Overlap case: both sources live in the x buffer, as the harness arranges.
        s0.data = dx; s1.data = (c.overlap ? dx + c.ncols : dy); d.data = dd;

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
            for (int64_t s = 0; s < ns; ++s) {
              double worst = 0.0; int64_t worst_i = -1;
              // Walk the same strides the tensors were given, not a contiguous assumption.
              const size_t xb  = (size_t)s0.nb[3]/4*s;
              const size_t xr  = (size_t)s0.nb[1]/4;
              const size_t yb  = (size_t)s1.nb[3]/4*s;
              const size_t yc  = (size_t)s1.nb[2]/4;
              const size_t ob  = (size_t)d.nb[3]/4*s;
              const size_t oc  = (size_t)d.nb[2]/4;
              for (int64_t ch = 0; ch < nch; ++ch) {
                for (int64_t row = 0; row < c.nrows; ++row) {
                    double acc = 0.0;
                    for (int64_t col = 0; col < c.ncols; ++col) {
                        acc += (double)x[xb + (size_t)row*xr + (size_t)col] *
                               (double)y[yb + (size_t)ch*yc + (size_t)col];
                    }
                    const double got = out[ob + (size_t)ch*oc + (size_t)row];
                    const double rel = std::fabs(got - acc) / std::fmax(1e-30, std::fabs(acc));
                    if (rel > worst) { worst = rel; worst_i = ch*c.nrows + row; }
                }
              }
              std::printf("ORACLE %s sample=%lld worst_rel=%.3e at=%lld\n",
                          c.name, (long long)s, worst, (long long)worst_i);
            }
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
