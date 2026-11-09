// GPU differential fixture for the Goose FWHT.
//
// The seam is `ggml_cuda_op_fwht`, which either runs the Goose warp path (F32 source,
// N in {64,128,256,512}) or, with GGML_GOOSE_CUDA_FWHT=0, the private-renamed native
// implementation. Wider sizes and half sources fall back inside the candidate, which
// the fixture does not exercise.
//
// The gate is bit-exact equality between the two arms. The transform is a fixed
// sequence of adds and subtractions scaled by 1/sqrt(N), so byte equality is the
// right criterion; a float tolerance would hide a wrong butterfly stage.
#include "common.cuh"
#include "fwht.cuh"
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

struct Case { int64_t n; int64_t rows; const char * name; };

static const Case CASES[] = {
    { 64,   1, "n64-r1"   },
    { 64,  17, "n64-r17"  },   // rows not a multiple of the 4 rows per block
    {128,   4, "n128-r4"  },
    {256,   9, "n256-r9"  },
    {512,  33, "n512-r33" },
};

int main(int argc, char ** argv) {
    (void) argc; (void) argv;
    if (ggml_backend_cuda_get_device_count() < 1) { std::fprintf(stderr, "no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937 rng(183001);
    std::uniform_real_distribution<float> dist(-3.0f, 3.0f);
    const char * dump = getenv("FG183F_DUMP");
    const bool native_arm = getenv("GGML_GOOSE_CUDA_FWHT") &&
                            std::strcmp(getenv("GGML_GOOSE_CUDA_FWHT"), "0") == 0;

    for (const Case & c : CASES) {
        const int64_t n_el = c.n * c.rows;
        std::vector<float> xs(n_el);
        for (auto & v : xs) v = dist(rng);

        float * ds = nullptr, * dd = nullptr;
        cudaMalloc(&ds, n_el*sizeof(float));
        cudaMalloc(&dd, n_el*sizeof(float));
        cudaMemcpy(ds, xs.data(), n_el*sizeof(float), cudaMemcpyHostToDevice);
        cudaMemset(dd, 0xCD, n_el*sizeof(float));

        ggml_tensor ts{}, td{};
        ts.ne[0] = c.n; ts.ne[1] = c.rows; ts.ne[2] = 1; ts.ne[3] = 1;
        td.ne[0] = c.n; td.ne[1] = c.rows; td.ne[2] = 1; td.ne[3] = 1;
        ts.type = td.type = GGML_TYPE_F32;
        ts.data = ds; td.data = dd;
        ts.nb[0] = 4; ts.nb[1] = c.n*4; ts.nb[2] = ts.nb[1]*c.rows; ts.nb[3] = ts.nb[2];
        td.nb[0] = 4; td.nb[1] = c.n*4; td.nb[2] = td.nb[1]*c.rows; td.nb[3] = td.nb[2];

        ggml_backend_cuda_context ctx(0);
        const bool ok = ggml_cuda_op_fwht(ctx, &ts, &td);
        check(ok, (std::string("handled ")+c.name).c_str());
        check(cudaDeviceSynchronize() == cudaSuccess, (std::string("sync ")+c.name).c_str());

        std::vector<float> out(n_el);
        cudaMemcpy(out.data(), dd, n_el*sizeof(float), cudaMemcpyDeviceToHost);
        if (dump) {
            std::string f = std::string(dump) + "/" + c.name + ".bin";
            if (FILE * fp = fopen(f.c_str(), "wb")) { fwrite(out.data(), 4, out.size(), fp); fclose(fp); }
        }
        // Every element must be written (the memset fill is 0xCDCDCDCD).
        size_t untouched = 0;
        for (int64_t i = 0; i < n_el; ++i) {
            int32_t bits; std::memcpy(&bits, &out[i], 4);
            if (bits == (int32_t)0xCDCDCDCD) ++untouched;
        }
        check(untouched == 0, (std::string("all written ")+c.name).c_str());
        cudaFree(ds); cudaFree(dd);
    }
    std::printf("FG183F_GPU_FIXTURE checks=%d failures=%d arm=%s\n",
                g_checks, g_fail, native_arm ? "native" : "goose");
    return g_fail ? 1 : 0;
}
