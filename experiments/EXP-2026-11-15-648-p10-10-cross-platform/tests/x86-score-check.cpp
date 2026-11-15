// Cross-check for the transcribed x86 score chain.
//
// The differential gate compares the port against a transcription of the native
// #ifdef chain. This program closes the remaining gap for the host: it includes
// the real ggml-cpu/arch/x86/cpu-feats.cpp, asks the real CPUID for the host's
// feature word, then requires
//
//     port score(host bits, compile flags) == real ggml_backend_score()
//
// for the compile-flag word the file was built with. tests/x86-score-check.sh
// builds it several times with different -DGGML_* words, so every flag block of
// the chain is exercised against the real function on this CPU.
#include <cstdio>
#include <cstdint>

#include "ggml-cpu/arch/x86/cpu-feats.cpp"
#include "feat.h"

extern "C" int ggml_backend_score(void);

int main() {
    goose_init(0, nullptr);

    cpuid_x86 is;
    int64_t bits = 0;
    if (is.FMA())        { bits |= 1; }
    if (is.F16C())       { bits |= 2; }
    if (is.SSE42())      { bits |= 4; }
    if (is.BMI2())       { bits |= 8; }
    if (is.AVX())        { bits |= 16; }
    if (is.AVX2())       { bits |= 32; }
    if (is.AVX_VNNI())   { bits |= 64; }
    if (is.AVX512F())    { bits |= 128; }
    if (is.AVX512CD())   { bits |= 256; }
    if (is.AVX512VL())   { bits |= 512; }
    if (is.AVX512DQ())   { bits |= 1024; }
    if (is.AVX512BW())   { bits |= 2048; }
    if (is.AVX512_VBMI()) { bits |= 4096; }
    if (is.AVX512_BF16()) { bits |= 8192; }
    if (is.AVX512_VNNI()) { bits |= 16384; }
    if (is.AMX_INT8())   { bits |= 32768; }

    int64_t flags = 0;
#ifdef GGML_FMA
    flags |= 1;
#endif
#ifdef GGML_F16C
    flags |= 2;
#endif
#ifdef GGML_SSE42
    flags |= 4;
#endif
#ifdef GGML_BMI2
    flags |= 8;
#endif
#ifdef GGML_AVX
    flags |= 16;
#endif
#ifdef GGML_AVX2
    flags |= 32;
#endif
#ifdef GGML_AVX_VNNI
    flags |= 64;
#endif
#ifdef GGML_AVX512
    flags |= 128;
#endif
#ifdef GGML_AVX512_VBMI
    flags |= 256;
#endif
#ifdef GGML_AVX512_BF16
    flags |= 512;
#endif
#ifdef GGML_AVX512_VNNI
    flags |= 1024;
#endif
#ifdef GGML_AMX_INT8
    flags |= 2048;
#endif

    const int64_t real = ggml_backend_score();
    const int64_t port = feat_x86_score(bits, flags);

    std::printf("x86-score host bits=%lld flags=%lld real=%lld port=%lld %s\n",
                (long long) bits, (long long) flags, (long long) real, (long long) port,
                real == port ? "OK" : "MISMATCH");
    return real == port ? 0 : 1;
}
