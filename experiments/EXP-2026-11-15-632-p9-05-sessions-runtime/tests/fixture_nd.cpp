// Differential gate for P9-05 S3: the draft source as the session admits and
// sizes it.
//
// Oracle: a transcription of runtime/core/src/engine/session.cpp (open-time spec
// wiring and the RunInfo report). Port: source/nd-draft-source.goose.
//
// Nothing here generates a token: the per-step policy is
// goose/misc/spec-decode-decisions.goose (EXP619) and stays there, and flag
// admission is goose/cli/config-validate.goose (EXP620). This gate covers the
// numbers a session builds from an already validated SpecConfig.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>

#include "nd.h"

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

static void expect_str(const char * group, const char * what, const std::string & got, const std::string & want) {
    ++g_checks;
    if (got != want) {
        ++g_fails;
        if (g_fails <= 60) {
            std::printf("FAIL %s / %s: got '%s' want '%s'\n", group, what, got.c_str(), want.c_str());
        }
    }
}

static void expect_f64(const char * group, const char * what, double got, double want) {
    ++g_checks;
    // Exact: every value in the table is representable and the native code only
    // copies or zeroes it.
    if (got != want) {
        ++g_fails;
        if (g_fails <= 60) {
            std::printf("FAIL %s / %s: got %.17g want %.17g\n", group, what, got, want);
        }
    }
}

// ── oracle: the native decisions, transcribed ───────────────────────────────

// session.cpp:1169
//   ri.spec = cfg.spec.is_mtp() ? "mtp" : cfg.spec.is_ngram() ? "ngram" : "off";
static const char * nat_source_name(int64_t src) {
    if (src == 1) return "mtp";
    if (src == 2) return "ngram";
    return "off";
}

// session.cpp:728 — mparams.load_mtp = cfg.spec.is_mtp();
static int64_t nat_load_mtp(int64_t src) { return src == 1 ? 1 : 0; }

// session.cpp:765-770
//   if (cfg.spec.is_mtp() && im.n_layer_nextn <= 0) return fail("--mtp needs ...");
static int64_t nat_mtp_admit(int64_t src, int64_t nextn) {
    if (src == 1 && nextn <= 0) return 0;
    return 1;
}

// session.cpp:802
//   const int n_layer_streamed = im.n_layer + (cfg.spec.is_mtp() ? im.n_layer_nextn : 0);
static int64_t nat_stream_layers(int64_t n_layer, int64_t src, int64_t nextn) {
    return n_layer + (src == 1 ? nextn : 0);
}

// session.cpp:843 — only written when speculation is on; the default survives otherwise.
static int64_t nat_rs_seq(int64_t src, int64_t draft_max, int64_t default_rs) {
    return src != 0 ? draft_max : default_rs;
}

// session.cpp:926-927
//   dparams.n_ubatch = (uint32_t) std::max(cfg.spec.draft_max + 1, mtp_draft_ubatch);
//   if (dparams.n_ubatch > cparams.n_ubatch) dparams.n_ubatch = cparams.n_ubatch;
static int64_t nat_draft_ubatch(int64_t draft_max, int64_t tgt) {
    int64_t w = draft_max + 1;
    const int64_t floor_w = 32;
    if (w < floor_w) w = floor_w;
    return w > tgt ? tgt : w;
}

// session.cpp:983
//   im.mtp_batch = llama_batch_init(std::max(cfg.n_batch, cfg.spec.draft_max + 1), 0, 1);
static int64_t nat_verify_batch(int64_t n_batch, int64_t draft_max) {
    const int64_t v = draft_max + 1;
    return n_batch > v ? n_batch : v;
}

// session.cpp:1170-1172
static int64_t nat_ri_draft_max(int64_t src, int64_t draft_max) { return src != 0 ? draft_max : 0; }
static double nat_ri_p_min(int64_t src, double p_min) { return src == 1 ? p_min : 0.0; }
static int64_t nat_ri_ngram_min(int64_t src, int64_t min_match) { return src == 2 ? min_match : 0; }

