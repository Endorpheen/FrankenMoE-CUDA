// Smoke gate for staged S3.2: the P9-05 session decisions behind FG_GOOSE_SESSION.
//
// One binary per build. In both builds the oracle is the transcribed native body held in the
// same library (`fg_session::native::*`); with the flag on the plain name crosses the bridge
// into the composed Goose module, with the flag off it *is* the transcription.
//
// Per switched decision: a case table over the decision space and a pinned table of
// hand-computed answers, so a matching transcription mistake cannot hide behind a matching
// port. Then the composed open-time plan over 24 synthetic states, field by field. No model,
// no CUDA device, no gguf file: every input is an integer.
#include <cstdio>
#include <cstdint>
#include <cstring>

#include "fg-session-decide.h"

static int g_cases = 0, g_checks = 0, g_fails = 0;
static bool g_dump = false;

static void expect(const char * group, const char * what, int64_t got, int64_t want) {
    ++g_checks;
    if (got != want) {
        ++g_fails;
        if (g_fails <= 60) {
            std::printf("FAIL %s / %s: got %lld want %lld\n", group, what, (long long) got,
                        (long long) want);
        }
    }
}

// The decision comparison: the dispatch (or the transcription, on OFF) against the
// transcription, plus the pinned literal where one is stated.
static void expect_both(const char * group, const char * what, int64_t got, int64_t nat,
                        int64_t pinned) {
    expect(group, "vs oracle", got, nat);
    expect(group, what, got, pinned);
}

namespace fs = fg_session;

// ── ss_ sentinels ───────────────────────────────────────────────────────────

static void gate_sentinels() {
    static const int64_t k[][2] = {
        // top_k, hand-computed max(8, top_k)
        {-1, 8},  {0, 8},   {1, 8},   {7, 8},   {8, 8},   {9, 9},
        {12, 12}, {32, 32}, {255, 255}, {100000, 100000},
    };
    for (const auto & c : k) {
        ++g_cases;
        char what[48];
        std::snprintf(what, sizeof what, "top_k=%lld", (long long) c[0]);
        expect_both("ss_sentinels", what, fs::sentinels(c[0]), fs::native::sentinels(c[0]), c[1]);
    }
}

// ── ss_ hot store sizing ────────────────────────────────────────────────────

struct hot_case {
    const char * name;
    int64_t budget_mb, meta_ok, n_expert, bytes_per_slot, top_k;
    int64_t want;   // slot count: -1 refusal, 0 budget off
    int64_t reason; // 0 none, 1 no meta, 2 no tensors, 3 sentinels do not fit
};

// bytes_per_slot is non-negative by construction in the native code (a uint64_t sum over the
// tensor table), so the table stays on that domain: a negative value is not a native input and
// the module's `<= 0` guard would be comparing against something the carrier cannot produce.
static const hot_case k_hot[] = {
    {"budget off",            0,     1,   8, 131072,  2,   0, 0},
    {"negative budget",      -1,     1,   8, 131072,  2,   0, 0},
    {"no metadata",        4096,     0,   8, 131072,  2,  -1, 1},
    {"expert count 0",     4096,     1,   0, 131072,  2,  -1, 1},
    {"expert count -1",    4096,     1,  -1, 131072,  2,  -1, 1},
    {"no expert tensors",  4096,     1,   8,      0,  2,  -1, 2},
    {"one MiB, 8 sentinels",  1,     1,  64, 131072,  8,  -1, 3},
    {"one MiB, top_k 12",     1,     1,  64, 131072, 12,  -1, 3},
    {"two MiB, 8 spare",      2,     1,  64, 131072,  8,   8, 0},
    {"two MiB, top_k 12",     2,     1,  64, 131072, 12,   4, 0},
    {"two MiB, top_k 2",      2,     1,  64, 131072,  2,   8, 0},
    {"clamped to experts", 1000,     1,  64, 131072,  8,  64, 0},
    {"clamped to 256",     1000,     1, 256, 131072,  8, 256, 0},
    {"4 MiB slots, 4096",  4096,     1, 4096, 4194304, 8, 1016, 0},
    {"4 MiB slots, 256",   4096,     1, 256,  4194304, 8, 256, 0},
    {"floor division",     4096,     1,4096, 3000000,  8, 1423, 0},
    {"huge budget",     1000000,     1, 512, 1048576,  8, 512, 0},
    {"exact sentinel",        1,     1,   8, 131072,  8,  -1, 3},
    {"sentinel + 1 slot",     1,     1,  64, 116508,  8,   1, 0},
    {"slot size 1 byte",      1,     1,  64,      1,  8,  64, 0},
    {"negative budget -7",   -7,     1,   8, 131072,  2,   0, 0},
    {"meta ok, expert 1",  4096,     1,   1, 131072,  8,   1, 0},
    {"budget 2, top_k 1",     2,     1,  64, 131072,  1,   8, 0},
};

