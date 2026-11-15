// Differential gate for P9-05 S1: the standalone session's lifecycle.
//
// Oracle: a transcription of runtime/core/src/engine/session.cpp, one function
// per decision, each quoting the native lines it reproduces. Port:
// source/ss-session-lifecycle.goose.
//
// Two tables per decision: the case table walks the decision space and compares
// oracle vs port, and a pinned table states the hand-computed answer so a
// transcription mistake cannot hide behind a matching port. No model, no CUDA
// device, no gguf file: every input is an integer.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "ss.h"

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

// ── oracle: the native decisions, transcribed ───────────────────────────────

static const int64_t NAT_MIB = 1048576;
static const int64_t NAT_RESERVE = 4096;

// session.cpp:73
//   const int sentinels = std::max(8, top_k);
static int64_t nat_sentinels(int64_t top_k) { return top_k > 8 ? top_k : 8; }

// session.cpp:49-81
//   if (budget_mb <= 0) return 0;
//   if (!meta.ok || meta.info.n_expert <= 0) { error = ...; return -1; }
//   ... bytes_per_slot ... if (bytes_per_slot == 0) { error = ...; return -1; }
//   const uint64_t budget = (uint64_t) budget_mb * 1024ull * 1024ull;
//   const uint64_t total_slots = budget / bytes_per_slot;
//   if (total_slots <= (uint64_t) sentinels) { error = ...; return -1; }
//   return std::min(meta.info.n_expert, (int) total_slots - sentinels);
static int64_t nat_hot_slots(int64_t budget_mb, int64_t meta_ok, int64_t n_expert, int64_t bytes_per_slot,
                             int64_t top_k) {
    if (budget_mb <= 0) return 0;
    if (!meta_ok || n_expert <= 0) return -1;
    if (bytes_per_slot == 0) return -1;
    const int64_t sentinels = nat_sentinels(top_k);
    const uint64_t budget = (uint64_t) budget_mb * 1024ull * 1024ull;
    const uint64_t total_slots = budget / (uint64_t) bytes_per_slot;
    if (total_slots <= (uint64_t) sentinels) return -1;
    const int64_t slots = (int64_t) total_slots - sentinels;
    return slots < n_expert ? slots : n_expert;
}

// session.cpp:74-79 — the MiB figure in the too-small message.
static int64_t nat_sentinel_mib(int64_t bytes_per_slot, int64_t top_k) {
    if (bytes_per_slot <= 0) return 0;
    const uint64_t bytes = (uint64_t) bytes_per_slot * (uint64_t) nat_sentinels(top_k);
    return (int64_t) ((bytes + (uint64_t) NAT_MIB - 1) / (uint64_t) NAT_MIB);
}

// session.cpp:669-685
//   if (effective_vram_cache_mb > 0 && cfg.n_gpu_layers != 0) {
//       if (free_mib <= reserve_mib) return fail("VRAM preflight failed: ...");
//       const int safe_mib = (int)(free_mib - reserve_mib);
//       if (effective_vram_cache_mb > safe_mib) effective_vram_cache_mb = safe_mib;
static int64_t nat_vram_clamp(int64_t free_mib, int64_t budget_mb, int64_t n_gpu_layers) {
    if (!(budget_mb > 0 && n_gpu_layers != 0)) return budget_mb;
    if ((uint64_t) free_mib <= (uint64_t) NAT_RESERVE) return -1;
    const int64_t safe_mib = free_mib - NAT_RESERVE;
    return budget_mb > safe_mib ? safe_mib : budget_mb;
}

// session.cpp:868-869
//   const uint64_t margin_mib = 256 + (cfg.spec.is_mtp() ? 512 : 0);
//   const int probe_budget = free_mib > margin_mib ? (int)(free_mib - margin_mib) : 0;
static int64_t nat_probe_margin(int64_t is_mtp) { return 256 + (is_mtp ? 512 : 0); }
static int64_t nat_probe_budget(int64_t free_mib, int64_t is_mtp) {
    const int64_t margin = nat_probe_margin(is_mtp);
    return (uint64_t) free_mib > (uint64_t) margin ? free_mib - margin : 0;
}

