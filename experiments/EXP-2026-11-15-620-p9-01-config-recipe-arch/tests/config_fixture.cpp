// P9-01 S1 gate: the Goose config validator against the real native validate().
//
// Both arms get the same RunConfig. The native arm is bmoe::validate() from
// runtime/core/src/config.cpp, compiled and linked into this fixture — not a
// transcription. Because the native function returns a text and the port
// returns a rule number, the fixture maps the native message back to the rule
// number through a table of distinctive substrings; a message that maps to zero
// or to more than one rule fails the gate instead of silently agreeing.
//
// Three independent checks per table case:
//   * native ok/fail equals port ok/fail;
//   * the native first-failing rule equals the port rule (order is a contract);
//   * the rule equals the pinned expectation, so a change in the native source
//     cannot make the gate pass by moving both arms together.
//
// A deterministic fuzz sweep then compares native against port alone over
// boundary-heavy random configs, with no pins: it is there to find rule-order
// differences the handwritten table did not think of.
//
// No model, no llama.cpp, no I/O. Build: tests/build.sh cfg.

#include "bmoe/config.h"
#include "cfg.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using namespace bmoe;

static int failures = 0;

struct IOverride {
    int idx;
    int64_t value;
};
struct FOverride {
    int idx;
    double value;
};

// A case is the base valid config plus the fields the rule needs. The override
// lists are sized by their initialisers, so a case cannot declare one count and
// carry another.
struct Case {
    const char * name;
    long long expect;
    std::vector<IOverride> i;
    std::vector<FOverride> f;
};

static Case mk(const char * name, long long expect, std::vector<IOverride> i,
               std::vector<FOverride> f = {}) {
    Case c;
    c.name = name;
    c.expect = expect;
    c.i = std::move(i);
    c.f = std::move(f);
    return c;
}

static const int64_t BASE_I[32] = {
    128, 4, 2048, 0, 0, 0, 0, 0,          // n_predict .. n_gpu_layers
    1, 0, 0,                             // model/capture/replay present
    40, 0, 3, 3, 12,                     // sampling.top_k, spec source/draft/ngram
    0, 4, 0, 0, 1536, 0, 0, 0, 0, 0, 2, 0, 0, 64, 0, 0 // moe fields
};
static const double BASE_F[5] = {0.0, 0.95, 0.0, 0.0, 0.0};

static void build(const Case & c, int64_t i[32], double f[5]) {
    for (int k = 0; k < 32; ++k) i[k] = BASE_I[k];
    for (int k = 0; k < 5; ++k) f[k] = BASE_F[k];
    for (const IOverride & o : c.i) i[o.idx] = o.value;
    for (const FOverride & o : c.f) f[o.idx] = o.value;
}

static RunConfig to_native(const int64_t i[32], const double f[5]) {
    RunConfig c;
    c.n_predict = (int) i[0];
    c.n_threads = (int) i[1];
    c.n_ctx = (int) i[2];
    c.n_ubatch = (int) i[3];
    c.n_expert_used = (int) i[4];
    c.vram_cache_mb = (int) i[5];
    c.expert_hot_slots = (int) i[6];
    c.n_gpu_layers = (int) i[7];
    c.model_path = i[8] ? "model.gguf" : "";
    c.workload_capture_path = i[9] ? "run.workload" : "";
    c.workload_replay_path = i[10] ? "run.workload" : "";
    c.sampling.top_k = (int) i[11];
    c.spec.source = i[12] == 1 ? DraftSource::mtp : (i[12] == 2 ? DraftSource::ngram : DraftSource::none);
    c.spec.draft_max = (int) i[13];
    c.spec.ngram_min_match = (int) i[14];
    c.spec.ngram_max_match = (int) i[15];
    c.moe.enabled = i[16] != 0;
    c.moe.io_threads = (int) i[17];
    c.moe.cache_mb = (int) i[18];
    c.moe.cache_auto = i[19] != 0;
    c.moe.cache_floor_mb = (int) i[20];
    c.moe.cache_ceil_mb = (int) i[21];
    c.moe.prefetch_layers = (int) i[22];
    c.moe.io_two_wave = i[23] != 0;
    c.moe.predict_log = i[24] != 0;
    c.moe.predict_prefetch = i[25] != 0;
    c.moe.predict_spec_max = (int) i[26];
    c.moe.route_ahead = (int) i[27];
    c.moe.row_stream = i[28] != 0;
    c.moe.row_stream_mb = (int) i[29];
    c.moe.overlap = i[30] != 0;
    c.moe.force_cache = i[31] != 0;
    c.sampling.temp = (float) f[0];
    c.sampling.top_p = (float) f[1];
    c.spec.draft_p_min = (float) f[2];
    c.moe.drop_cold_frac = (float) f[3];
    c.moe.substitute_lambda = (float) f[4];
    return c;
}