int main() {
    goose_init(0, nullptr);

    // ── source identity ─────────────────────────────────────────────────────
    {
        static const int64_t sources[] = {0, 1, 2, 7};
        for (int64_t src : sources) {
            ++g_cases;
            char buf[16] = {0};
            const int64_t n = nd_source_name(src, sl_u8{(uint8_t *) buf, 16});
            const std::string got(buf, (size_t) n);
            expect_str("source name", "label", got, nat_source_name(src));
            expect("source name", "length", n, (int64_t) std::strlen(nat_source_name(src)));
            expect("source", "enabled", nd_enabled(src), src != 0 ? 1 : 0);
            expect("source", "load_mtp", nd_load_mtp(src), nat_load_mtp(src));
        }
        // A short buffer must not be overrun; the returned length is the full one.
        char two[3] = {0, 0, 0};
        expect("source name", "short buffer length", nd_source_name(2, sl_u8{(uint8_t *) two, 2}), 5);
        expect_str("source name", "short buffer is a prefix", std::string(two, 2), "ng");
    }

    // ── MTP head admission ──────────────────────────────────────────────────
    {
        struct admit_case {
            const char * name;
            int64_t src, nextn, want;
        };
        static const admit_case cases[] = {
            {"mtp without a head",     1,  0, 0},
            {"mtp negative head",      1, -1, 0},
            {"mtp with one head layer", 1, 1, 1},
            {"mtp with two",           1,  2, 1},
            {"ngram needs no head",    2,  0, 1},
            {"off needs no head",      0,  0, 1},
        };
        for (const auto & c : cases) {
            ++g_cases;
            expect(c.name, "admit", nd_mtp_admit(c.src, c.nextn), c.want);
            expect(c.name, "admit vs oracle", nd_mtp_admit(c.src, c.nextn), nat_mtp_admit(c.src, c.nextn));
        }
    }

    // ── the streamed span ───────────────────────────────────────────────────
    {
        struct span_case {
            const char * name;
            int64_t n_layer, src, nextn;
        };
        static const span_case cases[] = {
            {"mtp adds the head",       32, 1, 1},
            {"mtp adds no head",        32, 1, 0},
            {"ngram ignores nextn",     32, 2, 5},
            {"off ignores nextn",       32, 0, 5},
            {"small trunk",              4, 1, 2},
        };
        for (const auto & c : cases) {
            ++g_cases;
            expect(c.name, "stream layers", nd_stream_layers(c.n_layer, c.src, c.nextn),
                   nat_stream_layers(c.n_layer, c.src, c.nextn));
        }
    }

    // ── rollback snapshots ──────────────────────────────────────────────────
    {
        struct rs_case {
            const char * name;
            int64_t src, draft_max, dflt;
        };
        static const rs_case cases[] = {
            {"mtp",                  1, 3, 0},
            {"ngram",                2, 5, 0},
            {"off keeps the default", 0, 3, 0},
            {"off keeps a non-zero default", 0, 3, 4},
            {"ngram draft 1",        2, 1, 2},
        };
        for (const auto & c : cases) {
            ++g_cases;
            expect(c.name, "n_rs_seq", nd_rs_seq(c.src, c.draft_max, c.dflt), nat_rs_seq(c.src, c.draft_max, c.dflt));
        }
    }

    // ── the draft context width ─────────────────────────────────────────────
    {
        struct ub_case {
            const char * name;
            int64_t draft_max, tgt;
        };
        static const ub_case cases[] = {
            {"draft 3 floors at 32",       3, 2048},
            {"draft 40 exceeds the floor", 40, 2048},
            {"draft 31 floors at 32",     31, 2048},
            {"draft 0 floors at 32",       0, 2048},
            {"clamped by a narrow target", 3,   16},
            {"clamped by target 1",        3,    1},
            {"target is the wider bound", 64, 4096},
        };
        for (const auto & c : cases) {
            ++g_cases;
            expect(c.name, "draft ubatch", nd_draft_ubatch(c.draft_max, c.tgt), nat_draft_ubatch(c.draft_max, c.tgt));
        }
    }

    // ── the verify batch ────────────────────────────────────────────────────
    {
        struct vb_case {
            const char * name;
            int64_t n_batch, draft_max;
        };
        static const vb_case cases[] = {
            {"prefill dominates",       2048, 3},
            {"verify dominates",           2, 3},
            {"verify at zero prefill",     0, 3},
            {"draft 0 needs one row",   2048, 0},
            {"draft 8 at n_batch 8",       8, 7},
            {"draft 8 at n_batch 9",       9, 8},
        };
        for (const auto & c : cases) {
            ++g_cases;
            expect(c.name, "verify batch", nd_verify_batch(c.n_batch, c.draft_max),
                   nat_verify_batch(c.n_batch, c.draft_max));
        }
    }

    // ── the RunInfo report ──────────────────────────────────────────────────
    {
        static const int64_t sources[] = {0, 1, 2};
        for (int64_t src : sources) {
            ++g_cases;
            expect("report", "spec_draft_max", nd_ri_draft_max(src, 3), nat_ri_draft_max(src, 3));
            expect("report", "p_min", (int64_t) (nd_ri_p_min(src, 0.25) == nat_ri_p_min(src, 0.25)), 1);
            expect("report", "ngram_min_match", nd_ri_ngram_min(src, 3), nat_ri_ngram_min(src, 3));
        }
    }

    // ── pinned, hand-computed ───────────────────────────────────────────────
    expect_str("pinned", "off", std::string("off"), nat_source_name(0));
    expect("pinned", "mtp load", nd_load_mtp(1), 1);
    expect("pinned", "ngram does not load a head", nd_load_mtp(2), 0);
    expect("pinned", "mtp admit needs nextn", nd_mtp_admit(1, 0), 0);
    expect("pinned", "span 32+1", nd_stream_layers(32, 1, 1), 33);
    expect("pinned", "rs_seq mtp", nd_rs_seq(1, 3, 0), 3);
    expect("pinned", "draft ubatch 3", nd_draft_ubatch(3, 2048), 32);
    expect("pinned", "draft ubatch 40", nd_draft_ubatch(40, 2048), 41);
    expect("pinned", "draft ubatch clamp", nd_draft_ubatch(3, 16), 16);
    expect("pinned", "verify 2048/3", nd_verify_batch(2048, 3), 2048);
    expect("pinned", "verify 2/3", nd_verify_batch(2, 3), 4);
    expect("pinned", "p_min mtp", (int64_t) (nd_ri_p_min(1, 0.5) == 0.5), 1);
    expect("pinned", "p_min ngram is 0", (int64_t) (nd_ri_p_min(2, 0.5) == 0.0), 1);
    expect("pinned", "ngram_min mtp is 0", nd_ri_ngram_min(1, 3), 0);
    expect("pinned", "ngram_min ngram", nd_ri_ngram_min(2, 3), 3);
    expect_f64("pinned", "p_min passthrough", nd_ri_p_min(1, 0.75), 0.75);

    std::printf("FG636_ND cases=%d checks=%d failures=%d\n", g_cases, g_checks, g_fails);
    return g_fails == 0 ? 0 : 1;
}