static void gate_hot_store() {
    for (const auto & c : k_hot) {
        ++g_cases;
        const int64_t got =
            fs::hot_slots(c.budget_mb, c.meta_ok, c.n_expert, c.bytes_per_slot, c.top_k);
        const int64_t nat =
            fs::native::hot_slots(c.budget_mb, c.meta_ok, c.n_expert, c.bytes_per_slot, c.top_k);
        expect_both("ss_hot_slots", c.name, got, nat, c.want);

        const int64_t got_reason =
            fs::hot_reason(c.budget_mb, c.meta_ok, c.n_expert, c.bytes_per_slot, c.top_k);
        const int64_t nat_reason = fs::native::hot_reason(c.budget_mb, c.meta_ok, c.n_expert,
                                                          c.bytes_per_slot, c.top_k);
        expect_both("ss_hot_reason", c.name, got_reason, nat_reason, c.reason);
    }

    // The MiB figure the too-small message quotes: ceil(bytes_per_slot * sentinels / MiB).
    static const int64_t mib[][3] = {
        // bytes_per_slot, top_k, hand-computed ceil
        {0, 8, 0},          {-1, 8, 0},        {131072, 8, 1},   {131072, 12, 2},
        {1048576, 8, 8},    {1048576, 12, 12}, {524288, 8, 4},   {1, 8, 1},
        {100000, 8, 1},     {200000, 8, 2},    {4194304, 2, 32}, {3000000, 8, 23},
    };
    for (const auto & c : mib) {
        ++g_cases;
        char what[48];
        std::snprintf(what, sizeof what, "bps=%lld top_k=%lld", (long long) c[0],
                      (long long) c[1]);
        expect_both("ss_sentinel_mib", what, fs::sentinel_mib(c[0], c[1]),
                    fs::native::sentinel_mib(c[0], c[1]), c[2]);
    }
}

// ── ss_ preflight clamp ─────────────────────────────────────────────────────

static void gate_vram_clamp() {
    static const int64_t k[][4] = {
        // free_mib, budget_mb, n_gpu_layers, hand-computed (budget, or -1 refusal)
        {0,         4096,  -1,     -1}, // free 0 is below the reserve
        {4095,      4096,  -1,     -1},
        {4096,      4096,  -1,     -1}, // reserve is inclusive
        {4097,      4096,  -1,      1}, // 4097 - 4096 = 1
        {8192,      3072,  -1,   3072}, // fits untouched
        {8192,      8192,  -1,   4096}, // clamped to safe
        {16384,    12288,  -1,  12288},
        {100,      30000,   0,  30000}, // no GPU layers: no check
        {100,      30000,  -1,     -1},
        {0,            0,  -1,      0}, // budget off: no check
        {0,         -128,  -1,   -128},
        {4096,         0,  -1,      0},
        {100000,   99999,  -1,  95904},
        {4097,     99999,  -1,      1},
        {5000,         1,  -1,      1},
        {5000,         0,   7,      0},
    };
    for (const auto & c : k) {
        ++g_cases;
        char what[64];
        std::snprintf(what, sizeof what, "free=%lld budget=%lld layers=%lld", (long long) c[0],
                      (long long) c[1], (long long) c[2]);
        expect_both("ss_vram_clamp", what, fs::vram_clamp(c[0], c[1], c[2]),
                    fs::native::vram_clamp(c[0], c[1], c[2]), c[3]);
    }
}