// The native message of each rule, reduced to a substring that only that
// message contains. The fixture fails if a message matches zero or two rules.
struct Needle {
    long long code;
    const char * text;
};
static const Needle NEEDLES[] = {
    {1, "model_path is required"},
    {2, "n_predict must be positive"},
    {3, "n_threads must be positive"},
    {4, "n_ctx must be positive"},
    {5, "n_ubatch must be >= 0"},
    {6, "exceeds n_ctx="},
    {7, "n_expert_used must be >= 0"},
    {8, "capture and replay are mutually exclusive"},
    {9, "deterministic workload capture/replay requires moe.enabled"},
    {10, "does not yet support speculative decoding"},
    {11, "vram_cache_mb must be >= 0"},
    {12, "expert_hot_slots must be >= 0"},
    {13, "VRAM expert cache requires n_gpu_layers"},
    {14, "sampling.top_k must be >= 0"},
    {15, "sampling.top_p must be in"},
    {16, "speculative decoding requires greedy"},
    {17, "spec.draft_max must be in"},
    {18, "spec.draft_p_min must be in [0, 1]"},
    {19, "has no meaning for the n-gram source"},
    {20, "spec.ngram_max_match must be in"},
    {21, "spec.ngram_min_match must be in"},
    {22, "narrower than the verify batch"},
    {23, "moe.overlap requires moe.enabled"},
    {24, "moe.predict_log requires moe.enabled"},
    {25, "moe.predict_prefetch requires moe.enabled"},
    {26, "moe.route_ahead requires moe.enabled"},
    {27, "moe.row_stream requires moe.enabled"},
    {28, "moe.row_stream_mb must be >= 0"},
    {29, "moe.io_threads must be in"},
    {30, "moe.cache_mb must be >= 0"},
    {31, "is in the pathological band"},
    {32, "moe.prefetch_layers must be in"},
    {33, "moe.cache_auto and an explicit moe.cache_mb"},
    {34, "moe.cache_floor_mb must be >= 0"},
    {35, "moe.cache_ceil_mb must be >= 0"},
    {36, "prefetch_layers requires the LRU cache"},
    {37, "predict_prefetch requires the LRU cache"},
    {38, "moe.io_two_wave requires moe.overlap"},
    {39, "moe.io_two_wave requires the LRU cache"},
    {40, "moe.predict_spec_max must be in"},
    {41, "predict_prefetch and moe.prefetch_layers"},
    {42, "moe.route_ahead must be in"},
    {43, "route_ahead excludes predict_log"},
    {44, "route_ahead and self-speculative decoding"},
    {45, "drop_cold_frac requires the LRU cache"},
    {46, "drop_cold_frac must be in"},
    {47, "substitute_lambda requires the LRU cache"},
    {48, "substitute_lambda must be in"},
};

static int64_t native_code(const RunConfig & c) {
    ValidationResult r = validate(c);
    if (r.ok) return 0;
    long long hit = 0;
    int n = 0;
    for (const Needle & nd : NEEDLES) {
        if (r.error.find(nd.text) != std::string::npos) {
            hit = nd.code;
            ++n;
        }
    }
    if (n != 1) {
        std::printf("[FAIL] native message maps to %d rules: %s\n", n, r.error.c_str());
        ++failures;
        return -1000 - n;
    }
    return hit;
}

static int64_t port_code(int64_t i[32], double f[5]) {
    sl_i64 ci{i, 32};
    sl_f64 cf{f, 5};
    return cfg_validate(ci, cf);
}

// One table case: pin, native and port must all agree.
static void check(const Case & c) {
    int64_t i[32];
    double f[5];
    build(c, i, f);
    int64_t n = native_code(to_native(i, f));
    int64_t p = port_code(i, f);
    if (n != c.expect || p != c.expect) {
        std::printf("[FAIL] %s: pin=%lld native=%lld port=%lld\n", c.name, c.expect, (long long) n, (long long) p);
        ++failures;
    }
}

