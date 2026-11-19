// The canonical validator (goose/cli/config-validate.goose) against the real bmoe::validate.
// The module takes the integer and float halves of the RunConfig as slices, so the harness fills
// them and compares the first-failing-rule index with the native message mapped back to its rule.
#include <dlfcn.h>
#include <cstdio>
#include <cstdint>
#include <random>
#include <string>
#include "core/include/bmoe/config.h"

struct sl_i64 { int64_t * data; int64_t len; };
struct sl_f64 { double  * data; int64_t len; };
typedef int64_t (*fn_validate)(sl_i64, sl_f64);

static int native_rule(const bmoe::ValidationResult & r) {
    if (r.ok) return 0;
    const std::string & m = r.error;
    auto pre = [&](const char * s) { return m.rfind(s, 0) == 0; };
    auto has = [&](const char * s) { return m.find(s) != std::string::npos; };
    if (pre("model_path is required")) return 1;
    if (pre("n_predict must be positive")) return 2;
    if (pre("n_threads must be positive")) return 3;
    if (pre("n_ctx must be positive")) return 4;
    if (pre("n_ubatch must be >= 0")) return 5;
    if (has("is narrower than the verify batch")) return 22;
    if (pre("n_ubatch=")) return 6;
    if (pre("n_expert_used must be >= 0")) return 7;
    if (pre("workload capture and replay are mutually exclusive")) return 8;
    if (pre("deterministic workload capture/replay requires moe.enabled")) return 9;
    if (pre("deterministic workload capture/replay does not yet support speculative")) return 10;
    if (pre("vram_cache_mb must be >= 0")) return 11;
    if (pre("expert_hot_slots must be >= 0")) return 12;
    if (pre("VRAM expert cache requires n_gpu_layers")) return 13;
    if (pre("sampling.top_k must be >= 0")) return 14;
    if (pre("sampling.top_p must be in (0, 1]")) return 15;
    if (pre("speculative decoding requires greedy decoding")) return 16;
    if (pre("spec.draft_max must be in")) return 17;
    if (pre("spec.draft_p_min must be in [0, 1]")) return 18;
    if (pre("spec.draft_p_min is an MTP knob")) return 19;
    if (pre("spec.ngram_max_match must be in")) return 20;
    if (pre("spec.ngram_min_match must be in")) return 21;
    if (pre("moe.overlap requires moe.enabled")) return 23;
    if (pre("moe.predict_log requires moe.enabled")) return 24;
    if (pre("moe.predict_prefetch requires moe.enabled")) return 25;
    if (pre("moe.route_ahead requires moe.enabled")) return 26;
    if (pre("moe.row_stream requires moe.enabled")) return 27;
    if (pre("moe.row_stream_mb must be >= 0")) return 28;
    if (pre("moe.io_threads must be in")) return 29;
    if (pre("moe.cache_mb must be >= 0")) return 30;
    if (has("is in the pathological band")) return 31;
    if (pre("moe.prefetch_layers must be in")) return 32;
    if (pre("moe.cache_auto and an explicit")) return 33;
    if (pre("moe.cache_floor_mb must be >= 0")) return 34;
    if (pre("moe.cache_ceil_mb must be >= 0")) return 35;
    if (pre("moe.prefetch_layers requires the LRU cache")) return 36;
    if (pre("moe.predict_prefetch requires the LRU cache")) return 37;
    if (pre("moe.io_two_wave requires moe.overlap")) return 38;
    if (pre("moe.io_two_wave requires the LRU cache")) return 39;
    if (pre("moe.predict_spec_max must be in")) return 40;
    if (pre("moe.predict_prefetch and moe.prefetch_layers are mutually exclusive")) return 41;
    if (pre("moe.route_ahead must be in")) return 42;
    if (pre("moe.route_ahead excludes")) return 43;
    if (pre("moe.route_ahead and self-speculative")) return 44;
    return -99;
}

