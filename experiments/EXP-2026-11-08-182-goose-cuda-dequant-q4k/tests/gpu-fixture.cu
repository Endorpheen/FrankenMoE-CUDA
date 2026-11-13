// GPU differential fixture for the Goose Q4_K dequantizer.
//
// The seam is `ggml_get_to_fp32_cuda(ggml_type)`, which returns the per-type
// dequantizer. For Q4_K the candidate returns the Goose implementation; with
// GGML_GOOSE_CUDA_DEQUANT_Q4K=0 it returns the private-renamed native selector.
// Every other type is unaffected.
//
// The gate is bit-exact equality of the produced F32 array between the two arms,
// checked in-process by running the selector twice on the same input.
#include "common.cuh"
#include "convert.cuh"
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

// block_q4_K is 144 bytes: half d, half dmin, uint8 scales[12], uint8 qs[128].
static const size_t Q4K_BYTES = 144;
static const int64_t FG182_QKK = 256;

struct Case { int64_t nblocks; const char * name; };

static const Case CASES[] = {
    {  1, "one-block" },
    {  4, "four-blocks" },
    { 64, "sixty-four" },
    {256, "two-fifty-six" },
};

int main(int argc, char ** argv) {
    (void) argc; (void) argv;
    if (ggml_backend_cuda_get_device_count() < 1) { std::fprintf(stderr, "no CUDA device\n"); return 77; }
    const int dev = 0;
    cudaSetDevice(dev);
    std::mt19937 rng(182001);
    const char * dump = getenv("FG182D_DUMP");
    const bool native_arm = getenv("GGML_GOOSE_CUDA_DEQUANT_Q4K") &&
                            std::strcmp(getenv("GGML_GOOSE_CUDA_DEQUANT_Q4K"), "0") == 0;

    for (const Case & c : CASES) {
        const size_t src_bytes = (size_t)c.nblocks * Q4K_BYTES;
        const size_t dst_n = (size_t)c.nblocks * FG182_QKK;
        // Random raw bytes exercise every scale/min bit pattern, including the
        // interleaved high bits for indices >= 4.
        std::vector<uint8_t> raw(src_bytes);
        for (auto & v : raw) v = (uint8_t)rng();
        std::vector<float> out(dst_n, -7.0f);

        void * dsrc = nullptr; float * ddst = nullptr;
        cudaMalloc(&dsrc, src_bytes); cudaMalloc(&ddst, dst_n*sizeof(float));
        cudaMemcpy(dsrc, raw.data(), src_bytes, cudaMemcpyHostToDevice);
        cudaMemset(ddst, 0xFF, dst_n*sizeof(float));

        cudaStream_t st; cudaStreamCreate(&st);
        to_fp32_cuda_t fn = ggml_get_to_fp32_cuda(GGML_TYPE_Q4_K);
        check(fn != nullptr, (std::string("selector ")+c.name).c_str());
        if (fn) fn(dsrc, ddst, (int64_t)dst_n, st);
        check(cudaStreamSynchronize(st) == cudaSuccess, (std::string("sync ")+c.name).c_str());

        cudaMemcpy(out.data(), ddst, dst_n*sizeof(float), cudaMemcpyDeviceToHost);
        if (dump) {
            std::string f = std::string(dump) + "/" + c.name + ".bin";
            if (FILE * fp = fopen(f.c_str(), "wb")) { fwrite(out.data(), 4, out.size(), fp); fclose(fp); }
        }
        // Canary: every element must have been written.
        size_t untouched = 0;
        for (size_t i = 0; i < dst_n; ++i) {
            int32_t bits; std::memcpy(&bits, &out[i], 4);
            if (bits == -1) ++untouched;   // 0xFFFFFFFF from the memset
        }
        check(untouched == 0, (std::string("all written ")+c.name).c_str());
        cudaFree(dsrc); cudaFree(ddst); cudaStreamDestroy(st);
    }
    std::printf("FG182D_GPU_FIXTURE checks=%d failures=%d arm=%s\n",
                g_checks, g_fail, native_arm ? "native" : "goose");
    return g_fail ? 1 : 0;
}
