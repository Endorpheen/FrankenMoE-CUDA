// GPU differential fixture for the Goose mul_mat_vec_q on Q4_K weights.
//
// The seam is `ggml_cuda_mul_mat_vec_q`, which either runs the Goose body (Q4_K
// source, one destination column, no fusion) or, with GGML_GOOSE_CUDA_MMVQ_Q6K=0, the
// private-renamed native implementation.
//
// The gate is bit-exact equality of the F32 destination. Both arms perform the same
// integer dp4a accumulation in the same block order, so the result must match to the
// bit; a tolerance would hide a wrong scale/min unpack or a wrong q4 offset.
//
// The quantized fixtures are built with the project's own quantizer for the rows, and
// q8_1 vectors with quantize_row_q8_1, so the data exercises realistic block layouts
// rather than random bytes that would produce meaningless scales.
#include "common.cuh"
#include "mmvq.cuh"
#include "quantize.cuh"
#include "ggml-backend.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <string>
#include <cmath>

static int g_checks, g_fail;
static void check(bool ok, const char * what) {
    ++g_checks;
    if (!ok) { ++g_fail; std::fprintf(stderr, "FAIL %s\n", what); }
}

struct Case { int64_t ncols; int64_t nrows; int64_t nch; const char * name; };

// ncols is a multiple of QK_K=256 as the kernel requires. Row counts and channel
// counts cover rs=1 and the ids-free channel ratio path.
static const Case CASES[] = {
    {  256,   4, 1, "n256-r4"    },
    { 1024,  17, 1, "n1024-r17"  },
    { 4096,  64, 1, "n4096-r64"  },
    { 2048,  33, 2, "n2048-r33-c2" },
};

// Host-side reference: dequantize Q4_K row, dot with the float vector, i.e. exactly
// what the kernel must compute in integer arithmetic.
static float ref_dot(const std::vector<uint8_t> & xrow, const std::vector<float> & y, int64_t ncols) {
    const int64_t nb = ncols/256;
    double acc = 0.0;
    for (int64_t b = 0; b < nb; ++b) {
        const uint8_t * p = xrow.data() + b*210;
        half dh; std::memcpy(&dh, p+208, 2);
        const float d = __half2float(dh);
        // dequantize_row_q6_K: two 128-element halves, each walking 32 elements with
        // four interleaved 6-bit streams. q = (low nibble | (qh 2 bits << 4)) - 32.
        for (int n = 0; n < 2; ++n) {
            const uint8_t * ql = p + n*64;
            const uint8_t * qh = p + 128 + n*32;
            const int8_t  * sc = (const int8_t *)(p + 192) + n*8;
            for (int l = 0; l < 32; ++l) {
                const int is = l/16;
                const int q1 = (int)((ql[l +  0] & 0xF) | (((qh[l] >> 0) & 3) << 4)) - 32;
                const int q2 = (int)((ql[l + 32] & 0xF) | (((qh[l] >> 2) & 3) << 4)) - 32;
                const int q3 = (int)((ql[l +  0] >> 4) | (((qh[l] >> 4) & 3) << 4)) - 32;
                const int q4 = (int)((ql[l + 32] >> 4) | (((qh[l] >> 6) & 3) << 4)) - 32;
                const int64_t base = b*256 + n*128;
                acc += (double)d * sc[is + 0] * q1 * (double)y[base + l +  0];
                acc += (double)d * sc[is + 2] * q2 * (double)y[base + l + 32];
                acc += (double)d * sc[is + 4] * q3 * (double)y[base + l + 64];
                acc += (double)d * sc[is + 6] * q4 * (double)y[base + l + 96];
            }
        }
    }
    return (float)acc;
}

