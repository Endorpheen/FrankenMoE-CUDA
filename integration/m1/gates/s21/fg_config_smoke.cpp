// S2.1 gate: bmoe::validate() against bmoe::validate_native().
//
// One binary, two builds. In the OFF build validate() forwards to
// validate_native(), so the comparison is a structural identity but still
// proves the default path did not change. In the ON build validate() goes
// through the Goose module: fg_config_to_flat() -> cfg_validate() -> the
// carrier's switch back to English. That is the arm this gate exists for.
//
// Four independent checks per case:
//   * accept/reject agreement between the two paths;
//   * identical error TEXT (the carrier re-creates the native message, so a
//     transcription slip fails here instead of being hidden by a rule number);
//   * the same rule NUMBER, derived from the native message through a
//     one-needle-per-rule table (a message that maps to zero or two rules fails);
//   * the pinned rule, so a change in the native source cannot move both arms
//     together and still pass.
//
// Then a deterministic differential sweep with no pins, to find order
// differences the handwritten table did not think of.
//
// No model, no llama.cpp, no I/O, no GPU.

#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include "bmoe/config.h"
#include "fg-config-native.h"

#ifdef FG_GOOSE_CONFIG
#include "fg-goose-config-bridge.h"
#endif

using namespace bmoe;

static int failures = 0;

// The native message of each rule, reduced to a substring only that message
// contains. EXP-620 used the same table; it is copied, not imported, so this
// gate stands on its own.
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
static const int N_NEEDLES = (int) (sizeof(NEEDLES) / sizeof(NEEDLES[0]));

// A message that matches no rule, or two, is a gate failure: the table must stay
// one-to-one with the native texts.
static long long rule_of(const std::string & text) {
    long long hit = 0;
    int n = 0;
    for (int k = 0; k < N_NEEDLES; ++k) {
        if (text.find(NEEDLES[k].text) != std::string::npos) {
            hit = NEEDLES[k].code;
            ++n;
        }
    }
    if (n != 1) {
        std::printf("[FAIL] message maps to %d rules: %s\n", n, text.c_str());
        ++failures;
        return -1000 - n;
    }
    return hit;
}

// The valid baseline: greedy, no streaming, no speculation, 2048 context.
static RunConfig base() {
    RunConfig c;
    c.model_path = "model.gguf";
    c.n_predict = 128;
    c.n_threads = 4;
    c.n_ctx = 2048;
    return c;
}

struct Case {
    const char * name;
    long long expect;              // pinned rule: 0 = valid
    void (*apply)(RunConfig &);    // the fields the rule needs
};

static RunConfig mk(const Case & c) {
    RunConfig cfg = base();
    c.apply(cfg);
    return cfg;
}

static long long check_case(const Case & c) {
    RunConfig cfg = mk(c);
    const ValidationResult native = validate_native(cfg);
    const ValidationResult selected = validate(cfg);
    const long long nrule = native.ok ? 0 : rule_of(native.error);
    const long long srule = selected.ok ? 0 : rule_of(selected.error);

    long long bad = 0;
    if (native.ok != selected.ok) {
        std::printf("[FAIL] %s: accept/reject native=%d selected=%d\n", c.name, (int) native.ok, (int) selected.ok);
        bad = 1;
    }
    if (!native.ok && native.error != selected.error) {
        std::printf("[FAIL] %s: text differs\n  native  : %s\n  selected: %s\n", c.name, native.error.c_str(),
                    selected.error.c_str());
        bad = 1;
    }
    if (nrule != srule) {
        std::printf("[FAIL] %s: rule native=%lld selected=%lld\n", c.name, nrule, srule);
        bad = 1;
    }
    if (nrule != c.expect) {
        std::printf("[FAIL] %s: pin=%lld native=%lld\n", c.name, c.expect, nrule);
        bad = 1;
    }
    failures += (int) bad;
    return srule;
}