// ── ss_ post-load probe ─────────────────────────────────────────────────────

static void gate_probe() {
    static const int64_t margin[][2] = {
        {0, 256},   {1, 768},   {2, 768},   {-1, 768},  {7, 768},
        {512, 768}, {1000, 768}, {255, 768}, {256, 768}, {4096, 768},
    };
    for (const auto & c : margin) {
        ++g_cases;
        char what[32];
        std::snprintf(what, sizeof what, "is_mtp=%lld", (long long) c[0]);
        expect_both("ss_probe_margin", what, fs::probe_margin(c[0]),
                    fs::native::probe_margin(c[0]), c[1]);
    }

    static const int64_t budget[][3] = {
        // free_mib, is_mtp, hand-computed
        {0, 0, 0},       {255, 0, 0},     {256, 0, 0},     {257, 0, 1},
        {1024, 0, 768},  {1024, 1, 256},  {768, 1, 0},     {769, 1, 1},
        {4096, 0, 3840}, {4096, 1, 3328}, {100, 1, 0},     {9000, 1, 8232},
        {7431, 0, 7175}, {7431, 1, 6663},
    };
    for (const auto & c : budget) {
        ++g_cases;
        char what[48];
        std::snprintf(what, sizeof what, "free=%lld is_mtp=%lld", (long long) c[0],
                      (long long) c[1]);
        expect_both("ss_probe_budget", what, fs::probe_budget(c[0], c[1]),
                    fs::native::probe_budget(c[0], c[1]), c[2]);
    }
}

struct resize_case {
    const char * name;
    int64_t plan_slots, plan_mib, n_gpu_layers, free_mib, is_mtp, meta_ok, n_expert,
        bytes_per_slot, top_k;
    int64_t action, budget, slots;
};

// Hand-computed with the transcription's arithmetic over the same rows.
static const resize_case k_resize[] = {
    {"not probed, no slots",    0, 4096,  -1, 16384, 0, 1,  64, 67108864,  8, 0, 4096,   0},
    {"not probed, no layers",  32, 4096,   0, 16384, 0, 1,  64, 67108864,  8, 0, 4096,  32},
    {"not probed, no budget",  32,    0,  -1, 16384, 0, 1,  64, 67108864,  8, 0,    0,  32},
    {"keep: probe fits",       32, 4096,  -1, 16384, 0, 1,  64, 67108864,  8, 0, 4096,  32},
    {"keep: exact fit",        32, 4096,  -1,  4352, 0, 1,  64, 67108864,  8, 0, 4096,  32},
    {"resize: one MiB less",   32, 4096,  -1,  4351, 0, 1,  64, 67108864,  8, 1, 4095,  55},
    {"resize: half budget",    32, 4096,  -1,  2304, 0, 1,  64, 67108864,  8, 1, 2048,  24},
    {"resize: mtp margin",     32, 4096,  -1,  2304, 1, 1,  64, 67108864,  8, 1, 1536,  16},
    {"resize: floor",          32, 4096,  -1,  1300, 0, 1, 256, 67108864,  8, 1, 1044,   8},
    {"disable: budget 0",      32, 4096,  -1,   256, 0, 1,  64, 67108864,  8, 2, 4096,   0},
    {"disable: 255 free",      32, 4096,  -1,   255, 0, 1,  64, 67108864,  8, 2, 4096,   0},
    {"disable: huge slot",     32, 4096,  -1,  1300, 0, 1,  64, 200000000, 8, 2, 4096,   0},
    {"disable: no meta",       32, 4096,  -1,  1300, 0, 0,   8, 67108864,  8, 2, 4096,   0},
    {"disable: no tensors",    32, 4096,  -1,  1300, 0, 1,   8,        0,  8, 2, 4096,   0},
    {"resize: clamp experts",  64, 8192,  -1,  6400, 0, 1,   8, 67108864,  8, 1, 6144,   8},
    {"resize: 4 MiB slots",    16, 8192,  -1,  6400, 0, 1, 256,  4194304,  8, 1, 6144, 256},
    {"resize: top_k 12",       16, 8192,  -1,  6400, 0, 1, 256,  4194304, 12, 1, 6144, 256},
    {"resize: mtp, top_k 12",  16, 8192,  -1,  6400, 1, 1, 256,  4194304, 12, 1, 5632, 256},
    {"keep: 1 slot plan",       1, 4096,  -1,  9000, 0, 1,  64, 67108864,  8, 0, 4096,   1},
    {"resize: single slot",     1, 4096,  -1,  3000, 0, 1,  64, 67108864,  8, 1, 2744,  34},
};