int main() {
    void * h = dlopen("/home/end0/AI/FrankenMoE-CUDA/builds/flint-acc/bin/libllama.so.0", RTLD_LAZY);
    if (!h) { printf("dlopen: %s\n", dlerror()); return 2; }
    auto port = (fn_validate) dlsym(h, "cfg_validate");
    if (!port) { printf("missing door cfg_validate\n"); return 3; }

    std::mt19937 rng(31337);
    int fails = 0, cases = 0, uncovered = 0;
    for (int t = 0; t < 4000; ++t) {
        bmoe::RunConfig c;
        c.model_path = (rng() % 8 == 0) ? "" : "/tmp/m.gguf";
        c.n_predict = (int)(rng() % 6) - 1;
        c.n_threads = (int)(rng() % 6) - 1;
        c.n_ctx = (int)(rng() % 6) - 1;
        c.n_ubatch = (int)(rng() % 8) - 2;
        c.n_expert_used = (int)(rng() % 4) - 1;
        c.workload_capture_path = (rng() % 5 == 0) ? "cap.bin" : "";
        c.workload_replay_path = (rng() % 5 == 0) ? "rep.bin" : "";
        c.moe.enabled = (rng() % 2);
        c.vram_cache_mb = (int)(rng() % 4) - 1;
        c.expert_hot_slots = (int)(rng() % 4) - 1;
        c.n_gpu_layers = (int)(rng() % 3) - 1;
        c.sampling.temp = (rng() % 2) ? 0.0f : 0.8f;
        c.sampling.top_k = (int)(rng() % 4) - 1;
        c.sampling.top_p = (rng() % 3 == 0) ? 0.0f : 0.9f;
        const int spec_kind = (int)(rng() % 3);
        c.spec.source = spec_kind == 1 ? bmoe::DraftSource::mtp : (spec_kind == 2 ? bmoe::DraftSource::ngram : bmoe::DraftSource::none);
        c.spec.draft_max = (int)(rng() % 12);
        c.spec.draft_p_min = (rng() % 2) ? 0.0f : (float)((rng() % 200) / 100.0 - 0.5);
        c.spec.ngram_max_match = (int)(rng() % 70);
        c.spec.ngram_min_match = (int)(rng() % 70);
        c.moe.overlap = (rng() % 3 == 0);
        c.moe.predict_log = (rng() % 3 == 0);
        c.moe.predict_prefetch = (rng() % 3 == 0);
        c.moe.row_stream = (rng() % 4 == 0);
        c.moe.row_stream_mb = (int)(rng() % 4) - 1;
        c.moe.io_threads = (int)(rng() % 11);
        c.moe.cache_mb = (rng() % 4 == 0) ? 0 : (int)(rng() % 2000);
        c.moe.force_cache = (rng() % 2);
        c.moe.prefetch_layers = (int)(rng() % 11);
        c.moe.cache_auto = (rng() % 3 == 0);
        c.moe.cache_floor_mb = (int)(rng() % 4) - 1;
        c.moe.cache_ceil_mb = (int)(rng() % 4) - 1;
        c.moe.io_two_wave = (rng() % 3 == 0);
        c.moe.predict_spec_max = (int)(rng() % 11) - 1;
        c.moe.route_ahead = (int)(rng() % 11) - 1;

        const auto res = bmoe::validate(c);
        const int want = native_rule(res);
        if (want == -99) { if (++uncovered <= 3) printf("UNCOVERED: %s\n", res.error.c_str()); continue; }

        int64_t iv[32] = {0};
        double  fv[5] = {0};
        iv[0]  = c.n_predict;                     iv[1]  = c.n_threads;
        iv[2]  = c.n_ctx;                         iv[3]  = c.n_ubatch;
        iv[4]  = c.n_expert_used;                 iv[5]  = c.vram_cache_mb;
        iv[6]  = c.expert_hot_slots;              iv[7]  = c.n_gpu_layers;
        iv[8]  = c.model_path.empty() ? 0 : 1;    iv[9]  = c.workload_capture_path.empty() ? 0 : 1;
        iv[10] = c.workload_replay_path.empty() ? 0 : 1;
        iv[11] = c.sampling.top_k;
        iv[12] = (int64_t) c.spec.source;
        iv[13] = c.spec.draft_max;                iv[14] = c.spec.ngram_min_match;
        iv[15] = c.spec.ngram_max_match;          iv[16] = c.moe.enabled ? 1 : 0;
        iv[17] = c.moe.io_threads;                iv[18] = c.moe.cache_mb;
        iv[19] = c.moe.cache_auto ? 1 : 0;        iv[20] = c.moe.cache_floor_mb;
        iv[21] = c.moe.cache_ceil_mb;             iv[22] = c.moe.prefetch_layers;
        iv[23] = c.moe.io_two_wave ? 1 : 0;       iv[24] = c.moe.predict_log ? 1 : 0;
        iv[25] = c.moe.predict_prefetch ? 1 : 0;  iv[26] = c.moe.predict_spec_max;
        iv[27] = c.moe.route_ahead;               iv[28] = c.moe.row_stream ? 1 : 0;
        iv[29] = c.moe.row_stream_mb;             iv[30] = c.moe.overlap ? 1 : 0;
        iv[31] = c.moe.force_cache ? 1 : 0;
        fv[0] = c.sampling.temp; fv[1] = c.sampling.top_p; fv[2] = c.spec.draft_p_min;
        fv[3] = c.moe.drop_cold_frac; fv[4] = c.moe.substitute_lambda;

        sl_i64 si{ iv, 32 };
        sl_f64 sf{ fv, 5 };
        const int64_t got = port(si, sf);
        ++cases;
        if (got != want) {
            ++fails;
            if (fails <= 5) printf("MISMATCH native=%d port=%lld msg=%s\n", want, (long long) got, res.error.c_str());
        }
    }
    printf("canonical validate: cases=%d fails=%d uncovered=%d\n", cases, fails, uncovered);
    return fails ? 1 : 0;
}
