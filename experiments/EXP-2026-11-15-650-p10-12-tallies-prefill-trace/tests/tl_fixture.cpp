// Differential gate for P10-12 S1: the generation and prefill accounting.
//
// The oracle is the carrier's own text, not a retyping: tests/native_extract.py
// copies GenTally (session.cpp:157-251), PrefillTally (259-285) and the three
// summary blocks (1934-1935, 1945-1952, 1967-1968) out of the base-commit
// file and this fixture #includes them. Both arms are driven by the same
// synthetic counter tables and every owned field is compared after every token.
//
// What is stubbed is exactly what the port DOES NOT own:
//   - the /proc and clock readers (pio::*). process_memory/device_memory return
//     false, so the rss/swap columns the port never sees stay at their defaults;
//     process_cpu_seconds reads a settable clock so PrefillTally's CPU delta is
//     reproducible instead of wall-clock noise;
//   - fault_bytes, which is the host page size. The gate asserts that the stub
//     agrees with sysconf(_SC_PAGESIZE), so the constant the port multiplies is
//     the constant the native code multiplies.
// Everything else — the delta, the residual, the clamp, the -1.0 "no cache"
// sentinel, the cursor advance and the per-token averages — runs as carrier code.
//
// Checks are explicit (no <cassert>): NDEBUG is defined in Release builds.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

namespace bmoe {

struct TokenMetrics {
    int step = 0;
    int steps = 0;
    double wall_ms = 0.0;
    double io_ms = 0.0;
    double mgmt_ms = 0.0;
    double compute_ms = 0.0;
    double stall_ms = 0.0;
    double drain_ms = 0.0;
    double adopt_ms = 0.0;
    double ra_issue_ms = 0.0;
    double ra_wd_ms = 0.0;
    double loop_overhead_ms = 0.0;
    uint64_t read_bytes = 0;
    double cache_hit_pct = 0.0;
    uint64_t majflt = 0;
    double cpu_ms = 0.0;
    double dense_resident_frac = -1.0;
    double majflt_mib = 0.0;
    double rss_mib = 0.0;
    double rss_anon_mib = 0.0;
    double rss_file_mib = 0.0;
    double swap_mib = 0.0;
    double mem_available_mib = 0.0;
    double mem_free_mib = 0.0;
    double swap_free_mib = 0.0;
    double cache_budget_mib = 0.0;
    int turn = 0;
};

namespace pio {
struct ProcessMemory {
    uint64_t rss_bytes = 0;
    uint64_t rss_anon_bytes = 0;
    uint64_t rss_file_bytes = 0;
    uint64_t swap_bytes = 0;
};
struct DeviceMemory {
    uint64_t available_bytes = 0;
    uint64_t free_bytes = 0;
    uint64_t swap_free_bytes = 0;
};
double process_cpu_seconds();
size_t fault_bytes();
bool process_memory(ProcessMemory *);
bool device_memory(DeviceMemory *);
} // namespace pio

class IExpertSource {
  public:
    struct Stats {
        uint64_t read_bytes = 0;
        double read_seconds = 0.0;
        double mgmt_seconds = 0.0;
        long long cache_hits = 0;
        long long cache_lookups = 0;
        uint64_t cache_resident_bytes = 0;
        double stall_seconds = 0.0;
        uint64_t spec_read_bytes = 0;
        long long spec_experts = 0;
        long long spec_useful = 0;
        uint64_t cache_budget_bytes = 0;
        long long cache_resizes = 0;
        bool o_direct = false;
        long long evictions = 0;
        long long rereads = 0;
        double drain_wait_seconds = 0.0;
        double adopt_wait_seconds = 0.0;
        double dense_resident_frac = -1.0;
        uint64_t token_demand_bytes = 0;
        uint64_t layer_demand_bytes = 0;
        bool ok = false;
    };
    virtual ~IExpertSource() = default;
    virtual Stats stats() const = 0;
};

} // namespace bmoe

