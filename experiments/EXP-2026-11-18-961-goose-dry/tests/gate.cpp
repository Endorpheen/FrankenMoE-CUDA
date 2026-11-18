// EXP961 differential gate: the Goose DRY penalty must equal the native one,
// bit for bit, on the same history and the same breakers.
//
// The oracle is llama_sampler_dry_apply transcribed with float arithmetic and
// the same four steps: restart-sequence limit, reverse Z-algorithm, max repeat
// per token, then the exponent-clamped penalty. Comparison is on bit patterns.
#include "runtime.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>


static int checks = 0;
static int failures = 0;
static void check(bool cond, const std::string & what) {
    checks++;
    if (!cond) { failures++; std::printf("FAIL %s\n", what.c_str()); }
}

static sl_i64 I(const std::vector<int64_t> & v) { return sl_i64{ const_cast<int64_t *>(v.data()), (long long) v.size() }; }
static sl_f32 F(std::vector<float> & v) { return sl_f32{ v.data(), (long long) v.size() }; }
static uint32_t bits(float f) { uint32_t u; std::memcpy(&u, &f, sizeof(u)); return u; }

struct Breaker {
    int64_t head;
    std::vector<int64_t> tail;  // empty means a single-token breaker
};

// llama-sampler.cpp:3473-3682, float throughout, std::map for max-token-repeat.
static void native_dry(const std::vector<int64_t> & hist,
                       std::vector<float> & logits, const std::vector<int64_t> & ids,
                       float multiplier, float base, int64_t allowed, int64_t last_n_p,
                       const std::vector<Breaker> & breakers) {
    if (multiplier == 0.0f || base < 1.0f || last_n_p == 0) return;
    int last_n = (int) std::min((int64_t) hist.size(), last_n_p);
    if (last_n <= allowed) return;

    std::vector<int> counts((size_t) last_n, 0);
    std::map<int64_t, int> max_rep;

    auto rat = [&](int i) { return hist[(size_t) i]; };

    // Step 1
    int rep_limit = last_n;
    for (int i = 0; i < last_n; ++i) {
        int64_t token = rat(i);
        int longest = -1;
        for (const auto & b : breakers) {
            if (b.head != token) continue;
            int seq_len = (int) b.tail.size();
            if (seq_len > longest && seq_len <= i) {
                bool hit = true;
                for (int off = 0; off < seq_len; ++off) {
                    if (b.tail[(size_t) off] != rat(i - off - 1)) { hit = false; break; }
                }
                if (hit) longest = seq_len;
            }
        }
        if (longest >= 0) { rep_limit = i - longest; break; }
    }
    if (rep_limit < allowed) return;

    // Step 2
    {
        const int last = last_n - 1;
        int rt = 0, lt = 0;
        for (int k = 1; k < last_n; ++k) {
            if (k > rt) {
                int n = 0;
                while (n + k < last_n && rat(n) == rat(n + k)) ++n;
                counts[(size_t) (last - k)] = std::min(n, rep_limit);
                if (n > 0) { lt = k; rt = k + n - 1; }
            } else {
                int p = k - lt;
                int right_part_len = rt - k + 1;
                if (counts[(size_t) (last - p)] < right_part_len) {
                    counts[(size_t) (last - k)] = std::min(counts[(size_t) (last - p)], rep_limit);
                } else {
                    int i = rt + 1;
                    while (i < last_n && rat(i) == rat(i - k)) i += 1;
                    counts[(size_t) (last - k)] = std::min(i - k, rep_limit);
                    lt = k;
                    rt = i - 1;
                }
            }
        }
    }

    // Step 3
    for (int i = 0; i < last_n - 1; ++i) {
        int repeat_len = counts[(size_t) i];
        if (repeat_len >= allowed) {
            int64_t token = rat(last_n - 2 - i);
            auto it = max_rep.find(token);
            if (it == max_rep.end() || it->second < repeat_len) max_rep[token] = repeat_len;
        }
    }

    // Step 4
    const float FLOAT_MAX_LOG = 88.7228391f;
    int max_exponent = 0;
    if (base > 1.000001f) max_exponent = (int) (FLOAT_MAX_LOG / std::log(base));

    for (size_t i = 0; i < logits.size(); ++i) {
        auto it = max_rep.find(ids[i]);
        if (it == max_rep.end()) continue;
        bool single_breaker = false;
        for (const auto & b : breakers) {
            if (b.head == ids[i] && b.tail.empty()) { single_breaker = true; break; }
        }
        if (single_breaker) continue;
        int repeat_exp = it->second - (int) allowed;
        if (max_exponent > 0 && repeat_exp > max_exponent) repeat_exp = max_exponent;
        float penalty = multiplier * std::pow(base, (float) repeat_exp);
        logits[i] -= penalty;
    }
}

static void run_case(const std::vector<int64_t> & hist, const std::vector<int64_t> & ids,
                     float multiplier, float base, int64_t allowed, int64_t last_n,
                     const std::vector<Breaker> & breakers, const char * label) {
    std::vector<float> got_logits(ids.size(), 10.0f);
    std::vector<float> want_logits(ids.size(), 10.0f);
    native_dry(hist, want_logits, ids, multiplier, base, allowed, last_n, breakers);

    // pack the breakers into the flat form the module takes
    std::vector<int64_t> brk, tails;
    for (const auto & b : breakers) {
        brk.push_back(b.head);
        brk.push_back((int64_t) tails.size());
        brk.push_back((int64_t) b.tail.size());
        for (int64_t t : b.tail) tails.push_back(t);
    }
    std::vector<int64_t> counts(hist.size() + 1, -7);
    std::vector<int64_t> ptok(ids.size() + 1, -7), plen(ids.size() + 1, -7), pcount(1, -7);
    const long long rc = dry_apply(I(hist), (long long) hist.size(), F(got_logits), I(ids),
                                   (long long) ids.size(), multiplier, base, allowed, last_n,
                                   I(brk), (long long) breakers.size(), I(tails), (long long) tails.size(),
                                   I(counts), I(ptok), I(plen), I(pcount));
    check(rc >= 0, std::string(label) + ": not refused");
    for (size_t i = 0; i < ids.size(); i++) {
        checks++;
        if (bits(got_logits[i]) != bits(want_logits[i])) {
            std::printf("FAIL %s: logit %zu bits %08x != %08x (got %g want %g)\n",
                        label, i, bits(got_logits[i]), bits(want_logits[i]), got_logits[i], want_logits[i]);
            failures++;
        }
    }
}

