// Differential gate for P10-12 S4: the `moe-predict` report.
//
// The oracle is the carrier's own printf sequence: tests/native_extract.py
// copies print_predict_report (runtime/cli/main.cpp:540-588) verbatim, and the
// real PredictorStats header (runtime/core/include/bmoe/predict_stats.h) is
// used as-is, so hit_frac()/exact_frac() are the carrier's functions and not a
// restatement. The fixture redirects stdout to a temporary file, runs the
// native report, and compares its bytes with the port's buffer, line by line
// and byte for byte — which is what pins the `%.1f` rounding, the six-column
// padding, the `-` rule and the em dash in the "scored" line.
//
// The only thing transcribed is the RunSummary member list the function reads
// (metrics.h cannot be included without the whole engine); each field is named
// exactly as in bmoe/metrics.h.
//
// Checks are explicit (no <cassert>): NDEBUG is defined in Release builds.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

#include "bmoe/predict_stats.h"

using bmoe::PredictorStats;

// bmoe/metrics.h:262-263, the six fields print_predict_report reads.
struct RunSummary {
    PredictorStats predict_stale, predict_stale2, predict_prev, predict_self;
    std::vector<PredictorStats> predict_stale_by_layer, predict_prev_by_layer, predict_self_by_layer;
    long long predict_unscored = 0;
};

#include "native_predict_report.inc"

#include "mp.h" // mp_report, goose_init

// The one conversion the port delegates to libc: "%.1f" of a fraction scaled to
// percent, at the offset the port chose. libc returns the length it would have
// written; the port adds it to its position, exactly as printf would.
extern "C" int64_t mp_fmt_f1(sl_u8 out, int64_t at, double x) {
    if (at < 0 || at >= out.len) return 0;
    return (int64_t) std::snprintf((char *) out.data + at, (size_t) (out.len - at), "%.1f", x);
}

namespace {

int g_cases = 0, g_values = 0, g_fails = 0;

void note(bool ok, const std::string & what) {
    ++g_values;
    if (!ok) {
        ++g_fails;
        std::printf("  FAIL [S4] %s\n", what.c_str());
    }
}

std::string capture_native(const RunSummary & s) {
    std::fflush(stdout);
    const int saved = dup(1);
    FILE * f = std::tmpfile();
    if (!f || saved < 0) {
        std::printf("  FAIL [S4] cannot redirect stdout\n");
        return std::string();
    }
    dup2(fileno(f), 1);
    print_predict_report(s);
    std::fflush(stdout);
    dup2(saved, 1);
    close(saved);
    std::rewind(f);
    std::string out;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
    return out;
}

std::string port_report(const RunSummary & s, long long n_layers) {
    int64_t head[12] = {s.predict_stale.rows,  s.predict_stale.slots,  s.predict_stale.hits,
                        s.predict_stale.exact, s.predict_prev.rows,   s.predict_prev.slots,
                        s.predict_prev.hits,   s.predict_prev.exact,  s.predict_self.rows,
                        s.predict_self.slots,  s.predict_self.hits,   s.predict_self.exact};
    int64_t st2[4] = {s.predict_stale2.rows, s.predict_stale2.slots, s.predict_stale2.hits,
                      s.predict_stale2.exact};
    std::vector<int64_t> layer((size_t) (n_layers * 12 > 0 ? n_layers * 12 : 12), 0);
    auto fill = [&](int slot, const std::vector<PredictorStats> & v) {
        for (size_t il = 0; il < (size_t) n_layers; ++il) {
            if (il < v.size()) {
                layer[il * 12 + slot * 4 + 0] = v[il].rows;
                layer[il * 12 + slot * 4 + 1] = v[il].slots;
                layer[il * 12 + slot * 4 + 2] = v[il].hits;
                layer[il * 12 + slot * 4 + 3] = v[il].exact;
            }
        }
    };
    fill(0, s.predict_stale_by_layer);
    fill(1, s.predict_prev_by_layer);
    fill(2, s.predict_self_by_layer);

    // One row is at most 40 bytes; 1100 layers need under 48 KiB. The port's
    // contract is that the caller sizes `out` for the table it asked for; the
    // gate gives every case the long-table bound.
    std::vector<uint8_t> out(1 << 16, 0);
    sl_i64 vh{head, 12}, vs{st2, 4}, vl{layer.data(), (int64_t) layer.size()};
    sl_u8 vo{out.data(), (int64_t) out.size()};
    const int64_t n = mp_report(vh, vs, s.predict_unscored, n_layers, vl, vo);
    return std::string((const char *) out.data(), (size_t) n);
}

std::vector<std::string> split_lines(const std::string & s) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start < s.size()) {
        size_t nl = s.find('\n', start);
        if (nl == std::string::npos) {
            out.push_back(s.substr(start));
            break;
        }
        out.push_back(s.substr(start, nl - start + 1));
        start = nl + 1;
    }
    return out;
}