int main() {
    const float nanf = std::numeric_limits<float>::quiet_NaN();

    const std::vector<Case> cases = {
        {"minimal valid", 0, [](RunConfig &) {}},
        {"empty model path", 1, [](RunConfig & c) { c.model_path.clear(); }},
        {"n_predict zero", 2, [](RunConfig & c) { c.n_predict = 0; }},
        {"n_threads negative", 3, [](RunConfig & c) { c.n_threads = -1; }},
        {"n_ctx zero", 4, [](RunConfig & c) { c.n_ctx = 0; }},
        {"ubatch negative", 5, [](RunConfig & c) { c.n_ubatch = -1; }},
        {"ubatch above ctx", 6, [](RunConfig & c) { c.n_ubatch = 2049; }},
        {"ubatch equal ctx is valid", 0, [](RunConfig & c) { c.n_ubatch = 2048; }},
        {"expert_used negative", 7, [](RunConfig & c) { c.n_expert_used = -1; }},
        {"capture and replay", 8, [](RunConfig & c) { c.workload_capture_path = "run.workload";
                                                     c.workload_replay_path = "run.workload"; }},
        {"replay without streaming", 9, [](RunConfig & c) { c.workload_replay_path = "run.workload"; }},
        {"replay with speculation", 10, [](RunConfig & c) { c.workload_capture_path = "run.workload";
                                                           c.moe.enabled = true;
                                                           c.moe.cache_auto = true;
                                                           c.spec.source = DraftSource::mtp; }},
        {"vram negative", 11, [](RunConfig & c) { c.vram_cache_mb = -1; }},
        {"slots negative", 12, [](RunConfig & c) { c.expert_hot_slots = -1; }},
        {"vram without gpu layers", 13, [](RunConfig & c) { c.vram_cache_mb = 64; }},
        {"vram with gpu layers", 0, [](RunConfig & c) { c.vram_cache_mb = 64;
                                                        c.n_gpu_layers = 8; }},
        {"top_k under sampling", 14, [](RunConfig & c) { c.sampling.temp = 0.8f;
                                                         c.sampling.top_k = -1; }},
        {"top_p zero under sampling", 15, [](RunConfig & c) { c.sampling.temp = 0.8f;
                                                              c.sampling.top_p = 0.0f; }},
        {"top_p above one", 15, [](RunConfig & c) { c.sampling.temp = 0.8f;
                                                    c.sampling.top_p = 1.5f; }},
        {"greedy ignores sampling knobs", 0, [](RunConfig & c) { c.sampling.temp = 0.0f;
                                                                 c.sampling.top_k = -3;
                                                                 c.sampling.top_p = 5.0f; }},
        {"spec with sampling", 16, [](RunConfig & c) { c.spec.source = DraftSource::mtp;
                                                       c.sampling.temp = 0.8f; }},
        {"draft zero", 17, [](RunConfig & c) { c.spec.source = DraftSource::mtp;
                                               c.spec.draft_max = 0; }},
        {"draft above limit", 17, [](RunConfig & c) { c.spec.source = DraftSource::mtp;
                                                      c.spec.draft_max = 9; }},
        {"draft at limit", 0, [](RunConfig & c) { c.spec.source = DraftSource::mtp;
                                                  c.spec.draft_max = 8;
                                                  c.n_ubatch = 9; }},
        {"mtp p_min above one", 18, [](RunConfig & c) { c.spec.source = DraftSource::mtp;
                                                        c.spec.draft_p_min = 1.5f; }},
        {"ngram rejects p_min", 19, [](RunConfig & c) { c.spec.source = DraftSource::ngram;
                                                        c.spec.draft_p_min = 0.6f; }},
        {"ngram max above limit", 20, [](RunConfig & c) { c.spec.source = DraftSource::ngram;
                                                          c.spec.ngram_max_match = 65; }},
        {"ngram min zero", 21, [](RunConfig & c) { c.spec.source = DraftSource::ngram;
                                                   c.spec.ngram_min_match = 0; }},
        {"ngram min above max", 21, [](RunConfig & c) { c.spec.source = DraftSource::ngram;
                                                        c.spec.ngram_min_match = 13; }},
        {"verify batch too narrow", 22, [](RunConfig & c) { c.spec.source = DraftSource::mtp;
                                                            c.n_ubatch = 3; }},
        {"verify batch exactly wide", 0, [](RunConfig & c) { c.spec.source = DraftSource::mtp;
                                                            c.n_ubatch = 4; }},
        {"overlap without streaming", 23, [](RunConfig & c) { c.moe.overlap = true; }},
        {"predict_log without streaming", 24, [](RunConfig & c) { c.moe.predict_log = true; }},
        {"predict_prefetch without streaming", 25, [](RunConfig & c) { c.moe.predict_prefetch = true; }},
        {"route_ahead without streaming", 26, [](RunConfig & c) { c.moe.route_ahead = 1; }},
        {"row_stream without streaming", 27, [](RunConfig & c) { c.moe.row_stream = true; }},
        {"row_stream_mb negative", 28, [](RunConfig & c) { c.moe.row_stream_mb = -1; }},
        {"io_threads zero", 29, [](RunConfig & c) { c.moe.enabled = true;
                                                    c.moe.io_threads = 0; }},
        {"io_threads above max", 29, [](RunConfig & c) { c.moe.enabled = true;
                                                         c.moe.io_threads = 9; }},
        {"cache_mb negative", 30, [](RunConfig & c) { c.moe.enabled = true;
                                                      c.moe.cache_mb = -1; }},
        {"cache in pathological band", 31, [](RunConfig & c) { c.moe.enabled = true;
                                                               c.moe.cache_mb = 1499; }},
        {"band allowed with force_cache", 0, [](RunConfig & c) { c.moe.enabled = true;
                                                                 c.moe.cache_mb = 1499;
                                                                 c.moe.force_cache = true; }},
        {"cache floor negative", 34, [](RunConfig & c) { c.moe.enabled = true;
                                                         c.moe.cache_floor_mb = -1; }},
        {"cache ceil negative", 35, [](RunConfig & c) { c.moe.enabled = true;
                                                        c.moe.cache_ceil_mb = -1; }},
        {"cache_auto with explicit budget", 33, [](RunConfig & c) { c.moe.enabled = true;
                                                                    c.moe.cache_mb = 1500;
                                                                    c.moe.cache_auto = true; }},
        {"prefetch range", 32, [](RunConfig & c) { c.moe.enabled = true;
                                                   c.moe.prefetch_layers = 9; }},
        {"prefetch without cache", 36, [](RunConfig & c) { c.moe.enabled = true;
                                                           c.moe.prefetch_layers = 2; }},
        {"prefetch with fixed cache", 0, [](RunConfig & c) { c.moe.enabled = true;
                                                             c.moe.prefetch_layers = 2;
                                                             c.moe.cache_mb = 1500; }},
        {"predict without cache", 37, [](RunConfig & c) { c.moe.enabled = true;
                                                          c.moe.predict_prefetch = true; }},
        {"two wave without overlap", 38, [](RunConfig & c) { c.moe.enabled = true;
                                                             c.moe.io_two_wave = true;
                                                             c.moe.cache_auto = true; }},
        {"two wave without cache", 39, [](RunConfig & c) { c.moe.enabled = true;
                                                           c.moe.io_two_wave = true;
                                                           c.moe.overlap = true; }},
        {"two wave valid", 0, [](RunConfig & c) { c.moe.enabled = true;
                                                  c.moe.io_two_wave = true;
                                                  c.moe.overlap = true;
                                                  c.moe.cache_mb = 1500; }},
        {"predict_spec_max above", 40, [](RunConfig & c) { c.moe.enabled = true;
                                                           c.moe.predict_spec_max = 9; }},
        {"two predictors", 41, [](RunConfig & c) { c.moe.enabled = true;
                                                   c.moe.cache_auto = true;
                                                   c.moe.prefetch_layers = 2;
                                                   c.moe.predict_prefetch = true; }},
        {"route_ahead range", 42, [](RunConfig & c) { c.moe.enabled = true;
                                                      c.moe.route_ahead = 9; }},
        {"route_ahead excludes probe", 43, [](RunConfig & c) { c.moe.enabled = true;
                                                               c.moe.cache_auto = true;
                                                               c.moe.route_ahead = 1;
                                                               c.moe.predict_log = true; }},
        {"route_ahead excludes spec", 44, [](RunConfig & c) { c.moe.enabled = true;
                                                              c.moe.cache_auto = true;
                                                              c.moe.route_ahead = 1;
                                                              c.spec.source = DraftSource::mtp; }},
        {"drop without cache", 45, [](RunConfig & c) { c.moe.enabled = true;
                                                       c.moe.drop_cold_frac = 0.5f; }},
        {"drop above one", 46, [](RunConfig & c) { c.moe.enabled = true;
                                                   c.moe.cache_mb = 1500;
                                                   c.moe.drop_cold_frac = 1.01f; }},
        {"drop negative", 46, [](RunConfig & c) { c.moe.enabled = true;
                                                  c.moe.cache_mb = 1500;
                                                  c.moe.drop_cold_frac = -0.1f; }},
        {"drop NaN", 46, [](RunConfig & c) { c.moe.enabled = true;
                                                 c.moe.cache_mb = 1500;
                                                 c.moe.drop_cold_frac = std::numeric_limits<float>::quiet_NaN(); }},
        {"drop at one", 0, [](RunConfig & c) { c.moe.enabled = true;
                                               c.moe.cache_mb = 1500;
                                               c.moe.drop_cold_frac = 1.0f; }},
        {"substitute without cache", 47, [](RunConfig & c) { c.moe.enabled = true;
                                                             c.moe.substitute_lambda = 0.15f; }},
        {"substitute above one", 48, [](RunConfig & c) { c.moe.enabled = true;
                                                         c.moe.cache_mb = 1500;
                                                         c.moe.substitute_lambda = 1.01f; }},
        {"substitute NaN", 48, [](RunConfig & c) { c.moe.enabled = true;
                                                       c.moe.cache_mb = 1500;
                                                       c.moe.substitute_lambda = std::numeric_limits<float>::quiet_NaN(); }},
        {"substitute at one", 0, [](RunConfig & c) { c.moe.enabled = true;
                                                     c.moe.cache_mb = 1500;
                                                     c.moe.substitute_lambda = 1.0f; }},
    };

    // Rule-order pins: two rules fail at once, the earlier one must be reported.
    // These six are the pairs the native chain actually allows to collide.
    const std::vector<Case> order_pins = {
        {"order: model_path before n_predict", 1, [](RunConfig & c) { c.model_path.clear();
                                                                      c.n_predict = 0; }},
        {"order: ubatch before expert_used", 6, [](RunConfig & c) { c.n_ubatch = 3000;
                                                                    c.n_expert_used = -1; }},
        {"order: cache band before auto conflict", 31, [](RunConfig & c) { c.moe.enabled = true;
                                                                           c.moe.cache_mb = 1000;
                                                                           c.moe.cache_auto = true; }},
        {"order: route_ahead range before exclude", 42, [](RunConfig & c) { c.moe.enabled = true;
                                                                            c.moe.route_ahead = 9;
                                                                            c.moe.predict_log = true; }},
        {"order: drop cache before drop range", 45, [](RunConfig & c) { c.moe.enabled = true;
                                                                        c.moe.drop_cold_frac = 5.0f; }},
        {"order: workload moe before vram", 9, [](RunConfig & c) { c.workload_replay_path = "run.workload";
                                                                  c.moe.enabled = false;
                                                                  c.vram_cache_mb = -1; }},
    };

    // One call per case: the goose counter at the end must equal the number of
    // dispatches, so a case may not be evaluated twice.
    long long covered[49] = {0};
    for (const Case & c : cases) {
        const long long r = check_case(c);
        if (r >= 0 && r <= 48) ++covered[r];
    }
    for (const Case & c : order_pins) {
        const long long r = check_case(c);
        if (r >= 0 && r <= 48) ++covered[r];
    }

    int rules = 0;
    for (int k = 1; k <= 48; ++k) rules += covered[k] > 0;

    // Deterministic differential sweep, no pins: native against the selected
    // path on boundary-heavy random configs.
    struct Lcg {
        unsigned long long s = 0x9E3779B97F4A7C15ull;
        unsigned long long next() {
            s = s * 6364136223846793005ull + 1442695040888963407ull;
            return s >> 11;
        }
        int pick(int n) { return (int) (next() % (unsigned long long) n); }
    } rng;
    // Boundary-heavy on purpose, and it must straddle every module constant:
    // io_threads/prefetch/route_ahead/draft limits (7..9), ngram limit (63..65),
    // cache_min_mb (1450..1501). A mutation control shifted cache_min_mb down
    // by 100 and was invisible to the first pool, which is why 1450/1498 exist.
    static const int POOL[] = {-1, 0, 1, 2, 3, 4, 7, 8, 9, 63, 64, 65,
                               1450, 1498, 1499, 1500, 1501, 2047, 2048, 2049, 4096};
    const int NP = (int) (sizeof(POOL) / sizeof(POOL[0]));
    static const float POOLF[] = {0.0f, -0.5f, 0.15f, 0.5f, 0.95f, 1.0f, 1.01f, 2.0f};
    const int FUZZ = 40000;
    long long ok_runs = 0, fail_runs = 0, text_mismatch = 0;

    for (int k = 0; k < FUZZ; ++k) {
        RunConfig cfg = base();
        const int taps = 1 + rng.pick(8);
        for (int t = 0; t < taps; ++t) {
            switch (rng.pick(24)) {
            case 0: cfg.model_path = rng.pick(6) ? "model.gguf" : ""; break;
            case 1: cfg.n_predict = POOL[rng.pick(NP)]; break;
            case 2: cfg.n_threads = POOL[rng.pick(NP)]; break;
            case 3: cfg.n_ctx = 1 + POOL[rng.pick(NP)]; break;
            case 4: cfg.n_ubatch = POOL[rng.pick(NP)]; break;
            case 5: cfg.n_expert_used = POOL[rng.pick(NP)]; break;
            case 6: cfg.vram_cache_mb = POOL[rng.pick(NP)]; break;
            case 7: cfg.expert_hot_slots = POOL[rng.pick(NP)]; break;
            case 8: cfg.n_gpu_layers = rng.pick(3) == 0 ? -1 : rng.pick(9); break;
            case 9: cfg.workload_capture_path = rng.pick(4) ? "" : "run.workload"; break;
            case 10: cfg.workload_replay_path = rng.pick(4) ? "" : "run.workload"; break;
            case 11: cfg.sampling.top_k = POOL[rng.pick(NP)]; break;
            case 12: cfg.spec.source = (DraftSource) rng.pick(3); break;
            case 13: cfg.spec.draft_max = POOL[rng.pick(NP)]; break;
            case 14: cfg.spec.ngram_min_match = POOL[rng.pick(NP)]; break;
            case 15: cfg.spec.ngram_max_match = POOL[rng.pick(NP)]; break;
            case 16: cfg.moe.enabled = rng.pick(2) != 0; break;
            case 17: cfg.moe.io_threads = POOL[rng.pick(NP)]; break;
            case 18: cfg.moe.cache_mb = rng.pick(3) == 0 ? 0 : POOL[rng.pick(NP)]; break;
            case 19: cfg.moe.cache_auto = rng.pick(2) != 0; break;
            case 20: cfg.moe.prefetch_layers = POOL[rng.pick(NP)]; break;
            case 21: cfg.moe.predict_spec_max = POOL[rng.pick(NP)]; break;
            case 22: cfg.moe.route_ahead = POOL[rng.pick(NP)]; break;
            case 23: cfg.moe.row_stream_mb = POOL[rng.pick(NP)]; break;
            }
        }
        if (rng.pick(2) == 0) cfg.sampling.temp = POOLF[rng.pick(8)];
        if (rng.pick(3) == 0) cfg.sampling.top_p = POOLF[rng.pick(8)];
        if (rng.pick(3) == 0) cfg.spec.draft_p_min = POOLF[rng.pick(8)];
        if (rng.pick(2) == 0) cfg.moe.drop_cold_frac = POOLF[rng.pick(8)];
        if (rng.pick(2) == 0) cfg.moe.substitute_lambda = POOLF[rng.pick(8)];
        if (rng.pick(20) == 0) {
            switch (rng.pick(5)) {
            case 0: cfg.sampling.temp = nanf; break;
            case 1: cfg.sampling.top_p = nanf; break;
            case 2: cfg.spec.draft_p_min = nanf; break;
            case 3: cfg.moe.drop_cold_frac = nanf; break;
            case 4: cfg.moe.substitute_lambda = nanf; break;
            }
        }
        // Remaining moe booleans, cheap to flip.
        cfg.moe.overlap = rng.pick(4) == 0;
        cfg.moe.io_two_wave = rng.pick(6) == 0;
        cfg.moe.predict_log = rng.pick(6) == 0;
        cfg.moe.predict_prefetch = rng.pick(4) == 0;
        cfg.moe.row_stream = rng.pick(6) == 0;
        cfg.moe.force_cache = rng.pick(6) == 0;

        const ValidationResult native = validate_native(cfg);
        const ValidationResult selected = validate(cfg);
        if (native.ok != selected.ok || (!native.ok && native.error != selected.error)) {
            std::printf("[FAIL] fuzz %d: native.ok=%d selected.ok=%d\n  native  : %s\n  selected: %s\n", k,
                        (int) native.ok, (int) selected.ok, native.error.c_str(), selected.error.c_str());
            ++text_mismatch;
            ++failures;
            if (failures > 10) break;
        }
        if (selected.ok) ++ok_runs; else ++fail_runs;
    }

    const long long calls = (long long) cases.size() + (long long) order_pins.size() + FUZZ;
#ifdef FG_GOOSE_CONFIG
    const long long goose_calls = fg_config_goose_calls();
    const char * mode = "goose";
#else
    const long long goose_calls = 0;
    const char * mode = "native";
#endif
#ifdef FG_GOOSE_CONFIG
    // Every dispatch must have entered the module: a call that stayed in C
    // would show up here as a shortfall.
    if (goose_calls != calls) {
        std::printf("[FAIL] goose_calls=%lld but validate() ran %lld times\n", goose_calls, calls);
        ++failures;
    }
#else
    // No Goose object is linked in this build, so the counter must not exist and
    // must not move: a non-zero value would mean the flag leaked into the OFF path.
    if (goose_calls != 0) {
        std::printf("[FAIL] OFF build reports goose_calls=%lld\n", goose_calls);
        ++failures;
    }
#endif

    std::printf("FG632_SMOKE mode=%s cases=%d pins=%d fuzz=%d failures=%d rules=%d/48 ok=%lld fail=%lld "
                "goose_calls=%lld\n",
                mode, (int) cases.size(), (int) order_pins.size(), FUZZ, failures, rules, ok_runs, fail_runs,
                goose_calls);
    if (failures == 0) {
        std::printf("all config-validate checks passed\n");
        return 0;
    }
    std::printf("%d check(s) failed\n", failures);
    return 1;
}
