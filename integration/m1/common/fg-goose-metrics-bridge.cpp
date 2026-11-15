// Staged S1.4: the bridge between the sinks and the Goose metrics/trace encoders.
//
// ON build: forwards to goose-gen/metrics-traces.c (generated with goose --standalone from
// goose-gen/metrics-traces.composed.goose, itself composed from
// goose/metrics/metrics-traces.goose). OFF build: only the counter accessor is defined.
//
// The buffer policy is the host's own: the module never truncates and reports the length it
// would have written, so the bridge sizes the buffer it wants and grows it once if the
// module asks for more. No output can be cut off at a fixed 4096-byte boundary.
#include "fg-goose-metrics-bridge.h"
#include "goose-host.h" // fg_goose_host_ensure_init

#ifdef FG_GOOSE_METRICS

#include "metrics-traces.h" // sl_u8 and the mt_* exports

#include <cstdio>
#include <mutex>

namespace {

int64_t g_calls = 0;

// The module's runtime has no globals of its own that need arguments; one process-wide
// init is enough. call_once keeps it safe if a sink is ever entered from two threads.
void ensure_init() {
    // M1: one start-up for the whole runtime. The per-stage guard of the
    // accepted card would be a tenth pthread_once in this link.
    fg_goose_host_ensure_init();
}

sl_u8 as_in(const std::string & s) {
    return sl_u8{reinterpret_cast<uint8_t *>(const_cast<char *>(s.data())), static_cast<int64_t>(s.size())};
}

sl_u8 as_out(std::string & s) {
    return sl_u8{reinterpret_cast<uint8_t *>(&s[0]), static_cast<int64_t>(s.size())};
}

// Runs one encoder, then re-runs it once with the exact length the module reported if the
// first buffer was too small. The retry cannot loop: the module's length is deterministic.
template <typename F>
std::string encode(F f, size_t initial) {
    ensure_init();
    std::string buf(initial != 0 ? initial : 1, '\0');
    int64_t n = f(buf);
    while (n > static_cast<int64_t>(buf.size())) {
        buf.assign(static_cast<size_t>(n), '\0');
        n = f(buf);
    }
    ++g_calls;
    if (n < 0) {
        n = 0;
    }
    buf.resize(static_cast<size_t>(n));
    return buf;
}

} // namespace

// The one conversion the module delegates to libc ("%.6g" at a column offset)
// is a host hook shared by every stage, so in M1 it lives in host/goose_host.c
// and not here: a second copy is a duplicate definition at link time.

namespace fgmt {

std::string field_encode(const std::string & v) {
    return encode(
        [&](std::string & buf) {
            return mt_field_encode(as_in(v), as_out(buf));
        },
        v.size() * 2 + 3); // every byte doubled, plus the two quotes and slack
}

std::string compute_row(int turn, int phase, int step, int seq, int layer, const std::string & op,
                        const std::string & name, uint64_t wall_ns, uint64_t majflt) {
    return encode(
        [&](std::string & buf) {
            return mt_compute_row_encode(turn, phase, step, seq, layer, as_in(op), as_in(name),
                                         static_cast<int64_t>(wall_ns), static_cast<int64_t>(majflt), as_out(buf));
        },
        op.size() * 2 + name.size() * 2 + 240);
}

std::string io_row(int turn, int phase, int step, int layer, int32_t expert, int8_t proj, int8_t lane, uint8_t spec,
                   uint64_t offset, uint64_t req_bytes, uint64_t read_bytes, uint64_t latency_ns) {
    return encode(
        [&](std::string & buf) {
            return mt_io_row_encode(turn, phase, step, layer, expert, proj, lane, spec, static_cast<int64_t>(offset),
                                    static_cast<int64_t>(req_bytes), static_cast<int64_t>(read_bytes),
                                    static_cast<int64_t>(latency_ns), as_out(buf));
        },
        320);
}

std::string route_row(int turn, int phase, int step, int layer, int slot, int32_t expert, double weight,
                      uint8_t residency, uint64_t expert_bytes, uint8_t dropped) {
    return encode(
        [&](std::string & buf) {
            return mt_route_row_encode(turn, phase, step, layer, slot, expert, weight, residency,
                                       static_cast<int64_t>(expert_bytes), dropped, as_out(buf));
        },
        320);
}

int64_t calls() {
    return g_calls;
}

} // namespace fgmt

#else // !FG_GOOSE_METRICS

namespace fgmt {

// OFF build: the native bodies run, the module is not compiled, nothing can be entered.
int64_t calls() {
    return 0;
}

} // namespace fgmt

#endif // FG_GOOSE_METRICS
