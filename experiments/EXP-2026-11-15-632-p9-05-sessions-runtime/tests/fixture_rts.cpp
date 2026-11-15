// Differential gate for P9-05 S2: the runtime facade.
//
// Oracle: a transcription of runtime/core/src/engine/runtime.cpp and of the one
// context-width line run() hands to Session::open(). Port:
// source/rts-runtime-facade.goose.
//
// The mapping is gated as a projection: model_path, SamplingConfig,
// MoeStreamConfig and SpecConfig are copied whole by the carrier and are not
// represented. Every integer/boolean field the mapping touches is compared
// field by field, then the n_batch derivation, then the call order and the
// request run() builds from the same config.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>

#include "rts.h"

static int g_cases = 0, g_checks = 0, g_fails = 0;

static void expect(const char * group, const char * what, int64_t got, int64_t want) {
    ++g_checks;
    if (got != want) {
        ++g_fails;
        if (g_fails <= 60) {
            std::printf("FAIL %s / %s: got %lld want %lld\n", group, what, (long long) got, (long long) want);
        }
    }
}

// ── oracle: session_config_from, transcribed (runtime.cpp:9-26) ─────────────

struct nat_run {
    const char * name;
    int64_t n_threads, n_ctx, n_ubatch, n_gpu_layers, vram_cache_mb, expert_hot_slots;
    int64_t chatml, n_expert_used, compute_trace_layers;
    int64_t n_predict, think, progress;
};

struct nat_session {
    int64_t n_threads, n_ctx, n_batch, n_ubatch, n_gpu_layers, vram_cache_mb, expert_hot_slots;
    int64_t chatml, n_expert_used, compute_trace_layers;
};

static nat_session nat_session_config_from(const nat_run & cfg) {
    nat_session sc{};
    sc.n_threads = cfg.n_threads;
    sc.n_ctx = cfg.n_ctx;
    sc.n_batch = cfg.n_ctx; // runtime.cpp:14
    sc.n_ubatch = cfg.n_ubatch;
    sc.n_gpu_layers = cfg.n_gpu_layers;
    sc.vram_cache_mb = cfg.vram_cache_mb;
    sc.expert_hot_slots = cfg.expert_hot_slots;
    sc.chatml = cfg.chatml;
    sc.n_expert_used = cfg.n_expert_used;
    sc.compute_trace_layers = cfg.compute_trace_layers;
    return sc;
}

// session.cpp:817
//   cparams.n_ubatch = cfg.n_ubatch > 0 ? (uint32_t) cfg.n_ubatch : (uint32_t) cfg.n_batch;
static int64_t nat_ubatch(int64_t n_ubatch, int64_t n_batch) { return n_ubatch > 0 ? n_ubatch : n_batch; }

// runtime.cpp:32-67 — the step sequence and its early returns.
static int64_t nat_plan(int64_t validate_ok, int64_t open_ok, int64_t * out, int64_t cap) {
    int64_t n = 0;
    if (cap > 0) out[n] = 1; // validate
    ++n;
    if (!validate_ok || n >= cap) return n;
    out[n++] = 2; // map
    if (n >= cap) return n;
    out[n++] = 3; // open
    if (!open_ok || n >= cap) return n;
    out[n++] = 4; // request
    if (n >= cap) return n;
    out[n++] = 5; // generate
    return n;
}