std::string visible(const std::string & s) {
    std::string o;
    for (char c : s) {
        if (c == '\n') o += "\\n";
        else o += c;
    }
    return o;
}

void run_case(const char * name, const RunSummary & s, long long n_layers) {
    ++g_cases;
    const std::string nat = capture_native(s);
    const std::string port = port_report(s, n_layers);
    note(nat == port, std::string(name) + " byte-identical report");
    note(nat.size() == port.size(), std::string(name) + " length native=" + std::to_string(nat.size()) +
                                        " port=" + std::to_string(port.size()));
    const std::vector<std::string> an = split_lines(nat), ap = split_lines(port);
    note(an.size() == ap.size(), std::string(name) + " line count");
    const size_t common = an.size() < ap.size() ? an.size() : ap.size();
    for (size_t k = 0; k < common; ++k) {
        if (an[k] != ap[k])
            std::printf("  [S4] %s line[%zu]\n     native=%s\n     port  =%s\n", name, k,
                        visible(an[k]).c_str(), visible(ap[k]).c_str());
        note(an[k] == ap[k], std::string(name) + " line[" + std::to_string(k) + "]");
    }
}

PredictorStats ps(long long rows, long long slots, long long hits, long long exact) {
    PredictorStats p;
    p.rows = rows;
    p.slots = slots;
    p.hits = hits;
    p.exact = exact;
    return p;
}

void add_layers(RunSummary & s, std::vector<PredictorStats> stale, std::vector<PredictorStats> prev,
                std::vector<PredictorStats> ctrl) {
    s.predict_stale_by_layer = std::move(stale);
    s.predict_prev_by_layer = std::move(prev);
    s.predict_self_by_layer = std::move(ctrl);
}

} // namespace

