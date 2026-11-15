// Differential gate for P10-12 S3: the trace framing order.
//
// The oracle is the carrier's two lambdas: tests/native_extract.py copies
// trace_begin and trace_flush (session.cpp:1485-1530) verbatim, and this
// fixture instantiates them against a recording hook, recording sinks and a
// recording I/O source. The port returns the same sequence as a plan; the gate
// compares the two event lists op by op, argument by argument.
//
// The event vocabulary is the module's (README.md, source/trace-frame-order.goose):
//    1 SET_BATCH_PHASE            2 BEGIN_DETERMINISTIC
//    3 BEGIN_TRACE_BATCH          4 BEGIN_COMPUTE_BATCH
//    5 END_COMPUTE_BATCH          6 END_TRACE_BATCH
//    7 ROUTE_ROWS                 8 COMPUTE_ROWS
//    9 IO_TAKE_ROWS              10 IO_STAMP_ROWS        11 IO_ROWS
//
// What the fixture adds is observation only: the hook records the call order,
// the I/O sink records the stamp that the flush's own loop wrote into the rows
// (turn/phase/step) before handing them over, and the source records the take.
// Nothing about the ordering decision is re-expressed here.
//
// Checks are explicit (no <cassert>): NDEBUG is defined in Release builds.

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace bmoe {
struct RouteTraceRow {
    long long turn = 0, phase = 0, step = 0, layer = 0;
};
struct ComputeTraceRow {
    long long turn = 0, phase = 0, step = 0;
};
struct IoTraceRow {
    long long turn = 0, phase = 0, step = 0, layer = 0;
};
} // namespace bmoe
using namespace bmoe;

#include "tr.h" // tr_begin, tr_stamp, tr_flush, goose_init

