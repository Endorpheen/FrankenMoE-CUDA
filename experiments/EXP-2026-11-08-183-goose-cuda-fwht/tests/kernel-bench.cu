// Kernel-time microbenchmark for the Goose FWHT against the native one.
//
// The Apex pair showed a decode regression with the Goose arm, while FWHT is only
// ~0.3% of GPU time in the census. Before attributing anything, measure the kernel
// itself: if the Goose body is several times slower per launch, and this op sits on
// the per-token path of the delta-net attention, the decode cost follows without any
// other explanation being needed.
//
// Both arms run in the same process through `ggml_cuda_op_fwht`, so the DSO, the
// driver and the clocks are shared. GGML_GOOSE_CUDA_FWHT selects the arm, and each
// arm is warmed up before timing.
#include "common.cuh"
#include "fwht.cuh"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <string>
#include <algorithm>

static float median(std::vector<float> v) {
    std::sort(v.begin(), v.end());
    return v[v.size()/2];
}

int main(int argc, char ** argv) {
    if (ggml_backend_cuda_get_device_count() < 1) { std::fprintf(stderr, "no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    const int64_t rows = argc > 1 ? atoll(argv[1]) : 256;
    const int64_t n    = argc > 2 ? atoll(argv[2]) : 64;
    const int reps     = argc > 3 ? atoi(argv[3]) : 200;

    const int64_t n_el = n*rows;
    std::vector<float> xs(n_el);
    std::mt19937 rng(183002);
    std::uniform_real_distribution<float> dist(-1.f, 1.f);
    for (auto & v : xs) v = dist(rng);

    float * ds = nullptr, * dd = nullptr;
    cudaMalloc(&ds, n_el*sizeof(float));
    cudaMalloc(&dd, n_el*sizeof(float));
    cudaMemcpy(ds, xs.data(), n_el*sizeof(float), cudaMemcpyHostToDevice);

    ggml_tensor ts{}, td{};
    for (int i = 0; i < 4; ++i) { ts.ne[i] = td.ne[i] = 1; }
    ts.ne[0] = td.ne[0] = n; ts.ne[1] = td.ne[1] = rows;
    ts.type = td.type = GGML_TYPE_F32;
    ts.data = ds; td.data = dd;
    ts.nb[0] = td.nb[0] = 4;
    ts.nb[1] = td.nb[1] = n*4;
    ts.nb[2] = td.nb[2] = ts.nb[1]*rows;
    ts.nb[3] = td.nb[3] = ts.nb[2];

    ggml_backend_cuda_context ctx(0);
    const bool is_native = getenv("GGML_GOOSE_CUDA_FWHT") &&
                           std::strcmp(getenv("GGML_GOOSE_CUDA_FWHT"), "0") == 0;

    std::vector<float> ms;
    for (int i = 0; i < reps; ++i) {
        cudaEvent_t a, b;
        cudaEventCreate(&a); cudaEventCreate(&b);
        cudaEventRecord(a);
        ggml_cuda_op_fwht(ctx, &ts, &td);
        cudaEventRecord(b);
        cudaEventSynchronize(b);
        float t = 0;
        cudaEventElapsedTime(&t, a, b);
        if (i >= reps/4) ms.push_back(t);   // discard warmup
        cudaEventDestroy(a); cudaEventDestroy(b);
    }
    const float med = median(ms);
    std::printf("FG183F_MICROBENCH arm=%s rows=%lld n=%lld launches=%zu median_us=%.3f min_us=%.3f max_us=%.3f\n",
                is_native ? "native" : "goose", (long long)rows, (long long)n,
                ms.size(), med*1000.0, *std::min_element(ms.begin(), ms.end())*1000.0,
                *std::max_element(ms.begin(), ms.end())*1000.0);
    cudaFree(ds); cudaFree(dd);
    return 0;
}
