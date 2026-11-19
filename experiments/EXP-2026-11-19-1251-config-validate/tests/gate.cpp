// Differential: the ported rule index against bmoe::validate, real native code, random configs.
// The native side reports a message; the harness maps the messages this port covers to indices.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include "core/include/bmoe/config.h"

typedef int64_t (*fn_rule)(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,
                           int64_t,int64_t,int64_t,int64_t,double,int64_t,double,int64_t,int64_t);
extern "C" int64_t cfg_rule_call(void * f, int64_t has_model, int64_t n_predict, int64_t n_threads,
                                 int64_t n_ctx, int64_t n_ubatch, int64_t n_expert_used,
                                 int64_t wl_cap, int64_t wl_rep, int64_t moe, int64_t spec,
                                 int64_t is_mtp, int64_t is_ngram, int64_t draft_max, double p_min,
                                 int64_t ng_max, int64_t ng_min, int64_t vram, int64_t slots,
                                 int64_t ngl, double temp, int64_t top_k, double top_p,
                                 int64_t overlap, int64_t plog, int64_t ppf, int64_t route_ahead);

static int native_rule(const bmoe::ValidationResult & r) {
    if (r.ok) return 0;
    const std::string & m = r.error;
    if (m.rfind("model_path is required", 0) == 0) return 1;
    if (m.rfind("n_predict must be positive", 0) == 0) return 2;
    if (m.rfind("n_threads must be positive", 0) == 0) return 3;
    if (m.rfind("n_ctx must be positive", 0) == 0) return 4;
    if (m.rfind("n_ubatch must be >= 0", 0) == 0) return 5;
    // rfind(s, 0) tests a PREFIX, not a substring: for this message it always returned npos
    if (m.find("is narrower than the verify batch") != std::string::npos) return 22;
    if (m.rfind("n_ubatch=", 0) == 0) return 6;
    if (m.rfind("n_expert_used must be >= 0", 0) == 0) return 7;
    if (m.rfind("workload capture and replay are mutually exclusive", 0) == 0) return 8;
    if (m.rfind("deterministic workload capture/replay requires moe.enabled", 0) == 0) return 9;
    if (m.rfind("deterministic workload capture/replay does not yet support speculative", 0) == 0) return 10;
    if (m.rfind("vram_cache_mb must be >= 0", 0) == 0) return 11;
    if (m.rfind("expert_hot_slots must be >= 0", 0) == 0) return 12;
    if (m.rfind("VRAM expert cache requires n_gpu_layers", 0) == 0) return 13;
    if (m.rfind("sampling.top_k must be >= 0", 0) == 0) return 14;
    if (m.rfind("sampling.top_p must be in (0, 1]", 0) == 0) return 15;
    if (m.rfind("speculative decoding requires greedy decoding", 0) == 0) return 16;
    if (m.rfind("spec.draft_max must be in", 0) == 0) return 17;
    if (m.rfind("spec.draft_p_min must be in [0, 1]", 0) == 0) return 18;
    if (m.rfind("spec.draft_p_min is an MTP knob", 0) == 0) return 19;
    if (m.rfind("spec.ngram_max_match must be in", 0) == 0) return 20;
    if (m.rfind("spec.ngram_min_match must be in", 0) == 0) return 21;
    if (m.rfind("moe.overlap requires moe.enabled", 0) == 0) return 23;
    if (m.rfind("moe.predict_log requires moe.enabled", 0) == 0) return 24;
    if (m.rfind("moe.predict_prefetch requires moe.enabled", 0) == 0) return 25;
    if (m.rfind("moe.route_ahead must be in", 0) == 0) return 25;
    if (m.rfind("moe.route_ahead requires moe.enabled", 0) == 0) return 26;
    return -99;   // a native rule this port does not cover
}

