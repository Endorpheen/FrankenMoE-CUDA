// S1.4 staged gate: metrics/trace text encoders behind FG_GOOSE_METRICS.
//
// The binary is built once per configuration and drives the real sinks through their public
// factories (bmoe::make_csv_compute_trace_sink and bmoe::make_csv_io_trace_sink from
// decode_trace_sink.cpp, bmoe::make_csv_route_trace_sink from route_trace_sink.cpp). For
// every case it writes a file, reads it back and compares the whole file — preamble, header
// and rows — with the bytes the pristine carrier would have written.
//
// The oracle text is the fixture's transcription of the native format (the same technique
// EXP-617 used). It is proved faithful, not assumed: the same binary linked against the
// pristine reference sources (reference/native/metrics) must pass every case, and the three
// per-case output directories must be identical byte for byte.
//
// OFF build : the native bodies run; the goose counter is absent (FG_METRICS_NO_COUNTER is
//             only set for the reference binary) and the selected-path figure is 0.
// ON build  : the Goose encoders run; the counter counts every bridge call.
//
// Checks are explicit (no <cassert>): NDEBUG is defined in Release builds.

#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "bmoe/decode_trace.h"
#include "bmoe/route_trace.h"

#ifndef FG_METRICS_NO_COUNTER
#include "fg-goose-metrics-bridge.h"
#endif

using namespace bmoe;

namespace {

int g_cases = 0;
int g_values = 0;
int g_fails = 0;
const char * g_slice = "S?";

void reset(const char * slice) {
    g_slice = slice;
    g_cases = g_values = g_fails = 0;
}

void note(bool ok, const std::string & what) {
    ++g_values;
    if (!ok) {
        ++g_fails;
        std::printf("  FAIL [%s] %s\n", g_slice, what.c_str());
    }
}

int report() {
    std::printf("FG633_%s cases=%d values=%d failures=%d\n", g_slice, g_cases, g_values, g_fails);
    return g_fails;
}

std::string ap(const char * f, ...) {
    va_list a1;
    va_start(a1, f);
    va_list a2;
    va_copy(a2, a1);
    const int n = std::vsnprintf(nullptr, 0, f, a1);
    va_end(a1);
    std::vector<char> buf(n > 0 ? (size_t) n + 1 : 1, '\0');
    if (n > 0) {
        std::vsnprintf(buf.data(), buf.size(), f, a2);
    }
    va_end(a2);
    return std::string(buf.data(), n > 0 ? (size_t) n : 0);
}

std::string slurp(const std::string & path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void spill(const std::string & path, const std::string & bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), (std::streamsize) bytes.size());
}

// ── the oracle: a transcription of the native format ──────────────────────────────────
//
// decode_trace_sink.cpp write_csv_field (19-30): quote iff ',' '"' '\n'; inside a quoted
// field a '"' is doubled and a '\n' becomes one space. '\r' travels raw.
std::string n_field(const std::string & v) {
    if (v.find_first_of(",\"\n") == std::string::npos) {
        return v;
    }
    std::string o;
    o.push_back('"');
    for (char c : v) {
        if (c == '"') {
            o.push_back('"');
        }
        o.push_back(c == '\n' ? ' ' : c);
    }
    o.push_back('"');
    return o;
}

std::string n_compute_static(const DecodeTraceStatic & s) {
    return ap("# compute_trace v1\n") +
           ap("# model=%s arch=%s n_layer=%d n_threads=%d io_threads=%d o_direct=%d overlap=%d\n", s.model.c_str(),
              s.arch.c_str(), s.n_layer, s.n_threads, s.io_threads, (int) s.o_direct, (int) s.overlap) +
           "turn,phase,step,seq,layer,op,name,wall_ns,majflt\n";
}

// decode_trace_sink.cpp:45-55, the four native statements in their native order.
std::string n_compute_row(const ComputeTraceRow & r) {
    return ap("%d,%d,%d,%d,%d,", r.turn, r.phase, r.step, r.seq, r.layer) + n_field(r.op) + "," + n_field(r.name) +
           ap(",%llu,%llu\n", (unsigned long long) r.wall_ns, (unsigned long long) r.majflt);
}

std::string n_io_static(const DecodeTraceStatic & s) {
    return ap("# io_trace v1\n") +
           ap("# model=%s arch=%s n_layer=%d n_threads=%d io_threads=%d o_direct=%d overlap=%d\n", s.model.c_str(),
              s.arch.c_str(), s.n_layer, s.n_threads, s.io_threads, (int) s.o_direct, (int) s.overlap) +
           "turn,phase,step,layer,expert,proj,lane,spec,offset,req_bytes,read_bytes,latency_ns\n";
}