namespace {

struct Event {
    long long op = 0;
    long long a0 = 0, a1 = 0, a2 = 0, a3 = 0;
    bool operator==(const Event & o) const {
        return op == o.op && a0 == o.a0 && a1 == o.a1 && a2 == o.a2 && a3 == o.a3;
    }
};

struct Recorder {
    std::vector<Event> * ev = nullptr;
    void push(int op, long long a0, long long a1 = 0, long long a2 = 0, long long a3 = 0) {
        ev->push_back(Event{op, a0, a1, a2, a3});
    }
};

struct FakeHook : Recorder {
    std::vector<RouteTraceRow> rt;
    std::vector<ComputeTraceRow> ct;
    void set_batch_phase(int phase) { push(1, phase); }
    void begin_deterministic_batch(int base, int n, int phase) { push(2, base, n, phase); }
    void begin_trace_batch(int base, int n, int phase, int turn) { push(3, base, n, phase, turn); }
    void begin_compute_batch(int last, int phase, int turn) { push(4, last, phase, turn); }
    void end_compute_batch() { push(5, 0); }
    void end_trace_batch() { push(6, 0); }
    std::vector<RouteTraceRow> & trace_rows() { return rt; }
    std::vector<ComputeTraceRow> & compute_rows() { return ct; }
};

struct SinkR : Recorder {
    void on_rows(RouteTraceRow * rows, std::size_t n) { push(7, (long long) n); }
};
struct SinkC : Recorder {
    void on_rows(ComputeTraceRow * rows, std::size_t n) { push(8, (long long) n); }
};
// The I/O rows arrive already stamped by trace_flush's own loop; the stamp is
// recorded from the row it wrote, so a wrong stamp is visible in the event.
struct SinkI : Recorder {
    void on_rows(IoTraceRow * rows, std::size_t n) {
        if (n > 0) push(10, (long long) n, rows[0].turn, rows[0].phase, rows[0].step);
        push(11, (long long) n);
    }
};
struct FakeSource : Recorder {
    std::vector<IoTraceRow> io;
    void take_io_trace_rows(std::vector<IoTraceRow> & out) {
        push(9, (long long) io.size());
        out.insert(out.end(), io.begin(), io.end());
    }
};

struct FakeIm {
    FakeHook * hook = nullptr;
    SinkR * route_trace = nullptr;
    SinkC * compute_trace = nullptr;
    SinkI * io_trace = nullptr;
    FakeSource source;
    std::vector<IoTraceRow> io_rows_scratch;
    int turn = 0;
};

int g_cases = 0, g_values = 0, g_fails = 0;

void note(bool ok, const std::string & what) {
    ++g_values;
    if (!ok) {
        ++g_fails;
        std::printf("  FAIL [S3] %s\n", what.c_str());
    }
}

std::string ev_str(const Event & e) {
    char b[128];
    std::snprintf(b, sizeof b, "op%lld(%lld,%lld,%lld,%lld)", e.op, e.a0, e.a1, e.a2, e.a3);
    return b;
}

void run_case(bool route, bool compute, bool io, int base, int n, int phase, int turn, int rr, int cr, int ior) {
    ++g_cases;
    std::vector<Event> native_ev, port_ev;

    FakeHook hook;
    SinkR sr;
    SinkC sc;
    SinkI si;
    FakeIm im;
    hook.ev = &native_ev;
    sr.ev = &native_ev;
    sc.ev = &native_ev;
    si.ev = &native_ev;
    im.hook = &hook;
    im.route_trace = route ? &sr : nullptr;
    im.compute_trace = compute ? &sc : nullptr;
    im.io_trace = io ? &si : nullptr;
    im.source.ev = &native_ev;
    im.turn = turn;
    hook.rt.assign((size_t) rr, RouteTraceRow{});
    hook.ct.assign((size_t) cr, ComputeTraceRow{});
    im.source.io.assign((size_t) ior, IoTraceRow{});

    int trace_phase = 0, trace_step = 0;
    (void) trace_phase;
    (void) trace_step;
    // The carrier's own lambdas, verbatim.
#include "native_trace_begin.inc"
#include "native_trace_flush.inc"

    trace_begin(base, n, phase);
    trace_flush();

    // The port's plan, driven with the same frame.
    int64_t out[64] = {0};
    sl_i64 v{out, 64};
    std::vector<Event> plan;
    int64_t cnt = tr_begin(route ? 1 : 0, compute ? 1 : 0, base, n, phase, turn, v);
    for (int64_t k = 0; k < cnt; ++k)
        plan.push_back(Event{out[5 * k], out[5 * k + 1], out[5 * k + 2], out[5 * k + 3], out[5 * k + 4]});

    int64_t st[2] = {0, 0};
    sl_i64 vs{st, 2};
    tr_stamp(base, n, phase, vs);

    char tag[160];
    std::snprintf(tag, sizeof tag, "frame(route=%d compute=%d io=%d base=%d n=%d phase=%d turn=%d rows=%d/%d/%d)",
                  route ? 1 : 0, compute ? 1 : 0, io ? 1 : 0, base, n, phase, turn, rr, cr, ior);
    const std::string t = tag;

    note(st[0] == trace_phase, t + " stamp phase");
    note(st[1] == trace_step, t + " stamp step");

    int64_t cnt2 = tr_flush(route ? 1 : 0, compute ? 1 : 0, io ? 1 : 0, rr, cr, ior, st[0], st[1], turn, v);
    for (int64_t k = 0; k < cnt2; ++k)
        plan.push_back(Event{out[5 * k], out[5 * k + 1], out[5 * k + 2], out[5 * k + 3], out[5 * k + 4]});

    // The two event lists must agree in length and in every argument.
    note(plan.size() == native_ev.size(), t + " event count native=" + std::to_string(native_ev.size()) +
                                               " port=" + std::to_string(plan.size()));
    const size_t common = plan.size() < native_ev.size() ? plan.size() : native_ev.size();
    for (size_t k = 0; k < common; ++k) {
        if (!(plan[k] == native_ev[k]))
            std::printf("  [S3] %s event[%zu] native=%s port=%s\n", t.c_str(), k, ev_str(native_ev[k]).c_str(),
                        ev_str(plan[k]).c_str());
        note(plan[k] == native_ev[k], t + " event[" + std::to_string(k) + "]");
    }

    // The framing decision is only useful if the hook's own phase wiring saw
    // the drop policy told first; that ordering is event[0] == SET_BATCH_PHASE.
    if (!native_ev.empty()) note(native_ev[0].op == 1, t + " phase is set before any trace call");
    // And the compute interval must close before any row leaves.
    int first_rows = -1;
    for (size_t k = 0; k < native_ev.size(); ++k)
        if (native_ev[k].op >= 7) {
            first_rows = (int) k;
            break;
        }
    if (first_rows >= 0 && compute) {
        bool closed_first = false;
        for (int k = 0; k < first_rows; ++k)
            if (native_ev[k].op == 5) closed_first = true;
        note(closed_first, t + " compute interval closes before the first row");
    }
}

} // namespace

int main(int argc, char ** argv) {
    goose_init(argc, argv);

    // Every combination of the three sinks, plus the empty-frame cases the
    // carrier tests with rows.empty().
    for (int mask = 0; mask < 8; ++mask) {
        const bool route = (mask & 1) != 0, compute = (mask & 2) != 0, io = (mask & 4) != 0;
        run_case(route, compute, io, 0, 1, 1, 1, 4, 4, 3);      // a decode frame
        run_case(route, compute, io, 128, 32, 0, 5, 0, 0, 0);   // an empty prefill chunk
        run_case(route, compute, io, 4096, 512, 0, 9, 64, 1, 200);
        run_case(route, compute, io, 7, 1, 1, 2, 0, 3, 0);      // route empty, compute not
        run_case(route, compute, io, 7, 1, 1, 2, 3, 0, 4);      // compute empty, route not
    }
    // A single token at a huge position, and a zero-length batch (which the
    // batch filler never produces but the framing arithmetic still defines).
    run_case(true, true, true, 1000000, 1, 1, 77, 1, 1, 1);
    run_case(true, true, true, 0, 0, 0, 3, 0, 0, 0);

    std::printf("FG649_TR cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails;
}