int main() {
    goose_init(0, nullptr);

    // ---- empty paths ----
    check(dry_is_empty(0.0f, 1.75f, 512), "multiplier 0 is empty");
    check(dry_is_empty(0.8f, 0.5f, 512), "base < 1 is empty");
    check(dry_is_empty(0.8f, 1.75f, 0), "last_n 0 is empty");
    check(!dry_is_empty(0.8f, 1.75f, 512), "normal parameters are not empty");

    const std::vector<Breaker> no_breakers;
    const std::vector<Breaker> newline = { { 13, {} } };
    const std::vector<Breaker> two_token = { { 100, { 101 } } };

    // ---- the documented example: a b c c b c y a b c ----
    {
        std::vector<int64_t> hist = { 1, 2, 3, 3, 2, 3, 9, 1, 2, 3 };
        std::vector<int64_t> ids;
        for (int64_t t = 0; t < 12; t++) ids.push_back(t);
        run_case(hist, ids, 0.8f, 1.75f, 2, 512, no_breakers, "documented example");
    }

    // ---- a plain repeat with no breakers ----
    {
        std::vector<int64_t> hist = { 5, 6, 7, 5, 6, 7, 5, 6, 7 };
        std::vector<int64_t> ids = { 5, 6, 7, 8 };
        run_case(hist, ids, 0.8f, 1.75f, 2, 512, no_breakers, "plain repeat");
    }

    // ---- a single-token breaker is never penalised ----
    {
        std::vector<int64_t> hist = { 13, 5, 6, 13, 5, 6, 13 };
        std::vector<int64_t> ids = { 5, 6, 13 };
        run_case(hist, ids, 0.8f, 1.75f, 2, 512, newline, "single-token breaker");
    }

    // ---- a two-token breaker limits the window ----
    {
        std::vector<int64_t> hist = { 100, 101, 5, 6, 5, 6, 100, 101, 5, 6 };
        std::vector<int64_t> ids = { 5, 6, 7 };
        run_case(hist, ids, 0.8f, 1.75f, 2, 512, two_token, "two-token breaker");
    }

    // ---- window shorter than allowed_length ----
    {
        std::vector<int64_t> hist = { 5, 6 };
        std::vector<int64_t> ids = { 5, 6 };
        run_case(hist, ids, 0.8f, 1.75f, 4, 512, no_breakers, "short window");
    }

    // ---- penalty_last_n clamps the window ----
    {
        std::vector<int64_t> hist = { 5, 6, 7, 5, 6, 7, 5, 6, 7 };
        std::vector<int64_t> ids = { 5, 6, 7 };
        run_case(hist, ids, 0.8f, 1.75f, 2, 4, no_breakers, "clamped window");
    }

    // ---- randomised ----
    {
        std::mt19937 rng(961);
        std::uniform_int_distribution<int> tok(0, 6);
        std::uniform_int_distribution<int> len(3, 40);
        for (int trial = 0; trial < 300; trial++) {
            const int n = len(rng);
            std::vector<int64_t> hist((size_t) n);
            for (int i = 0; i < n; i++) hist[(size_t) i] = tok(rng);
            std::vector<int64_t> ids;
            for (int64_t t = 0; t < 7; t++) ids.push_back(t);
            const float mult = (trial % 3 == 0) ? 0.0f : 0.5f + 0.1f * (float) (trial % 5);
            const float base = (trial % 4 == 0) ? 1000.0f : 1.75f;
            const std::vector<Breaker> & br = (trial % 2 == 0) ? no_breakers : newline;
            run_case(hist, ids, mult, base, 2, 64, br, "random");
        }
    }

    // ---- bad arguments ----
    {
        std::vector<int64_t> hist = { 1, 2, 3 };
        std::vector<int64_t> ids = { 1, 2 };
        std::vector<float> logits = { 1.0f, 2.0f };
        std::vector<int64_t> counts(3), ptok(2), plen(2), pcount(1);
        std::vector<int64_t> brk, tails;
        check(dry_apply(I(hist), 4, F(logits), I(ids), 2, 0.8f, 1.75f, 2, 512,
                        I(brk), 0, I(tails), 0, I(counts), I(ptok), I(plen), I(pcount)) == -1,
              "history past its array refused");
        check(dry_apply(I(hist), 3, F(logits), I(ids), 3, 0.8f, 1.75f, 2, 512,
                        I(brk), 0, I(tails), 0, I(counts), I(ptok), I(plen), I(pcount)) == -1,
              "logits past its array refused");
        check(dry_apply(I(hist), 3, F(logits), I(ids), 2, 0.8f, 1.75f, 2, 512,
                        I(brk), 0, I(tails), 0, I(counts), I(ptok), I(plen), I(std::vector<int64_t>{0})) >= 0,
              "well-formed call accepted");
    }

    std::printf("%s checks=%d failures=%d\n", failures == 0 ? "PASS" : "BAD", checks, failures);
    return failures ? 1 : 0;
}