static double g_cpu_clock = 0.0;
static size_t g_page = 4096;

namespace bmoe::pio {
double process_cpu_seconds() { return g_cpu_clock; }
size_t fault_bytes() { return g_page; }
bool process_memory(ProcessMemory *) { return false; }
bool device_memory(DeviceMemory *) { return false; }
} // namespace bmoe::pio

using namespace bmoe;

#include "native_gen_tally.h"
#include "native_prefill_tally.h"

#include "tl.h" // tl_record, tl_prefill, tl_summary, goose_init

// ── the summary wrapper: the native blocks, in the native order ─────────────

struct SumStub {
    double majflt_per_token = 0.0;
    double cpu_s_per_token = 0.0;
    double s_per_token = 0.0;
    double moe_read_mib = 0.0;
    double moe_io_seconds = 0.0;
    double moe_io_s_per_token = 0.0;
    double moe_mgmt_s_per_token = 0.0;
    double moe_stall_s_per_token = 0.0;
    double moe_compute_s_per_token = 0.0;
    double moe_drain_s_per_token = 0.0;
    double moe_adopt_s_per_token = 0.0;
};

static void native_summary(SumStub & s, const GenTally & tally, int n_gen, bool enabled, bool overlap) {
    struct MoeCfg {
        bool enabled;
        bool overlap;
    } moe{enabled, overlap};
    (void) moe;
    (void) n_gen;
#include "native_summary_a.inc"
    if (moe.enabled) {
#include "native_summary_b.inc"
#include "native_summary_c.inc"
    }
}

// ── checks ──────────────────────────────────────────────────────────────────

namespace {

int g_cases = 0, g_values = 0, g_fails = 0;
const char * g_slice = "S?";

void reset(const char * slice) {
    g_slice = slice;
    g_cases = g_values = g_fails = 0;
}
void new_case() { ++g_cases; }
void note(bool ok, const std::string & what) {
    ++g_values;
    if (!ok) {
        ++g_fails;
        std::printf("  FAIL [%s] %s\n", g_slice, what.c_str());
    }
}
void note_f(double a, double b, const std::string & what) {
    // Bit-exact: both arms run the same IEEE-754 operations in the same order.
    bool ok = (a == b) && (std::memcmp(&a, &b, sizeof a) == 0 || (a == 0.0 && b == 0.0));
    if (!ok) std::printf("  [%s] %s: native=%.17g port=%.17g\n", g_slice, what.c_str(), a, b);
    note(ok, what);
}
void note_i(long long a, long long b, const std::string & what) {
    if (a != b) std::printf("  [%s] %s: native=%lld port=%lld\n", g_slice, what.c_str(), a, b);
    note(a == b, what);
}

// ── the port's running state, one field per native GenTally member ──────────

struct PortTally {
    long long prev_bytes = 0;
    double prev_io_s = 0.0;
    double prev_mgmt_s = 0.0;
    double prev_stall_s = 0.0;
    double prev_drain_s = 0.0;
    double prev_adopt_s = 0.0;
    uint64_t majflt = 0;
    double cpu_seconds = 0.0;
    long long read_bytes = 0;
    double io_seconds = 0.0;
    double mgmt_seconds = 0.0;
    double stall_seconds = 0.0;
    double drain_seconds = 0.0;
    double adopt_seconds = 0.0;

    // the last token's row, in the documented column order
    double f[24];
    long long i[8];

