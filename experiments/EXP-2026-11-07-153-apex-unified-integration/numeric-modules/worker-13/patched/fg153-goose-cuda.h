// fg153-goose-cuda.h - default-on bridge to the Goose-emitted CUDA kernels.
// Set GGML_CUDA_GOOSE_GDN=0 or GGML_CUDA_GOOSE_SSM_CONV=0 to roll back to the
// native ggml-cuda kernels.  The launch helpers only fill the ABI struct and
// enqueue the Goose kernel on the caller's stream; no host synchronisation is
// added, so the original async ordering is preserved.
#ifndef FG153_GOOSE_CUDA_H
#define FG153_GOOSE_CUDA_H
#include <cuda_runtime.h>
#include <cstdint>
#include <cstdlib>
#include "fg153-gdn-abi.h"

extern "C" __global__ void fg153_goose_gdn_kernel(fg153_gdn_cfg cfg);
extern "C" __global__ void fg153_goose_ssm_conv_kernel(fg153_conv_cfg cfg);

static inline bool fg153_goose_gdn_enabled(void) {
    const char *e = std::getenv("GGML_CUDA_GOOSE_GDN");
    return !(e != nullptr && e[0] == '0' && e[1] == '\0');
}
static inline bool fg153_goose_ssm_conv_enabled(void) {
    const char *e = std::getenv("GGML_CUDA_GOOSE_SSM_CONV");
    return !(e != nullptr && e[0] == '0' && e[1] == '\0');
}

static inline bool fg153_goose_gdn_launch(
        const float * q, const float * k, const float * v,
        const float * g, const float * beta, const float * state_in,
        float * dst, float * state_out,
        int64_t S_v, int64_t H, int64_t n_tokens, int64_t n_seqs,
        int64_t sq1, int64_t sq2, int64_t sq3,
        int64_t sv1, int64_t sv2, int64_t sv3,
        int64_t sb1, int64_t sb2, int64_t sb3,
        int64_t neqk1, int64_t rq3, float scale,
        int64_t state_slot_stride, int K, bool kda, bool keep_rs,
        cudaStream_t stream) {
    if (!fg153_goose_gdn_enabled()) return false;
    if (S_v <= 0 || H <= 0 || n_seqs <= 0 || n_tokens < 0 || neqk1 <= 0 || rq3 <= 0 || K < 1) return false;
    const int64_t cta = 16;
    fg153_gdn_cfg c{};
    c.v[FG153_GDN_Q] = (int64_t)(uintptr_t) q;
    c.v[FG153_GDN_K] = (int64_t)(uintptr_t) k;
    c.v[FG153_GDN_V] = (int64_t)(uintptr_t) v;
    c.v[FG153_GDN_G] = (int64_t)(uintptr_t) g;
    c.v[FG153_GDN_BETA] = (int64_t)(uintptr_t) beta;
    c.v[FG153_GDN_STATE_IN] = (int64_t)(uintptr_t) state_in;
    c.v[FG153_GDN_DST] = (int64_t)(uintptr_t) dst;
    c.v[FG153_GDN_STATE_OUT] = (int64_t)(uintptr_t) state_out;
    c.v[FG153_GDN_H] = H;
    c.v[FG153_GDN_NTOK] = n_tokens;
    c.v[FG153_GDN_NSEQ] = n_seqs;
    c.v[FG153_GDN_SQ1] = sq1; c.v[FG153_GDN_SQ2] = sq2; c.v[FG153_GDN_SQ3] = sq3;
    c.v[FG153_GDN_SV1] = sv1; c.v[FG153_GDN_SV2] = sv2; c.v[FG153_GDN_SV3] = sv3;
    c.v[FG153_GDN_SB1] = sb1; c.v[FG153_GDN_SB2] = sb2; c.v[FG153_GDN_SB3] = sb3;
    c.v[FG153_GDN_NEQK1] = neqk1;
    c.v[FG153_GDN_RQ3] = rq3;
    c.v[FG153_GDN_STATE_SLOT_STRIDE] = state_slot_stride;
    c.v[FG153_GDN_K_SLOTS] = K;
    c.v[FG153_GDN_KDA] = kda ? 1 : 0;
    c.v[FG153_GDN_KEEP_RS] = keep_rs ? 1 : 0;
    c.v[FG153_GDN_COLS_PER_CTA] = cta;
    c.v[FG153_GDN_SV] = S_v;
    c.f[0] = scale;
    dim3 grid((unsigned) H, (unsigned) n_seqs, (unsigned) ((S_v + cta - 1) / cta));
    dim3 block((unsigned) cta, 1, 1);
    fg153_goose_gdn_kernel<<<grid, block, 0, stream>>>(c);
    return cudaGetLastError() == cudaSuccess;
}