int main(int argc, char ** argv) {
    goose_init(argc, argv);

    // 1. Nothing measured: three aggregate lines and no table.
    {
        RunSummary s;
        run_case("empty", s, 0);
    }
    // 2. A run with no predictors scored and one dense layer.
    {
        RunSummary s;
        add_layers(s, {ps(0, 0, 0, 0)}, {ps(0, 0, 0, 0)}, {ps(0, 0, 0, 0)});
        run_case("dense-layer-only", s, 1);
    }
    // 3. The ordinary case: three predictors over four MoE layers, control at 100%.
    {
        RunSummary s;
        s.predict_stale = ps(96, 768, 600, 40);
        s.predict_prev = ps(100, 800, 720, 70);
        s.predict_self = ps(100, 800, 800, 100);
        s.predict_stale2 = ps(90, 720, 500, 30);
        s.predict_unscored = 4;
        add_layers(s, {ps(0, 0, 0, 0), ps(32, 256, 200, 12), ps(32, 256, 190, 10), ps(32, 256, 210, 18)},
                   {ps(0, 0, 0, 0), ps(33, 264, 240, 20), ps(33, 264, 235, 18), ps(34, 272, 245, 32)},
                   {ps(0, 0, 0, 0), ps(33, 264, 264, 33), ps(33, 264, 264, 33), ps(34, 272, 272, 34)});
        run_case("four-layers-control-100", s, 4);
    }
    // 4. A control below 100%: the warning must print, and not otherwise.
    {
        RunSummary s;
        s.predict_stale = ps(50, 400, 300, 20);
        s.predict_prev = ps(50, 400, 320, 25);
        s.predict_self = ps(50, 400, 398, 49);
        add_layers(s, {ps(50, 400, 300, 20)}, {ps(50, 400, 320, 25)}, {ps(50, 400, 398, 49)});
        run_case("control-below-floor", s, 1);
    }
    // 5. Exactly 0.999: the predicate is `< 0.999`, so no warning.
    {
        RunSummary s;
        s.predict_stale = ps(1, 1000, 999, 0);
        s.predict_prev = ps(1, 1000, 999, 0);
        s.predict_self = ps(1, 1000, 999, 0);
        add_layers(s, {ps(1, 1000, 999, 0)}, {ps(1, 1000, 999, 0)}, {ps(1, 1000, 999, 0)});
        run_case("control-exactly-0999", s, 1);
    }
    // 6. Control with rows but no slots: hit_frac() is 0.0, so the warning prints.
    {
        RunSummary s;
        s.predict_self = ps(7, 0, 0, 0);
        run_case("control-rows-no-slots", s, 0);
    }
    // 7. Layer 0 scored by prev and the control but not by the stale gate: "-".
    {
        RunSummary s;
        s.predict_stale = ps(10, 80, 50, 5);
        s.predict_prev = ps(12, 96, 70, 9);
        s.predict_self = ps(12, 96, 96, 12);
        add_layers(s, {ps(0, 0, 0, 0), ps(10, 80, 50, 5)}, {ps(12, 96, 70, 9), ps(12, 96, 70, 9)},
                   {ps(12, 96, 96, 12), ps(12, 96, 96, 12)});
        run_case("layer0-unscored", s, 2);
    }
    // 8. Rounding: 1/16 is 6.25 exactly, and %.1f must agree on the tie.
    {
        RunSummary s;
        s.predict_stale = ps(16, 256, 16, 1);
        s.predict_prev = ps(16, 256, 24, 2);
        s.predict_self = ps(16, 256, 255, 15);
        add_layers(s, {ps(16, 256, 16, 1)}, {ps(16, 256, 24, 2)}, {ps(16, 256, 255, 15)});
        run_case("half-even-rounding", s, 1);
    }
    // 9. A long table with dense gaps and a layer index past four digits.
    {
        RunSummary s;
        s.predict_stale = ps(1000, 8000, 6000, 400);
        s.predict_prev = ps(1000, 8000, 6500, 500);
        s.predict_self = ps(1000, 8000, 7900, 900);
        s.predict_stale2 = ps(900, 7200, 5200, 300);
        std::vector<PredictorStats> a, b, c;
        for (int il = 0; il < 1100; ++il) {
            const bool dense = (il % 7) == 3;
            a.push_back(dense ? ps(0, 0, 0, 0) : ps(4, 32, 20 + il % 13, 1 + il % 3));
            b.push_back(dense ? ps(0, 0, 0, 0) : ps(4, 32, 24 + il % 7, 2 + il % 2));
            c.push_back(dense ? ps(0, 0, 0, 0) : ps(4, 32, 32, 4));
        }
        add_layers(s, a, b, c);
        run_case("long-table-dense-gaps", s, 1100);
    }
    // 10. Large counters: %lld without padding, and 100.0% of a big denominator.
    {
        RunSummary s;
        s.predict_stale = ps(4000000000LL, 32000000000LL, 24000000000LL, 1000000000LL);
        s.predict_prev = ps(4000000000LL, 32000000000LL, 28000000000LL, 1500000000LL);
        s.predict_self = ps(4000000000LL, 32000000000LL, 32000000000LL, 4000000000LL);
        s.predict_unscored = 123456789LL;
        add_layers(s, {ps(4000000000LL, 32000000000LL, 24000000000LL, 1000000000LL)},
                   {ps(4000000000LL, 32000000000LL, 28000000000LL, 1500000000LL)},
                   {ps(4000000000LL, 32000000000LL, 32000000000LL, 4000000000LL)});
        run_case("huge-counters", s, 1);
    }
    // 11. A layer with zero slots but rows: the pct column reads 0.0, not "-".
    {
        RunSummary s;
        s.predict_stale = ps(3, 0, 0, 0);
        s.predict_prev = ps(3, 0, 0, 0);
        s.predict_self = ps(3, 0, 0, 0);
        add_layers(s, {ps(3, 0, 0, 0)}, {ps(3, 0, 0, 0)}, {ps(3, 0, 0, 0)});
        run_case("rows-without-slots", s, 1);
    }
    // 12. Only the stale-2 horizon scored: the aggregate lines still print.
    {
        RunSummary s;
        s.predict_stale2 = ps(5, 40, 30, 3);
        run_case("stale2-only", s, 0);
    }
    // 13. A single layer whose values need every padding width at once.
    {
        RunSummary s;
        s.predict_stale = ps(1, 1, 1, 1);
        s.predict_prev = ps(1, 1, 0, 0);
        s.predict_self = ps(1, 1, 1, 1);
        add_layers(s, {ps(1, 1, 1, 1)}, {ps(1, 1, 0, 0)}, {ps(1, 1, 1, 1)});
        run_case("minimal-widths", s, 1);
    }
    // 14. A control between the two candidate thresholds (0.9985): the native
    // predicate is `< 0.999`, so this one warns and a 0.998 cut would not.
    {
        RunSummary s;
        s.predict_stale = ps(10, 10000, 7500, 5);
        s.predict_prev = ps(10, 10000, 8000, 6);
        s.predict_self = ps(10, 10000, 9985, 9);
        add_layers(s, {ps(10, 10000, 7500, 5)}, {ps(10, 10000, 8000, 6)}, {ps(10, 10000, 9985, 9)});
        run_case("control-between-thresholds", s, 1);
    }
    // 15. The control at zero rows but the stale gate scored: no warning, no crash.
    {
        RunSummary s;
        s.predict_stale = ps(20, 160, 120, 10);
        s.predict_prev = ps(20, 160, 130, 12);
        add_layers(s, {ps(20, 160, 120, 10)}, {ps(20, 160, 130, 12)}, {ps(0, 0, 0, 0)});
        run_case("control-absent", s, 1);
    }

    std::printf("FG649_MP cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails;
}