// A pinned value with no case name, used for the boundary tables.
static void pin(const char * name, long long expect, int64_t i[32], double f[5]) {
    long long n = native_code(to_native(i, f));
    int64_t p = port_code(i, f);
    if (n != expect || p != expect) {
        std::printf("[FAIL] pin %s: want=%lld native=%lld port=%lld\n", name, expect, (long long) n, (long long) p);
        ++failures;
    }
}

// Deterministic LCG; the pools are boundary-heavy on purpose.
struct Lcg {
    unsigned long long s = 0x9E3779B97F4A7C15ull;
    unsigned long long next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return s >> 11;
    }
    int pick(int n) { return (int) (next() % (unsigned long long) n); }
};

static const long long POOL_NEG[] = {-1, 0, 1, 2, 7, 8, 9, 1499, 1500, 1501, 2047, 2048, 2049, 4096};
static const double POOL_F[] = {0.0, -0.5, 0.15, 0.5, 0.95, 1.0, 1.01, 2.0};

int main() {
    goose_init(0, nullptr);

    const double nan = std::numeric_limits<double>::quiet_NaN();

    const std::vector<Case> cases = {
        mk("minimal valid", 0, {}),
        mk("empty model path", 1, {{8, 0}}),
        mk("n_predict zero", 2, {{0, 0}}),
        mk("n_threads negative", 3, {{1, -1}}),
        mk("n_ctx zero", 4, {{2, 0}}),
        mk("ubatch negative", 5, {{3, -1}}),
        mk("ubatch above ctx", 6, {{3, 2049}}),
        mk("ubatch equal ctx is valid", 0, {{3, 2048}}),
        mk("expert_used negative", 7, {{4, -1}}),
        mk("capture and replay", 8, {{9, 1}, {10, 1}}),
        mk("replay without streaming", 9, {{10, 1}}),
        mk("replay with speculation", 10, {{16, 1}, {10, 1}, {12, 1}}),
        mk("vram negative", 11, {{5, -1}}),
        mk("slots negative", 12, {{6, -1}}),
        mk("vram without gpu layers", 13, {{5, 64}}),
        mk("vram with gpu layers", 0, {{5, 64}, {7, 8}}),
        mk("top_k under sampling", 14, {{11, -1}}, {{0, 0.8}}),
        mk("top_p zero under sampling", 15, {}, {{0, 0.8}, {1, 0.0}}),
        mk("top_p above one", 15, {{11, 40}}, {{0, 0.8}, {1, 1.5}}),
        mk("greedy ignores sampling knobs", 0, {{11, -3}}, {{0, 0.0}, {1, 5.0}}),
        mk("spec with sampling", 16, {{12, 1}}, {{0, 0.8}}),
        mk("draft zero", 17, {{12, 1}, {13, 0}}),
        mk("draft above limit", 17, {{12, 1}, {13, 9}}),
        mk("draft at limit", 0, {{12, 1}, {13, 8}}),
        mk("mtp p_min above one", 18, {{12, 1}}, {{2, 1.5}}),
        mk("ngram rejects p_min", 19, {{12, 2}}, {{2, 0.6}}),
        mk("ngram max above limit", 20, {{12, 2}, {15, 65}}),
        mk("ngram min zero", 21, {{12, 2}, {14, 0}}),
        mk("ngram min above max", 21, {{12, 2}, {14, 13}}),
        mk("verify batch too narrow", 22, {{12, 1}, {3, 3}}),
        mk("verify batch exactly wide", 0, {{12, 1}, {3, 4}}),
        mk("overlap without streaming", 23, {{30, 1}}),
        mk("predict_log without streaming", 24, {{24, 1}}),
        mk("predict_prefetch without streaming", 25, {{25, 1}}),
        mk("route_ahead without streaming", 26, {{27, 1}}),
        mk("row_stream without streaming", 27, {{28, 1}}),
        mk("row_stream_mb negative", 28, {{29, -1}}),
        mk("io_threads zero", 29, {{16, 1}, {17, 0}}),
        mk("io_threads above max", 29, {{16, 1}, {17, 9}}),
        mk("cache_mb negative", 30, {{16, 1}, {18, -1}}),
        mk("cache in pathological band", 31, {{16, 1}, {18, 1499}}),
        mk("band allowed with force_cache", 0, {{16, 1}, {18, 1499}, {31, 1}}),
        mk("cache floor negative", 34, {{16, 1}, {20, -1}}),
        mk("cache ceil negative", 35, {{16, 1}, {21, -1}}),
        mk("cache_auto with explicit budget", 33, {{16, 1}, {18, 1500}, {19, 1}}),
        mk("prefetch range", 32, {{16, 1}, {22, 9}}),
        mk("prefetch without cache", 36, {{16, 1}, {22, 2}}),
        mk("prefetch with fixed cache", 0, {{16, 1}, {22, 2}, {18, 1500}}),
        mk("predict without cache", 37, {{16, 1}, {25, 1}}),
        mk("two wave without overlap", 38, {{16, 1}, {23, 1}, {19, 1}}),
        mk("two wave without cache", 39, {{16, 1}, {23, 1}, {30, 1}}),
        mk("two wave valid", 0, {{16, 1}, {23, 1}, {30, 1}, {18, 1500}}),
        mk("predict_spec_max above", 40, {{16, 1}, {26, 9}}),
        mk("two predictors", 41, {{16, 1}, {19, 1}, {22, 2}, {25, 1}}),
        mk("route_ahead range", 42, {{16, 1}, {27, 9}}),
        mk("route_ahead excludes probe", 43, {{16, 1}, {27, 1}, {24, 1}}),
        mk("route_ahead excludes spec", 44, {{16, 1}, {27, 1}, {12, 1}, {19, 1}}),
        mk("drop without cache", 45, {{16, 1}}, {{3, 0.5}}),
        mk("drop above one", 46, {{16, 1}, {18, 1500}}, {{3, 1.01}}),
        mk("drop negative", 46, {{16, 1}, {18, 1500}}, {{3, -0.1}}),
        mk("drop NaN", 46, {{16, 1}, {18, 1500}}, {{3, nan}}),
        mk("drop at one", 0, {{16, 1}, {18, 1500}}, {{3, 1.0}}),
        mk("substitute without cache", 47, {{16, 1}}, {{4, 0.15}}),
        mk("substitute above one", 48, {{16, 1}, {18, 1500}}, {{4, 1.01}}),
        mk("substitute NaN", 48, {{16, 1}, {18, 1500}}, {{4, nan}}),
        mk("substitute at one", 0, {{16, 1}, {18, 1500}}, {{4, 1.0}}),
    };
    const int n_cases = (int) cases.size();
    for (int k = 0; k < n_cases; ++k) check(cases[k]);

    // Rule-order pins: two rules fail at once, the earlier one must be reported.
    {
        int64_t i[32];
        double f[5];
        Case c = mk("order: model_path before n_predict", 1, {{8, 0}, {0, 0}});
        build(c, i, f);
        pin(c.name, 1, i, f);
        Case d = mk("order: ubatch before expert_used", 6, {{3, 3000}, {4, -1}});
        build(d, i, f);
        pin(d.name, 6, i, f);
        Case e = mk("order: cache band before auto conflict", 31, {{16, 1}, {18, 1000}, {19, 1}});
        build(e, i, f);
        pin(e.name, 31, i, f);
        Case g = mk("order: route_ahead range before exclude", 42, {{16, 1}, {27, 9}, {24, 1}});
        build(g, i, f);
        pin(g.name, 42, i, f);
        Case h = mk("order: drop cache before drop range", 45, {{16, 1}}, {{3, 5.0}});
        build(h, i, f);
        pin(h.name, 45, i, f);
        Case j = mk("order: workload moe before vram", 9, {{16, 0}, {10, 1}, {5, -1}});
        build(j, i, f);
        pin(j.name, 9, i, f);
    }

    // Short layouts are a caller error, not a config verdict.
    {
        int64_t i[32] = {0};
        double f[5] = {0};
        sl_i64 ci{i, 4};
        sl_f64 cf{f, 5};
        int64_t p = cfg_validate(ci, cf);
        if (p != -1) {
            std::printf("[FAIL] short layout: port=%lld want=-1\n", (long long) p);
            ++failures;
        }
        sl_i64 cj{i, 32};
        sl_f64 cg{f, 2};
        p = cfg_validate(cj, cg);
        if (p != -1) {
            std::printf("[FAIL] short float layout: port=%lld want=-1\n", (long long) p);
            ++failures;
        }
    }

    // Differential fuzz: native against port, no pins.
    Lcg rng;
    const int FUZZ = 40000;
    int64_t agrees = 0, ok_runs = 0, fail_runs = 0;
    long long codes[49] = {0};
    for (int k = 0; k < FUZZ; ++k) {
        int64_t i[32];
        double f[5];
        for (int q = 0; q < 32; ++q) i[q] = BASE_I[q];
        for (int q = 0; q < 5; ++q) f[q] = BASE_F[q];
        // A random subset of fields is perturbed; the rest keeps the valid base.
        const int taps = 1 + rng.pick(6);
        for (int t = 0; t < taps; ++t) {
            switch (rng.pick(32)) {
            case 0: i[0] = POOL_NEG[rng.pick(14)]; break;
            case 1: i[1] = POOL_NEG[rng.pick(14)]; break;
            case 2: i[2] = 1 + POOL_NEG[rng.pick(14)]; break;
            case 3: i[3] = POOL_NEG[rng.pick(14)]; break;
            case 4: i[4] = POOL_NEG[rng.pick(14)]; break;
            case 5: i[5] = POOL_NEG[rng.pick(14)]; break;
            case 6: i[6] = POOL_NEG[rng.pick(14)]; break;
            case 7: i[7] = (rng.pick(3) == 0) ? -1 : rng.pick(9); break;
            case 8: i[8] = rng.pick(6) != 0; break;
            case 9: i[9] = rng.pick(2); break;
            case 10: i[10] = rng.pick(2); break;
            case 11: i[11] = POOL_NEG[rng.pick(14)]; break;
            case 12: i[12] = rng.pick(3); break;
            case 13: i[13] = POOL_NEG[rng.pick(14)]; break;
            case 14: i[14] = POOL_NEG[rng.pick(14)]; break;
            case 15: i[15] = POOL_NEG[rng.pick(14)]; break;
            case 16: i[16] = rng.pick(2); break;
            case 17: i[17] = POOL_NEG[rng.pick(14)]; break;
            case 18: i[18] = (rng.pick(3) == 0) ? 0 : POOL_NEG[rng.pick(14)]; break;
            case 19: i[19] = rng.pick(2); break;
            case 20: i[20] = POOL_NEG[rng.pick(14)]; break;
            case 21: i[21] = POOL_NEG[rng.pick(14)]; break;
            case 22: i[22] = POOL_NEG[rng.pick(14)]; break;
            case 23: i[23] = rng.pick(2); break;
            case 24: i[24] = rng.pick(2); break;
            case 25: i[25] = rng.pick(2); break;
            case 26: i[26] = POOL_NEG[rng.pick(14)]; break;
            case 27: i[27] = POOL_NEG[rng.pick(14)]; break;
            case 28: i[28] = rng.pick(2); break;
            case 29: i[29] = POOL_NEG[rng.pick(14)]; break;
            case 30: i[30] = rng.pick(2); break;
            case 31: i[31] = rng.pick(2); break;
            }
        }
        if (rng.pick(2) == 0) f[0] = POOL_F[rng.pick(8)];
        if (rng.pick(3) == 0) f[1] = POOL_F[rng.pick(8)];
        if (rng.pick(3) == 0) f[2] = POOL_F[rng.pick(8)];
        if (rng.pick(2) == 0) f[3] = POOL_F[rng.pick(8)];
        if (rng.pick(2) == 0) f[4] = POOL_F[rng.pick(8)];
        if (rng.pick(20) == 0) f[rng.pick(5)] = nan;

        long long n = native_code(to_native(i, f));
        int64_t p = port_code(i, f);
        if (n != p) {
            std::printf("[FAIL] fuzz %d: native=%lld port=%lld (`%s`)\n", k, (long long) n, (long long) p,
                        to_native(i, f).model_path.c_str());
            ++failures;
            if (failures > 10) break;
        } else {
            ++agrees;
        }
        if (p == 0) ++ok_runs; else ++fail_runs;
        if (p >= 0 && p <= 48) ++codes[p];
    }

    int covered = 0;
    for (int k = 1; k <= 48; ++k) covered += codes[k] > 0;

    std::printf("FG622_S1 cases=%d fuzz=%d values=%lld failures=%d covered_rules=%d/48 ok=%lld fail=%lld\n",
                n_cases, FUZZ, (long long) agrees, failures, covered, (long long) ok_runs, (long long) fail_runs);
    if (failures == 0) {
        std::printf("all config-validate checks passed\n");
        return 0;
    }
    std::printf("%d check(s) failed\n", failures);
    return 1;
}