// decode_trace_sink.cpp:74-83.
std::string n_io_row(const IoTraceRow & r) {
    return ap("%d,%d,%d,%d,%d,%d,%d,%u,%llu,%llu,%llu,%llu\n", r.turn, r.phase, r.step, r.layer, (int) r.expert,
              (int) r.proj, (int) r.lane, (unsigned) r.spec, (unsigned long long) r.offset,
              (unsigned long long) r.req_bytes, (unsigned long long) r.read_bytes,
              (unsigned long long) r.latency_ns);
}

std::string n_route_static(const RouteTraceStatic & s) {
    std::string o = ap("# route_trace v1\n") +
                    ap("# model=%s arch=%s n_layer=%d n_expert=%d n_expert_used=%d\n", s.model.c_str(), s.arch.c_str(),
                       s.n_layer, s.n_expert, s.n_expert_used);
    for (size_t il = 0; il < s.expert_bytes_per_layer.size(); ++il) {
        const uint64_t dense = il < s.dense_bytes_per_layer.size() ? s.dense_bytes_per_layer[il] : 0;
        o += ap("# layer=%zu expert_bytes=%llu dense_bytes=%llu\n", il,
                (unsigned long long) s.expert_bytes_per_layer[il], (unsigned long long) dense);
    }
    o += "turn,phase,step,layer,slot,expert,weight,residency,expert_bytes,dropped\n";
    return o;
}

// route_trace_sink.cpp:33-48: the NaN arm is the literal `nan`, everything else "%.6g" of
// the value widened from float.
std::string n_route_row(const RouteTraceRow & r) {
    if (std::isnan(r.weight)) {
        return ap("%d,%d,%d,%d,%d,%d,nan,%u,%llu,%u\n", r.turn, r.phase, r.step, r.layer, r.slot, (int) r.expert,
                  (unsigned) r.residency, (unsigned long long) r.expert_bytes, (unsigned) r.dropped);
    }
    return ap("%d,%d,%d,%d,%d,%d,%.6g,%u,%llu,%u\n", r.turn, r.phase, r.step, r.layer, r.slot, (int) r.expert,
              (double) r.weight, (unsigned) r.residency, (unsigned long long) r.expert_bytes, (unsigned) r.dropped);
}

// ── driving the real sinks ────────────────────────────────────────────────────────────

std::string run_compute(const std::string & path, const DecodeTraceStatic & st,
                        const std::vector<ComputeTraceRow> & rows) {
    std::unique_ptr<IComputeTraceSink> sink(make_csv_compute_trace_sink(path));
    if (!sink) {
        std::printf("  cannot open %s\n", path.c_str());
        return std::string();
    }
    sink->on_static(st);
    if (!rows.empty()) {
        sink->on_rows(rows.data(), rows.size());
    }
    sink.reset(); // closes the file
    return slurp(path);
}

std::string run_io(const std::string & path, const DecodeTraceStatic & st, const std::vector<IoTraceRow> & rows) {
    std::unique_ptr<IIoTraceSink> sink(make_csv_io_trace_sink(path));
    if (!sink) {
        std::printf("  cannot open %s\n", path.c_str());
        return std::string();
    }
    sink->on_static(st);
    if (!rows.empty()) {
        sink->on_rows(rows.data(), rows.size());
    }
    sink.reset();
    return slurp(path);
}

std::string run_route(const std::string & path, const RouteTraceStatic & st, const std::vector<RouteTraceRow> & rows) {
    std::unique_ptr<IRouteTraceSink> sink(make_csv_route_trace_sink(path));
    if (!sink) {
        std::printf("  cannot open %s\n", path.c_str());
        return std::string();
    }
    sink->on_static(st);
    if (!rows.empty()) {
        sink->on_rows(rows.data(), rows.size());
    }
    sink.reset();
    return slurp(path);
}

// Compares a written file line by line: the preamble must come first, then one exact line
// per expected row, then nothing. One case per row.
void check_rows(const char * what, const std::string & got, const std::string & preamble,
                const std::vector<std::string> & want_rows) {
    if (got.size() < preamble.size()) {
        // The file could not be written (missing output directory, full disk): one explicit
        // failure rather than a crash in the line walk below.
        ++g_cases;
        note(false, ap("%s: %zu bytes written, preamble alone needs %zu", what, got.size(), preamble.size()));
        return;
    }
    note(got.compare(0, preamble.size(), preamble) == 0, ap("%s: preamble differs", what));
    const std::string rest = got.substr(preamble.size());
    size_t pos = 0;
    for (size_t i = 0; i < want_rows.size(); ++i) {
        ++g_cases;
        const size_t nl = rest.find('\n', pos);
        const std::string line =
            nl == std::string::npos ? rest.substr(pos) : rest.substr(pos, nl - pos + 1);
        note(line == want_rows[i], ap("%s row %zu differs", what, i));
        pos = nl == std::string::npos ? rest.size() : nl + 1;
    }
    note(pos == rest.size(), ap("%s: %zu trailing bytes", what, rest.size() - pos));
}

