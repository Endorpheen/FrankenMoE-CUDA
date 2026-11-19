// Against the real class: a workload with two route records, driven with matching and mismatching
// batches, comparing the rule and the cursor the native leaves behind.
#include "/home/end0/AI/FrankenMoE-CUDA/runtime/core/src/moe/deterministic_workload.cpp"
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
extern "C" int64_t g_topk_call(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,
                               int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t*);

static int native_rule(const std::string & err) {
    if (err.empty()) return 0;
    if (err.find("replay produced more route records") != std::string::npos) return 1;
    if (err.find("route shape mismatch") != std::string::npos) return 2;
    return -99;
}

int main() {
    const char * text = "BMOE_DETERMINISTIC_WORKLOAD 1 arch a layers 1 top_k 2 prompt 0 output 0 routes 2 "
                        "route 1 5 2 3 2 1 2 7 8\nroute 0 0 1 0 2 1 2 9 10\n";
    const char * path = "/tmp/wl_topk.txt";
    { FILE * f = fopen(path, "wb"); fwrite(text, 1, strlen(text), f); fclose(f); }

    std::mt19937 rng(6060);
    int fails = 0, cases = 0;
    for (int t = 0; t < 1000; ++t) {
        std::string err;
        auto wl = bmoe::DeterministicWorkload::load(path, err);
        if (!wl) { printf("load failed\n"); return 2; }

        // the cursor is advanced by calls that succeed, so drive it deliberately
        const int64_t cursor = (int64_t)(rng() % 3);
        for (int64_t c = 0; c < cursor; ++c) {
            const int32_t * ids = nullptr;
            wl->begin_batch(c == 0 ? 5 : 0, c == 0 ? 2 : 1, c == 0 ? 1 : 0);
            std::string e2; (void) wl->replay_topk(c == 0 ? 3 : 0, 2, 1, ids);
        }
        // the batch under test: either the record under the cursor or a perturbed one
        const int slot = (int)(cursor % 2);
        const int ph = slot == 0 ? 1 : 0, bp = slot == 0 ? 5 : 0, bn = slot == 0 ? 2 : 1;
        const int la = slot == 0 ? 3 : 0;
        const int jitter = (int)(rng() % 6);
        int aph = ph, abp = bp, abn = bn, ala = la;
        switch (jitter) {
          case 0: aph = ph ^ 1; break;  case 1: abp = bp + 1; break;
          case 2: abn = bn + 1; break;  case 3: ala = la + 1; break;
          default: break;
        }
        wl->begin_batch(abp, abn, aph);
        const int32_t * ids = nullptr;
        const bool ok = wl->replay_topk(ala, 2, 1, ids);
        // classification uses the state the harness itself set up: the native error text is not
        // exposed by the class, and my first version hardcoded the shape message, so an exhausted
        // workload was misreported as a shape mismatch in 356 of 1000 cases.
        const int want = ok ? 0 : (cursor >= 2 ? 1 : 2);

        int64_t out[1] = { -1 };
        const int64_t got = g_topk_call(cursor, 2, ph, bp, bn, la, 2, 1, aph, abp, abn, ala, 2, 1, out);
        ++cases;
        if (got != want) {
            ++fails;
            if (fails <= 3) printf("MISMATCH cursor=%lld jitter=%d native=%d port=%lld\n",
                                   (long long) cursor, jitter, want, (long long) got);
        }
        // the cursor the native left behind: one more successful replay must land on the next record
        if (out[0] != (cursor >= 2 ? cursor : cursor + 1)) {
            ++fails;
            if (fails <= 3) printf("CURSOR MISMATCH cursor=%lld port_cursor=%lld\n",
                                   (long long) cursor, (long long) out[0]);
        }
    }
    printf("replay_topk: cases=%d fails=%d\n", cases, fails);
    return fails ? 1 : 0;
}
