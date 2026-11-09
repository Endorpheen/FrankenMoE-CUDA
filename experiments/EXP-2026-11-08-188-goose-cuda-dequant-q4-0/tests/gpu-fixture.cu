// GPU differential fixture for the Goose Q4_0 -> F16 dequantizer.
//
// The seam is `ggml_get_to_fp16_cuda(ggml_type)`. For Q4_0 the candidate returns the
// Goose implementation; with GGML_GOOSE_CUDA_DEQUANT_Q4_0=0 it returns the
// private-renamed native selector. Every other type is unaffected.
//
// The gate is bit-exact equality of the F16 output, which is the right criterion here:
// the transform is `d*(q&0xF)+dm` and `d*(q>>4)+dm` with a single rounding to half, so
// a tolerance would hide a wrong nibble order or a wrong -8d bias.
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

// block_q4_0 is 18 bytes: half d at 0, uint8 qs[16] at 2. QK4_0 = 32.
static const size_t Q40_BYTES = 18;

struct Case { int64_t nelem; const char * name; };

// The kernel works in blocks of 256 elements (8 q4_0 blocks of 32). Sizes are chosen
// so the tail CTA is exercised: 256 is exact, 512 too, 1024 exact, and 768 leaves the
// third CTA with a partial 8-block window.
static const Case CASES[] = {
    {  256, "e256"  },
    {  512, "e512"  },
    {  768, "e768"  },
    { 1024, "e1024" },
    { 4096, "e4096" },
};

int main(int argc, char ** argv) {
    (void) argc; (void) argv;
    if (ggml_backend_cuda_get_device_count() < 1) { std::fprintf(stderr, "no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937 rng(188001);
    const char * dump = getenv("FG188D_DUMP");
    const bool native_arm = getenv("GGML_GOOSE_CUDA_DEQUANT_Q4_0") &&
                            std::strcmp(getenv("GGML_GOOSE_CUDA_DEQUANT_Q4_0"), "0") == 0;

    for (const Case & c : CASES) {
        const int64_t nblk = c.nelem/32;
        const size_t src_bytes = (size_t)nblk*Q40_BYTES;
        std::vector<uint8_t> raw(src_bytes);
        for (auto & v : raw) v = (uint8_t)rng();
        // Small deltas so the F16 result stays finite and every nibble pattern shows.
        for (int64_t b = 0; b < nblk; ++b) {
            half d = __float2half(0.01f + 0.0005f*(b%13));
            std::memcpy(raw.data() + b*Q40_BYTES, &d, 2);
        }
        std::vector<half> out((size_t)c.nelem, __float2half(-7.0f));

        void * dsrc = nullptr; half * ddst = nullptr;
        cudaMalloc(&dsrc, src_bytes); cudaMalloc(&ddst, (size_t)c.nelem*2);
        cudaMemcpy(dsrc, raw.data(), src_bytes, cudaMemcpyHostToDevice);
        cudaMemset(ddst, 0xFF, (size_t)c.nelem*2);

        cudaStream_t st; cudaStreamCreate(&st);
        to_fp16_cuda_t fn = ggml_get_to_fp16_cuda(GGML_TYPE_Q4_0);
        check(fn != nullptr, (std::string("selector ")+c.name).c_str());
        if (fn) fn(dsrc, ddst, c.nelem, st);
        check(cudaStreamSynchronize(st) == cudaSuccess, (std::string("sync ")+c.name).c_str());

        cudaMemcpy(out.data(), ddst, (size_t)c.nelem*2, cudaMemcpyDeviceToHost);
        if (dump) {
            std::string f = std::string(dump) + "/" + c.name + ".bin";
            if (FILE * fp = fopen(f.c_str(), "wb")) { fwrite(out.data(), 2, out.size(), fp); fclose(fp); }
        }
        // Canary: the memset fill 0xFFFF is half -nan; every element must be written.
        size_t untouched = 0;
        for (half v : out) { uint16_t b; std::memcpy(&b, &v, 2); if (b == 0xFFFF) ++untouched; }
        check(untouched == 0, (std::string("all written ")+c.name).c_str());
        cudaFree(dsrc); cudaFree(ddst); cudaStreamDestroy(st);
    }
    std::printf("FG188D_GPU_FIXTURE checks=%d failures=%d arm=%s\n",
                g_checks, g_fail, native_arm ? "native" : "goose");
    return g_fail ? 1 : 0;
}
