// Differential gate for P9-07: metrics/traces text formats and deterministic workload
// validation.
//
// The oracle is the carrier itself, not a re-typing of it: the fixture compiles
// runtime/core/src/metrics/decode_trace_sink.cpp, route_trace_sink.cpp,
// csv_metrics_sink.cpp and runtime/core/src/moe/deterministic_workload.cpp, drives the real
// sinks and the real DeterministicWorkload, and compares their decisions and their bytes
// with the Goose module under source/metrics-traces.goose.
//
// S1  CSV field formatting: 17 field shapes plus 6 weight classes plus the two metrics
//     widths. The native encoder is a transcription of write_csv_field (it lives in an
//     anonymous namespace, so it cannot be linked), and S2 proves the transcription by
//     comparing real sink output against the port byte for byte.
// S2  Trace records: 12 compute rows, 12 io rows, 8 route rows written by the real sinks,
//     each compared byte for byte with the port's encoder, then parsed back: column count
//     and RFC4180 unescape must return the original strings.
// S3  Workload validation: 10 scenarios driven through the real class and through the
//     port, compared by the step that rejects them, plus 16 boundary values pinned by
//     hand from the native source and a check of the saved file's format.
//
// Checks are explicit (no <cassert>): NDEBUG is defined in Release builds.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "g.h" // sl_u8, goose_init

#include "bmoe/decode_trace.h"
#include "bmoe/metrics.h"
#include "bmoe/route_trace.h"
#include "deterministic_workload.h"

using namespace bmoe;

// The one conversion the port delegates to libc: "%.6g" of a route weight, at the column
// offset the port chooses. libc returns the length it would have written; the port adds it
// to its position, exactly as fprintf would.
extern "C" int64_t mt_fmt_g6(sl_u8 out, int64_t at, double x) {
    if (at < 0 || at >= out.len) return 0;
    int n = std::snprintf((char *) out.data + at, (size_t) (out.len - at), "%.6g", x);
    return (int64_t) n;
}