static void gate_resize_plan() {
    for (const auto & c : k_resize) {
        ++g_cases;
        const fs::ResizePlan got =
            fs::resize_plan(c.plan_slots, c.plan_mib, c.n_gpu_layers, c.free_mib, c.is_mtp,
                            c.meta_ok, c.n_expert, c.bytes_per_slot, c.top_k);
        const fs::ResizePlan nat =
            fs::native::resize_plan(c.plan_slots, c.plan_mib, c.n_gpu_layers, c.free_mib, c.is_mtp,
                                    c.meta_ok, c.n_expert, c.bytes_per_slot, c.top_k);
        expect_both("ss_resize_plan", c.name, got.action, nat.action, c.action);
        expect_both("ss_resize_plan", c.name, got.budget, nat.budget, c.budget);
        expect_both("ss_resize_plan", c.name, got.slots, nat.slots, c.slots);
    }
}

// ── rts_ widths and run() order ─────────────────────────────────────────────

static void gate_rts() {
    static const int64_t nb[][2] = {
        {0, 0},   {1, 1},   {512, 512},   {2048, 2048},   {4096, 4096},
        {8192, 8192}, {16384, 16384}, {32768, 32768}, {65536, 65536}, {-1, -1},
    };
    for (const auto & c : nb) {
        ++g_cases;
        char what[32];
        std::snprintf(what, sizeof what, "n_ctx=%lld", (long long) c[0]);
        expect_both("rts_n_batch", what, fs::n_batch(c[0]), fs::native::n_batch(c[0]), c[1]);
    }

    static const int64_t ub[][3] = {
        // n_ubatch, n_batch, hand-computed
        {0, 512, 512},      {0, 2048, 2048},    {-1, 512, 512},     {-7, 4096, 4096},
        {256, 512, 256},    {512, 512, 512},    {1024, 512, 1024},  {1, 1, 1},
        {64, 4096, 64},     {4096, 4096, 4096}, {0, 0, 0},          {100000, 4096, 100000},
    };
    for (const auto & c : ub) {
        ++g_cases;
        char what[48];
        std::snprintf(what, sizeof what, "n_ubatch=%lld n_batch=%lld", (long long) c[0],
                      (long long) c[1]);
        expect_both("rts_ubatch", what, fs::ubatch(c[0], c[1]), fs::native::ubatch(c[0], c[1]),
                    c[2]);
    }
}

