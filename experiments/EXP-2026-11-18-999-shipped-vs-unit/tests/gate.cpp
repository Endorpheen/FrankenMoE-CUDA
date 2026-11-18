// Two implementations claim to make the same decision: the frozen bridge the shipped
// library calls (fg386s_pick, built from experiments/EXP-2026-11-11-386) and the
// canonical unit's sm_select. EXP998 proved sm_select equals the native sampler chain.
// This asks the question that matters for the artifact: do the two agree with each
// other? If they do not, everything proved about sm_select says nothing about what
// Apex actually runs, because the shipped path is the older bridge.
//
// The comparison is exact and needs no tolerance on the selection: seeding std::mt19937
// with the same seed and taking std::generate_canonical<double, 53> reproduces the draw
// that sm_select makes internally, so both are handed the identical random number.
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

#include "llama.h"
#include "runtime.h"

extern "C" int64_t fg386s_pick(const float * logits, int64_t n, int32_t sorted, float * out, double rnd);
extern "C" int fg386s_enabled(void);

static int checks = 0;
static int fails = 0;

static void check(int cond, const char *what) {
    checks++;
    if (!cond) {
        fails++;
        std::printf("FAIL %s\n", what);
    }
}

static sl_i64 I(std::vector<int64_t> &v) { return sl_i64{ v.data(), (int64_t) v.size() }; }
static sl_f32 F(std::vector<float> &v) { return sl_f32{ v.data(), (int64_t) v.size() }; }

int main() {
    goose_init(0, (char **) 0);
    check(fg386s_enabled() != 0, "the shipped bridge is enabled by default (rule 10)");

    const int n = 64;
    const unsigned seeds[4] = { 3u, 17u, 4242u, 77u };
    int agree = 0, total = 0;

    for (int si = 0; si < 4; si++) {
        std::mt19937 r(seeds[si] ^ 0xa5a5a5a5u);
        std::uniform_real_distribution<float> u(-6.0f, 6.0f);
        std::vector<float> base((size_t) n);
        for (int i = 0; i < n; i++) base[(size_t) i] = u(r);
        base[0] += 0.31f;   // no ties, so the answer cannot depend on ordering

        // The draw both sides will use. sm_select makes it internally from the MT state;
        // reproducing it here is what makes the comparison exact.
        std::mt19937 rng(seeds[si]);
        const double rnd = std::generate_canonical<double, 53>(rng);

        std::vector<float> pb((size_t) n, 0.0f);
        const int64_t shipped = fg386s_pick(base.data(), n, 0, pb.data(), rnd);

        // k=0, top_p=1, min_p=0: sm_select reduces to softmax, cumulative cut, draw -
        // the same work the bridge does, which is the comparison worth making.
        std::vector<float> lv = base;
        std::vector<int64_t> id((size_t) n, 0);
        std::vector<float> p((size_t) n, 0.0f);
        std::vector<int64_t> ip = { 0, 0, 1 };
        std::vector<float> fp = { 1.0f, 0.0f, 1.0f };
        std::vector<int64_t> st(625, 0), out(4, -1);
        sm_mt_seed(I(st), (int64_t) seeds[si]);
        const int64_t rc = sm_select(F(lv), I(id), F(p), n, (sl_i64){ ip.data(), 3 },
                                     (sl_f32){ fp.data(), 3 }, I(st), I(out));

        total++;
        if (rc >= 0 && (int64_t) shipped == out[0]) {
            agree++;
        } else {
            std::printf("     seed %u: shipped %lld, unit %lld (rc %lld)\n", seeds[si],
                        (long long) shipped, (long long) (rc >= 0 ? out[0] : -1), (long long) rc);
        }

        // The probabilities are a separate fact: two sides can pick the same index and
        // still normalise differently, which would move every later comparison.
        if (rc >= 0) {
            double err = 0.0;
            for (int i = 0; i < n; i++) {
                const double d = (double) pb[(size_t) i] - (double) p[(size_t) i];
                if (d > err) err = d;
                if (-d > err) err = -d;
            }
            char msg[128];
            std::snprintf(msg, sizeof msg, "seed %u: both write the same probabilities", seeds[si]);
            check(err < 1e-6, msg);
            if (err >= 1e-6) std::printf("     max probability difference %.3g\n", err);
        }
    }

    check(agree == total, "the shipped bridge and the canonical unit pick the same index");

    std::printf("%s checks=%d failures=%d\n", fails == 0 ? "PASS" : "BAD", checks, fails);
    return fails ? 1 : 0;
}
