// Differential gate for P10-02 S3 (fit-params fitted command line).
//
// Native arm: a transcription of tools/fit-params/fit-params.cpp lines 44-71,
// because main loads a model before printing and cannot be linked here. The
// transcription keeps the native printf formats and the uint32_t cast.
//
// Port arm: the fp_ exports of source/fit-params-line.goose.
#include "fp.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int cases = 0;
static int values = 0;
static int fails = 0;

static sl_u8 SL(const std::string & s) {
    return sl_u8{(uint8_t *) (void *) s.data(), (int64_t) s.size()};
}

static void fail(const char * name, const std::string & want, const std::string & got) {
    fails++;
    printf("FAIL %-24s want=[%s] got=[%s]\n", name, want.c_str(), got.c_str());
}

static void eq_str(const char * name, const std::string & want, const std::string & got) {
    values++;
    if (want != got) {
        fail(name, want, got);
    }
}

static void eq_int(const char * name, int64_t want, int64_t got) {
    values++;
    if (want != got) {
        fail(name, std::to_string(want), std::to_string(got));
    }
}

// ── native transcription (fit-params.cpp 44-71) ────────────────────────────
static std::string nat_fp_line(unsigned n_ctx, int ngl, const std::vector<float> & tensor_split,
                               int ndev, const std::vector<std::string> & patterns,
                               const std::vector<std::string> & bufts) {
    std::string s;
    char        buf[128];
    snprintf(buf, sizeof buf, "-c %u -ngl %i", n_ctx, ngl);
    s += buf;

    size_t nd = (size_t) ndev;
    while (nd > 1 && tensor_split[nd - 1] == 0.0f) {
        nd--;
    }
    if (nd > 1) {
        for (size_t id = 0; id < nd; id++) {
            if (id == 0) {
                s += " -ts ";
            }
            snprintf(buf, sizeof buf, "%s%u", id > 0 ? "," : "", uint32_t(tensor_split[id]));
            s += buf;
        }
    }

    bool any_tbo = false;
    for (size_t itbo = 0; itbo < patterns.size(); itbo++) {
        if (itbo == 0) {
            s += " -ot \"";
        }
        snprintf(buf, sizeof buf, "%s%s=%s", itbo > 0 ? "," : "", patterns[itbo].c_str(),
                 bufts[itbo].c_str());
        s += buf;
        any_tbo = true;
    }
    s += any_tbo ? "\"\n" : "\n";
    return s;
}

static std::string port_fp_line(int64_t n_ctx, int64_t ngl, const std::vector<double> & ts,
                                int64_t ndev, const std::vector<std::string> & patterns,
                                const std::vector<std::string> & bufts) {
    std::string pblob, bblob;
    std::vector<int64_t> poff, plen, boff, blen;
    for (size_t i = 0; i < patterns.size(); i++) {
        poff.push_back((int64_t) pblob.size());
        plen.push_back((int64_t) patterns[i].size());
        pblob += patterns[i];
        boff.push_back((int64_t) bblob.size());
        blen.push_back((int64_t) bufts[i].size());
        bblob += bufts[i];
    }
    if (patterns.empty()) {
        poff.push_back(0);
        plen.push_back(-1);
        boff.push_back(0);
        blen.push_back(0);
    }
    char b[1024];
    memset(b, 0x5a, sizeof b);
    int64_t n = fp_line(n_ctx, ngl, sl_f64{(double *) (void *) ts.data(), (int64_t) ts.size()}, ndev,
                        sl_u8{(uint8_t *) (void *) pblob.data(), (int64_t) pblob.size()},
                        sl_i64{poff.data(), (int64_t) poff.size()},
                        sl_i64{plen.data(), (int64_t) plen.size()},
                        sl_u8{(uint8_t *) (void *) bblob.data(), (int64_t) bblob.size()},
                        sl_i64{boff.data(), (int64_t) boff.size()},
                        sl_i64{blen.data(), (int64_t) blen.size()},
                        (int64_t) patterns.size(), sl_u8{(uint8_t *) b, (int64_t) sizeof b});
    if (n < 0) {
        return "!";
    }
    return std::string(b, (size_t) n);
}