static void gate_run_plan() {
    struct pc {
        const char * name;
        int64_t validate_ok, open_ok, cap;
        int64_t want_n;
        int64_t want[5];
    };
    static const pc cases[] = {
        {"validate fails",          0, 1, 5, 1, {1, 0, 0, 0, 0}},
        {"open fails",              1, 0, 5, 3, {1, 2, 3, 0, 0}},
        {"success",                 1, 1, 5, 5, {1, 2, 3, 4, 5}},
        {"both fail",               0, 0, 5, 1, {1, 0, 0, 0, 0}},
        {"cap 4",                   1, 1, 4, 4, {1, 2, 3, 4, 0}},
        {"cap 3",                   1, 1, 3, 3, {1, 2, 3, 0, 0}},
        {"cap 2",                   1, 1, 2, 2, {1, 2, 0, 0, 0}},
        {"cap 1",                   1, 1, 1, 1, {1, 0, 0, 0, 0}},
        {"cap 0",                   1, 1, 0, 1, {0, 0, 0, 0, 0}},
        {"negative validate",      -1, 1, 5, 5, {1, 2, 3, 4, 5}},
        {"cap 2, validate fails",   0, 1, 2, 1, {1, 0, 0, 0, 0}},
        {"open fails, wide",        1, 0, 5, 3, {1, 2, 3, 0, 0}},
    };
    for (const auto & c : cases) {
        ++g_cases;
        int64_t out[5] = {-1, -1, -1, -1, -1};
        int64_t nat[5] = {-7, -7, -7, -7, -7};
        const int64_t n = fs::run_plan(c.validate_ok, c.open_ok, out, c.cap);
        const int64_t nn = fs::native::run_plan(c.validate_ok, c.open_ok, nat, c.cap);
        expect_both("rts_plan", c.name, n, nn, c.want_n);
        if (c.cap > 0) {
            for (int64_t i = 0; i < c.want_n; ++i) {
                expect("rts_plan", c.name, out[i], c.want[i]);
                expect("rts_plan", "step vs oracle", out[i], nat[i]);
            }
        }
    }
}

// ── nd_ draft and verify widths ─────────────────────────────────────────────

static void gate_nd() {
    static const int64_t k[][3] = {
        // draft_max, target ubatch, hand-computed max(draft_max+1, 32) clamped to target
        {0, 512, 32},      {1, 512, 32},     {31, 512, 32},    {32, 512, 33},
        {63, 512, 64},     {127, 512, 128},  {511, 512, 512},  {512, 512, 512},
        {1023, 512, 512},  {0, 0, 0},        {10, 11, 11},     {10, 32, 32},
        {31, 33, 32},      {40, 41, 41},
    };
    for (const auto & c : k) {
        ++g_cases;
        char what[48];
        std::snprintf(what, sizeof what, "draft_max=%lld tgt=%lld", (long long) c[0],
                      (long long) c[1]);
        expect_both("nd_draft_ubatch", what, fs::draft_ubatch(c[0], c[1]),
                    fs::native::draft_ubatch(c[0], c[1]), c[2]);
    }

    static const int64_t v[][3] = {
        // n_batch, draft_max, hand-computed max(n_batch, draft_max+1)
        {0, 0, 1},          {1, 0, 1},        {512, 0, 512},     {512, 1, 512},
        {512, 511, 512},    {512, 512, 513},  {1024, 512, 1024}, {2048, 4095, 4096},
        {1, 1, 2},          {3, 7, 8},        {4096, 63, 4096},  {511, 511, 512},
        {1023, 1024, 1025}, {0, 100, 101},
    };
    for (const auto & c : v) {
        ++g_cases;
        char what[48];
        std::snprintf(what, sizeof what, "n_batch=%lld draft_max=%lld", (long long) c[0],
                      (long long) c[1]);
        expect_both("nd_verify_batch", what, fs::verify_batch(c[0], c[1]),
                    fs::native::verify_batch(c[0], c[1]), c[2]);
    }
}

// ── the composed open-time plan ─────────────────────────────────────────────