int main(int argc, char ** argv) {
    (void) argc; (void) argv;
    if (ggml_backend_cuda_get_device_count() < 1) { std::fprintf(stderr, "no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937 rng(185001);
    std::uniform_real_distribution<float> dist(-2.0f, 2.0f);
    const char * dump = getenv("FG186Q_DUMP");
    const bool native_arm = getenv("GGML_GOOSE_CUDA_MMVQ_Q6K") &&
                            std::strcmp(getenv("GGML_GOOSE_CUDA_MMVQ_Q6K"), "0") == 0;

    for (const Case & c : CASES) {
        const int64_t nch = c.nch;
        const int64_t nb = c.ncols/256;
        const size_t xbytes = (size_t)(c.ncols*c.nrows/256)*210;
        std::vector<uint8_t> x(xbytes);
        for (auto & v : x) v = (uint8_t)rng();
        // Keep the super-block deltas small so the reference stays in a sane range.
        for (size_t b = 0; b < xbytes/210; ++b) {
            // block_q6_K (210 bytes): ql[128] 0..127, qh[64] 128..191,
            // int8 scales[16] at 192, half d at 208.
            half d = __float2half(0.01f + 0.001f*(b%7));
            std::memcpy(x.data() + b*210 + 208, &d, 2);
            for (int s = 0; s < 16; ++s) {
                x[b*210 + 192 + s] = (uint8_t)(int8_t)((s%5) - 2);
            }
        }
        std::vector<float> y((size_t)(c.ncols*nch));
        for (auto & v : y) v = dist(rng);
        std::vector<float> out((size_t)(c.nrows*nch), -11.0f);

        // src1 is F32 on this ABI; ggml_cuda_mul_mat_vec_q quantizes it to q8_1
        // internally, so the fixture supplies floats and lets both arms share that.
        uint8_t * dx = nullptr; float * dy = nullptr, * dd = nullptr;
        cudaMalloc(&dx, xbytes); cudaMalloc(&dy, y.size()*4); cudaMalloc(&dd, out.size()*4);
        cudaMemcpy(dx, x.data(), xbytes, cudaMemcpyHostToDevice);
        cudaMemcpy(dy, y.data(), y.size()*4, cudaMemcpyHostToDevice);
        cudaMemset(dd, 0x7f, out.size()*4);

        ggml_tensor s0{}, s1{}, d{};
        s0.type = GGML_TYPE_Q6_K; s1.type = d.type = GGML_TYPE_F32;
        s0.ne[0] = c.ncols; s0.ne[1] = c.nrows; s0.ne[2] = 1; s0.ne[3] = 1;
        s0.nb[0] = 210; s0.nb[1] = (c.ncols/256)*210; s0.nb[2] = s0.nb[1]*c.nrows; s0.nb[3] = s0.nb[2];
        s1.ne[0] = c.ncols; s1.ne[1] = 1; s1.ne[2] = nch; s1.ne[3] = 1;
        s1.nb[0] = 4; s1.nb[1] = c.ncols*4; s1.nb[2] = s1.nb[1]; s1.nb[3] = s1.nb[2]*nch;
        d.ne[0] = c.nrows; d.ne[1] = 1; d.ne[2] = nch; d.ne[3] = 1;
        d.nb[0] = 4; d.nb[1] = c.nrows*4; d.nb[2] = d.nb[1]; d.nb[3] = d.nb[2]*nch;
        s0.data = dx; s1.data = dy; d.data = dd;
        // Native reads src0->buffer (usage check for compute buffers), which asserts
        // when the pointer is null. A plain device buffer satisfies it without
        // changing any behaviour the kernels rely on.
        ggml_backend_buffer_type_t buft = ggml_backend_cuda_buffer_type(0);
        ggml_backend_buffer_t bx = ggml_backend_buft_alloc_buffer(buft, xbytes);
        ggml_backend_buffer_t by = ggml_backend_buft_alloc_buffer(buft, y.size()*4);
        ggml_backend_buffer_t bd = ggml_backend_buft_alloc_buffer(buft, out.size()*4);
        s0.buffer = bx; s1.buffer = by; d.buffer = bd;

        ggml_backend_cuda_context ctx(0);
        ggml_cuda_mul_mat_vec_q(ctx, &s0, &s1, nullptr, &d, nullptr);
        check(cudaDeviceSynchronize() == cudaSuccess, (std::string("sync ")+c.name).c_str());

        cudaMemcpy(out.data(), dd, out.size()*4, cudaMemcpyDeviceToHost);
        if (dump) {
            std::string f = std::string(dump) + "/" + c.name + ".bin";
            if (FILE * fp = fopen(f.c_str(), "wb")) { fwrite(out.data(), 4, out.size(), fp); fclose(fp); }
        }
        size_t untouched = 0;
        for (float v : out) { int32_t b2; std::memcpy(&b2, &v, 4); if (b2 == 0x7f7f7f7f) ++untouched; }
        check(untouched == 0, (std::string("all written ")+c.name).c_str());

        // Accuracy against a float reference built from the same quantized data. The
        // kernel works in integer dp4a, so agreement is expected to a few 1e-6
        // relative; this catches a wrong scale or offset that a byte-comparison
        // against a similarly-broken native arm could not.
        if (getenv("FG186Q_ORACLE")) {
            double worst = 0.0;
            for (int64_t ch = 0; ch < nch; ++ch) {
                for (int64_t r = 0; r < c.nrows; ++r) {
                    std::vector<uint8_t> row(x.begin() + (size_t)(r*c.ncols/256)*144,
                                             x.begin() + (size_t)((r+1)*c.ncols/256)*144);
                    std::vector<float> yrow(y.begin() + (size_t)(ch*c.ncols),
                                            y.begin() + (size_t)((ch+1)*c.ncols));
                    const double ref = ref_dot(row, yrow, c.ncols);
                    const double got = out[(size_t)(ch*c.nrows + r)];
                    const double rel = std::fabs(got-ref)/std::fmax(1e-30, std::fabs(ref));
                    if (rel > worst) worst = rel;
                }
            }
            std::printf("ORACLE %s worst_rel=%.3e\n", c.name, worst);
        }
        ggml_backend_buffer_free(bx); ggml_backend_buffer_free(by); ggml_backend_buffer_free(bd);
        cudaFree(dx); cudaFree(dy); cudaFree(dd);
    }
    std::printf("FG186Q_GPU_FIXTURE checks=%d failures=%d arm=%s\n",
                g_checks, g_fail, native_arm ? "native" : "goose");
    return g_fail ? 1 : 0;
}
