// Against the real class: router-free, so a white-box include is enough and no model is needed.
#include "/home/end0/AI/FrankenMoE-CUDA/runtime/core/src/moe/deterministic_workload.cpp"
#include <cstdio>
#include <random>
#include <string>
extern "C" int64_t g_wl_check(int64_t, int64_t, int64_t);

static const char * const kArchs[] = { "qwen3moe", "gemma4", "gpt-oss", "deepseek4", "" };

int main() {
    std::mt19937 rng(5150);
    int fails = 0, cases = 0;
    for (int t = 0; t < 2000; ++t) {
        const char * capArch = kArchs[rng() % 5];
        const int capLayers = (int)(rng() % 5) + 1;
        const int capTopK   = (int)(rng() % 5) + 1;

        auto wl = bmoe::DeterministicWorkload::capture();
        wl->set_static(capArch, capLayers, capTopK);

        // the "current model" the replay is checked against
        const char * curArch = (rng() % 3 == 0) ? kArchs[rng() % 5] : capArch;
        const int curLayers = (rng() % 3 == 0) ? (int)(rng() % 5) + 1 : capLayers;
        const int curTopK   = (rng() % 3 == 0) ? (int)(rng() % 5) + 1 : capTopK;

        const bool native = wl->check_static(curArch, curLayers, curTopK);
        const int64_t port = g_wl_check(std::string(capArch) == std::string(curArch) ? 1 : 0,
                                        capLayers == curLayers ? 1 : 0,
                                        capTopK == curTopK ? 1 : 0);
        ++cases;
        if ((port == 1) != native) {
            ++fails;
            if (fails <= 3) printf("MISMATCH cap=(%s,%d,%d) cur=(%s,%d,%d) native=%d port=%lld\n",
                                   capArch, capLayers, capTopK, curArch, curLayers, curTopK,
                                   native ? 1 : 0, (long long) port);
        }
    }
    printf("workload check_static: cases=%d fails=%d\n", cases, fails);
    return fails ? 1 : 0;
}