static const fs::OpenState k_states[] = {
    // name, meta_ok, n_expert, top_k, bps, budget, layers, n_ctx, n_ubatch, draft_max, src,
    // free_before, free_after, validate_ok, open_ok
    {"budget off",              1,   64,  8, 131072,      0,  -1, 4096,    0,  0, 0,     0,     0, 1, 1},
    {"no metadata",             0,    0,  8, 131072,   4096,  -1, 4096,    0,  0, 0, 16384, 16384, 1, 1},
    {"no expert tensors",       1,   64,  8,      0,   4096,  -1, 4096,    0,  0, 0, 16384, 16384, 1, 1},
    {"too small for sentinels", 1,   64,  8, 131072,      1,  -1, 4096,    0,  0, 0, 16384, 16384, 1, 1},
    {"preflight 4096 exact",    1,   64,  8, 131072,   4096,  -1, 4096,    0,  0, 0,  4096, 16384, 1, 1},
    {"preflight 4097",          1,   64,  8, 131072,   4096,  -1, 4096,    0,  0, 0,  4097, 16384, 1, 1},
    {"preflight below reserve", 1,   64,  8, 131072,  30000,  -1, 4096,    0,  0, 0,   100, 16384, 1, 1},
    {"probe keeps plan",        1,   64,  8, 131072,   4096,  -1, 4096,    0,  0, 0, 16384, 16384, 1, 1},
    {"probe resizes",           1,   64,  8, 131072,   4096,  -1, 4096,    0,  0, 0, 16384,  2304, 1, 1},
    {"probe disables",          1,   64,  8, 131072,   4096,  -1, 4096,    0,  0, 0, 16384,   257, 1, 1},
    {"probe resizes mtp",       1,   64,  8, 131072,   4096,  -1, 4096,    0,  8, 1, 16384,  2304, 1, 1},
    {"ngram resizes",           1,   64,  8, 131072,   4096,  -1, 4096,    0, 16, 2, 16384,  2304, 1, 1},
    {"no gpu layers",           1,   64,  8, 131072,   4096,   0, 4096,    0,  0, 0,     0,     0, 1, 1},
    {"ubatch explicit 256",     1,   64,  8, 131072,   4096,  -1, 8192,  256,  0, 0, 16384, 16384, 1, 1},
    {"ubatch follows ctx",      1,   64,  8, 131072,   4096,  -1, 2048,    0,  0, 0, 16384, 16384, 1, 1},
    {"ctx 1",                   1,   64,  8, 131072,   4096,  -1,    1,    0,  0, 0, 16384, 16384, 1, 1},
    {"mtp draft 0",             1,   64,  8, 131072,   4096,  -1, 4096,    0,  0, 1, 16384, 16384, 1, 1},
    {"mtp draft 31",            1,   64,  8, 131072,   4096,  -1, 4096,    0, 31, 1, 16384, 16384, 1, 1},
    {"mtp draft 63",            1,   64,  8, 131072,   4096,  -1, 4096,    0, 63, 1, 16384, 16384, 1, 1},
    {"mtp draft 511",           1,   64,  8, 131072,   4096,  -1, 4096,    0,511, 1, 16384, 16384, 1, 1},
    {"top_k 12, 4 MiB slots",   1,  256, 12,4194304,   8192,  -1, 4096,    0,  0, 0, 20000, 20000, 1, 1},
    {"clamped budget",          1,  256,  8, 524288,  30000,  -1, 4096,  512,  0, 0,  8192,  8192, 1, 1},
    {"validate fails",          1,   64,  8, 131072,   4096,  -1, 4096,    0,  0, 0, 16384, 16384, 0, 1},
    {"open fails",              1,   64,  8, 131072,   4096,  -1, 4096,    0,  0, 0, 16384, 16384, 1, 0},
};

// The state lines carry no mode: the OFF and ON dumps must be byte-identical, which is the
// cross-build parity claim. The mode is in the summary line instead.
static void dump_state(const fs::OpenState & st, const fs::OpenPlan & p) {
    std::printf("%-26s ok=%d vram=%lld slots=%lld reason=%lld sent_mib=%lld "
                "probe=%lld/%lld/%lld n_batch=%lld n_ubatch=%lld draft=%lld verify=%lld steps=%lld",
                st.name, p.ok, (long long) p.effective_vram_mb, (long long) p.hot_slots,
                (long long) p.hot_reason, (long long) p.sentinel_mib, (long long) p.probe_action,
                (long long) p.probe_mib, (long long) p.probe_slots, (long long) p.n_batch,
                (long long) p.n_ubatch, (long long) p.draft_ubatch, (long long) p.verify_batch,
                (long long) p.steps);
    for (int64_t i = 0; i < p.steps; ++i) std::printf(" %lld", (long long) p.step[i]);
    std::printf("\n");
}