DecodeTraceStatic decode_static_fixture() {
    DecodeTraceStatic s;
    s.model = "/models/apex-35b-a3b-Q4_K_M.gguf";
    s.arch = "qwen3moe";
    s.n_layer = 48;
    s.n_threads = 8;
    s.io_threads = 16;
    s.o_direct = true;
    s.overlap = true;
    return s;
}

RouteTraceStatic route_static_fixture() {
    RouteTraceStatic s;
    s.model = "/models/apex-35b-a3b-Q4_K_M.gguf";
    s.arch = "qwen3moe";
    s.n_layer = 48;
    s.n_expert = 128;
    s.n_expert_used = 8;
    s.expert_bytes_per_layer.assign(3, 2359296ULL);
    s.dense_bytes_per_layer.assign(3, 1048576ULL);
    return s;
}

// ── S1: the CSV field ────────────────────────────────────────────────────────────────
//
// Fields can only reach the encoder through a row: in the ON build the field text is
// produced inside mt_compute_row_encode (the same mt_field_into the standalone export
// wraps), so each field is emitted as the `op` column of its own one-row file. A whole-line
// byte compare therefore covers the field encoder, the separators and the row terminator.
void slice_fields(const std::string & outdir) {
    reset("S1");
    const DecodeTraceStatic st = decode_static_fixture();
    const std::vector<std::string> fields = {
        "",
        "plain",
        "blk.0.ffn_up-0",
        "a,b",
        "say \"hi\"",
        "line\nbreak",
        "cr\rhere",
        "\"\"",
        ",",
        "\"",
        "\n",
        "a\r\nb",
        "x,\"y\"\nz",
        std::string("\xff\x80\x01", 3),          // raw non-UTF-8 bytes
        std::string(3000, 'n'),                  // longer than any fixed stack buffer
        std::string(600, ','),                   // 600 commas: quoted, never truncated
    };
    for (size_t i = 0; i < fields.size(); ++i) {
        ComputeTraceRow r;
        r.turn = 0;
        r.phase = 1;
        r.step = 7;
        r.seq = (int) i;
        r.layer = 12;
        r.op = fields[i];
        r.name = "field";
        r.wall_ns = 1000 + i;
        r.majflt = i;
        char name[64];
        std::snprintf(name, sizeof name, "S1-%02zu.csv", i);
        const std::string got = run_compute(outdir + "/" + name, st, {r});
        const std::string label = ap("field %zu (%zu bytes)", i, fields[i].size());
        check_rows(label.c_str(), got, n_compute_static(st), {n_compute_row(r)});
    }
}

// ── S2: compute rows ─────────────────────────────────────────────────────────────────
void slice_compute(const std::string & outdir) {
    reset("S2");
    const DecodeTraceStatic st = decode_static_fixture();
    std::vector<ComputeTraceRow> rows(12);
    for (size_t i = 0; i < rows.size(); ++i) {
        ComputeTraceRow & r = rows[i];
        r.turn = (int) i % 3;
        r.phase = (int) i % 2;
        r.step = (int) (i * 37);
        r.seq = (int) i;
        r.layer = (int) i - 1; // -1 = the honest "outside a layer" value
        r.op = (i % 4 == 0) ? "MUL_MAT" : (i % 4 == 1 ? "ADD" : (i % 4 == 2 ? "ROPE" : "NONE"));
        r.name = ap("blk.%d.ffn_%s-%d", (int) i, (i % 2 ? "down" : "up"), (int) i);
        r.wall_ns = (uint64_t) i * 1000003ULL;
        r.majflt = (uint64_t) i * 7ULL;
    }
    // Boundaries the format has to carry: the widest u64 and a negative int.
    rows[10].wall_ns = 18446744073709551615ULL;
    rows[10].majflt = 4294967296ULL;
    rows[11].layer = -2147483647 - 1;
    rows[11].step = -1;
    rows[11].op = "a,b\"c\nd\re";
    rows[11].name = "";

    const std::string got = run_compute(outdir + "/S2-00.csv", st, rows);
    std::vector<std::string> want;
    for (const ComputeTraceRow & r : rows) {
        want.push_back(n_compute_row(r));
    }
    check_rows("compute", got, n_compute_static(st), want);
}