static std::vector<double> widen(const std::vector<float> & v) {
    return std::vector<double>(v.begin(), v.end());
}

int main() {
    goose_init(0, nullptr);

    // ── the uint32_t cast (45-56) ─────────────────────────────────────────
    std::vector<float> casts = {0.0f, 0.5f, 1.0f, 1.9f, 2.999999f, 99.0f, 4096.0f, 4294967040.0f};
    for (float f : casts) {
        cases++;
        eq_int("fp_u32", (int64_t) uint32_t(f), fp_u32((double) f));
    }

    // ── trailing-zero trimming (47-49) ───────────────────────────────────
    struct T { std::vector<float> v; int ndev; int want; } trims[] = {
        {{0, 0, 0}, 16, 1},
        {{1, 0, 0}, 16, 1},
        {{1, 1, 0}, 16, 2},
        {{1, 2, 3}, 16, 3},
        {{1, 0, 3, 0}, 16, 3},
        {{0}, 1, 1},
        {{1, 0, 0}, 3, 1},
        {{1, 1, 1}, 3, 3},
    };
    for (auto & t : trims) {
        cases++;
        std::vector<float>  pad(16, 0.0f);
        std::vector<double> dpad(16, 0.0);
        for (size_t i = 0; i < t.v.size(); i++) {
            pad[i] = t.v[i];
            dpad[i] = (double) t.v[i];
        }
        int64_t got = fp_ts_count(sl_f64{(double *) dpad.data(), 16}, t.ndev);
        eq_int("fp_ts_count", (int64_t) [&] {
            size_t nd = (size_t) t.ndev;
            while (nd > 1 && pad[nd - 1] == 0.0f) nd--;
            return nd;
        }(), got);
    }

    // ── composed lines (44-71) ───────────────────────────────────────────
    struct L {
        unsigned n_ctx;
        int ngl;
        std::vector<float> ts;
        std::vector<std::string> pats, bufts;
        const char * name;
    };
    std::vector<L> ls = {
        {4096, 99, {}, {}, {}, "plain"},
        {1, 0, {}, {}, {}, "min"},
        {131072, -1, {}, {}, {}, "negative_ngl"},
        {4096, 99, {1, 0, 0}, {}, {}, "ts_one_effective"},
        {4096, 99, {1, 1, 0}, {}, {}, "ts_two"},
        {4096, 99, {1, 1, 1}, {}, {}, "ts_three"},
        {4096, 99, {0.5f, 0.25f, 0}, {}, {}, "ts_truncated"},
        {4096, 99, {1.9f, 2.9f, 0}, {}, {}, "ts_round_down"},
        {4096, 99, {0, 0, 0}, {"blk.0"}, {"CUDA0"}, "ot_one"},
        {4096, 99, {0, 0, 0}, {"blk.0", "blk.1"}, {"CUDA0", "CPU"}, "ot_two"},
        {4096, 99, {1, 1, 0}, {"blk.0"}, {"CUDA0"}, "ts_and_ot"},
        {0, 0, {}, {"a.b", "c"}, {"X", "Y"}, "ot_zero_ctx"},
    };
    for (auto & l : ls) {
        cases++;
        std::vector<float> pad(16, 0.0f);
        for (size_t i = 0; i < l.ts.size(); i++) {
            pad[i] = l.ts[i];
        }
        std::string want = nat_fp_line(l.n_ctx, l.ngl, pad, 16, l.pats, l.bufts);

        std::vector<double> d = widen(pad);
        std::string got = port_fp_line((int64_t) l.n_ctx, (int64_t) l.ngl, d, 16, l.pats, l.bufts);
        eq_str(l.name, want, got);
    }

    printf("FG637_FP cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
