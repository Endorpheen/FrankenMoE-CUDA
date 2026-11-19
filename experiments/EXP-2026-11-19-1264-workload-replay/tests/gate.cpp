// Against the real class: a replay workload is loaded from a crafted file, then token() and finish()
// are driven with the same numbers as the port.
#include "/home/end0/AI/FrankenMoE-CUDA/runtime/core/src/moe/deterministic_workload.cpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <random>
extern "C" int64_t g_tok_call(int64_t,int64_t);
extern "C" int64_t g_fin_call(int64_t,int64_t,int64_t,int64_t);

int main() {
    // two output tokens, one route record
    const char * text = "BMOE_DETERMINISTIC_WORKLOAD 1 arch a layers 1 top_k 2 prompt 0 "
                        "output 2 5 6 routes 1 route 0 0 1 0 2 1 2 7 8\n";
    const char * path = "/tmp/wl_replay.txt";
    { FILE * f = fopen(path, "wb"); fwrite(text, 1, strlen(text), f); fclose(f); }

    std::mt19937 rng(2024);
    int fails = 0, cases = 0;
    for (int t = 0; t < 500; ++t) {
        std::string err;
        auto wl = bmoe::DeterministicWorkload::load(path, err);
        if (!wl) { printf("native load failed: %s\n", err.c_str()); return 2; }

        // token(): index within or beyond the two recorded outputs
        const int64_t idx = (int64_t)(rng() % 4);
        int32_t sel = -1;
        const bool ok_tok = wl->token((size_t) idx, 5, sel);
        const int64_t got_tok = g_tok_call(idx, 2);
        ++cases;
        if ((got_tok != 0) == ok_tok) {
            ++fails;
            if (fails <= 3) printf("TOKEN MISMATCH idx=%lld native=%d port=%lld\n",
                                   (long long) idx, ok_tok ? 1 : 0, (long long) got_tok);
        }

        // finish(): route cursor and emitted count, each on both sides of the limit
        const int64_t cursor = (int64_t)(rng() % 3);
        const int64_t emitted = (int64_t)(rng() % 4);
        // the native state has whatever this object consumed so far; drive it to match
        std::string err2;
        auto wl2 = bmoe::DeterministicWorkload::load(path, err2);
        for (int64_t r = 0; r < cursor; ++r) {
            const int32_t ids[2] = { 7, 8 };
            wl2->begin_batch(0, 1, 0);
            wl2->replay_topk(0, 2, 1, (const int32_t *&) *&ids);   // advances the cursor
        }
        const bool ok_fin = wl2->finish((size_t) emitted);
        const int64_t got_fin = g_fin_call(cursor, 1, emitted, 2);
        ++cases;
        if ((got_fin == 0) != ok_fin) {
            ++fails;
            if (fails <= 3) printf("FINISH MISMATCH cursor=%lld emitted=%lld native=%d port=%lld\n",
                                   (long long) cursor, (long long) emitted, ok_fin ? 1 : 0, (long long) got_fin);
        }
    }
    printf("workload replay: cases=%d fails=%d\n", cases, fails);
    return fails ? 1 : 0;
}