// session.cpp:856-902 — the action and its two outputs.
struct nat_plan {
    int64_t action; // 0 keep, 1 resize, 2 disable
    int64_t budget;
    int64_t slots;
};
static nat_plan nat_resize(int64_t plan_slots, int64_t plan_mib, int64_t n_gpu_layers, int64_t free_mib,
                           int64_t is_mtp, int64_t meta_ok, int64_t n_expert, int64_t bytes_per_slot,
                           int64_t top_k) {
    nat_plan p{0, plan_mib, plan_slots};
    if (!(plan_slots > 0 && n_gpu_layers != 0 && plan_mib > 0)) return p;
    const int64_t pb = nat_probe_budget(free_mib, is_mtp);
    if (pb < plan_mib) {
        const int64_t slots = pb > 0 ? nat_hot_slots(pb, meta_ok, n_expert, bytes_per_slot, top_k) : -1;
        if (slots > 0) {
            p.action = 1;
            p.budget = pb;
            p.slots = slots;
        } else {
            p.action = 2;
            p.slots = 0;
        }
    }
    return p;
}

// session.cpp:415-431 — the ~Impl sequence, as a rank per stage id.
//   0 source, 1 smpl, 2 mtp, 3 mtp_batch, 4 ctx_dft, 5 ctx, 6 hook, 7 model, 8 backend
static int64_t nat_rank(int64_t stage) {
    switch (stage) {
        case 4: return 0; // source.shutdown()
        case 8: return 1; // llama_sampler_free
        case 6: return 2; // mtp.reset()
        case 7: return 3; // llama_batch_free
        case 5: return 4; // ctx_dft.reset()
        case 1: return 5; // ctx.reset()
        case 2: return 6; // hook.reset()
        case 0: return 7; // model.reset()
        case 9: return 8; // llama_backend_free
        default: return -1;
    }
}
static int64_t nat_check(const int64_t * order, int64_t n) {
    if (n <= 0) return 0;
    int64_t prev = -1;
    for (int64_t i = 0; i < n; ++i) {
        const int64_t r = nat_rank(order[i]);
        if (r < 0 || r <= prev) return 0;
        prev = r;
    }
    return 1;
}

// ── case tables ─────────────────────────────────────────────────────────────

struct hot_case {
    const char * name;
    int64_t budget_mb, meta_ok, n_expert, bps, top_k;
};

static void run_hot() {
    static const hot_case cases[] = {
        // name                                budget  meta n_exp  bps        topk
        {"budget off",                            0,   1,   64,  1048576,   8},
        {"budget negative",                      -5,   1,   64,  1048576,   8},
        {"no metadata",                           64,   0,   64,  1048576,   8},
        {"no experts in model",                   64,   1,    0,  1048576,   8},
        {"no expert tensors",                     64,   1,   64,        0,   8},
        {"exactly the sentinels",                  8,   1,   64,  1048576,   8},
        {"one above the sentinels",                9,   1,   64,  1048576,   8},
        {"clamped by the expert count",          100,   1,    8,  1048576,   8},
        {"top-k 2 still asks 8 sentinels",         8,   1,   64,  1048576,   2},
        {"top-k 12 raises the floor",             12,   1,   64,  1048576,  12},
        {"top-k 12 one above",                    13,   1,   64,  1048576,  12},
        {"4 MiB slots",                           64,   1,   64,  4194304,   8},
        {"floor division: 3 MiB slots",           10,   1,   64,  3145728,   8},
        {"huge budget",                    1000000,   1,  256,  1048576,   8},
    };
    for (const auto & c : cases) {
        ++g_cases;
        expect(c.name, "hot_slots", ss_hot_slots(c.budget_mb, c.meta_ok, c.n_expert, c.bps, c.top_k),
               nat_hot_slots(c.budget_mb, c.meta_ok, c.n_expert, c.bps, c.top_k));
        expect(c.name, "hot_reason", ss_hot_reason(c.budget_mb, c.meta_ok, c.n_expert, c.bps, c.top_k),
               nat_hot_slots(c.budget_mb, c.meta_ok, c.n_expert, c.bps, c.top_k) >= 0
                   ? 0
                   : (!c.meta_ok || c.n_expert <= 0 ? 1 : (c.bps == 0 ? 2 : 3)));
    }

    // The reason codes, stated once by hand as well.
    expect("reason", "off", ss_hot_reason(0, 0, 0, 0, 8), 0);
    expect("reason", "no-meta", ss_hot_reason(8, 0, 64, 1048576, 8), 1);
    expect("reason", "no-tensors", ss_hot_reason(8, 1, 64, 0, 8), 2);
    expect("reason", "too-small", ss_hot_reason(8, 1, 64, 1048576, 8), 3);

    // Pinned: hand-computed, independent of the transcription.
    expect("pinned", "sentinels(2)", ss_sentinels(2), 8);
    expect("pinned", "sentinels(8)", ss_sentinels(8), 8);
    expect("pinned", "sentinels(12)", ss_sentinels(12), 12);
    expect("pinned", "60 MiB / 4 MiB = 15 - 8", ss_hot_slots(60, 1, 64, 4194304, 8), 7);
    expect("pinned", "sentinel_mib 1.5 MB x 8", ss_sentinel_mib(1500000, 8), 12);
    expect("pinned", "sentinel_mib 0", ss_sentinel_mib(0, 8), 0);
    expect("pinned", "sentinel_mib 1 MiB", ss_sentinel_mib(1048576, 8), 8);
    expect("pinned", "sentinel_mib top-k 2", ss_sentinel_mib(1048576, 2), 8);
    expect("pinned", "sentinel_mib top-k 16", ss_sentinel_mib(1048576, 16), 16);
}