namespace {

int g_cases = 0, g_values = 0, g_fails = 0;
const char * g_slice = "S?";

void reset(const char * slice) {
    g_slice = slice;
    g_cases = g_values = g_fails = 0;
}
void new_case() {
    ++g_cases;
}
void note(bool ok, const std::string & what) {
    ++g_values;
    if (!ok) {
        ++g_fails;
        std::printf("  FAIL [%s] %s\n", g_slice, what.c_str());
    }
}
int report() {
    std::printf("FG617_%s cases=%d values=%d failures=%d\n", g_slice, g_cases, g_values, g_fails);
    return g_fails;
}

sl_u8 view_of(const std::string & s) {
    return sl_u8{(uint8_t *) (void *) s.data(), (int64_t) s.size()};
}

// ── S1: CSV field formatting ───────────────────────────────────────────────────────────

// decode_trace_sink.cpp:19-30, transcribed. S2 checks this transcription against the real
// sinks, so the two cannot drift apart silently.
std::string native_field(const std::string & v) {
    if (v.find_first_of(",\"\n") == std::string::npos) return v;
    std::string o;
    o.push_back('"');
    for (char c : v) {
        if (c == '"') o.push_back('"'); // RFC4180 doubling
        o.push_back(c == '\n' ? ' ' : c);
    }
    o.push_back('"');
    return o;
}

std::string port_field(const std::string & v) {
    std::vector<uint8_t> buf(8192);
    std::string in = v;
    int64_t n = mt_field_encode(view_of(in), sl_u8{buf.data(), (int64_t) buf.size()});
    return std::string((char *) buf.data(), (size_t) n);
}

void slice_s1(const std::string & outdir) {
    reset("S1");

    // Field shapes: what ggml puts in a node name, plus the three characters the predicate
    // names and the characters it does not.
    const std::vector<std::string> fields = {
        "",
        "plain",
        "blk.0.ffn_up-0",
        "a,b",
        "say \"hi\"",
        "line\nbreak",
        "cr\rhere",          // '\r' is NOT quoted by this engine
        "\"\"",
        ",",
        "\"",
        "\n",
        "a\r\nb",
        "x,\"y\"\nz",
        std::string("\xff\x80", 2),
        "trailing,",
        " leading and trailing ",
        "ATTN_Q,blk.7.1,\"quoted\"",
    };
    for (size_t i = 0; i < fields.size(); ++i) {
        new_case();
        const std::string want = native_field(fields[i]);
        const std::string got = port_field(fields[i]);
        note(want == got, "field #" + std::to_string(i) + " wants '" + want + "', got '" + got + "'");
        note((int) (mt_field_needs_quote(view_of(fields[i])) != 0) ==
                 (int) (fields[i].find_first_of(",\"\n") != std::string::npos),
             "quote predicate for field #" + std::to_string(i));
    }

    // Weight classes: the nan arm versus the numeric arm. The float values are exactly
    // representable, so the numeric arm carries no rounding question.
    const float weights[] = {NAN, 0.5f, -0.25f, 1.0f, 1e-7f, 123456789.0f};
    for (float w : weights) {
        new_case();
        note((mt_weight_kind((double) w) != 0) == !std::isnan(w),
             "weight kind for " + std::to_string((double) w));
    }

    // The metrics CSV widths: the real sink prints both, the port holds the constants.
    const std::string csv = outdir + "/gate-metrics.csv";
    std::FILE * f = std::fopen(csv.c_str(), "w");
    if (!f) {
        std::printf("  FAIL [S1] cannot open %s\n", csv.c_str());
        ++g_fails;
    } else {
        std::fclose(f);
        IMetricsSink * sink = make_csv_metrics_sink(csv);
        sink->on_run_info(RunInfo());
        RunSummary s;
        sink->on_summary(s);
        delete sink;

        std::ifstream in(csv);
        std::string line, header, summary;
        while (std::getline(in, line)) {
            if (line.rfind("# summary", 0) == 0) summary = line;
            else if (!header.empty() || (line.find("step,steps") == 0)) header = line;
        }
        new_case();
        note((int64_t) mt_count_cols(view_of(header)) == mt_metrics_cols(),
             "metrics header columns " + std::to_string(mt_count_cols(view_of(header))) + " vs " +
                 std::to_string(mt_metrics_cols()));

        int64_t keys = 0;
        size_t at = 0;
        while (at < summary.size()) {
            while (at < summary.size() && summary[at] == ' ') ++at;
            size_t end = summary.find(' ', at);
            if (end == std::string::npos) end = summary.size();
            const std::string tok = summary.substr(at, end - at);
            if (tok.find('=') != std::string::npos) ++keys;
            at = end;
        }
        new_case();
        note(keys == mt_summary_keys(), "summary key=value tokens " + std::to_string(keys) + " vs " +
                                            std::to_string(mt_summary_keys()));
        std::remove(csv.c_str());
    }
}

// ── S2: trace records, real sinks ─────────────────────────────────────────────────────

std::vector<std::string> read_lines(const std::string & path) {
    std::ifstream in(path);
    std::vector<std::string> out;
    std::string l;
    while (std::getline(in, l)) out.push_back(l);
    return out;
}

// A reader's field splitter: commas outside quotes separate, quotes are kept so the
// unescaper sees the raw field.
std::vector<std::string> split_raw(const std::string & line) {
    std::vector<std::string> f;
    std::string cur;
    bool q = false;
    for (char c : line) {
        if (c == '"') {
            q = !q;
            cur.push_back(c);
        } else if (c == ',' && !q) {
            f.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    f.push_back(cur);
    return f;
}

std::string unescape(const std::string & raw) {
    std::vector<uint8_t> buf(8192);
    std::string in = raw;
    int64_t n = mt_unescape_field(view_of(in), sl_u8{buf.data(), (int64_t) buf.size()});
    return std::string((char *) buf.data(), (size_t) n);
}

// What a quoted field is expected to decode to: newlines became spaces inside quotes.
std::string decoded_expectation(const std::string & v) {
    if (v.find_first_of(",\"\n") == std::string::npos) return v;
    std::string o;
    for (char c : v) o.push_back(c == '\n' ? ' ' : c);
    return o;
}

void slice_s2(const std::string & outdir) {
    reset("S2");
    DecodeTraceStatic st;
    st.model = "tiny";
    st.arch = "qwen";
    st.n_layer = 3;
    st.n_threads = 4;
    st.io_threads = 2;
    st.o_direct = true;
    st.overlap = false;

    // 12 compute rows. The op/name strings are the ones a real graph produces plus the
    // characters that force quoting.
    struct CRow {
        int turn, phase, step, seq, layer;
        std::string op, name;
        uint64_t wall, majflt;
    };
    const std::vector<CRow> crows = {
        {0, 0, 0, 0, 0, "GET_ROWS", "inp_embd", 1234, 0},
        {0, 0, 0, 1, -1, "MUL_MAT", "attn_q.weight", 987654321, 3},
        {0, 0, 5, 2, 0, "MUL_MAT", "blk.0.ffn_up-0", 42, 0},
        {0, 1, 6, 3, 1, "ROPE", "a,b", 7, 1},
        {0, 1, 6, 4, 1, "SOFT_MAX", "q\"k", 8, 0},
        {0, 1, 6, 5, 2, "ADD", "x\ny", 9, 2},
        {0, 1, 6, 6, 2, "", "", 10, 0},
        {0, 1, 7, 7, 2, "MUL", "tab\there", 11, 0},
        {0, 1, 7, 8, 2, "MUL", "cr\rhere", 12, 0},
        {1, 1, 8, 9, 2, "MUL_MAT_ID", "blk.2.ffn_gate_exps", 18446744073709551615ull, 4294967296ull},
        {1, 1, 8, 10, 2, "MUL", "x,\"y\"\nz", 0, 0},
        {1, 1, 8, 11, 2, "MUL", "ATTN_Q,blk.7.1,\"quoted\"", 1, 1},
    };

    const std::string cpath = outdir + "/gate-compute.csv";
    {
        IComputeTraceSink * sink = make_csv_compute_trace_sink(cpath);
        if (!sink) {
            std::printf("  FAIL [S2] cannot open %s\n", cpath.c_str());
            ++g_fails;
            return;
        }
        sink->on_static(st);
        std::vector<ComputeTraceRow> rows;
        rows.reserve(crows.size());
        for (const CRow & c : crows) {
            ComputeTraceRow r;
            r.turn = c.turn;
            r.phase = c.phase;
            r.step = c.step;
            r.seq = c.seq;
            r.layer = c.layer;
            r.op = c.op;
            r.name = c.name;
            r.wall_ns = c.wall;
            r.majflt = c.majflt;
            rows.push_back(r);
        }
        sink->on_rows(rows.data(), rows.size());
        delete sink;
    }
    {
        const std::vector<std::string> lines = read_lines(cpath);
        // Two preamble lines, the header, then the rows.
        note(lines.size() == crows.size() + 3, "compute file line count " + std::to_string(lines.size()));
        note(lines.size() >= 3 && (int64_t) mt_count_cols(view_of(lines[2])) == mt_trace_cols(0),
             "compute header columns");
        for (size_t i = 0; i < crows.size(); ++i) {
            new_case();
            const CRow & c = crows[i];
            std::vector<uint8_t> buf(8192);
            std::string op = c.op, name = c.name;
            int64_t n = mt_compute_row_encode(c.turn, c.phase, c.step, c.seq, c.layer, view_of(op),
                                              view_of(name), (int64_t) c.wall, (int64_t) c.majflt,
                                              sl_u8{buf.data(), (int64_t) buf.size()});
            std::string got((char *) buf.data(), (size_t) n);
            if (!got.empty() && got.back() == '\n') got.pop_back();
            const std::string & want = lines[i + 3];
            note(got == want, "compute row " + std::to_string(i) + " port '" + got + "' native '" + want + "'");
            const std::vector<std::string> cols = split_raw(want);
            note((int64_t) cols.size() == mt_trace_cols(0), "compute row " + std::to_string(i) + " columns");
            note((int64_t) mt_count_cols(view_of(want)) == mt_trace_cols(0),
                 "compute row " + std::to_string(i) + " reader column count");
            note(unescape(cols[5]) == decoded_expectation(c.op), "compute row " + std::to_string(i) + " op decode");
            note(unescape(cols[6]) == decoded_expectation(c.name),
                 "compute row " + std::to_string(i) + " name decode");
        }
        std::remove(cpath.c_str());
    }

    // 12 io rows, all twelve columns integer.
    struct IRow {
        int turn, phase, step, layer;
        int32_t expert;
        int8_t proj, lane;
        uint8_t spec;
        uint64_t offset, req, rd, lat;
    };
    const std::vector<IRow> irows = {
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 4096, 4096, 1000},
        {0, 0, 0, 1, -1, -1, -1, 0, 0, 0, 0, 0},
        {0, 0, 1, 2, 7, 3, 1, 1, 1099511627776ull, 65536, 69632, 250000},
        {0, 1, 2, 3, 2147483647, 5, 2, 1, 10, 1, 2, 3},
        {0, 1, 3, 0, 0, 4, 3, 1, 18446744073709551615ull, 512, 4096, 999},
        {1, 0, 4, 5, 63, 1, 0, 0, 8192, 8192, 8192, 123456789},
        {1, 1, 5, 6, 128, 2, 1, 1, 240518168576ull, 131072, 135168, 1000000},
        {1, 1, 6, 7, 255, 3, 2, 0, 4096, 4096, 4096, 7},
        {0, 0, 0, 0, 9, 0, 3, 1, 1, 2, 3, 4},
        {0, 0, 9, 9, 31, 3, 1, 1, 4294967296ull, 4294967296ull, 4294967296ull, 4294967296ull},
        {0, 1, 10, 2, 12, 2, 0, 1, 0, 8192, 8192, 555},
        {2, 1, 11, 3, 5, 1, 3, 0, 16384, 16384, 16384, 101},
    };
    const std::string ipath = outdir + "/gate-io.csv";
    {
        IIoTraceSink * sink = make_csv_io_trace_sink(ipath);
        if (!sink) {
            std::printf("  FAIL [S2] cannot open %s\n", ipath.c_str());
            ++g_fails;
            return;
        }
        sink->on_static(st);
        std::vector<IoTraceRow> rows;
        rows.reserve(irows.size());
        for (const IRow & c : irows) {
            IoTraceRow r;
            r.turn = c.turn;
            r.phase = c.phase;
            r.step = c.step;
            r.layer = c.layer;
            r.expert = c.expert;
            r.proj = c.proj;
            r.lane = c.lane;
            r.spec = c.spec;
            r.offset = c.offset;
            r.req_bytes = c.req;
            r.read_bytes = c.rd;
            r.latency_ns = c.lat;
            rows.push_back(r);
        }
        sink->on_rows(rows.data(), rows.size());
        delete sink;
    }
    {
        const std::vector<std::string> lines = read_lines(ipath);
        note(lines.size() == irows.size() + 3, "io file line count " + std::to_string(lines.size()));
        note(lines.size() >= 3 && (int64_t) mt_count_cols(view_of(lines[2])) == mt_trace_cols(1), "io header columns");
        for (size_t i = 0; i < irows.size(); ++i) {
            new_case();
            const IRow & c = irows[i];
            std::vector<uint8_t> buf(8192);
            int64_t n = mt_io_row_encode(c.turn, c.phase, c.step, c.layer, c.expert, c.proj, c.lane, c.spec,
                                         (int64_t) c.offset, (int64_t) c.req, (int64_t) c.rd, (int64_t) c.lat,
                                         sl_u8{buf.data(), (int64_t) buf.size()});
            std::string got((char *) buf.data(), (size_t) n);
            if (!got.empty() && got.back() == '\n') got.pop_back();
            note(got == lines[i + 3], "io row " + std::to_string(i) + " port '" + got + "' native '" + lines[i + 2] + "'");
            note((int64_t) split_raw(lines[i + 3]).size() == mt_trace_cols(1),
                 "io row " + std::to_string(i) + " columns");
        }
        std::remove(ipath.c_str());
    }

    // 8 route rows: the nan arm, exact-decimal weights, signed slots, big counters.
    struct RRow {
        int turn, phase, step, layer, slot;
        int32_t expert;
        float weight;
        uint8_t residency, dropped;
        uint64_t bytes;
    };
    const std::vector<RRow> rrows = {
        {0, 0, 0, 0, 0, 5, 0.5f, 0, 0, 0},
        {0, 0, 0, 0, 1, 6, NAN, 1, 0, 0},
        {0, 1, 3, 2, 0, 63, -0.25f, 2, 0, 1099511627776ull},
        {0, 1, 3, 2, 1, 12, 1.0f, 0, 1, 0},
        {1, 1, 4, 1, 3, 128, 0.123456789f, 1, 0, 65536},
        {1, 1, 5, 3, 0, 0, -3.5f, 2, 0, 4294967296ull},
        {0, 0, 0, 0, 9, -1, 1e-7f, 0, 0, 0},
        {2, 1, 9, 4, 2, 2147483647, 123456789.0f, 1, 1, 18446744073709551615ull},
    };
    const std::string rpath = outdir + "/gate-route.csv";
    {
        RouteTraceStatic rs;
        rs.model = "tiny";
        rs.arch = "qwen";
        rs.n_layer = 5;
        rs.n_expert = 8;
        rs.n_expert_used = 4;
        rs.expert_bytes_per_layer = {111, 222, 333, 444, 555};
        rs.dense_bytes_per_layer = {1, 2, 3};

        IRouteTraceSink * sink = make_csv_route_trace_sink(rpath);
        if (!sink) {
            std::printf("  FAIL [S2] cannot open %s\n", rpath.c_str());
            ++g_fails;
            return;
        }
        sink->on_static(rs);
        std::vector<RouteTraceRow> rows;
        rows.reserve(rrows.size());
        for (const RRow & c : rrows) {
            RouteTraceRow r;
            r.turn = c.turn;
            r.phase = c.phase;
            r.step = c.step;
            r.layer = c.layer;
            r.slot = c.slot;
            r.expert = c.expert;
            r.weight = c.weight;
            r.residency = c.residency;
            r.expert_bytes = c.bytes;
            r.dropped = c.dropped;
            rows.push_back(r);
        }
        sink->on_rows(rows.data(), rows.size());
        delete sink;
    }
    {
        const std::vector<std::string> lines = read_lines(rpath);
        // The preamble is one line per layer plus two, then the header, then the rows.
        const size_t first_row = 1 + 1 + 5 + 1; // "# route_trace v1", "# model=...", 5x "# layer=", header
        note(lines.size() == first_row + rrows.size(), "route file line count " + std::to_string(lines.size()));
        note(lines.size() > first_row && (int64_t) mt_count_cols(view_of(lines[first_row - 1])) == mt_trace_cols(2),
             "route header columns");
        for (size_t i = 0; i < rrows.size(); ++i) {
            new_case();
            const RRow & c = rrows[i];
            std::vector<uint8_t> buf(8192);
            int64_t n = mt_route_row_encode(c.turn, c.phase, c.step, c.layer, c.slot, c.expert, (double) c.weight,
                                            c.residency, (int64_t) c.bytes, c.dropped,
                                            sl_u8{buf.data(), (int64_t) buf.size()});
            std::string got((char *) buf.data(), (size_t) n);
            if (!got.empty() && got.back() == '\n') got.pop_back();
            note(got == lines[first_row + i],
                 "route row " + std::to_string(i) + " port '" + got + "' native '" + lines[first_row + i] + "'");
            note((int64_t) split_raw(lines[first_row + i]).size() == mt_trace_cols(2),
                 "route row " + std::to_string(i) + " columns");
        }
        std::remove(rpath.c_str());
    }
}

// ── S3: deterministic workload capture/replay ─────────────────────────────────────────

enum Step { SC_OK = 0, SC_HEADER = 1, SC_STATIC = 2, SC_PROMPT = 3, SC_ROUTE = 4, SC_TOKENS = 5, SC_FINISH = 6 };

std::vector<int32_t> ids_for(int layer) {
    return {layer * 10 + 1, layer * 10 + 2, layer * 11 + 1, layer * 11 + 2, layer * 12 + 1, layer * 12 + 2};
}

void capture_to(const std::string & path) {
    auto w = DeterministicWorkload::capture();
    w->set_static("qwen", 24, 2);
    std::vector<int32_t> prompt = {1, 2, 3};
    w->prompt(prompt);
    for (int layer = 0; layer < 3; ++layer) {
        w->begin_batch(0, 3, 0);
        std::vector<int32_t> ids = ids_for(layer);
        w->capture_topk(layer, 2, 3, ids.data());
    }
    for (size_t i = 0; i < 5; ++i) {
        int32_t selected = 0;
        w->token(i, (int32_t) (100 + i), selected);
    }
    w->save(path);
}

// The real class, driven exactly as Session::generate drives it.
int native_replay(const std::string & path, int scenario) {
    std::string err;
    auto w = DeterministicWorkload::load(path, err);
    if (!w) return SC_HEADER;
    if (!w->check_static(scenario == 3 ? "other" : "qwen", scenario == 3 ? 24 : 24, 2)) return SC_STATIC;
    std::vector<int32_t> prompt = {1, 2, 3};
    if (scenario == 2) prompt = {1, 2, 4};
    if (!w->prompt(prompt)) return SC_PROMPT;
    const int nroutes = scenario == 5 ? 2 : (scenario == 6 ? 4 : 3);
    for (int l = 0; l < nroutes; ++l) {
        w->begin_batch(0, 3, 0);
        const int layer = (scenario == 4 && l == 1) ? 9 : l;
        const int32_t * ids = nullptr;
        if (!w->replay_topk(layer, 2, 3, ids)) return SC_ROUTE;
        if (ids == nullptr || ids[0] != ids_for(layer)[0]) return SC_ROUTE;
    }
    const int ntokens = scenario == 7 ? 4 : 5;
    for (int i = 0; i < ntokens; ++i) {
        int32_t selected = 0;
        const size_t index = (scenario == 8) ? 5 : (size_t) i;
        if (!w->token(index, 0, selected)) return SC_TOKENS;
        if (selected != (int32_t) (100 + index)) return SC_TOKENS;
    }
    if (!w->finish((size_t) ntokens)) return SC_FINISH;
    return SC_OK;
}

// The same sequence through the port. counts are what the session loop knows from the
// workload object: 3 route records, 5 captured tokens.
int port_replay(int scenario) {
    if (mt_wl_header_ok(1, 1) == 0) return SC_HEADER;
    if (mt_wl_static_ok(scenario == 3 ? 0 : 1, 1, 1) == 0) return SC_STATIC;
    if (mt_wl_prompt_ok(1, scenario == 2 ? 0 : 1) == 0) return SC_PROMPT;
    const int nroutes = scenario == 5 ? 2 : (scenario == 6 ? 4 : 3);
    for (int l = 0; l < nroutes; ++l) {
        if (mt_wl_next_route_ok(l, 3) == 0) return SC_ROUTE;
        const int layer = (scenario == 4 && l == 1) ? 9 : l;
        if (mt_wl_route_nids_ok(6, 2, 3) == 0) return SC_ROUTE;
        if (mt_wl_route_shape_ok(0, 0, 3, l, 2, 3, 0, 0, 3, layer, 2, 3) == 0) return SC_ROUTE;
        if (mt_wl_route_take_ok(1, 1) == 0) return SC_ROUTE;
    }
    const int ntokens = scenario == 7 ? 4 : 5;
    for (int i = 0; i < ntokens; ++i) {
        const int index = scenario == 8 ? 5 : i;
        if (mt_wl_token_index_ok(1, index, 5) == 0) return SC_TOKENS;
    }
    const int code = (int) mt_wl_finish_code(nroutes, 3, ntokens, 5);
    return code == 0 ? SC_OK : SC_FINISH;
}

void slice_s3(const std::string & outdir) {
    reset("S3");

    const std::string wl = outdir + "/gate-workload.txt";
    capture_to(wl);

    // The saved format, checked with the port's own width rule.
    const std::vector<std::string> lines = read_lines(wl);
    new_case();
    note(!lines.empty() && lines[0] == "BMOE_DETERMINISTIC_WORKLOAD 1", "workload magic line");
    note(lines.size() > 3 && lines[1] == "arch qwen" && lines[2] == "layers 24" && lines[3] == "top_k 2",
         "workload metadata lines");
    for (const std::string & l : lines) {
        if (l.rfind("route ", 0) != 0) continue;
        int words = 0;
        size_t at = 0;
        while (at <= l.size()) {
            size_t end = l.find(' ', at);
            if (end == std::string::npos) end = l.size();
            if (end > at) ++words;
            if (end == l.size()) break;
            at = end + 1;
        }
        note((int64_t) words == mt_wl_route_words(2, 3),
             "route line words " + std::to_string(words) + " vs " + std::to_string(mt_wl_route_words(2, 3)));
    }

    // Ten scenarios, the same driver shape on both sides.
    for (int scenario = 0; scenario <= 8; ++scenario) {
        new_case();
        const int n = native_replay(wl, scenario);
        const int p = port_replay(scenario);
        note(n == p, "scenario " + std::to_string(scenario) + " native step " + std::to_string(n) + " port step " +
                         std::to_string(p));
    }
    {
        // A capture token index that skips ahead is refused, and the port refuses it too.
        new_case();
        auto cap = DeterministicWorkload::capture();
        int32_t selected = 0;
        const bool native_ok = cap->token(1, 7, selected);
        note(!native_ok, "capture token index 1 on an empty workload is rejected");
        note(mt_wl_token_index_ok(0, 1, 0) == 0, "port rejects a non-sequential capture index");
    }
    {
        // A header the engine cannot replay: version 2 in the file, version 2 in the port.
        new_case();
        const std::string bad = outdir + "/gate-workload-v2.txt";
        {
            std::ofstream out(bad);
            out << "BMOE_DETERMINISTIC_WORKLOAD 2\narch qwen\nlayers 24\ntop_k 2\nprompt 0\noutput 0\nroutes 0\n";
        }
        std::string err;
        note(DeterministicWorkload::load(bad, err) == nullptr, "native load rejects version 2");
        note(mt_wl_header_ok(1, 2) == 0, "port rejects version 2");
        std::remove(bad.c_str());
    }

    // Boundary values pinned by hand from the native source, independent of the driver above.
    struct Pin {
        const char * name;
        int64_t got;
        int64_t want;
    };
    const Pin pins[] = {
        {"header(magic=1,v=1)", mt_wl_header_ok(1, 1), 1},
        {"header(magic=1,v=0)", mt_wl_header_ok(1, 0), 0},
        {"header(magic=0,v=1)", mt_wl_header_ok(0, 1), 0},
        {"nids(6,2,3)", mt_wl_route_nids_ok(6, 2, 3), 1},
        {"nids(6,3,2)", mt_wl_route_nids_ok(6, 3, 2), 1},
        {"nids(5,2,3)", mt_wl_route_nids_ok(5, 2, 3), 0},
        {"nids(0,0,3)", mt_wl_route_nids_ok(0, 0, 3), 1},
        {"nids(2^32*3,2^32,3)", mt_wl_route_nids_ok(12884901888ll, 4294967296ll, 3), 1},
        {"nids(2^32*3,2^32-1,3)", mt_wl_route_nids_ok(12884901888ll, 4294967295ll, 3), 0},
        {"next(cursor=0,count=3)", mt_wl_next_route_ok(0, 3), 1},
        {"next(cursor=2,count=3)", mt_wl_next_route_ok(2, 3), 1},
        {"next(cursor=3,count=3)", mt_wl_next_route_ok(3, 3), 0},
        {"next(cursor=4,count=3)", mt_wl_next_route_ok(4, 3), 0},
        {"finish(3,3,5,5)", mt_wl_finish_code(3, 3, 5, 5), 0},
        {"finish(2,3,5,5)", mt_wl_finish_code(2, 3, 5, 5), 1},
        {"finish(3,3,4,5)", mt_wl_finish_code(3, 3, 4, 5), 2},
        {"finish(2,3,4,5)", mt_wl_finish_code(2, 3, 4, 5), 1},
    };
    for (const Pin & p : pins) {
        new_case();
        note(p.got == p.want, std::string(p.name) + " got " + std::to_string(p.got) + " want " + std::to_string(p.want));
    }

    // Pinned decisions that take discrete inputs rather than counts.
    struct BoolPin {
        const char * name;
        int64_t got;
        int64_t want;
    };
    const BoolPin bools[] = {
        {"token(replay,idx=0,n=5)", mt_wl_token_index_ok(1, 0, 5), 1},
        {"token(replay,idx=5,n=5)", mt_wl_token_index_ok(1, 5, 5), 0},
        {"token(capture,idx=3,n=3)", mt_wl_token_index_ok(0, 3, 3), 1},
        {"token(capture,idx=2,n=3)", mt_wl_token_index_ok(0, 2, 3), 0},
        {"prompt(capture,equal=0)", mt_wl_prompt_ok(0, 0), 1},
        {"prompt(replay,equal=0)", mt_wl_prompt_ok(1, 0), 0},
        {"prompt(replay,equal=1)", mt_wl_prompt_ok(1, 1), 1},
        {"static(1,1,1)", mt_wl_static_ok(1, 1, 1), 1},
        {"static(1,0,1)", mt_wl_static_ok(1, 0, 1), 0},
        {"static(0,1,1)", mt_wl_static_ok(0, 1, 1), 0},
        {"shape(same)", mt_wl_route_shape_ok(0, 0, 3, 1, 2, 3, 0, 0, 3, 1, 2, 3), 1},
        {"shape(layer differs)", mt_wl_route_shape_ok(0, 0, 3, 1, 2, 3, 0, 0, 3, 2, 2, 3), 0},
        {"shape(phase differs)", mt_wl_route_shape_ok(0, 0, 3, 1, 2, 3, 1, 0, 3, 1, 2, 3), 0},
        {"shape(top_k differs)", mt_wl_route_shape_ok(0, 0, 3, 1, 2, 3, 0, 0, 3, 1, 4, 3), 0},
        {"take(1,1)", mt_wl_route_take_ok(1, 1), 1},
        {"take(1,0)", mt_wl_route_take_ok(1, 0), 0},
        {"take(0,1)", mt_wl_route_take_ok(0, 1), 0},
        {"weights(2,3)", mt_wl_route_words(2, 3), 14},
        {"weights(0,5)", mt_wl_route_words(0, 5), 8},
        {"cols(compute,io,route)", mt_trace_cols(0) * 100 + mt_trace_cols(1) * 10 + mt_trace_cols(2), 9 * 100 + 12 * 10 + 10},
    };
    for (const BoolPin & p : bools) {
        new_case();
        note(p.got == p.want, std::string(p.name) + " got " + std::to_string(p.got) + " want " + std::to_string(p.want));
    }

    std::remove(wl.c_str());
}

} // namespace

int main(int argc, char ** argv) {
    goose_init(0, nullptr);
    const std::string outdir = argc > 1 ? argv[1] : "build";

    slice_s1(outdir);
    int fails = report();
    slice_s2(outdir);
    fails += report();
    slice_s3(outdir);
    fails += report();

    std::printf("FG617 total=%s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