int main() {
    goose_init(0, nullptr);

    // ── the mapping, field by field ─────────────────────────────────────────
    static const nat_run runs[] = {
        {"defaults",              4, 2048,    0,  0,    0,   0, 0,  0, 0,  128, 1, 0},
        {"streamed launcher",     8, 4096,  512, -1, 3072,   0, 1,  8, 0, 1024, 0, 1},
        {"pinned hot slots",     16, 8192, 1024, 99,    0,  64, 1,  6, 1,    1, 0, 0},
        {"wider ubatch than ctx", 2,  512,  256, -1,  128,   2, 0,  1, 0,   64, 1, 1},
        {"ctx 1",                 1,    1,    0, -1,    1,   0, 1,  1, 1,    1, 1, 0},
    };
    for (const auto & r : runs) {
        ++g_cases;
        const int64_t cfg_i[12] = {r.n_threads, r.n_ctx,          r.n_ubatch,   r.n_gpu_layers,
                                   r.vram_cache_mb, r.expert_hot_slots, r.chatml, r.n_expert_used,
                                   r.compute_trace_layers, r.n_predict, r.think, r.progress};
        int64_t sc[10] = {0};
        const int64_t wrote = rts_config(sl_i64{(int64_t *) cfg_i, 12}, sl_i64{sc, 10});
        const nat_session want = nat_session_config_from(r);

        expect(r.name, "fields written", wrote, 10);
        expect(r.name, "n_threads", sc[0], want.n_threads);
        expect(r.name, "n_ctx", sc[1], want.n_ctx);
        expect(r.name, "n_ubatch", sc[2], want.n_ubatch);
        expect(r.name, "n_gpu_layers", sc[3], want.n_gpu_layers);
        expect(r.name, "vram_cache_mb", sc[4], want.vram_cache_mb);
        expect(r.name, "expert_hot_slots", sc[5], want.expert_hot_slots);
        expect(r.name, "chatml", sc[6], want.chatml);
        expect(r.name, "n_expert_used", sc[7], want.n_expert_used);
        expect(r.name, "compute_trace_layers", sc[8], want.compute_trace_layers);
        expect(r.name, "n_batch", sc[9], want.n_batch);
        expect(r.name, "n_batch = n_ctx", sc[9], r.n_ctx);

        // The same run's context width.
        expect(r.name, "ubatch", rts_ubatch(sc[2], sc[9]), nat_ubatch(want.n_ubatch, want.n_batch));

        // And the request run() builds from it (runtime.cpp:55-65).
        int64_t req[4] = {-1, -1, -1, -1};
        expect(r.name, "request fields", rts_request(r.n_predict, r.think, r.progress, sl_i64{req, 4}), 4);
        expect(r.name, "clear_kv", req[0], 1);
        expect(r.name, "n_predict", req[1], r.n_predict);
        expect(r.name, "think", req[2], r.think);
        expect(r.name, "render_text = progress", req[3], r.progress);
    }

    // A slice too short to hold the mapping must be refused, not truncated.
    {
        ++g_cases;
        int64_t small[5] = {0};
        const int64_t cfg_i[12] = {4, 2048, 0, 0, 0, 0, 0, 0, 0, 128, 1, 0};
        expect("short slice", "refused", rts_config(sl_i64{(int64_t *) cfg_i, 12}, sl_i64{small, 5}), -1);
        int64_t req[2] = {0, 0};
        expect("short slice", "request refused", rts_request(1, 0, 0, sl_i64{req, 2}), -1);
    }

    // ── the call order ──────────────────────────────────────────────────────
    {
        struct plan_case {
            const char * name;
            int64_t validate_ok, open_ok, cap;
            int64_t want_n;
            int64_t want[5];
        };
        static const plan_case cases[] = {
            {"validate fails", 0, 1, 5, 1, {1, 0, 0, 0, 0}},
            {"open fails",     1, 0, 5, 3, {1, 2, 3, 0, 0}},
            {"success",        1, 1, 5, 5, {1, 2, 3, 4, 5}},
            {"cap 2",          1, 1, 2, 2, {1, 2, 0, 0, 0}},
            {"cap 1",          1, 1, 1, 1, {1, 0, 0, 0, 0}},
        };
        for (const auto & c : cases) {
            ++g_cases;
            int64_t out[5] = {-1, -1, -1, -1, -1};
            int64_t nat[5] = {-1, -1, -1, -1, -1};
            const int64_t n = rts_plan(c.validate_ok, c.open_ok, sl_i64{out, c.cap});
            const int64_t nn = nat_plan(c.validate_ok, c.open_ok, nat, c.cap > 5 ? 5 : c.cap);
            expect(c.name, "steps", n, c.want_n);
            expect(c.name, "steps vs oracle", n, nn);
            for (int64_t i = 0; i < n; ++i) {
                expect(c.name, "step", out[i], c.want[i]);
                expect(c.name, "step vs oracle", out[i], nat[i]);
            }
        }
    }

    // ── the RunResult invariant run() must not disturb ───────────────────────
    expect("result", "ok", rts_result(1, 0), 1);
    expect("result", "cancelled is still ok", rts_result(1, 1), 2);
    expect("result", "failure", rts_result(0, 0), 0);
    expect("result", "failure beats cancelled", rts_result(0, 1), 0);

    // ── pinned, hand-computed ───────────────────────────────────────────────
    expect("pinned", "n_batch(4096)", rts_n_batch(4096), 4096);
    expect("pinned", "n_batch(0)", rts_n_batch(0), 0);
    expect("pinned", "ubatch 0 follows n_batch", rts_ubatch(0, 512), 512);
    expect("pinned", "ubatch explicit", rts_ubatch(256, 512), 256);
    expect("pinned", "ubatch negative is not explicit", rts_ubatch(-1, 512), 512);

    std::printf("FG636_RTS cases=%d checks=%d failures=%d\n", g_cases, g_checks, g_fails);
    return g_fails == 0 ? 0 : 1;
}