static void run_clamp() {
    ++g_cases;
    struct clamp_case {
        const char * name;
        int64_t free_mib, budget_mb, layers;
    };
    static const clamp_case cases[] = {
        {"no gpu layers",          0,  500,  0},
        {"budget off",          8192,    0, -1},
        {"exactly the reserve", 4096,  100, -1},
        {"one MiB above",       4097,  100, -1},
        {"budget fits",         8192,  100, -1},
        {"budget clamped",      5000, 10000, -1},
        {"free below reserve",   100,  100, -1},
    };
    for (const auto & c : cases) {
        expect(c.name, "vram_clamp", ss_vram_clamp(c.free_mib, c.budget_mb, c.layers),
               nat_vram_clamp(c.free_mib, c.budget_mb, c.layers));
    }
    expect("clamp pinned", "budget fits", ss_vram_clamp(8192, 100, -1), 100);
    expect("clamp pinned", "clamped to free-4096", ss_vram_clamp(5000, 10000, -1), 904);
    expect("clamp pinned", "reserve is fatal", ss_vram_clamp(4096, 1, -1), -1);

    expect("margin", "no mtp", ss_probe_margin(0), 256);
    expect("margin", "mtp", ss_probe_margin(1), 768);
    expect("probe", "non-mtp 1000", ss_probe_budget(1000, 0), 744);
    expect("probe", "mtp 1000", ss_probe_budget(1000, 1), 232);
    expect("probe", "at margin", ss_probe_budget(256, 0), 0);
    expect("probe", "below margin", ss_probe_budget(100, 0), 0);
    expect("probe", "mtp at margin", ss_probe_budget(768, 1), 0);
    expect("probe", "mtp one above", ss_probe_budget(769, 1), 1);
}

