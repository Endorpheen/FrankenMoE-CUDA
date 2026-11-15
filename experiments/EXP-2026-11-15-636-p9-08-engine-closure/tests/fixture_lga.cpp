// Differential gate for P9-08 S1: what Session::open() admits and in which
// order it initialises.
//
// Oracle: a transcription of runtime/core/src/engine/session.cpp:693-697
// (explicit hot slots), :737-751 (the n_expert_used override), :699-722 (the
// model-parameter projection), :819-847 (the context parameters that are not a
// width) and :628-1271 (the stage spine). Port:
// source/lga-load-admission.goose.
//
// Everything that needs a gguf, a device, a file or llama.cpp is an input here:
// the already-clamped budget, meta_ok, arch_len, n_expert, cpu_dev_present and
// the per-stage failure injection.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <utility>

#include "lga.h"

static int g_cases = 0, g_checks = 0, g_fails = 0;

static void expect(const char * group, const char * what, int64_t got, int64_t want) {
    ++g_checks;
    if (got != want) {
        ++g_fails;
        if (g_fails <= 80) {
            std::printf("FAIL %s / %s: got %lld want %lld\n", group, what, (long long) got, (long long) want);
        }
    }
}

// ── oracle: the stage spine, transcribed (session.cpp:628-1271) ─────────────

enum {
    SG_BACKEND = 1, SG_META, SG_PREFLIGHT, SG_HOTPLAN, SG_MPARAMS, SG_LOAD,
    SG_HEAD, SG_ARCH, SG_TEMPLATE, SG_HOOK, SG_CPARAMS, SG_PROBE, SG_CTX,
    SG_DCTX, SG_SAMPLER, SG_ABORT, SG_MTP, SG_VERIFY, SG_STREAM, SG_READY,
    SG_COUNT = 20
};

static int64_t nat_init_plan(const int64_t * fail, int64_t * out, int64_t cap) {
    int64_t n = 0;
    for (int64_t stage = SG_BACKEND; stage <= SG_READY; ++stage) {
        if (n >= cap) return -1;
        out[n++] = stage;
        if (fail[stage - 1] != 0) return n;
    }
    return n;
}

static int64_t nat_refusal(int64_t stage) {
    switch (stage) {
        case SG_PREFLIGHT: return 1;
        case SG_HOTPLAN:   return 2;
        case SG_LOAD:      return 3;
        case SG_HEAD:      return 4;
        case SG_ARCH:      return 5;
        case SG_CTX:       return 6;
        case SG_DCTX:      return 7;
        case SG_MTP:       return 8;
        case SG_STREAM:    return 9;
        default:           return 0;
    }
}

// session.cpp:737-751
enum { OV_APPLIED = 0, OV_NOT_SET, OV_NO_META, OV_NO_ARCH, OV_DENSE, OV_TOO_BIG };

static int64_t nat_override_admit(int64_t n_expert_used, int64_t meta_ok, int64_t arch_len, int64_t n_expert) {
    if (n_expert_used <= 0) return OV_NOT_SET;
    if (!meta_ok) return OV_NO_META;
    if (arch_len <= 0) return OV_NO_ARCH;
    if (n_expert <= 0) return OV_DENSE;
    if (n_expert_used > n_expert) return OV_TOO_BIG;
    return OV_APPLIED;
}

// session.cpp:699-722
static int64_t nat_mparams(int64_t n_gpu_layers, int64_t cpu_dev, int64_t moe, int64_t hot, int64_t is_mtp) {
    int64_t m = 1 + 2; // MMAP, use_extra_bufts = false
    if (n_gpu_layers == 0 && cpu_dev) m += 4;
    if ((moe || hot > 0) && n_gpu_layers != 0) m += 8;
    if (is_mtp) m += 16;
    return m;
}

// session.cpp:819-847
static void nat_cparams(int64_t hot, int64_t moe, int64_t trace, int64_t * out) {
    out[0] = hot;
    out[1] = 98;
    out[2] = 110;
    out[3] = 100;
    out[4] = (hot > 0 && moe) ? 1 : 0;
    out[5] = (moe || trace) ? 1 : 0;
}

