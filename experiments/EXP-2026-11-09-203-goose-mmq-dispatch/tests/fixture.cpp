// Differential gate for the Goose dispatch module.
//
// The reference is a faithful transcription of mmq.cu:355-481. It is transcribed rather than
// linked because the native function lives in a .cu translation unit that pulls device
// intrinsics through mmq.cuh; the project's established pattern for host policy is to
// transcribe and compare, as the loader gate did for std::algorithm.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <random>
#include <algorithm>

extern "C" {
int64_t fg203d_type_supported(int64_t);
int64_t fg203d_should_use_mmq(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t,
                              int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
}

static int checks, failures;
static void ck(bool ok, const char * what, long long a, long long b) {
    ++checks;
    if (!ok) { if (failures < 20) printf("FAIL %s: goose=%lld ref=%lld\n", what, a, b); ++failures; }
}

// ---- native transcriptions -------------------------------------------------
enum {
    Q4_0=2, Q4_1=3, Q5_0=6, Q5_1=7, Q8_0=8, Q2_K=10, Q3_K=11, Q4_K=12, Q5_K=13, Q6_K=14,
    IQ2_XXS=16, IQ2_XS=17, IQ3_XXS=18, IQ1_S=19, IQ4_NL=20, IQ3_S=21, IQ2_S=22, IQ4_XS=23,
    MXFP4=39, NVFP4=40, Q1_0=41, Q2_0=42, F32=0, F16=1, Q8_1=9, BF16=30
};
static const int CC_PASCAL=600, CC_DP4A=610, CC_TURING=750;
static const int CC_MTHREADS=0x0100000, CC_AMD=0x1000000;
static const int CC_VEGA=CC_AMD+0x900, GFX909=CC_AMD+0x909, GFX90C=CC_AMD+0x90c;
static const int CC_CDNA1=CC_AMD+0x908, CC_CDNA3=CC_AMD+0x942, CC_CDNA4=CC_AMD+0x950;
static const int CC_RDNA1=CC_AMD+0x1010, CC_RDNA3=CC_AMD+0x1100, CC_RDNA3_5=CC_AMD+0x1150;
static const int CC_RDNA4=CC_AMD+0x1200;

static bool ref_is_nvidia(int cc) { return cc < CC_MTHREADS; }
static bool ref_gcn_apu(int cc) { return cc == GFX909 || cc == GFX90C; }
static bool ref_is_cdna(int cc) { return !ref_gcn_apu(cc) && cc >= CC_CDNA1 && cc < CC_RDNA1; }
static bool ref_is_cdna3(int cc) { return ref_is_cdna(cc) && cc >= CC_CDNA3 && cc < CC_CDNA4; }
static bool ref_rdna3_0(int cc) { return cc >= CC_RDNA3 && cc < CC_RDNA3_5; }
static bool ref_rdna3_5(int cc) { return cc >= CC_RDNA3_5 && cc < CC_RDNA4; }
static bool ref_rdna3(int cc) { return ref_rdna3_0(cc) || ref_rdna3_5(cc); }

static bool ref_type_supported(int t) {
    switch (t) {
        case Q1_0: case Q2_0: case Q4_0: case Q4_1: case Q5_0: case Q5_1: case Q8_0:
        case Q2_K: case Q3_K: case Q4_K: case Q5_K: case Q6_K:
        case IQ1_S: case IQ2_XXS: case IQ2_XS: case IQ2_S: case IQ3_XXS: case IQ3_S:
        case IQ4_XS: case IQ4_NL: case MXFP4: case NVFP4:
            return true;
        default: return false;
    }
}

static bool ref_should_use_mmq(int t, int cc, int64_t ne11, int64_t n_experts, size_t smpbo,
                               int highest_arch, bool turing_mma, bool fp16_mma_hw,
                               bool mfma, bool wmma, bool force_cublas, bool force_mmq) {
    if (force_cublas) return false;
    if (!ref_type_supported(t)) return false;
    if (smpbo < 48u*1024u) return false;
    if (turing_mma) return true;
    if (highest_arch < CC_DP4A) return cc >= CC_PASCAL && n_experts > 0;
    if (force_mmq) return true;
    if (ref_is_nvidia(cc)) return !fp16_mma_hw || ne11 < 64;
    if (mfma) {
        if (ref_is_cdna3(cc)) return true;
        if (n_experts > 64 || ne11 <= 128) return true;
        if (t==Q4_0||t==Q4_1||t==Q5_0||t==Q5_1) return true;
        if (ne11 <= 256 && (t==Q4_K||t==Q5_K)) return true;
        return false;
    }
    if (wmma) {
        if (ref_rdna3(cc)) {
            if (n_experts >= 64) return true;
            switch (t) {
                case Q2_K: return ne11 <= 128;
                case Q6_K: return ne11 <= (ref_rdna3_0(cc) ? 128 : 256);
                case IQ2_XS: case IQ2_S: return ref_rdna3_5(cc) || ne11 <= 128;
                default: return true;
            }
        }
        return true;
    }
    if (cc == CC_VEGA || ref_gcn_apu(cc)) return n_experts > 0;
    return (!ref_is_cdna(cc)) || ne11 < 64;
}

static void run_trap(const char * what, int expected, void (*fn)(void)) {
    int fd[2]; if (pipe(fd) != 0) { ++failures; return; }
    pid_t pid = fork();
    if (pid == 0) { close(fd[0]); dup2(fd[1], 2); close(fd[1]); fn(); _exit(0); }
    close(fd[1]);
    char buf[256] = {0}; ssize_t n = read(fd[0], buf, sizeof buf - 1); close(fd[0]);
    int st = 0; waitpid(pid, &st, 0); ++checks;
    bool died = WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT;
    long long code = (n > 0 && strstr(buf, "validation code")) ? atoll(strstr(buf, "validation code") + 16) : -1;
    if (!died || code != expected) { if (failures < 20) printf("FAIL trap %s: died=%d code=%lld expected=%d\n", what, died, code, expected); ++failures; }
}
static void trap_negative(void) { fg203d_should_use_mmq(12, 890, -1, 0, 101376, 1200, 1, 1, 0, 0, 0, 0); }

int main(void) {
    std::mt19937 rng(203011);

    // 1. type gate over the full enum range, including gaps.
    for (int t = 0; t < 64; ++t) {
        ck((fg203d_type_supported(t) != 0) == ref_type_supported(t), "type_supported", t, ref_type_supported(t));
    }

    // 2. the decision over a grid that reaches every branch.
    const int types[] = {Q4_0,Q4_1,Q5_0,Q5_1,Q8_0,Q2_K,Q3_K,Q4_K,Q5_K,Q6_K,IQ1_S,IQ2_XXS,
                         IQ2_XS,IQ2_S,IQ3_XXS,IQ3_S,IQ4_XS,IQ4_NL,MXFP4,NVFP4,Q1_0,Q2_0,
                         F32,F16,Q8_1,BF16,999};
    const int ccs[] = {500,600,610,700,750,800,890,900,1200,CC_MTHREADS+0x220,CC_MTHREADS+0x310,
                       CC_VEGA,GFX909,GFX90C,CC_CDNA1,CC_CDNA3,CC_CDNA4,CC_RDNA1,CC_RDNA3,
                       CC_RDNA3_5,CC_RDNA4,CC_AMD+0x9999};
    const int64_t batches[] = {0,1,63,64,65,128,129,256,257,1000};
    const int64_t experts[] = {0,1,64,65,256};
    const size_t smpbos[] = {0,48u*1024u-1,48u*1024u,101376u};
    const int archs[] = {0,600,610,750,1200};
    const bool bools[] = {false,true};

    for (int t : types) for (int cc : ccs) for (int64_t ne : batches)
    for (int64_t ex : experts) for (size_t sm : smpbos) for (int ha : archs)
    for (bool tm : bools) for (bool fh : bools) for (bool mf : bools) for (bool wm : bools)
    for (bool fc : bools) for (bool fm : bools) {
        // the grid is huge; sample deterministically to keep the gate quick
        if ((rng() & 63) != 0) continue;
        const bool ref = ref_should_use_mmq(t, cc, ne, ex, sm, ha, tm, fh, mf, wm, fc, fm);
        const int64_t got = fg203d_should_use_mmq(t, cc, ne, ex, (int64_t)sm, ha,
                                                  tm, fh, mf, wm, fc, fm);
        if ((got != 0) != ref && failures < 12)
            printf("  DIFF t=%d cc=%d ne=%lld ex=%lld sm=%zu ha=%d tm=%d fh=%d mf=%d wm=%d fc=%d fm=%d\n",
                   t,cc,(long long)ne,(long long)ex,sm,ha,(int)tm,(int)fh,(int)mf,(int)wm,(int)fc,(int)fm);
        ck((got != 0) == ref, "should_use_mmq", got, ref);
    }

    // 3. the exact Apex device and shapes, checked by hand.
    //    sm_89 => cc 890, highest compiled arch 1200, turing_mma true, fp16_mma true.
    ck(fg203d_should_use_mmq(Q4_K, 890, 4096, 0, 101376, 1200, 1, 1, 0, 0, 0, 0) == 1, "apex q4k", 0, 1);
    ck(fg203d_should_use_mmq(Q6_K, 890, 4096, 0, 101376, 1200, 1, 1, 0, 0, 0, 0) == 1, "apex q6k", 0, 1);
    ck(fg203d_should_use_mmq(Q4_K, 890, 4096, 0, 101376, 1200, 1, 1, 0, 0, 1, 0) == 0, "force_cublas", 0, 0);
    //    a device below the 48 KiB shared-memory floor must fall back.
    ck(fg203d_should_use_mmq(Q4_K, 890, 4096, 0, 49151, 1200, 1, 1, 0, 0, 0, 0) == 0, "small smpbo", 0, 0);
    //    a non-MMQ type must fall back even with everything else favourable.
    ck(fg203d_should_use_mmq(F32, 890, 4096, 0, 101376, 1200, 1, 1, 0, 0, 0, 0) == 0, "f32", 0, 0);

    run_trap("negative ne11", 10, trap_negative);

    printf("FG203D_FIXTURE checks=%d failures=%d\n", checks, failures);
    if (failures) { printf("EXP203_LOCAL_GATES_FAIL\n"); return 1; }
    printf("EXP203_LOCAL_GATES_PASS\n");
    return 0;
}