static void run_resize() {
    ++g_cases;
    struct rz {
        const char * name;
        int64_t plan_slots, plan_mib, layers, free_mib, is_mtp, n_expert, bps, top_k;
    };
    // meta_ok is 1 in every case: a readable header is the normal path.
    static const rz cases[] = {
        {"keep: probe above plan",        4, 1024, -1,  4096, 0,   64, 1048576, 8},
        {"keep: equal to plan",           4, 1024, -1,  1280, 0,   64, 1048576, 8},
        {"resize: probe below plan",      4, 4096, -1,  2048, 0,   64, 1048576, 8},
        {"resize: wide budget",           4, 1024, -1,  1000, 0, 1000, 1048576, 8},
        {"disable: below sentinels",      4, 4096, -1,   260, 0,   64, 1048576, 8},
        {"disable: mtp margin eats it",   4, 4096, -1,   300, 1,   64, 1048576, 8},
        {"keep: no hot store",            0, 4096, -1,  2048, 0,   64, 1048576, 8},
        {"keep: cpu session",             4, 4096,  0,  2048, 0,   64, 1048576, 8},
        {"keep: no budget",               4,    0, -1,  2048, 0,   64, 1048576, 8},
    };
    for (const auto & c : cases) {
        int64_t got[3] = {-7, -7, -7};
        ss_resize_plan(c.plan_slots, c.plan_mib, c.layers, c.free_mib, c.is_mtp, 1, c.n_expert, c.bps, c.top_k,
                       sl_i64{got, 3});
        const nat_plan want = nat_resize(c.plan_slots, c.plan_mib, c.layers, c.free_mib, c.is_mtp, 1, c.n_expert,
                                         c.bps, c.top_k);
        expect(c.name, "action", got[0], want.action);
        expect(c.name, "budget", got[1], want.budget);
        expect(c.name, "slots", got[2], want.slots);
    }

    // Pinned: the three actions by hand.
    int64_t out[3] = {0, 0, 0};
    sl_i64 sl{out, 3};
    ss_resize_plan(4, 1024, -1, 4096, 0, 1, 64, 1048576, 8, sl);
    expect("pinned resize", "keep", out[0], 0);
    expect("pinned resize", "keep budget", out[1], 1024);
    expect("pinned resize", "keep slots", out[2], 4);
    int64_t out2[3] = {0, 0, 0};
    ss_resize_plan(4, 4096, -1, 2048, 0, 1, 64, 1048576, 8, sl_i64{out2, 3});
    expect("pinned resize", "resize", out2[0], 1);
    expect("pinned resize", "resize budget", out2[1], 1792);
    expect("pinned resize", "resize slots", out2[2], 64);
    int64_t out3[3] = {0, 0, 0};
    ss_resize_plan(4, 4096, -1, 260, 0, 1, 64, 1048576, 8, sl_i64{out3, 3});
    expect("pinned resize", "disable", out3[0], 2);
    expect("pinned resize", "disable budget untouched", out3[1], 4096);
    expect("pinned resize", "disable slots", out3[2], 0);
}

static void run_teardown() {
    ++g_cases;
    const int64_t stages[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    for (int64_t s : stages) {
        expect("teardown", "rank", ss_teardown_rank(s), nat_rank(s));
    }
    expect("teardown", "workload has no stage", ss_teardown_rank(3), -1);
    expect("teardown", "unknown id", ss_teardown_rank(42), -1);

    // The order itself: source before everything it feeds.
    expect("teardown", "source before ctx", ss_teardown_before(4, 1), 1);
    expect("teardown", "ctx before source", ss_teardown_before(1, 4), 0);
    expect("teardown", "hook before ctx", ss_teardown_before(2, 1), 0);
    expect("teardown", "model before backend", ss_teardown_before(0, 9), 1);
    expect("teardown", "unknown stage", ss_teardown_before(3, 0), 0);

    // The native sequence, then three ways to break it.
    {
        const int64_t native[] = {4, 8, 6, 7, 5, 1, 2, 0, 9};
        expect("teardown", "native order", ss_teardown_check(sl_i64{(int64_t *) native, 9}), nat_check(native, 9));
        expect("teardown", "native order pinned", ss_teardown_check(sl_i64{(int64_t *) native, 9}), 1);
        const int64_t swapped[] = {1, 4, 6, 7, 5, 8, 2, 0, 9};
        expect("teardown", "ctx before source", ss_teardown_check(sl_i64{(int64_t *) swapped, 9}), 0);
        const int64_t dup[] = {4, 4, 6};
        expect("teardown", "duplicate", ss_teardown_check(sl_i64{(int64_t *) dup, 3}), 0);
        const int64_t empty[] = {0};
        expect("teardown", "empty", ss_teardown_check(sl_i64{(int64_t *) empty, 0}), nat_check(empty, 0));
        const int64_t one[] = {4};
        expect("teardown", "single", ss_teardown_check(sl_i64{(int64_t *) one, 1}), 1);
        const int64_t bad[] = {4, 3};
        expect("teardown", "unknown in order", ss_teardown_check(sl_i64{(int64_t *) bad, 2}), 0);
    }
}

int main() {
    goose_init(0, nullptr);
    run_hot();
    run_clamp();
    run_resize();
    run_teardown();
    std::printf("FG636_SS cases=%d checks=%d failures=%d\n", g_cases, g_checks, g_fails);
    return g_fails == 0 ? 0 : 1;
}