int main() {
    goose_init(0, nullptr);

    // ── explicit hot slots above the expert count ───────────────────────────
    {
        struct c { const char * name; int64_t hot, n_expert, want; } cases[] = {
            {"budget off",        0,  60, 0},
            {"zero experts",      0,   0, 0},
            {"equal is allowed", 60,  60, 0},
            {"one below",        59,  60, 0},
            {"one above",        61,  60, 1},
            {"huge",           4096,   8, 1},
            {"single expert",     2,   1, 1},
        };
        for (const auto & c : cases) {
            ++g_cases;
            expect(c.name, "admit", lga_hot_admit(c.hot, c.n_expert), c.want);
        }
    }

    // ── the active-expert override ──────────────────────────────────────────
    {
        struct c {
            const char * name; int64_t used, meta, arch, n_expert, want;
        } cases[] = {
            {"unset",                 0, 1, 12,  8, OV_NOT_SET},
            {"negative is unset",    -1, 1, 12,  8, OV_NOT_SET},
            {"applied",               6, 1, 12,  8, OV_APPLIED},
            {"equal to n_expert",     8, 1, 12,  8, OV_APPLIED},
            {"above n_expert",        9, 1, 12,  8, OV_TOO_BIG},
            {"no metadata",           6, 0, 12,  8, OV_NO_META},
            {"no arch",               6, 1,  0,  8, OV_NO_ARCH},
            {"dense model",           6, 1, 12,  0, OV_DENSE},
            {"meta beats arch",       6, 0,  0,  0, OV_NO_META},
            {"arch beats dense",      6, 1,  0,  0, OV_NO_ARCH},
            {"dense beats range",     9, 1, 12,  0, OV_DENSE},
            {"unset beats every fail", 0, 0,  0,  0, OV_NOT_SET},
        };
        for (const auto & c : cases) {
            ++g_cases;
            const int64_t got = lga_override_admit(c.used, c.meta, c.arch, c.n_expert);
            expect(c.name, "code", got, c.want);
            expect(c.name, "code vs oracle", got,
                   nat_override_admit(c.used, c.meta, c.arch, c.n_expert));
            expect(c.name, "tag", lga_override_tag(got), c.want == OV_APPLIED ? 1 : 0);
            expect(c.name, "value", lga_override_value(got, c.used),
                   c.want == OV_APPLIED ? c.used : 0);
        }
    }

    // ── the model-parameter projection over its whole grid ──────────────────
    {
        int64_t n = 0;
        for (int64_t gpu = -1; gpu <= 2; ++gpu) {
            for (int64_t cpu = 0; cpu <= 1; ++cpu) {
                for (int64_t moe = 0; moe <= 1; ++moe) {
                    for (int64_t hot = 0; hot <= 1; ++hot) {
                        for (int64_t mtp = 0; mtp <= 1; ++mtp) {
                            ++n;
                            char name[96];
                            std::snprintf(name, sizeof(name), "grid gpu=%lld cpu=%lld moe=%lld hot=%lld mtp=%lld",
                                          (long long) gpu, (long long) cpu, (long long) moe,
                                          (long long) hot, (long long) mtp);
                            const int64_t got = lga_mparams(gpu, cpu, moe, hot * 32, mtp);
                            const int64_t want = nat_mparams(gpu, cpu, moe, hot, mtp);
                            ++g_checks;
                            if (got != want) {
                                ++g_fails;
                                std::printf("FAIL %s: got %lld want %lld\n", name,
                                            (long long) got, (long long) want);
                            }
                            // mmap and no-repack are unconditional.
                            expect(name, "mmap", got % 2, 1);
                            expect(name, "no extra bufts", (got / 2) % 2, 1);
                        }
                    }
                }
            }
        }
        g_cases += n;

        // Pinned readings of the two conditional arms.
        expect("mparams", "cpu-only on a CPU session", lga_mparams(0, 1, 0, 0, 0), 1 + 2 + 4);
        expect("mparams", "cpu-only without a device", lga_mparams(0, 0, 0, 0, 0), 1 + 2);
        expect("mparams", "gpu session drops the cpu list", lga_mparams(-1, 1, 0, 0, 0), 1 + 2);
        expect("mparams", "hot store arms the override", lga_mparams(-1, 1, 0, 8, 0), 1 + 2 + 8);
        expect("mparams", "moe arms the override", lga_mparams(-1, 1, 1, 0, 0), 1 + 2 + 8);
        expect("mparams", "cpu moe needs no override", lga_mparams(0, 1, 1, 8, 0), 1 + 2 + 4);
        expect("mparams", "mtp sets load_mtp", lga_mparams(-1, 1, 0, 0, 1), 1 + 2 + 16);
    }

    // ── the context parameters ──────────────────────────────────────────────
    {
        const int64_t hots[] = {0, 1, 64};
        for (int64_t hot : hots) {
            for (int64_t moe = 0; moe <= 1; ++moe) {
                for (int64_t trace = 0; trace <= 1; ++trace) {
                    ++g_cases;
                    char name[96];
                    std::snprintf(name, sizeof(name), "cparams hot=%lld moe=%lld trace=%lld",
                                  (long long) hot, (long long) moe, (long long) trace);
                    int64_t got[6] = {-1, -1, -1, -1, -1, -1};
                    int64_t want[6] = {-1, -1, -1, -1, -1, -1};
                    expect(name, "fields", lga_cparams(hot, moe, trace, sl_i64{got, 6}), 6);
                    nat_cparams(hot, moe, trace, want);
                    for (int i = 0; i < 6; ++i) {
                        expect(name, "field", got[i], want[i]);
                    }
                }
            }
        }
        // A short slice is refused, not truncated.
        ++g_cases;
        int64_t small[5] = {0};
        expect("cparams", "short slice refused", lga_cparams(8, 1, 1, sl_i64{small, 5}), -1);
        expect("cparams", "second short slice refused", lga_cparams(8, 0, 0, sl_i64{small, 5}), -1);
    }

    // ── the init order and its first refusal ────────────────────────────────
    {
        struct c { const char * name; int64_t fail_at; int64_t want_n; } cases[] = {
            {"no failure",      0, 20},
            {"backend fails",   1,  1},
            {"meta fails",      2,  2},
            {"preflight fails", 3,  3},
            {"hot plan fails",  4,  4},
            {"load fails",      6,  6},
            {"head fails",      7,  7},
            {"ctx fails",      13, 13},
            {"stream fails",   19, 19},
            {"ready fails",    20, 20},
        };
        for (const auto & c : cases) {
            ++g_cases;
            int64_t fail[20] = {0};
            if (c.fail_at > 0) fail[c.fail_at - 1] = 1;
            int64_t got[20] = {0};
            int64_t want[20] = {0};
            const int64_t n = lga_init_plan(sl_i64{fail, 20}, sl_i64{got, 20});
            const int64_t nn = nat_init_plan(fail, want, 20);
            expect(c.name, "stage count", n, c.want_n);
            expect(c.name, "stage count vs oracle", n, nn);
            for (int64_t i = 0; i < n; ++i) {
                expect(c.name, "stage", got[i], want[i]);
            }
            if (c.fail_at > 0) {
                expect(c.name, "stops at the failing stage", got[n - 1], c.fail_at);
                expect(c.name, "refusal code", lga_refusal_code(got[n - 1]),
                       nat_refusal(c.fail_at));
            }
        }

        // Two injected failures: the earlier stage wins and the later one is
        // never reached, which is the ordering property the plan exists for.
        {
            ++g_cases;
            int64_t fail[20] = {0};
            fail[SG_LOAD - 1] = 1;
            fail[SG_CTX - 1] = 1;
            int64_t got[20] = {0};
            const int64_t n = lga_init_plan(sl_i64{fail, 20}, sl_i64{got, 20});
            expect("precedence", "stops at load", n, SG_LOAD);
            expect("precedence", "reports the load", got[n - 1], SG_LOAD);
        }
        {
            ++g_cases;
            int64_t fail[20] = {0};
            fail[SG_PREFLIGHT - 1] = 1;
            fail[SG_HOTPLAN - 1] = 1;
            fail[SG_LOAD - 1] = 1;
            int64_t got[20] = {0};
            const int64_t n = lga_init_plan(sl_i64{fail, 20}, sl_i64{got, 20});
            expect("precedence", "preflight first", n, SG_PREFLIGHT);
            expect("precedence", "preflight code", lga_refusal_code(got[n - 1]), 1);
        }

        // Capacity and input slices.
        {
            ++g_cases;
            int64_t fail[20] = {0};
            int64_t small[19] = {0};
            expect("plan", "short out refused", lga_init_plan(sl_i64{fail, 20}, sl_i64{small, 19}), -1);
            int64_t out[20] = {0};
            int64_t fail_short[19] = {0};
            expect("plan", "short fail refused", lga_init_plan(sl_i64{fail_short, 19}, sl_i64{out, 20}), -1);
        }

        // A failed context never reaches the stream source, whatever else is
        // injected after it.
        {
            ++g_cases;
            int64_t fail[20] = {0};
            for (int i = SG_DCTX; i <= SG_READY; ++i) fail[i - 1] = 1;
            int64_t got[20] = {0};
            const int64_t n = lga_init_plan(sl_i64{fail, 20}, sl_i64{got, 20});
            expect("chain", "ctx failure stops the chain", n, SG_DCTX);
            expect("chain", "nothing past ctx runs", got[n - 1], SG_DCTX);
        }
    }

    // ── the order checker ───────────────────────────────────────────────────
    {
        ++g_cases;
        int64_t native_order[20];
        for (int i = 0; i < 20; ++i) native_order[i] = i + 1;
        expect("order", "native order passes", lga_stage_check(sl_i64{native_order, 20}), 1);

        ++g_cases;
        int64_t swapped[20];
        std::memcpy(swapped, native_order, sizeof(swapped));
        std::swap(swapped[5], swapped[6]);
        expect("order", "load after head fails", lga_stage_check(sl_i64{swapped, 20}), 0);

        ++g_cases;
        int64_t dup[20];
        std::memcpy(dup, native_order, sizeof(dup));
        dup[6] = dup[5];
        expect("order", "duplicate fails", lga_stage_check(sl_i64{dup, 20}), 0);

        ++g_cases;
        int64_t unknown[20];
        std::memcpy(unknown, native_order, sizeof(unknown));
        unknown[9] = 0;
        expect("order", "unknown id fails", lga_stage_check(sl_i64{unknown, 20}), 0);

        ++g_cases;
        int64_t past_end[20];
        std::memcpy(past_end, native_order, sizeof(past_end));
        past_end[19] = 21;
        expect("order", "id past the spine fails", lga_stage_check(sl_i64{past_end, 20}), 0);

        ++g_cases;
        expect("order", "empty fails", lga_stage_check(sl_i64{native_order, 0}), 0);

        ++g_cases;
        expect("order", "prefix passes", lga_stage_check(sl_i64{native_order, 7}), 1);
    }

    // ── refusal codes ───────────────────────────────────────────────────────
    {
        ++g_cases;
        for (int64_t stage = 0; stage <= 21; ++stage) {
            expect("refusal", "code", lga_refusal_code(stage), nat_refusal(stage));
        }
        expect("refusal", "backend cannot refuse", lga_refusal_code(SG_BACKEND), 0);
        expect("refusal", "ready cannot refuse", lga_refusal_code(SG_READY), 0);
    }

    std::printf("FG634_LGA cases=%d checks=%d failures=%d\n", g_cases, g_checks, g_fails);
    return g_fails == 0 ? 0 : 1;
}