    void record(bool overlap, double wall, uint64_t faults, double cpu_s, int turn, const IExpertSource::Stats * st,
                size_t page) {
        double out_f[24] = {0};
        int64_t out_i[8] = {0};
        sl_f64 vf{out_f, 24};
        sl_i64 vi{out_i, 8};
        const uint64_t rb = st ? st->read_bytes : 0;
        const double rs = st ? st->read_seconds : 0.0;
        const double mg = st ? st->mgmt_seconds : 0.0;
        const double sl = st ? st->stall_seconds : 0.0;
        const long long ch = st ? st->cache_hits : 0;
        const long long cl = st ? st->cache_lookups : 0;
        const double dr = st ? st->drain_wait_seconds : 0.0;
        const double ad = st ? st->adopt_wait_seconds : 0.0;
        const double df = st ? st->dense_resident_frac : 0.0;
        const uint64_t cb = st ? st->cache_budget_bytes : 0;
        tl_record(overlap ? 1 : 0, prev_bytes, prev_io_s, prev_mgmt_s, prev_stall_s, prev_drain_s, prev_adopt_s,
                  wall, faults, cpu_s, turn, st ? 1 : 0, rb, rs, mg, sl, ch, cl, dr, ad, df, cb,
                  (int64_t) page, majflt, cpu_seconds, read_bytes, io_seconds, mgmt_seconds, stall_seconds,
                  drain_seconds, adopt_seconds, vf, vi);
        for (int k = 0; k < 24; ++k) f[k] = out_f[k];
        for (int k = 0; k < 8; ++k) i[k] = (long long) out_i[k];
        // advance exactly as the native member does
        if (st) {
            prev_bytes = i[5];
            prev_io_s = f[18];
            prev_mgmt_s = f[19];
            prev_stall_s = f[20];
            prev_drain_s = f[21];
            prev_adopt_s = f[22];
            read_bytes = i[4];
            io_seconds = f[12];
            mgmt_seconds = f[13];
            stall_seconds = f[14];
            drain_seconds = f[15];
            adopt_seconds = f[16];
        }
        majflt = (uint64_t) i[3];
        cpu_seconds = f[17];
    }
};

// ── S1: GenTally::record ────────────────────────────────────────────────────

struct Tok {
    bool on = false;
    double wall = 0.0;
    uint64_t faults = 0;
    double cpu = 0.0;
    int turn = 1;
    IExpertSource::Stats st;
};

struct Scen {
    const char * name;
    bool overlap;
    long long prev_bytes;
    double prev_io_s, prev_mgmt_s, prev_stall_s, prev_drain_s, prev_adopt_s;
    std::vector<Tok> toks;
};

IExpertSource::Stats mk(uint64_t rb, double rs, double mg, double sl, long long hits, long long lookups, double dr,
                        double ad, double dense, uint64_t budget) {
    IExpertSource::Stats s;
    s.read_bytes = rb;
    s.read_seconds = rs;
    s.mgmt_seconds = mg;
    s.stall_seconds = sl;
    s.cache_hits = hits;
    s.cache_lookups = lookups;
    s.drain_wait_seconds = dr;
    s.adopt_wait_seconds = ad;
    s.dense_resident_frac = dense;
    s.cache_budget_bytes = budget;
    return s;
}

Tok tok(bool on, double wall, uint64_t faults, double cpu, int turn, IExpertSource::Stats st = IExpertSource::Stats{}) {
    Tok t;
    t.on = on;
    t.wall = wall;
    t.faults = faults;
    t.cpu = cpu;
    t.turn = turn;
    t.st = st;
    return t;
}

void run_scen(const Scen & sc, size_t page) {
    new_case();
    GenTally native;
    native.overlap = sc.overlap;
    native.prev_bytes = sc.prev_bytes;
    native.prev_io_s = sc.prev_io_s;
    native.prev_mgmt_s = sc.prev_mgmt_s;
    native.prev_stall_s = sc.prev_stall_s;
    native.prev_drain_s = sc.prev_drain_s;
    native.prev_adopt_s = sc.prev_adopt_s;

    PortTally port;
    port.prev_bytes = sc.prev_bytes;
    port.prev_io_s = sc.prev_io_s;
    port.prev_mgmt_s = sc.prev_mgmt_s;
    port.prev_stall_s = sc.prev_stall_s;
    port.prev_drain_s = sc.prev_drain_s;
    port.prev_adopt_s = sc.prev_adopt_s;

    for (size_t k = 0; k < sc.toks.size(); ++k) {
        const Tok & t = sc.toks[k];
        const std::string tag = std::string(sc.name) + " tok" + std::to_string(k);
        TokenMetrics m;
        native.record(m, t.wall, t.faults, t.cpu, t.turn, t.on ? &t.st : nullptr);
        port.record(sc.overlap, t.wall, t.faults, t.cpu, t.turn, t.on ? &t.st : nullptr, page);

        note_f(m.wall_ms, port.f[0], tag + " wall_ms");
        note_i((long long) m.majflt, port.i[0], tag + " majflt");
        note_f(m.cpu_ms, port.f[1], tag + " cpu_ms");
        note_f(m.majflt_mib, port.f[2], tag + " majflt_mib");
        note_f(m.dense_resident_frac, port.f[3], tag + " dense_resident_frac");
        note_f(m.cache_budget_mib, port.f[4], tag + " cache_budget_mib");
        note_f(m.io_ms, port.f[5], tag + " io_ms");
        note_f(m.mgmt_ms, port.f[6], tag + " mgmt_ms");
        note_f(m.compute_ms, port.f[7], tag + " compute_ms");
        note_f(m.cache_hit_pct, port.f[8], tag + " cache_hit_pct");
        note_f(m.stall_ms, port.f[9], tag + " stall_ms");
        note_f(m.drain_ms, port.f[10], tag + " drain_ms");
        note_f(m.adopt_ms, port.f[11], tag + " adopt_ms");
        note_i(m.turn, port.i[1], tag + " turn");
        note_i((long long) m.read_bytes, port.i[2], tag + " read_bytes delta");

        note_i((long long) native.majflt, port.i[3], tag + " acc majflt");
        note_i((long long) native.read_bytes, port.i[4], tag + " acc read_bytes");
        note_f(native.io_seconds, port.f[12], tag + " acc io_s");
        note_f(native.mgmt_seconds, port.f[13], tag + " acc mgmt_s");
        note_f(native.stall_seconds, port.f[14], tag + " acc stall_s");
        note_f(native.drain_seconds, port.f[15], tag + " acc drain_s");
        note_f(native.adopt_seconds, port.f[16], tag + " acc adopt_s");
        note_f(native.cpu_seconds, port.f[17], tag + " acc cpu_s");
        note_i(native.prev_bytes, port.i[5], tag + " cursor bytes");
        note_f(native.prev_io_s, port.f[18], tag + " cursor io_s");
        note_f(native.prev_mgmt_s, port.f[19], tag + " cursor mgmt_s");
        note_f(native.prev_stall_s, port.f[20], tag + " cursor stall_s");
        note_f(native.prev_drain_s, port.f[21], tag + " cursor drain_s");
        note_f(native.prev_adopt_s, port.f[22], tag + " cursor adopt_s");
    }
}

void slice_gen(size_t page) {
    std::vector<Scen> scens;

    // 1. dense only: no stats, the residual is the whole decode, the cache is unmeasured
    scens.push_back({"dense-3", false, 0, 0, 0, 0, 0, 0,
                     {tok(false, 0.031, 12, 0.02, 1), tok(false, 0.028, 3, 0.019, 2),
                      tok(false, 0.030, 0, 0.021, 3)}});
    // 2. serial, cumulative counters: every figure a delta
    scens.push_back({"serial-deltas", false, 0, 0, 0, 0, 0, 0,
                     {tok(true, 0.050, 5, 0.04, 1, mk(1u << 20, 0.010, 0.002, 0.0, 50, 100, 0.001, 0.0005, 0.9, 64u << 20)),
                      tok(true, 0.048, 4, 0.039, 2, mk(3u << 20, 0.025, 0.005, 0.0, 150, 200, 0.002, 0.001, 0.88, 64u << 20)),
                      tok(true, 0.041, 0, 0.033, 3, mk(4u << 20, 0.031, 0.007, 0.0, 200, 250, 0.003, 0.002, 0.85, 64u << 20))}});
    // 3. overlap: stall subtracts from the wall, io does not
    scens.push_back({"overlap-stall", true, 0, 0, 0, 0, 0, 0,
                     {tok(true, 0.060, 2, 0.05, 1, mk(2u << 20, 0.020, 0.003, 0.012, 80, 100, 0.0, 0.0, 0.95, 128u << 20)),
                      tok(true, 0.055, 1, 0.045, 2, mk(3u << 20, 0.030, 0.006, 0.020, 120, 160, 0.0, 0.0, 0.94, 128u << 20))}});
    // 4. serial clamp: read time exceeds the wall, compute goes to 0 not negative
    scens.push_back({"clamp-serial", false, 0, 0, 0, 0, 0, 0,
                     {tok(true, 0.010, 0, 0.008, 1, mk(1u << 20, 0.030, 0.004, 0.0, 10, 10, 0.0, 0.0, 0.5, 8u << 20))}});
    // 5. overlap clamp: stall exceeds the wall
    scens.push_back({"clamp-overlap", true, 0, 0, 0, 0, 0, 0,
                     {tok(true, 0.010, 0, 0.008, 1, mk(1u << 20, 0.002, 0.004, 0.020, 10, 10, 0.0, 0.0, 0.5, 8u << 20))}});
    // 6. no cache: 0 lookups gives the -1.0 sentinel, not a 0% hit rate
    scens.push_back({"no-cache", false, 0, 0, 0, 0, 0, 0,
                     {tok(true, 0.025, 0, 0.02, 1, mk(1u << 19, 0.004, 0.001, 0.0, 0, 0, 0.0, 0.0, 0.0, 0))}});
    // 7. half cache, 8. full cache
    scens.push_back({"half-cache", false, 0, 0, 0, 0, 0, 0,
                     {tok(true, 0.025, 0, 0.02, 1, mk(1u << 19, 0.004, 0.001, 0.0, 50, 100, 0.0, 0.0, 0.0, 0))}});
    scens.push_back({"full-cache", false, 0, 0, 0, 0, 0, 0,
                     {tok(true, 0.025, 0, 0.02, 1, mk(1u << 19, 0.0, 0.0, 0.0, 7, 7, 0.0, 0.0, 1.0, 0))}});
    // 9. drain and adopt waits
    scens.push_back({"drain-adopt", false, 0, 0, 0, 0, 0, 0,
                     {tok(true, 0.033, 0, 0.03, 1, mk(1u << 20, 0.003, 0.002, 0.0, 3, 9, 0.0015, 0.00025, 0.77, 1u << 20)),
                      tok(true, 0.031, 0, 0.028, 2, mk(1u << 20, 0.004, 0.003, 0.0, 6, 12, 0.0025, 0.00075, 0.76, 1u << 20))}});
    // 10. the counters go backwards: the delta is negative and the total follows it
    scens.push_back({"counter-went-back", false, 5u << 20, 0.05, 0.01, 0, 0, 0,
                     {tok(true, 0.02, 0, 0.015, 1, mk(2u << 20, 0.02, 0.004, 0.0, 5, 20, 0.0, 0.0, 0.3, 1u << 20))}});
    // 11. large byte counts, still under 2^53 so the double columns are exact
    scens.push_back({"large-bytes", false, 0, 0, 0, 0, 0, 0,
                     {tok(true, 0.400, 1000, 0.35, 1, mk(1ull << 40, 0.30, 0.02, 0.0, 1000, 4000, 0.0, 0.0, 0.99, 4ull << 30)),
                      tok(true, 0.380, 900, 0.33, 2, mk(3ull << 40, 0.60, 0.05, 0.0, 3000, 6000, 0.0, 0.0, 0.98, 4ull << 30))}});
    // 12. zero wall with a null stats pointer
    scens.push_back({"zero-wall", false, 0, 0, 0, 0, 0, 0, {tok(false, 0.0, 0, 0.0, 1)}});
    // 13. a dense token between two streaming ones: the cursors must not move
    scens.push_back({"dense-between", false, 0, 0, 0, 0, 0, 0,
                     {tok(true, 0.04, 1, 0.03, 1, mk(1u << 20, 0.01, 0.001, 0.0, 10, 20, 0.0, 0.0, 0.5, 1u << 20)),
                      tok(false, 0.02, 0, 0.015, 2),
                      tok(true, 0.045, 2, 0.035, 3, mk(4u << 20, 0.03, 0.003, 0.0, 40, 60, 0.0, 0.0, 0.6, 1u << 20))}});
    // 14. a million faults in one token
    scens.push_back({"huge-faults", false, 0, 0, 0, 0, 0, 0,
                     {tok(false, 1.2, 1000000, 1.0, 1), tok(false, 1.1, 999999, 0.9, 2)}});
    // 15. overlap with a zero stall delta and a nonzero mgmt delta
    scens.push_back({"overlap-no-stall", true, 0, 0, 0, 0, 0, 0,
                     {tok(true, 0.05, 0, 0.04, 1, mk(1u << 20, 0.01, 0.004, 0.0, 20, 40, 0.0, 0.0, 0.9, 1u << 20)),
                      tok(true, 0.05, 0, 0.04, 2, mk(2u << 20, 0.02, 0.008, 0.0, 40, 80, 0.0, 0.0, 0.9, 1u << 20))}});

    for (const Scen & sc : scens) run_scen(sc, page);
}

// ── S1b: PrefillTally ───────────────────────────────────────────────────────

struct FakeSource : IExpertSource {
    Stats s;
    Stats stats() const override { return s; }
};

void run_prefill(bool moe_on, double cpu0, double cpu_now, IExpertSource::Stats pre, IExpertSource::Stats post) {
    new_case();
    FakeSource src;
    PrefillTally native;
    g_cpu_clock = cpu0;
    src.s = pre;
    native.begin(moe_on, src);
    src.s = post;
    g_cpu_clock = cpu_now;
    native.end(moe_on, src);

    double out[8] = {0};
    sl_f64 vf{out, 8};
    tl_prefill(moe_on ? 1 : 0, cpu0, cpu_now, pre.read_bytes, pre.read_seconds, pre.mgmt_seconds, pre.stall_seconds,
               post.read_bytes, post.read_seconds, post.mgmt_seconds, post.stall_seconds, vf);

    const std::string tag = std::string("prefill(moe=") + (moe_on ? "1" : "0") + ")";
    note_f(native.cpu_seconds, out[0], tag + " cpu_seconds");
    note_f(native.read_mib, out[1], tag + " read_mib");
    note_f(native.io_seconds, out[2], tag + " io_seconds");
    note_f(native.stall_seconds, out[3], tag + " stall_seconds");
    note_f(native.mgmt_seconds, out[4], tag + " mgmt_seconds");
}

void slice_prefill() {
    IExpertSource::Stats a = mk(0, 0, 0, 0, 0, 0, 0, 0, -1.0, 0);
    IExpertSource::Stats b = mk(3u << 20, 0.020, 0.004, 0.010, 0, 0, 0.001, 0.0, 0.5, 1u << 20);
    run_prefill(true, 10.5, 12.25, a, b);
    run_prefill(true, 0.0, 0.0, a, a);
    run_prefill(false, 3.0, 4.5, a, b);   // moe off: counters ignored, cpu still measured
    run_prefill(false, 3.0, 3.0, a, a);
    run_prefill(true, 1.0, 1.0, b, a);    // the counters went backwards over the turn
    run_prefill(true, 0.0, 100.0, a, mk(1ull << 40, 1.5, 0.25, 0.5, 0, 0, 0, 0, 1.0, 0));
}

// ── S1c: the summary ────────────────────────────────────────────────────────

void run_summary(bool moe_on, int n_gen, bool overlap, double read_bytes, double io_s, double mgmt_s, double stall_s,
                 double drain_s, double adopt_s, double s_per_token, double majflt_per_gen, double cpu_per_gen) {
    new_case();
    GenTally tally;
    tally.overlap = overlap;
    tally.majflt = (uint64_t) (majflt_per_gen * n_gen);
    tally.cpu_seconds = cpu_per_gen * n_gen;
    tally.read_bytes = (uint64_t) read_bytes;
    tally.io_seconds = io_s;
    tally.mgmt_seconds = mgmt_s;
    tally.stall_seconds = stall_s;
    tally.drain_seconds = drain_s;
    tally.adopt_seconds = adopt_s;

    SumStub s;
    s.s_per_token = s_per_token;
    native_summary(s, tally, n_gen, moe_on, overlap);

    double out[12] = {0};
    sl_f64 vf{out, 12};
    tl_summary(moe_on ? 1 : 0, n_gen, tally.majflt, tally.cpu_seconds, (long long) tally.read_bytes, io_s, mgmt_s,
               stall_s, drain_s, adopt_s, s_per_token, overlap ? 1 : 0, vf);

    char tag[96];
    std::snprintf(tag, sizeof tag, "summary(moe=%d n_gen=%d overlap=%d)", moe_on ? 1 : 0, n_gen, overlap ? 1 : 0);
    note_f(s.majflt_per_token, out[0], std::string(tag) + " majflt_per_token");
    note_f(s.cpu_s_per_token, out[1], std::string(tag) + " cpu_s_per_token");
    note_f(s.moe_read_mib, out[2], std::string(tag) + " moe_read_mib");
    note_f(s.moe_io_s_per_token, out[3], std::string(tag) + " moe_io_s_per_token");
    note_f(s.moe_mgmt_s_per_token, out[4], std::string(tag) + " moe_mgmt_s_per_token");
    note_f(s.moe_stall_s_per_token, out[5], std::string(tag) + " moe_stall_s_per_token");
    note_f(s.moe_compute_s_per_token, out[6], std::string(tag) + " moe_compute_s_per_token");
    note_f(s.moe_drain_s_per_token, out[7], std::string(tag) + " moe_drain_s_per_token");
    note_f(s.moe_adopt_s_per_token, out[8], std::string(tag) + " moe_adopt_s_per_token");
}

void slice_summary() {
    run_summary(true, 32, false, 64.0 * 1024 * 1024, 0.5, 0.02, 0.0, 0.004, 0.001, 0.05, 3.5, 0.2);
    run_summary(true, 32, true, 64.0 * 1024 * 1024, 0.5, 0.02, 0.3, 0.004, 0.001, 0.05, 3.5, 0.2);
    run_summary(true, 1, false, 0, 0, 0, 0, 0, 0, 0.01, 0, 0);
    run_summary(true, 0, false, 123, 0.5, 0.02, 0.0, 0.004, 0.001, 0.0, 3.5, 0.2);
    run_summary(false, 16, false, 0, 0, 0, 0, 0, 0, 0.04, 1.0, 0.1);
    run_summary(true, 8, false, 1u << 20, 9.0, 9.0, 0.0, 0.0, 0.0, 0.01, 0, 0); // compute clamps to 0
    run_summary(true, 8, true, 1u << 20, 0.0, 0.0, 9.0, 0.0, 0.0, 0.01, 0, 0);  // stall arm clamps to 0
}

} // namespace

int main(int argc, char ** argv) {
    goose_init(argc, argv);
    g_page = (size_t) sysconf(_SC_PAGESIZE);
    reset("S1");
    if (g_page != 4096) std::printf("  note: host page size is %zu, fault_bytes stub follows it\n", g_page);
    slice_gen(g_page);
    slice_prefill();
    slice_summary();
    std::printf("FG649_TL cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails;
}