// ── S3: io rows ──────────────────────────────────────────────────────────────────────
void slice_io(const std::string & outdir) {
    reset("S3");
    const DecodeTraceStatic st = decode_static_fixture();
    std::vector<IoTraceRow> rows(12);
    for (size_t i = 0; i < rows.size(); ++i) {
        IoTraceRow & r = rows[i];
        r.turn = (int) i % 3;
        r.phase = (int) i % 2;
        r.step = (int) (i * 11);
        r.layer = (int) i;
        r.expert = (int32_t) i * 17 - 5;
        r.proj = (int8_t) (i % 3);
        r.lane = (int8_t) (i % 16);
        r.spec = (uint8_t) (i % 2);
        r.offset = (uint64_t) i * 1000000007ULL;
        r.req_bytes = 2359296ULL;
        r.read_bytes = 2363392ULL + i;
        r.latency_ns = (uint64_t) i * 91007ULL;
    }
    // Signed bytes and the widest unsigned counters.
    rows[10].expert = -1;
    rows[10].proj = -1;
    rows[10].lane = -1;
    rows[10].offset = 18446744073709551615ULL;
    rows[10].read_bytes = 18446744073709551615ULL;
    rows[11].expert = -2147483647 - 1;
    rows[11].spec = 255;
    rows[11].latency_ns = 18446744073709551615ULL;
    const std::string got = run_io(outdir + "/S3-00.csv", st, rows);
    std::vector<std::string> want;
    for (const IoTraceRow & r : rows) {
        want.push_back(n_io_row(r));
    }
    check_rows("io", got, n_io_static(st), want);
}

// ── S4: route rows ───────────────────────────────────────────────────────────────────
void slice_route(const std::string & outdir) {
    reset("S4");
    const RouteTraceStatic st = route_static_fixture();
    const float nan_w = std::nanf("");
    const float neg_nan_w = -std::nanf(""); // %.6g prints "-nan"; the NaN arm always writes "nan"
    const float inf_w = std::numeric_limits<float>::infinity();
    const std::vector<float> weights = {
        0.5f,
        0.0f,
        -0.0f,
        nan_w,
        neg_nan_w,
        1.0f / 3.0f,
        1e-40f,                 // subnormal: %.6g keeps six significant digits
        3.4028235e38f,
        inf_w,
        -inf_w,
        1234567.0f,
        0.000123456f,
        0.9999999f,
    };
    std::vector<RouteTraceRow> rows(weights.size());
    for (size_t i = 0; i < rows.size(); ++i) {
        RouteTraceRow & r = rows[i];
        r.turn = (int) i % 3;
        r.phase = (int) i % 2;
        r.step = (int) (i * 13);
        r.layer = (int) i;
        r.slot = (int) (i % 8);
        r.expert = (int32_t) (i * 9);
        r.weight = weights[i];
        r.residency = (uint8_t) (i % 3); // route_miss / route_hit / route_hit_spec
        r.expert_bytes = (r.residency == route_miss) ? 2359296ULL : 0ULL;
        r.dropped = (uint8_t) (i == 4 ? 1 : 0);
    }
    rows[9].expert = -1;
    rows[9].slot = 7;
    rows[10].expert_bytes = 18446744073709551615ULL;
    const std::string got = run_route(outdir + "/S4-00.csv", st, rows);
    std::vector<std::string> want;
    for (const RouteTraceRow & r : rows) {
        want.push_back(n_route_row(r));
    }
    check_rows("route", got, n_route_static(st), want);
}

} // namespace

int main(int argc, char ** argv) {
    const std::string outdir = argc > 1 ? argv[1] : ".";
    slice_fields(outdir);
    const int f1 = report();
    const int c1 = g_cases, v1 = g_values;
    slice_compute(outdir);
    const int f2 = report();
    const int c2 = g_cases, v2 = g_values;
    slice_io(outdir);
    const int f3 = report();
    const int c3 = g_cases, v3 = g_values;
    slice_route(outdir);
    const int f4 = report();
    const int c4 = g_cases, v4 = g_values;

    const int fails = f1 + f2 + f3 + f4;
#ifdef FG_GOOSE_METRICS
    const char * mode = "goose";
#else
    const char * mode = "native";
#endif
#ifdef FG_METRICS_NO_COUNTER
    // The reference library carries no bridge, so it has no counter to ask.
    std::printf("FG633 mode=%s cases=%d values=%d failures=%d goose_calls=n/a\n", mode, c1 + c2 + c3 + c4,
                v1 + v2 + v3 + v4, fails);
#else
    std::printf("FG633 mode=%s cases=%d values=%d failures=%d goose_calls=%lld\n", mode, c1 + c2 + c3 + c4,
                v1 + v2 + v3 + v4, fails, (long long) fgmt::calls());
#endif
    return fails == 0 ? 0 : 1;
}