static void gate_open_plan() {
    for (const auto & st : k_states) {
        ++g_cases;
        const fs::OpenPlan got = fs::plan_open(st);
        const fs::OpenPlan nat = fs::plan_open_native(st);

        expect("plan", st.name, got.ok, nat.ok);
        expect("plan", st.name, got.effective_vram_mb, nat.effective_vram_mb);
        expect("plan", st.name, got.hot_slots, nat.hot_slots);
        expect("plan", st.name, got.hot_reason, nat.hot_reason);
        expect("plan", st.name, got.sentinel_mib, nat.sentinel_mib);
        expect("plan", st.name, got.probe_action, nat.probe_action);
        expect("plan", st.name, got.probe_mib, nat.probe_mib);
        expect("plan", st.name, got.probe_slots, nat.probe_slots);
        expect("plan", st.name, got.n_batch, nat.n_batch);
        expect("plan", st.name, got.n_ubatch, nat.n_ubatch);
        expect("plan", st.name, got.draft_ubatch, nat.draft_ubatch);
        expect("plan", st.name, got.verify_batch, nat.verify_batch);
        expect("plan", st.name, got.steps, nat.steps);
        for (int64_t i = 0; i < nat.steps && i < 5; ++i) {
            expect("plan", st.name, got.step[i], nat.step[i]);
        }
        if (g_dump) dump_state(st, got);
    }
}

// ── pinned, hand-computed ───────────────────────────────────────────────────

static void gate_pinned() {
    // One MiB at 128 KiB per slot is 8 slots, which is exactly the sentinel floor: refused.
    expect("pinned", "8 slots == 8 sentinels", fs::hot_slots(1, 1, 64, 131072, 8), -1);
    expect("pinned", "reason too small", fs::hot_reason(1, 1, 64, 131072, 8), 3);
    expect("pinned", "sentinel MiB", fs::sentinel_mib(131072, 8), 1);
    expect("pinned", "2 MiB gives 8 spare", fs::hot_slots(2, 1, 64, 131072, 8), 8);
    expect("pinned", "4096 MiB / 4 MiB = 1024", fs::hot_slots(4096, 1, 4096, 4194304, 8), 1016);
    expect("pinned", "clamp 4097 -> 1", fs::vram_clamp(4097, 4096, -1), 1);
    expect("pinned", "reserve is inclusive", fs::vram_clamp(4096, 4096, -1), -1);
    expect("pinned", "mtp margin 768", fs::probe_margin(1), 768);
    expect("pinned", "probe 2304 - 768", fs::probe_budget(2304, 1), 1536);
    expect("pinned", "draft width floor 32", fs::draft_ubatch(0, 512), 32);
    expect("pinned", "draft width 33", fs::draft_ubatch(32, 512), 33);
    expect("pinned", "verify 513", fs::verify_batch(512, 512), 513);
    expect("pinned", "n_batch mirrors ctx", fs::n_batch(8192), 8192);
    expect("pinned", "ubatch falls back", fs::ubatch(0, 2048), 2048);
}

int main(int argc, char ** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--dump") == 0) g_dump = true;
    }

    gate_sentinels();
    gate_hot_store();
    gate_vram_clamp();
    gate_probe();
    gate_resize_plan();
    gate_rts();
    gate_run_plan();
    gate_nd();
    gate_open_plan();
    gate_pinned();

    const long long calls = fs::goose_calls();
#ifdef FG_GOOSE_SESSION
    const char * mode = "goose";
    ++g_checks;
    if (calls <= 0) {
        ++g_fails;
        std::printf("FAIL witness / the Goose module was never entered\n");
    }
#else
    const char * mode = "native";
    ++g_checks;
    if (calls != 0) {
        ++g_fails;
        std::printf("FAIL witness / an OFF build reported %lld Goose calls\n", calls);
    }
#endif

    // Under --dump stdout is the transcript stream only, so the two builds diff cleanly; the
    // summary still reaches the operator on stderr.
    std::FILE * summary = g_dump ? stderr : stdout;
    std::fprintf(summary, "FG644_SESSION mode=%s cases=%d checks=%d failures=%d goose_calls=%lld\n",
                 mode, g_cases, g_checks, g_fails, calls);
    return g_fails == 0 ? 0 : 1;
}
