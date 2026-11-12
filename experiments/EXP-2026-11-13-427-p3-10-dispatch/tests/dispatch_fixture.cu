// Differential gate for the Goose dense-dispatch predicates.
//
// Oracle: the exported native ggml_cuda_should_use_mmf / _mmvf from
// libggml-cuda.so. The Goose arm receives the same type facts plus device
// capability booleans derived from cc. No transcription is used as the oracle.
#include "ggml.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

extern bool ggml_cuda_should_use_mmf(ggml_type type, int cc, int warp_size,
    const int64_t * src0_ne, const size_t * src0_nb, int src1_ncols, bool mul_mat_id);
extern bool ggml_cuda_should_use_mmvf(ggml_type type, int cc, int warp_size,
    const int64_t * src0_ne, const size_t * src0_nb, int64_t ne11);
extern "C" int fg427_mmf(const int64_t * a);
extern "C" int fg427_mmvf(const int64_t * a);

enum { CC_AMD = 0x1000000, CDNA1 = CC_AMD + 0x908, CDNA2 = CC_AMD + 0x90a,
       CDNA3 = CC_AMD + 0x942, CDNA4 = CC_AMD + 0x950, RDNA1 = CC_AMD + 0x1010,
       RDNA3 = CC_AMD + 0x1100, RDNA3_5 = CC_AMD + 0x1150, RDNA4 = CC_AMD + 0x1200 };

struct Flags {
    int64_t nvidia, amd, ampere, volta, turing, ada, wmma, mfma;
    int64_t cdna1, cdna2, cdna3, rdna, rdna3, rdna3_0, rdna4;
    int64_t fp32mma, fp16mma, bf16mma, rpb, warp;
};

static Flags flags_for(int cc) {
    Flags f{};
    f.nvidia = cc < 0x100000;
    f.amd = cc >= CC_AMD;
    const bool cdna = f.amd && cc >= CDNA1 && cc < RDNA1;
    const bool rdna3 = cc >= RDNA3 && cc < RDNA4;
    f.cdna1 = cdna && cc < CDNA2;
    f.cdna2 = cdna && cc >= CDNA2 && cc < CDNA3;
    f.cdna3 = cdna && cc >= CDNA3 && cc < CDNA4;
    f.rdna = cc >= RDNA1;
    f.rdna3 = rdna3;
    f.rdna3_0 = cc >= RDNA3 && cc < RDNA3_5;
    f.rdna4 = cc >= RDNA4;
    f.volta = 0; // the build compiles sm_89; volta_mma_available is false
    f.turing = f.nvidia && cc >= 750;
    f.ampere = f.nvidia && cc >= 800;
    f.ada = f.nvidia && cc >= 890;
    f.wmma = f.rdna3 || f.rdna4;
    f.mfma = cdna;
    f.fp32mma = cdna;
    f.fp16mma = (f.nvidia && cc >= 700) || cdna || f.rdna3 || f.rdna4;
    f.bf16mma = (f.nvidia && cc >= 800) || cdna || cc >= RDNA3;
    f.rpb = cdna ? 64 : 32;
    f.warp = f.amd ? 64 : 32;
    return f;
}

static int64_t type_code(ggml_type t) {
    if (t == GGML_TYPE_F32) return 0;
    if (t == GGML_TYPE_F16) return 1;
    if (t == GGML_TYPE_BF16) return 2;
    return 3;
}

int main() {
    const ggml_type types[] = { GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_BF16,
                                GGML_TYPE_Q4_K, GGML_TYPE_Q8_0 };
    const int ccs[] = { 890, 900,
                        CDNA1, CDNA2, CDNA3, RDNA3, RDNA3_5, RDNA4 };
    std::mt19937_64 rng(427011);
    int checks = 0, fails = 0;
    const int rounds = 200000;

    for (int it = 0; it < rounds; ++it) {
        const ggml_type ty = types[rng() % 5];
        const int cc = ccs[rng() % (sizeof(ccs) / sizeof(ccs[0]))];
        const Flags f = flags_for(cc);
        const int64_t ts = (int64_t) ggml_type_size(ty);
        const int64_t quant = ggml_is_quantized(ty) ? 1 : 0;

        int64_t ne[4];
        const int64_t ne_choices[] = {1, 2, 3, 31, 32, 33, 64, 65, 128, 255, 256, 512, 1024, 2048};
        for (int i = 0; i < 4; ++i) ne[i] = ne_choices[rng() % 14];
        size_t nb[4];
        nb[0] = (size_t) ts + (rng() % 4 == 0 ? 1 : 0);
        for (int i = 1; i < 4; ++i) {
            nb[i] = (size_t) ((2 * ts) * (1 + (int64_t) (rng() % 4)) + (rng() % 4 == 0 ? 1 : 0));
        }
        const int64_t ne11_choices[] = {1, 2, 3, 4, 5, 8, 9, 16, 17, 64, 128, 129, 256, 512, 513};
        const int64_t ne11 = ne11_choices[rng() % 15];
        const bool ids = (rng() % 2) == 1;
        const int warp = (int) f.warp;

        const bool n_mmf = ggml_cuda_should_use_mmf(ty, cc, warp, ne, nb, (int) ne11, ids);
        int64_t a_mmf[26] = { quant, type_code(ty), ts,
            ne[0], ne[1], ne[2], ne[3], (int64_t) nb[0], (int64_t) nb[1], (int64_t) nb[2], (int64_t) nb[3], ne11,
            f.nvidia, f.amd, f.ampere, f.volta, f.turing, f.wmma, f.mfma,
            f.cdna1, f.cdna2, f.cdna3, f.rdna3_0, f.rpb, f.warp, ids ? 1 : 0 };
        const bool g_mmf = fg427_mmf(a_mmf) != 0;

        const bool n_mmvf = ggml_cuda_should_use_mmvf(ty, cc, warp, ne, nb, ne11);
        int64_t a_mmvf[31] = { quant, type_code(ty), ts,
            ne[0], ne[1], ne[2], ne[3], (int64_t) nb[0], (int64_t) nb[1], (int64_t) nb[2], (int64_t) nb[3], ne11,
            f.nvidia, f.amd, f.ampere, f.volta, f.turing, f.ada, f.wmma, f.mfma,
            f.cdna1, f.cdna2, f.cdna3, f.rdna, f.rdna3, f.rdna4,
            f.fp32mma, f.fp16mma, f.bf16mma, f.rpb, f.warp };
        const bool g_mmvf = fg427_mmvf(a_mmvf) != 0;

        ++checks; if (n_mmf != g_mmf) ++fails;
        ++checks; if (n_mmvf != g_mmvf) ++fails;
        if ((n_mmf != g_mmf || n_mmvf != g_mmvf) && fails <= 5) {
            printf("MISMATCH it=%d cc=%d ty=%d ne0=%lld ne1=%lld ne11=%lld ids=%d mmf=%d/%d mmvf=%d/%d\n",
                   it, cc, (int) ty, (long long) ne[0], (long long) ne[1], (long long) ne11, (int) ids,
                   (int) n_mmf, (int) g_mmf, (int) n_mmvf, (int) g_mmvf);
        }
    }
    printf("FG427_DISPATCH checks=%d failures=%d\n", checks, fails);
    return fails ? 1 : 0;
}
