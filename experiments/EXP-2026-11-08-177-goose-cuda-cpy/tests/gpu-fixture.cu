// GPU differential fixture for the Goose CPY replacement.
//
// Contract: the candidate DSO redefines ggml_cuda_cpy / ggml_cuda_dup, which
// either run the Goose route or, with GGML_GOOSE_CUDA_CPY=0, the private-renamed
// native ones. This fixture builds real ggml tensors, invokes the public entry and
// compares the destination against an independent host reference.
//
// Cases exercise every route the operator selects on this profile:
//   route 0  same type, both contiguous -> device memcpy
//   route 1  2D strided memcpy
//   route 2  Goose scalar element walk (strided or reshaped)
//   route 3  Goose contiguous element walk (same type but not both contiguous)
#include "common.cuh"
void ggml_cuda_cpy(ggml_backend_cuda_context & ctx, const ggml_tensor * src0, ggml_tensor * src1);
#include <cstdio>
#include <cstring>
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <string>
#include <cstdlib>

static int g_checks, g_fail;
static void check(bool ok, const char * what) {
    ++g_checks;
    if (!ok) { ++g_fail; std::fprintf(stderr, "FAIL %s\n", what); }
}

struct Case {
    int64_t ne0[4];      // logical extents, shared by src and dst
    int     src_pad;     // extra elements appended to the src row (strided src)
    int     dst_pad;     // extra elements appended to the dst row
    const char * name;
};

static const Case CASES[] = {
    // fully contiguous, same type -> route 0
    {{64, 4, 1, 1}, 0, 0, "contig-basic"},
    {{256, 8, 2, 1}, 0, 0, "contig-planes"},
    // dst rows padded only: not both contiguous -> Goose route 3 or 1
    {{32, 4, 1, 1}, 0, 8, "dst-padded"},
    // src rows padded only
    {{32, 4, 1, 1}, 8, 0, "src-padded"},
    // both padded -> strided scalar walk
    {{16, 3, 1, 1}, 4, 4, "both-padded"},
    // larger strided case
    {{128, 6, 2, 1}, 16, 16, "strided-planes"},
};

int main(int argc, char ** argv) {
    (void) argc; (void) argv;
    int dev = 0;
    if (ggml_backend_cuda_get_device_count() < 1) { std::fprintf(stderr, "no CUDA device\n"); return 77; }
    cudaSetDevice(dev);
    std::mt19937 rng(177001);

    for (const Case & c : CASES) {
        const int64_t n = c.ne0[0]*c.ne0[1]*c.ne0[2]*c.ne0[3];
        // Dense element counts including the per-row padding.
        const int64_t rows = c.ne0[1]*c.ne0[2]*c.ne0[3];
        const int64_t src_row = c.ne0[0] + c.src_pad;
        const int64_t dst_row = c.ne0[0] + c.dst_pad;
        const int64_t src_n = src_row * rows;
        const int64_t dst_n = dst_row * rows;

        std::vector<float> hsrc(src_n), hdst(dst_n, -555.0f), ref(dst_n, -555.0f);
        std::uniform_real_distribution<float> dist(-5.0f, 5.0f);
        for (auto & v : hsrc) v = dist(rng);

        // Reference: dst[r* dst_row + i] = src[r* src_row + i] for i < ne0[0].
        for (int64_t r = 0; r < rows; ++r)
            for (int64_t i = 0; i < c.ne0[0]; ++i)
                ref[r*dst_row + i] = hsrc[r*src_row + i];

        float * ds = nullptr, * dd = nullptr;
        cudaMalloc(&ds, src_n*sizeof(float));
        cudaMalloc(&dd, dst_n*sizeof(float));
        cudaMemcpy(ds, hsrc.data(), src_n*sizeof(float), cudaMemcpyHostToDevice);
        cudaMemcpy(dd, hdst.data(), dst_n*sizeof(float), cudaMemcpyHostToDevice);

        ggml_tensor t0{}, t1{};
        for (int i = 0; i < 4; ++i) { t0.ne[i] = c.ne0[i]; t1.ne[i] = c.ne0[i]; }
        t0.type = t1.type = GGML_TYPE_F32;
        t0.data = ds; t1.data = dd;
        t0.nb[0] = sizeof(float); t1.nb[0] = sizeof(float);
        t0.nb[1] = src_row*sizeof(float); t1.nb[1] = dst_row*sizeof(float);
        t0.nb[2] = t0.nb[1]*t0.ne[1];    t1.nb[2] = t1.nb[1]*t1.ne[1];
        t0.nb[3] = t0.nb[2]*t0.ne[2];    t1.nb[3] = t1.nb[2]*t1.ne[2];
        t1.op = GGML_OP_CPY;
        t1.src[0] = &t0;

        ggml_backend_cuda_context ctx(dev);
        ggml_cuda_cpy(ctx, &t0, &t1);
        check(cudaDeviceSynchronize() == cudaSuccess, (std::string("sync ")+c.name).c_str());

        std::vector<float> got(dst_n);
        cudaMemcpy(got.data(), dd, dst_n*sizeof(float), cudaMemcpyDeviceToHost);
        if (const char * dir = getenv("FG177C_DUMP")) {
            std::string fn = std::string(dir) + "/" + c.name + ".bin";
            if (FILE * f = fopen(fn.c_str(), "wb")) { fwrite(got.data(), 1, got.size()*4, f); fclose(f); }
        }
        int64_t bad = 0;
        for (int64_t i = 0; i < dst_n; ++i) if (std::memcmp(&got[i], &ref[i], sizeof(float)) != 0) ++bad;
        if (bad) {
            int64_t first = -1;
            for (int64_t i = 0; i < dst_n; ++i) if (std::memcmp(&got[i], &ref[i], sizeof(float)) != 0) { first = i; break; }
            std::fprintf(stderr, "DIAG %s first_bad=%lld got=%f want=%f\n", c.name, (long long)first, got[first], ref[first]);
        }
        check(bad == 0, (std::string("bytes ")+c.name).c_str());
        std::vector<float> back(src_n);
        cudaMemcpy(back.data(), ds, src_n*sizeof(float), cudaMemcpyDeviceToHost);
        check(std::memcmp(back.data(), hsrc.data(), src_n*sizeof(float)) == 0, (std::string("src preserved ")+c.name).c_str());
        cudaFree(ds); cudaFree(dd);
    }
    std::printf("FG177C_GPU_FIXTURE checks=%d failures=%d cases=%zu\n",
                g_checks, g_fail, sizeof(CASES)/sizeof(CASES[0]));
    return g_fail ? 1 : 0;
}