int main() {
    std::mt19937 rng(777);
    int fails = 0, cases = 0, uncovered = 0;
    for (int t = 0; t < 4000; ++t) {
        bmoe::RunConfig c;
        randomize:;
        c.model_path = (rng() % 8 == 0) ? "" : "/tmp/m.gguf";
        c.n_predict  = (int)(rng() % 6) - 1;
        c.n_threads  = (int)(rng() % 6) - 1;
        c.n_ctx      = (int)(rng() % 6) - 1;
        c.n_ubatch   = (int)(rng() % 8) - 2;
        c.n_expert_used = (int)(rng() % 4) - 1;
        c.workload_capture_path = (rng() % 5 == 0) ? "cap.bin" : "";
        c.workload_replay_path  = (rng() % 5 == 0) ? "rep.bin" : "";
        c.moe.enabled = (rng() % 2);
        c.vram_cache_mb   = (int)(rng() % 4) - 1;
        c.expert_hot_slots = (int)(rng() % 4) - 1;
        c.n_gpu_layers    = (int)(rng() % 3) - 1;
        c.sampling.temp = (rng() % 2) ? 0.0f : 0.8f;
        c.sampling.top_k = (int)(rng() % 4) - 1;
        c.sampling.top_p = (rng() % 3 == 0) ? 0.0f : 0.9f;
        c.moe.overlap = (rng() % 3 == 0);
        c.moe.predict_log = (rng() % 3 == 0);
        // prefetch on with the cache off is a nested-block rule (separate slice), so it is only
        // exercised where that block cannot fire: with streaming disabled
        c.moe.predict_prefetch = c.moe.enabled ? false : (rng() % 3 == 0);
        // The nested MoE block (io_threads, cache band, prefetch_layers, row_stream, route_ahead range)
        // is a separate slice: its fields are held valid so that only the rules this slice covers can
        // fire. Anything the native rejects outside them shows up as UNCOVERED, not as a false pass.
        c.moe.io_threads = 2;
        c.moe.cache_mb = 0;
        c.moe.prefetch_layers = 0;
        c.moe.row_stream = false;
        c.moe.route_ahead = 0;
        // speculation: off, or one of the two source families with a random width
        const int spec_kind = (int)(rng() % 3);   // 0 off, 1 mtp, 2 ngram
        c.spec.source = spec_kind == 1 ? bmoe::DraftSource::mtp : (spec_kind == 2 ? bmoe::DraftSource::ngram : bmoe::DraftSource::none);
        c.spec.draft_max   = (int)(rng() % 12);
        c.spec.draft_p_min = (rng() % 2) ? 0.0f : (float)((rng() % 200) / 100.0 - 0.5);
        c.spec.ngram_max_match = (int)(rng() % 70);
        c.spec.ngram_min_match = (int)(rng() % 70);

        const auto res = bmoe::validate(c);
        const int want = native_rule(res);
        if (want == -99) { if (++uncovered <= 3) printf("UNCOVERED: %s\n", res.error.c_str()); continue; }
        // the spec-enabled paths are left out of this slice: spec is off in every generated config
        const int64_t got = cfg_rule_call(nullptr,
            c.model_path.empty() ? 0 : 1, c.n_predict, c.n_threads, c.n_ctx, c.n_ubatch,
            c.n_expert_used, c.workload_capture_path.empty() ? 0 : 1,
            c.workload_replay_path.empty() ? 0 : 1, c.moe.enabled ? 1 : 0,
            c.spec.enabled() ? 1 : 0, c.spec.is_mtp() ? 1 : 0, c.spec.is_ngram() ? 1 : 0,
            c.spec.draft_max, (double) c.spec.draft_p_min, c.spec.ngram_max_match,
            c.spec.ngram_min_match, c.vram_cache_mb, c.expert_hot_slots, c.n_gpu_layers,
            (double) c.sampling.temp, c.sampling.top_k, (double) c.sampling.top_p,
            c.moe.overlap ? 1 : 0, c.moe.predict_log ? 1 : 0, c.moe.predict_prefetch ? 1 : 0,
            c.moe.route_ahead);
        ++cases;
        if (got != want) {
            ++fails;
            if (fails <= 5) {
                printf("MISMATCH native=%d port=%lld | ubatch=%d ctx=%d dmax=%d spec=%d spec_en=%d msg=%s\n",
                       want, (long long) got, c.n_ubatch, c.n_ctx, c.spec.draft_max,
                       (int) c.spec.source, c.spec.enabled() ? 1 : 0, res.error.c_str());
            }
        }
    }
    printf("config validate: cases=%d fails=%d uncovered=%d\n", cases, fails, uncovered);
    return fails ? 1 : 0;
}
