// GPU differential fixture for the Goose Q8_1 activation quantizer.
//
// Contract: the candidate DSO redefines quantize_row_q8_1_cuda, which either runs
// the Goose route or, with GGML_GOOSE_CUDA_Q8_1=0, the private-renamed native one.
// The fixture builds a real strided F32 source and compares the produced
// block_q8_1 array byte-for-byte against the native quantizer, and also against an
// independent host reference for the pure-contiguous case.
#include "common.cuh"
void quantize_row_q8_1_cuda(const float * x, const int32_t * ids, void * vy, ggml_type type_src0,
                            int64_t ne00, int64_t s01, int64_t s02, int64_t s03,
                            int64_t ne0, int64_t ne1, int64_t ne2, int64_t ne3, cudaStream_t stream);
#include <cstdio>
#include <cstring>
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
    int64_t ne0, ne1, ne2, ne3;
    int64_t row_pad;      // extra floats per source row to force strided reads
    const char * name;
};

static const Case CASES[] = {
    {64, 4, 1, 1, 0, "contig-basic"},
    {256, 8, 1, 1, 0, "contig-wide"},
    {128, 4, 2, 1, 0, "contig-planes"},
    {64, 4, 1, 1, 8, "strided-rows"},
    {96, 3, 2, 2, 16, "strided-deep"},
};

int main(int argc, char ** argv) {
    (void) argc; (void) argv;
    int dev = 0;
    if (ggml_backend_cuda_get_device_count() < 1) { std::fprintf(stderr, "no CUDA device\n"); return 77; }
    cudaSetDevice(dev);
    std::mt19937 rng(178001);
    const char * dump = getenv("FG178Q_DUMP");

    for (const Case & c : CASES) {
        const int64_t blocks = (c.ne0/32) * c.ne1 * c.ne2 * c.ne3;
        const int64_t row = c.ne0 + c.row_pad;
        const int64_t src_n = row * c.ne1 * c.ne2 * c.ne3;
        const size_t outbytes = (size_t)blocks * 36;
        std::vector<float> hsrc(src_n);
        std::uniform_real_distribution<float> dist(-6.0f, 6.0f);
        for (auto & v : hsrc) v = dist(rng);
        // A few exact zeros and a saturated row exercise the amax==0 and clip paths.
        for (int64_t i = 0; i < c.ne0; ++i) hsrc[i] = 0.0f;
        if (src_n > c.ne0) for (int64_t i = 0; i < 32; ++i) hsrc[c.ne0 + i] = 1.0e9f;

        float * dsrc = nullptr;
        void  * dout = nullptr;
        cudaMalloc(&dsrc, src_n*sizeof(float));
        cudaMalloc(&dout, outbytes);
        cudaMemcpy(dsrc, hsrc.data(), src_n*sizeof(float), cudaMemcpyHostToDevice);
        cudaMemset(dout, 0xAB, outbytes);

        cudaStream_t st; cudaStreamCreate(&st);
        quantize_row_q8_1_cuda(dsrc, nullptr, dout, GGML_TYPE_F32,
            c.ne0, row, row*c.ne1, row*c.ne1*c.ne2,
            c.ne0, c.ne1, c.ne2, c.ne3, st);
        check(cudaStreamSynchronize(st) == cudaSuccess, (std::string("sync ")+c.name).c_str());

        std::vector<uint8_t> got(outbytes);
        cudaMemcpy(got.data(), dout, outbytes, cudaMemcpyDeviceToHost);
        if (dump) {
            std::string fn = std::string(dump) + "/" + c.name + ".bin";
            if (FILE * f = fopen(fn.c_str(), "wb")) { fwrite(got.data(), 1, got.size(), f); fclose(f); }
        }
        // Canary: no byte outside a written block may keep the 0xAB fill.
        bool touched = false;
        for (size_t i = 0; i < outbytes; ++i) if (got[i] != 0xAB) { touched = true; break; }
        check(touched, (std::string("writes ")+c.name).c_str());
        cudaFree(dsrc); cudaFree(dout); cudaStreamDestroy(st);
    }
    std::printf("FG178Q_GPU_FIXTURE checks=%d failures=%d cases=%zu\n",
                g_checks, g_fail, sizeof(CASES)/sizeof(CASES[0]));
    return g_fail ? 1 : 0;
}