static inline bool fg153_goose_ssm_conv_launch(
        const float * x, const float * w, const float * bias, float * y,
        int64_t nc, int64_t nr, int64_t n_t, int64_t n_s,
        int64_t xsc, int64_t xss, int64_t wsc,
        int64_t ysc, int64_t yst, int64_t yss,
        bool apply_silu, cudaStream_t stream) {
    if (!fg153_goose_ssm_conv_enabled()) return false;
    if (x == nullptr || w == nullptr || y == nullptr) return false;
    if (nr <= 0 || n_t < 0 || n_s <= 0) return false;
    if (!(nc == 3 || nc == 4 || nc == 5 || nc == 7 || nc == 9 || nc == 15)) return false;
    if (nr % 128 != 0) return false;
    const int64_t nth = 128;
    fg153_conv_cfg c{};
    c.v[FG153_CONV_X] = (int64_t)(uintptr_t) x;
    c.v[FG153_CONV_W] = (int64_t)(uintptr_t) w;
    c.v[FG153_CONV_BIAS] = (int64_t)(uintptr_t) bias;
    c.v[FG153_CONV_Y] = (int64_t)(uintptr_t) y;
    c.v[FG153_CONV_NC] = nc;
    c.v[FG153_CONV_NR] = nr;
    c.v[FG153_CONV_NT] = n_t;
    c.v[FG153_CONV_NS] = n_s;
    c.v[FG153_CONV_XSC] = xsc;
    c.v[FG153_CONV_XSS] = xss;
    c.v[FG153_CONV_WSC] = wsc;
    c.v[FG153_CONV_YSC] = ysc;
    c.v[FG153_CONV_YST] = yst;
    c.v[FG153_CONV_YSS] = yss;
    c.v[FG153_CONV_SILU] = apply_silu ? 1 : 0;
    c.v[FG153_CONV_NTHREADS] = nth;
    c.v[FG153_CONV_SMEM_CAP] = 12288;
    if (n_t <= 32) {
        c.v[FG153_CONV_MODE] = 0;
        c.v[FG153_CONV_SPLIT] = 0;
        dim3 grid((unsigned) n_s, (unsigned) (nr / nth), 1);
        dim3 block((unsigned) nth, 1, 1);
        fg153_goose_ssm_conv_kernel<<<grid, block, 0, stream>>>(c);
    } else {
        const int64_t split = 32;
        const size_t smem = (size_t) nth * (size_t) (nc - 1 + split) * sizeof(float);
        if (smem > 48 * 1024) return false;
        c.v[FG153_CONV_MODE] = 1;
        c.v[FG153_CONV_SPLIT] = split;
        dim3 grid((unsigned) n_s, (unsigned) (nr / nth), (unsigned) ((n_t + split - 1) / split));
        dim3 block((unsigned) nth, 1, 1);
        fg153_goose_ssm_conv_kernel<<<grid, block, smem, stream>>>(c);
    }
    return cudaGetLastError() == cudaSuccess;
}
#endif
