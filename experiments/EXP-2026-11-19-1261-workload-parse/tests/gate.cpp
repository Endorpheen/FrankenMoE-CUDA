// Eight crafted files against the real load(): one valid, one per failure mode. The native error
// text is mapped back to the rule index; nothing is transcribed.
#include "/home/end0/AI/FrankenMoE-CUDA/runtime/core/src/moe/deterministic_workload.cpp"
#include <cstdio>
#include <cstring>
#include <string>
extern "C" int64_t g_wlp_call(const char *, int64_t);

static int native_rule(const std::string & err) {
    if (err.empty()) return 0;
    if (err.find("unsupported deterministic workload header") != std::string::npos) return 1;
    if (err.find("invalid deterministic workload header") != std::string::npos) return 2;
    if (err.find("invalid deterministic workload metadata") != std::string::npos) return 3;
    if (err.find("missing prompt tokens") != std::string::npos) return 4;
    if (err.find("truncated prompt tokens") != std::string::npos) return 4;
    if (err.find("missing output tokens") != std::string::npos) return 5;
    if (err.find("truncated output tokens") != std::string::npos) return 5;
    if (err.find("missing routes") != std::string::npos) return 6;
    if (err.find("invalid route record") != std::string::npos) return 7;
    if (err.find("truncated route record") != std::string::npos) return 8;
    return -99;
}

int main() {
    struct C { const char * name; const char * text; int want; };
    const C cases[] = {
      {"valid", "BMOE_DETERMINISTIC_WORKLOAD 1 arch qwen3moe layers 2 top_k 4 prompt 3 10 11 12 "
                "output 2 20 21 routes 1 route 0 0 1 0 2 1 2 7 8\n", 0},
      {"bad magic",   "BMOE_OTHER 1 arch a layers 1 top_k 1 prompt 0 output 0 routes 0\n", 1},
      {"bad version", "BMOE_DETERMINISTIC_WORKLOAD 2 arch a layers 1 top_k 1 prompt 0 output 0 routes 0\n", 1},
      {"bad metadata","BMOE_DETERMINISTIC_WORKLOAD 1 arch a layerz 1 top_k 1 prompt 0 output 0 routes 0\n", 3},
      {"prompt short","BMOE_DETERMINISTIC_WORKLOAD 1 arch a layers 1 top_k 1 prompt 3 10 output 0 routes 0\n", 4},
      {"output short","BMOE_DETERMINISTIC_WORKLOAD 1 arch a layers 1 top_k 1 prompt 0 output 2 1 routes 0\n", 5},
      {"routes key",  "BMOE_DETERMINISTIC_WORKLOAD 1 arch a layers 1 top_k 1 prompt 0 output 0 rutes 0\n", 6},
      {"route shape", "BMOE_DETERMINISTIC_WORKLOAD 1 arch a layers 1 top_k 2 prompt 0 output 0 routes 1 "
                      "route 0 0 1 0 2 1 9 1 2 3\n", 7},
      {"route short", "BMOE_DETERMINISTIC_WORKLOAD 1 arch a layers 1 top_k 2 prompt 0 output 0 routes 1 "
                      "route 0 0 1 0 2 1 2 5\n", 8},
    };
    int fails = 0, n = (int)(sizeof(cases)/sizeof(cases[0]));
    for (int i = 0; i < n; ++i) {
        // native reads the same text through a file
        const std::string path = std::string("/tmp/wl_case_") + std::to_string(i) + ".txt";
        { FILE * f = fopen(path.c_str(), "wb"); fwrite(cases[i].text, 1, strlen(cases[i].text), f); fclose(f); }
        std::string err;
        auto wl = bmoe::DeterministicWorkload::load(path, err);
        const int want = native_rule(wl ? "" : err);
        const int64_t got = g_wlp_call(cases[i].text, (int64_t) strlen(cases[i].text));
        if (want == -99 || got != want || want != cases[i].want) {
            ++fails;
            printf("MISMATCH %-14s native=%d port=%lld expected=%d err='%s'\n",
                   cases[i].name, want, (long long) got, cases[i].want, err.c_str());
        }
    }
    printf("workload parse: cases=%d fails=%d\n", n, fails);
    return fails ? 1 : 0;
}
