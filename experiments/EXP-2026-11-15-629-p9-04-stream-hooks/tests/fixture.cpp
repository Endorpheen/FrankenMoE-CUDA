// Differential gate for P9-04 (router hook, I/O-compute overlap, row source).
//
// Native side: a transcription of the DECISIONS in
// runtime/core/src/moe/router_hook.cpp, expert_stream_source.cpp (overlap path),
// row_stream.cpp (takeover acceptance), stall_union.h and session.cpp (the
// dense/row split) at commit 9981c403, with the source line cited at every
// function. The transcription keeps the native state fields and the native
// order of every step; what it does not keep is the graph, the file, the
// threads and the clocks — tensor reads, pread outcomes, commit outcomes and
// timestamps are inputs here.
//
// Port side: the exports of source/hook_rules.goose, overlap_rules.goose and
// row_select.goose, combined by source/stream_hooks.goose.
//
// Both sides run the same scripted scenarios over synthetic node names, flag
// vectors and event schedules; the traces are compared. No disk, no GPU, no
// model, no thread.
#include "g.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using std::string;
using std::vector;

static int g_cases = 0;
static int g_checks = 0;
static int g_fails = 0;
static int g_shown = 0;

static void cmp(const char * what, const string & nat, const string & port) {
    g_checks++;
    if (nat == port) return;
    g_fails++;
    if (g_shown < 12) {
        g_shown++;
        std::printf("FAIL %s\n  nat : %s\n  port: %s\n", what, nat.c_str(), port.c_str());
    }
}

static string fmti(std::initializer_list<long long> v) {
    string s = "[";
    bool first = true;
    for (long long x : v) {
        if (!first) s += ",";
        first = false;
        s += std::to_string(x);
    }
    return s + "]";
}

template <typename T>
static string fmtv(const vector<T> & v) {
    string s = "[";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) s += ",";
        s += std::to_string((long long) v[i]);
    }
    return s + "]";
}

static sl_i64 sl(vector<int64_t> & v) { return sl_i64{v.data(), (int64_t) v.size()}; }
static sl_i64 csl(const vector<int64_t> & v) {
    return sl_i64{(int64_t *) (void *) v.data(), (int64_t) v.size()};
}
static sl_i64 csl64(const vector<long long> & v) {
    return sl_i64{(int64_t *) (void *) v.data(), (int64_t) v.size()};
}
static sl_u8 slu(vector<uint8_t> & v) { return sl_u8{v.data(), (int64_t) v.size()}; }
static sl_u8 cslu(const vector<uint8_t> & v) {
    return sl_u8{(uint8_t *) (void *) v.data(), (int64_t) v.size()};
}

// ggml tensor names are char[64], zero filled. Every native matcher reads that
// buffer, so the fixture does too rather than handing out a short C string.
struct Name {
    char a[64];
    explicit Name(const char * s) {
        std::memset(a, 0, sizeof(a));
        size_t n = std::strlen(s);
        if (n > 63) n = 63;
        std::memcpy(a, s, n);
    }
};

// ─────────────── slice A: the router hook ────────────────────────────────────
// router_hook.cpp.
namespace nat_rh {

// 48-50.
static bool is_moe_node(const char * name) { return std::memcmp(name, "ffn_moe_", 8) == 0; }

// 51-63.
static int parse_tail(const char * p) {
    if (*p < '0' || *p > '9') return -1;
    int il = 0;
    for (; *p; ++p) {
        if (*p < '0' || *p > '9') return -1;
        il = il * 10 + (*p - '0');
        if (il > (1 << 20)) return -1;
    }
    return il;
}

// 66-70.
static int match_layer_node(const char * name, const char * prefix) {
    const size_t n = std::strlen(prefix);
    return std::memcmp(name, prefix, n) == 0 ? parse_tail(name + n) : -1;
}

// 83-88.
static const char * kWeightNodes[] = {"ffn_moe_weights", "ffn_moe_weights_softmax", "ffn_moe_weights_norm",
                                      "ffn_moe_weights_scaled"};

// 90-100. Returns the variant; *il_out is written only on a match.
static int match_weights(const char * name, int & il_out) {
    for (int v = 0; v < 4; ++v) {
        const char * n = kWeightNodes[v];
        const size_t sl_ = std::strlen(n);
        if (std::memcmp(name, n, sl_) != 0 || name[sl_] != '-') continue;
        const int il = parse_tail(name + sl_ + 1);
        if (il < 0) continue;
        il_out = il;
        return v;
    }
    return -1;
}

// 239-241.
static bool drop_armed(float frac, bool has_source, int phase, bool prefill) {
    return frac > 0.0f && has_source && (phase == 1 || prefill);
}

// 232-234.
static bool sub_armed(float lambda, bool has_source, int phase) {
    return lambda > 0.0f && has_source && phase == 1;
}

// 1476.
static bool want_weights(bool trace_on, bool drop_armed_) { return trace_on || drop_armed_; }

// 1489-1490.
static bool weights_iso(int variant, int wl, const vector<int> & term) {
    const bool is_weights = variant >= 0;
    return is_weights && (wl < 0 || wl >= (int) term.size() || term[wl] < 0 || term[wl] == variant);
}

// 1496-1498.
static bool logits_scope(bool moe_node, bool predict_or_ra, bool has_source, int phase) {
    return moe_node && predict_or_ra && has_source && phase == 1;
}

// 1510-1522. ctrace_ask_layer is in/out through `cursor`.
static bool ctrace_iso(bool ctrace_on, bool layers, int nl, int & cursor) {
    if (!ctrace_on) return false;
    if (!layers) return true;
    if (nl >= 0 && nl != cursor) {
        cursor = nl;
        return true;
    }
    return false;
}

// 1528.
static bool ask_iso(bool ctrace, bool is_topk, bool weights, bool is_logits, bool predict_log) {
    return ctrace || is_topk || weights || (is_logits && predict_log);
}

// 1410-1420.
static int capture_take(int p, bool in_range, int ne2) {
    if (p < 0) return 0;
    return (in_range && ne2 > 0) ? 1 : 0;
}

// 1668-1671.
static bool defer_layer(bool drop_armed_, int il, const vector<int> & term) {
    return drop_armed_ && il >= 0 && il < (int) term.size() && term[il] >= 0;
}

// 321-341. 0 nothing, 1 learned, 2 deferred and forgotten.
static int close_layer(vector<int> & term, int lay, int chain_last, bool deferred) {
    if (lay < 0) return 0;
    int r = 0;
    if (lay < (int) term.size() && term[lay] < 0 && chain_last >= 0) {
        term[lay] = chain_last;
        r = 1;
    }
    if (deferred) {
        if (lay < (int) term.size()) term[lay] = -1;
        r = 2;
    }
    return r;
}

// 1547-1551. `chain_last` in/out.
static int observe_weights(bool ready, int wl, int variant, int drop_layer, bool deferred,
                           const vector<int> & term, int & chain_last) {
    if (!ready || wl < 0 || drop_layer != wl) return 0;
    chain_last = variant;
    if (deferred && wl < (int) term.size() && term[wl] == variant) return 2;
    return 1;
}

// 1588-1600.
static int workload(int mode, bool shape_ok, bool matches) {
    if (mode == 0) return 0;
    if (!shape_ok) return 3;
    if (mode == 1) return 1;
    return matches ? 2 : 3;
}

// The flags of one topk callback, named as the native fields are.
struct TopkIn {
    bool sub = false;
    int route_ahead = 0;
    int phase = 1;
    int workload = 0;
    bool trace = false;
    float drop_frac = 0.0f;
    bool predict = false;
    bool defer = false;
    bool ids = true;
    bool predict_log = false;
    int prefetch = 0;
    bool record = false;
    bool wl_shape_ok = true;
    bool wl_matches = true;
};

// 1553-1715: the sequence of calls the topk callback makes.
static vector<long long> topk_plan(const TopkIn & in) {
    vector<long long> out;
    out.push_back(1);                                                        // 1559 close_drop_layer
    if (in.sub) out.push_back(2);                                            // 1563
    if (in.route_ahead > 0 && in.phase == 1) {                               // 1571
        out.push_back(3);                                                    // 1572 collect
        out.push_back(4);                                                    // 1573 submit
        out.push_back(5);                                                    // 1574 apply
    }
    out.push_back(6);                                                        // 1580 gather
    if (in.workload == 1) out.push_back(in.wl_shape_ok ? 7 : 18);            // 1590
    else if (in.workload == 2) out.push_back((in.wl_shape_ok && in.wl_matches) ? 8 : 18); // 1594
    if (in.trace) out.push_back(9);                                          // 1602
    if (in.drop_frac > 0.0f) out.push_back(10);                              // 1619
    if (in.predict) out.push_back(11);                                       // 1625
    if (in.defer) {
        out.push_back(12);                                                   // 1671
    } else {
        if (in.ids) out.push_back(13);                                       // 1687
        if (in.predict) out.push_back(14);                                   // 1692
        if (in.route_ahead > 0 && in.phase == 1) out.push_back(3);            // 1693
    }
    if (in.predict_log && in.phase == 1) out.push_back(15);                  // 1699
    if (in.prefetch > 0) out.push_back(16);                                  // 1705
    if (in.record) out.push_back(17);                                        // 1713
    return out;
}

// The flags of one ask pass.
struct AskIn {
    bool capturing = false;
    bool row_source = false;
    bool is_logits = false;
    int gl = -1;
    int n_layer = 0;
    bool ctrace_iso = false;
    bool is_topk = false;
    bool weights_iso = false;
    bool predict_log = false;
};

// 1405-1528: the sequence of effects in the ask pass, with the isolation answer
// reported separately (the native returns it instead of acting on it).
static vector<long long> ask_plan(const AskIn & in) {
    vector<long long> out;
    if (in.capturing) {
        out.push_back(1); // 1405, then `return false` at 1436
        return out;
    }
    if (in.row_source) out.push_back(2);                       // 1446
    if (in.is_logits && in.gl < in.n_layer) out.push_back(3);  // 1500
    if (in.ctrace_iso) out.push_back(4);                       // 1511
    if (in.is_topk) out.push_back(5);                          // 1528
    if (in.weights_iso) out.push_back(6);                      // 1528
    if (in.is_logits && in.predict_log) out.push_back(7);      // 1528
    return out;
}

} // namespace nat_rh

// ─────────────── slice B: the I/O-compute overlap ────────────────────────────
// expert_stream_source.cpp (overlap path) and stall_union.h.
namespace nat_ov {

const int MAXP = 3;

// 1163-1167.
static int p0(int mask) {
    for (int p = 0; p < MAXP; ++p)
        if (mask & (1 << p)) return p;
    return -1;
}

// 1168.
static bool two_wave(bool cfg, bool load_all, int p0_) { return cfg && !load_all && p0_ >= 0; }

// 1189-1194.
static int commit_only(bool two_wave_, int p0_, int p) { return (two_wave_ && p == p0_) ? p0_ : -1; }

// 1119-1140. `seen` is the native seen_ scratch, `out` the sorted staged set.
static vector<long long> staged(const vector<int> & ids, int n_expert, bool load_all, vector<uint8_t> & seen) {
    vector<int> st;
    std::fill(seen.begin(), seen.end(), 0);
    for (size_t i = 0; i < ids.size(); ++i) {
        const int e = load_all ? ((int) i < n_expert ? (int) i : -1) : ids[i];
        if (e < 0 || e >= n_expert || seen[e]) continue;
        seen[e] = 1;
        st.push_back(e);
    }
    if (load_all) {
        for (int e = 0; e < n_expert; ++e) {
            if (seen[e]) continue;
            seen[e] = 1;
            st.push_back(e);
        }
    }
    std::sort(st.begin(), st.end());
    vector<long long> out;
    for (int e : st) out.push_back(e);
    return out;
}

// 1176-1186. -1 commit failed, 1 hit, 0 miss.
static int stage_verdict(bool commit_ok, bool hit) {
    if (!commit_ok) return -1;
    return hit ? 1 : 0;
}

// 1188-1196.
static long long wave_jobs(int mask, const vector<uint8_t> & miss, int n_staged) {
    long long n = 0;
    for (int p = 0; p < MAXP; ++p) {
        if (!(mask & (1 << p))) continue;
        for (int i = 0; i < n_staged; ++i)
            if (miss[i]) n++;
    }
    return n;
}

static vector<long long> wave_list(int mask, const vector<int> & staged_, const vector<uint8_t> & miss) {
    vector<long long> out;
    for (int p = 0; p < MAXP; ++p) {
        if (!(mask & (1 << p))) continue;
        for (size_t i = 0; i < staged_.size(); ++i) {
            if (!miss[i]) continue;
            out.push_back(p);
            out.push_back(staged_[i]);
        }
    }
    return out;
}

// 1189-1212 / 1266-1280.
static vector<long long> publish(bool jobs_reset, long long gen, long long flag_gen_in) {
    long long ni = -1, dc = -1, fg = flag_gen_in;
    if (jobs_reset) {
        ni = 0;
        dc = 0;
        fg = gen;
    }
    return {ni, dc, fg};
}

// 1089-1103.
static bool drain_done(long long done_cnt, long long batch_njobs, bool io_stop) {
    return done_cnt == batch_njobs || io_stop;
}

// 371-380.
static bool worker_wait(bool io_stop, long long batch_gen, long long seen, long long next_idx,
                        long long batch_njobs, long long spec_next, long long spec_n) {
    return io_stop || batch_gen > seen || next_idx < batch_njobs || spec_next < spec_n;
}

// 338-340.
static bool drain_more(long long batch_gen, long long my_gen, long long next_idx, long long batch_njobs) {
    if (batch_gen != my_gen) return false;
    return next_idx < batch_njobs;
}

// 1327-1335.
static int gate_scope(int il, int cur_il, int expert, int n_expert) {
    return (il != cur_il || expert < 0 || expert >= n_expert) ? 1 : 0;
}

// 1315-1365. 0 not ours, 1 fault, 2 ready, 3 stall.
static int gate_path(bool ours, bool fault, unsigned flag_gen, unsigned want) {
    if (!ours) return 0;
    if (fault) return 1;
    if (flag_gen == want) return 2;
    return 3;
}

// 336-365.
static int publish_mode(bool has_flag, int waiters) {
    if (!has_flag) return 0;
    return waiters != 0 ? 2 : 1;
}

// 342-346.
static int read_failure(bool overlap) { return overlap ? 1 : 0; }

// 1197-1233.
static int commit_failure(bool published) { return published ? 1 : 0; }

// 1106.
static long long quiesce_arg(bool spec_adopt, long long il) { return spec_adopt ? il : -1; }

// stall_union.h:11-20, transcribed with the same fields.
struct Union {
    int stalled = 0;
    long long open_ns = 0;
    long long total_ns = 0;
    int enter_at(long long now) {
        if (stalled++ == 0) {
            open_ns = now;
            return 1;
        }
        return 0;
    }
    int exit_at(long long now) {
        if (stalled == 0) return 0;
        if (--stalled == 0) {
            total_ns += now - open_ns;
            open_ns = 0;
            return 1;
        }
        return 0;
    }
    long long total_at(long long now) const { return total_ns + (stalled > 0 ? now - open_ns : 0); }
};

} // namespace nat_ov

// ─────────────── slice C: the row-stream source ──────────────────────────────
// router_hook.cpp capture/dispatch, row_stream.cpp takeover, session.cpp split.
namespace nat_rs {

// 173-193. Node kinds: 0 NONE, 1 VIEW, 2 RESHAPE, 3 PERMUTE, 4 TRANSPOSE, 5 other.
struct Node {
    int op = 5;
    bool i32 = false;
};

static bool index_materialized(const vector<Node> & nodes) {
    int at = 0;
    for (int guard = 0; guard < 8; ++guard) {
        if (at < 0 || at >= (int) nodes.size()) return false; // nullptr
        const Node & t = nodes[(size_t) at];
        switch (t.op) {
        case 0:
            return t.i32;
        case 1:
        case 2:
        case 3:
        case 4:
            at += 1; // src[0]
            break;
        default:
            return false;
        }
    }
    return false;
}

// 1425-1426.
static bool leaf_admit(bool op_none, bool named) { return op_none && named; }

// 1427-1433. 1 gathered, 2 disqualified.
static int ref_verdict(bool get_rows, int slot, bool idx_ok) {
    return (get_rows && slot == 0 && idx_ok) ? 1 : 2;
}

// 195-200.
static bool table_verdict(int gathered_refs, int disq_refs) { return gathered_refs > 0 && disq_refs == 0; }

// session.cpp:1060-1078.
static int session_take(bool is_expert, bool has_tensor, bool has_buffer, bool host_ok, bool in_gguf,
                        bool row_wanted) {
    if (is_expert) return 0;
    if (!has_tensor || !has_buffer) return 1;
    if (!host_ok) return 2;
    if (!in_gguf) return 3;
    return row_wanted ? 5 : 4;
}

// session.cpp:1080.
static long long row_budget(long long mb) { return (mb > 0 ? mb : 0) * 1048576ll; }

// row_stream.cpp:45-61.
static int init_accept(bool present, long long size, bool file_ok, long long row_bytes, long long n_rows,
                       bool reserve_ok) {
    if (!present || size <= 0) return 0;
    if (!file_ok) return 3;
    if (row_bytes == 0 || n_rows == 0) return 1;
    if (!reserve_ok) return 4;
    return 2;
}

// row_stream.cpp:74-78.
static bool policy_on(long long taken) { return taken > 0; }

// router_hook.cpp:1446-1462.
static int dispatch(bool get_rows, bool served, bool ids_readable) {
    if (get_rows) {
        if (!served) return 0;
        return ids_readable ? 1 : 2;
    }
    return served ? 3 : 0;
}

// 1463.
static int prep_fatal(bool refused) { return refused ? 1 : 0; }

// row_stream.cpp:213-217.
static long long materialize_need(bool whole, long long n_slabs) {
    if (whole) return 0;
    return n_slabs < 0 ? 0 : n_slabs;
}

// row_stream.cpp:233-241.
static long long whole_unlink(long long n_slabs, const vector<uint8_t> & in_lru) {
    long long n = 0;
    for (long long s = 0; s < n_slabs && s < (long long) in_lru.size(); ++s)
        if (in_lru[(size_t) s]) n++;
    return n;
}

} // namespace nat_rs

// ─────────────── the gate ────────────────────────────────────────────────────
static vector<int64_t> i64v(std::initializer_list<long long> v) {
    vector<int64_t> o;
    for (long long x : v) o.push_back((int64_t) x);
    return o;
}
static vector<uint8_t> u8v(std::initializer_list<int> v) {
    vector<uint8_t> o;
    for (int x : v) o.push_back((uint8_t) x);
    return o;
}

// ── slice A ──
static void case_names() {
    const char * names[] = {"ffn_moe_topk-3",   "ffn_moe_weights-0", "ffn_moe_logits-39", "ffn_mo",
                            "ffn_moex",         "ffn_moe",           "model.layers.0",    "attn_q-0",
                            "",                 "ffn_moe_",          "ffn_moe_topk_softmax-7"};
    for (const char * s : names) {
        Name n(s);
        g_cases++;
        cmp((string("a_is_moe ") + s).c_str(), std::to_string((int) nat_rh::is_moe_node(n.a)),
            std::to_string((int) rh_is_moe_node(sl_u8{(uint8_t *) (void *) n.a, (int64_t) std::strlen(n.a)})));
    }
    // kind 1: the topk; kind 2: the gate matmul; kind 3: a prefix that must not
    // be confused with either; kind 0: not a routing node.
    for (const char * s : names) {
        for (int kind = 0; kind <= 3; ++kind) {
            const char * lit = kind == 1 ? "ffn_moe_topk-" : kind == 2 ? "ffn_moe_logits-" : kind == 3 ? "ffn_moe_topk_softmax-" : "";
            Name n(s);
            const int nat = kind == 0 ? -1 : nat_rh::match_layer_node(n.a, lit);
            g_cases++;
            cmp((string("a_layer_node k") + std::to_string(kind) + " " + s).c_str(), std::to_string(nat),
                std::to_string(rh_match_layer_node(sl_u8{(uint8_t *) (void *) n.a, (int64_t) std::strlen(n.a)}, kind)));
        }
    }
}

static void case_tails() {
    struct T {
        const char * name;
        int from;
    } ts[] = {{"ffn_moe_topk-12", 13}, {"ffn_moe_topk-12abc", 13}, {"ffn_moe_topk-", 13},
              {"ffn_moe_topk-1048576", 13}, {"ffn_moe_topk-1048577", 13}, {"ffn_moe_topk-0", 13},
              {"ffn_moe_topk-007", 13}, {"ffn_moe_topk-+1", 13}, {"ffn_moe_topk-3", 14},
              {"ffn_moe_topk-3", 40}, {"", 0}, {"-999", 1}, {"ffn_moe_topk-2147483648", 13}};
    for (const T & t : ts) {
        Name n(t.name);
        const int nat = t.from >= 0 && t.from <= (int) std::strlen(n.a) ? nat_rh::parse_tail(n.a + t.from) : -1;
        g_cases++;
        cmp((string("a_tail ") + t.name + " @" + std::to_string(t.from)).c_str(), std::to_string(nat),
            std::to_string(rh_parse_tail(sl_u8{(uint8_t *) (void *) n.a, (int64_t) std::strlen(n.a)}, t.from)));
    }
}

static void case_weights_match() {
    const char * names[] = {"ffn_moe_weights-0",
                            "ffn_moe_weights_softmax-1",
                            "ffn_moe_weights_norm-2",
                            "ffn_moe_weights_scaled-3",
                            "ffn_moe_weights_sum-3",
                            "ffn_moe_weights_softmax_sum-1",
                            "ffn_moe_weights-",
                            "ffn_moe_weights-3x",
                            "ffn_moe_weights_scaled-",
                            "ffn_moe_weights_norm-1048577",
                            "ffn_moe_gate-2",
                            "ffn_moe_weights_scaledx-3",
                            "ffn_moe_weights3",
                            "ffn_moe_weights_norm2"};
    for (const char * s : names) {
        Name n(s);
        int il = -1;
        const int nat = nat_rh::match_weights(n.a, il);
        vector<int64_t> out = {-1};
        const int port = (int) rh_match_weights(sl_u8{(uint8_t *) (void *) n.a, (int64_t) std::strlen(n.a)}, sl(out));
        g_cases++;
        cmp((string("a_match_w ") + s).c_str(), fmti({nat, il}), fmti({port, out[0]}));
    }
}

static void case_iso_and_arming() {
    // weights_iso over the learning states of one layer.
    struct T {
        int variant, wl;
        vector<int> term;
    } ts[] = {{0, 0, {-1}},      {0, 0, {0}},       {0, 0, {2}},      {2, 0, {0}},      {0, 0, {}},
              {0, -1, {0}},      {3, 7, {0, 1, 2, 3, 4, 5, 6, 3}}, {-1, 0, {0}},    {1, 2, {0, 1, -1}}};
    for (const T & t : ts) {
        vector<int64_t> term;
        for (int x : t.term) term.push_back(x);
        g_cases++;
        cmp("a_weights_iso", std::to_string((int) nat_rh::weights_iso(t.variant, t.wl, t.term)),
            std::to_string(rh_weights_iso(t.variant, t.wl, csl(term))));
    }

    const float fracs[] = {0.0f, 0.5f, -0.25f, 1.0f};
    for (float f : fracs)
        for (int src = 0; src <= 1; ++src)
            for (int phase = 0; phase <= 1; ++phase)
                for (int pf = 0; pf <= 1; ++pf) {
                    g_cases++;
                    cmp("a_drop_armed", std::to_string((int) nat_rh::drop_armed(f, src != 0, phase, pf != 0)),
                        std::to_string(rh_drop_armed((double) f, src, phase, pf)));
                }
    for (float l : fracs)
        for (int src = 0; src <= 1; ++src)
            for (int phase = 0; phase <= 1; ++phase) {
                g_cases++;
                cmp("a_sub_armed", std::to_string((int) nat_rh::sub_armed(l, src != 0, phase)),
                    std::to_string(rh_sub_armed((double) l, src, phase)));
            }
    for (int tr = 0; tr <= 1; ++tr)
        for (int dr = 0; dr <= 1; ++dr) {
            g_cases++;
            cmp("a_want_weights", std::to_string((int) nat_rh::want_weights(tr != 0, dr != 0)),
                std::to_string(rh_want_weights(tr, dr)));
        }
    for (int moe = 0; moe <= 1; ++moe)
        for (int pr = 0; pr <= 1; ++pr)
            for (int src = 0; src <= 1; ++src)
                for (int phase = 0; phase <= 1; ++phase) {
                    g_cases++;
                    cmp("a_logits_scope", std::to_string((int) nat_rh::logits_scope(moe != 0, pr != 0, src != 0, phase)),
                        std::to_string(rh_logits_scope(moe, pr, src, phase)));
                }
}

static void case_ctrace_and_ask() {
    struct T {
        bool on, layers;
        int nl;
    } ts[] = {{false, false, 3}, {true, false, 3}, {true, true, 3}, {true, true, -1}, {true, true, 0}};
    for (const T & t : ts) {
        int cursor = -1;
        vector<int64_t> st = {-1};
        // Two nodes at a time: the cursor is part of the answer.
        for (int nl : {0, 0, 1, -1, 1, 2}) {
            int cn = cursor;
            const int nat = nat_rh::ctrace_iso(t.on, t.layers, nl == -1 ? nl : nl, cn) ? 1 : 0;
            cursor = cn;
            const int port = (int) rh_ctrace_iso(t.on ? 1 : 0, t.layers ? 1 : 0, nl, sl(st));
            g_cases++;
            cmp("a_ctrace", fmti({nat, cursor}), fmti({port, st[0]}));
        }
    }
    for (int a = 0; a <= 1; ++a)
        for (int b = 0; b <= 1; ++b)
            for (int c = 0; c <= 1; ++c)
                for (int d = 0; d <= 1; ++d)
                    for (int e = 0; e <= 1; ++e) {
                        g_cases++;
                        cmp("a_ask_iso", std::to_string((int) nat_rh::ask_iso(a, b, c, d, e)),
                            std::to_string(rh_ask_iso(a, b, c, d, e)));
                    }
}

static void case_capture_defer() {
    for (int p = -1; p <= 1; ++p)
        for (int r = 0; r <= 1; ++r)
            for (int ne2 = 0; ne2 <= 1; ++ne2) {
                g_cases++;
                cmp("a_capture_take", std::to_string(nat_rh::capture_take(p, r != 0, ne2)),
                    std::to_string(rh_capture_take(p, r, ne2)));
            }
    struct T {
        bool armed;
        int il;
        vector<int> term;
    } ts[] = {{true, 0, {0}}, {true, 0, {-1}}, {false, 0, {0}}, {true, 2, {0, 1, 2}}, {true, 3, {0, 1, 2}},
              {true, -1, {0}}};
    for (const T & t : ts) {
        vector<int64_t> term;
        for (int x : t.term) term.push_back(x);
        g_cases++;
        cmp("a_defer", std::to_string((int) nat_rh::defer_layer(t.armed, t.il, t.term)),
            std::to_string(rh_defer(t.armed ? 1 : 0, t.il, csl(term))));
    }
}

static void case_lifecycle() {
    // A schedule of topk arrivals and weight nodes for one run. The terminal of a
    // layer is learned from the last weight node seen, honoured while deferred,
    // and FORGOTTEN when the graph stops emitting it. Both sides are driven by the
    // same schedule; only the decisions are compared.
    struct Ev {
        int kind;       // 0 = topk of `layer`, 1 = weight node (layer, variant), 2 = close
        int layer;
        int variant;
        bool deferred;  // the topk's defer decision (kind 0)
        bool ready;     // the weight node's tensor is F32 and has data (kind 1)
    } script[] = {
        {0, 0, 0, false, true}, {1, 0, 0, false, true},   // learn variant 0 for layer 0
        {2, -1, 0, false, true},                          // next graph: nothing pending
        {0, 0, 0, false, true}, {1, 0, 0, false, false},  // a weight node with no readable data
        {2, -1, 0, false, true},
        {0, 1, 0, false, true}, {1, 1, 0, false, true}, {1, 1, 2, false, true}, // last wins
        {2, -1, 0, false, true},
        {0, 1, 0, false, true}, {2, -1, 0, false, true},  // layer 1 is now learned: variant 2
        {0, 1, 0, true, true}, {1, 1, 0, true, true},     // deferred, and the terminal is 2
        {1, 1, 2, true, true},                            // the terminal arrives: decide here
        {2, -1, 0, true, true},                           // and the record closes clean
        {0, 1, 0, true, true}, {2, -1, 0, true, true},    // deferred, terminal never arrives
        {0, 1, 0, true, true}, {1, 1, 0, true, true}, {2, -1, 0, true, true}, // re-learned
        {0, 3, 0, true, true}, {2, -1, 0, true, true},    // a layer past the term bank
        {0, 2, 0, false, true}, {1, 2, 3, false, true}, {2, -1, 0, false, true},
    };
    vector<int> nterm(4, -1);
    vector<int64_t> pterm = {-1, -1, -1, -1};
    int nlast = -1, npend = -1;
    bool ndef = false;
    vector<int64_t> plast = {-1};
    int ppend = -1;
    bool pdef = false;

    for (const Ev & e : script) {
        if (e.kind == 1) {
            g_cases++;
            cmp("a_observe", std::to_string(nat_rh::observe_weights(e.ready, e.layer, e.variant, npend, ndef,
                                                                   nterm, nlast)),
                std::to_string(rh_observe_weights(e.ready ? 1 : 0, e.layer, e.variant, ppend, pdef ? 1 : 0,
                                                  csl(pterm), sl(plast))));
            continue;
        }
        // The next topk (or the explicit close) ends the layer whose record is open.
        g_cases++;
        cmp("a_close", std::to_string(nat_rh::close_layer(nterm, npend, nlast, ndef)),
            std::to_string(rh_close_layer(sl(pterm), ppend, plast[0], pdef ? 1 : 0)));
        nlast = -1;
        plast[0] = -1;
        if (e.kind == 0) {
            npend = e.layer;
            ndef = e.deferred;
            ppend = e.layer;
            pdef = e.deferred;
        } else {
            npend = -1;
            ndef = false;
            ppend = -1;
            pdef = false;
        }
    }
    g_cases++;
    cmp("a_lifecycle_term", fmtv(nterm), fmtv(pterm));
}

static void case_workload() {
    for (int mode = 0; mode <= 2; ++mode)
        for (int shape = 0; shape <= 1; ++shape)
            for (int m = 0; m <= 1; ++m) {
                g_cases++;
                cmp("a_workload", std::to_string(nat_rh::workload(mode, shape != 0, m != 0)),
                    std::to_string(rh_workload(mode, shape, m)));
            }
}

static void case_topk_plans() {
    nat_rh::TopkIn ins[16];
    // 0: the default decode callback
    ins[0] = nat_rh::TopkIn{};
    // 1: substitution armed
    ins[1].sub = true;
    // 2: route-ahead in decode
    ins[2].route_ahead = 2;
    // 3: route-ahead in prefill (must not run)
    ins[3].route_ahead = 2;
    ins[3].phase = 0;
    // 4: workload capture, shape ok
    ins[4].workload = 1;
    // 5: workload capture, shape broken
    ins[5].workload = 1;
    ins[5].wl_shape_ok = false;
    // 6: replay that matches
    ins[6].workload = 2;
    // 7: replay that does not
    ins[7].workload = 2;
    ins[7].wl_matches = false;
    // 8: trace + drop + predict
    ins[8].trace = true;
    ins[8].drop_frac = 0.5f;
    ins[8].predict = true;
    // 9: deferred layer with everything else on
    ins[9] = ins[8];
    ins[9].defer = true;
    // 10: a prefill batch with a trace and a probe
    ins[10].trace = true;
    ins[10].predict_log = true;
    ins[10].phase = 0;
    // 11: an empty routing at the topk
    ins[11].ids = false;
    // 12: prefetch + record only
    ins[12].prefetch = 2;
    ins[12].record = true;
    // 13: probe scoring in decode
    ins[13].predict_log = true;
    ins[13].predict = true;
    // 14: everything at once
    ins[14] = ins[9];
    ins[14].sub = true;
    ins[14].route_ahead = 3;
    ins[14].workload = 2;
    // 15: deferred replay that does not match
    ins[15] = ins[7];
    ins[15].defer = true;
    for (nat_rh::TopkIn in : ins) {
        vector<int64_t> f = {in.sub ? 1 : 0,          in.route_ahead,     in.phase,
                             in.workload,             in.trace ? 1 : 0,   (int64_t) (in.drop_frac > 0.0f ? 1 : 0),
                             in.predict ? 1 : 0,      in.defer ? 1 : 0,   in.ids ? 1 : 0,
                             in.predict_log ? 1 : 0,  in.prefetch,        in.record ? 1 : 0,
                             in.wl_shape_ok ? 1 : 0,  in.wl_matches ? 1 : 0};
        vector<int64_t> out(32, -1);
        const int n = (int) rh_topk_plan(csl(f), sl(out));
        vector<long long> port;
        for (int i = 0; i < n; ++i) port.push_back(out[(size_t) i]);
        g_cases++;
        cmp("a_topk_plan", fmtv(nat_rh::topk_plan(in)), fmtv(port));
    }
    nat_rh::AskIn asks[8];
    asks[0] = nat_rh::AskIn{};                              // a plain node
    asks[1].capturing = true;                               // the capture pass
    asks[2].row_source = true;                              // row prep, no routing node
    asks[3].is_topk = true;                                 // the topk barrier
    asks[4].is_logits = true;                               // a gate matmul, probe on
    asks[4].gl = 3;
    asks[4].n_layer = 40;
    asks[4].predict_log = true;
    asks[5].is_logits = true;                               // a gate matmul for a layer past the model
    asks[5].gl = 40;
    asks[5].n_layer = 40;
    asks[6].weights_iso = true;                             // a weight node still being learned
    asks[6].ctrace_iso = true;                              // and a compute-trace boundary
    asks[7].row_source = true;                              // row prep plus the topk
    asks[7].is_topk = true;
    asks[7].is_logits = true;
    asks[7].gl = 0;
    asks[7].n_layer = 4;
    asks[7].predict_log = true;
    for (const nat_rh::AskIn & in : asks) {
        vector<int64_t> f = {in.capturing ? 1 : 0, in.row_source ? 1 : 0, in.is_logits ? 1 : 0,
                             in.gl < in.n_layer ? 1 : 0, in.ctrace_iso ? 1 : 0, in.is_topk ? 1 : 0,
                             in.weights_iso ? 1 : 0, in.predict_log ? 1 : 0};
        vector<int64_t> out(16, -1);
        const int n = (int) rh_ask_plan(csl(f), sl(out));
        vector<long long> port;
        for (int i = 0; i < n; ++i) port.push_back(out[(size_t) i]);
        g_cases++;
        cmp("a_ask_plan", fmtv(nat_rh::ask_plan(in)), fmtv(port));
    }
}

// ── slice B ──
static void case_waves() {
    for (int mask = 0; mask <= 7; ++mask) {
        g_cases++;
        cmp("b_p0", std::to_string(nat_ov::p0(mask)), std::to_string(ov_p0(mask)));
        for (int cfg = 0; cfg <= 1; ++cfg)
            for (int la = 0; la <= 1; ++la) {
                g_cases++;
                cmp("b_two_wave", std::to_string((int) nat_ov::two_wave(cfg != 0, la != 0, nat_ov::p0(mask))),
                    std::to_string(ov_two_wave(cfg, la, ov_p0(mask))));
                for (int p = 0; p < 3; ++p) {
                    g_cases++;
                    cmp("b_commit_only", std::to_string(nat_ov::commit_only(cfg && !la && nat_ov::p0(mask) >= 0,
                                                                          nat_ov::p0(mask), p)),
                        std::to_string(ov_commit_only(ov_two_wave(cfg, la, ov_p0(mask)), ov_p0(mask), p)));
                }
            }
    }
}

static void case_staged() {
    struct T {
        vector<int> ids;
        int n_expert;
        bool load_all;
    } ts[] = {{{3, 1, 3, 0, 1}, 8, false},
              {{-1, 8, 9, 2, 2}, 8, false},
              {{}, 8, false},
              {{7}, 8, true},
              {{5, 6}, 3, true},
              {{0, 1, 2, 3, 4, 5, 6, 7, 8}, 8, false},
              {{2, 2, 2}, 4, true}};
    for (const T & t : ts) {
        vector<int> ids = t.ids;
        vector<uint8_t> nseen((size_t) t.n_expert, 0), pseen((size_t) t.n_expert, 0);
        const vector<long long> nat = nat_ov::staged(ids, t.n_expert, t.load_all, nseen);
        vector<int64_t> cids;
        for (int x : ids) cids.push_back(x);
        vector<int64_t> out(64, -1);
        const int n = (int) ov_staged(csl(cids), t.n_expert, t.load_all ? 1 : 0, slu(pseen), sl(out));
        vector<long long> port;
        for (int i = 0; i < n; ++i) port.push_back(out[(size_t) i]);
        g_cases++;
        cmp("b_staged", fmtv(nat), fmtv(port));
    }
}

static void case_jobs() {
    const vector<int> staged = {1, 2, 5, 9};
    for (int sm = 0; sm < 16; ++sm) {
        vector<uint8_t> miss = {(uint8_t) (sm & 1), (uint8_t) ((sm >> 1) & 1), (uint8_t) ((sm >> 2) & 1),
                                (uint8_t) ((sm >> 3) & 1)};
        for (int mask = 0; mask <= 7; ++mask) {
            g_cases++;
            cmp("b_wave_jobs", std::to_string(nat_ov::wave_jobs(mask, miss, 4)),
                std::to_string(ov_wave_jobs(mask, cslu(miss), 4)));
            vector<int64_t> cst;
            for (int x : staged) cst.push_back(x);
            vector<int64_t> out(32, -1);
            const int n = (int) ov_wave_list(mask, csl(cst), 4, cslu(miss), sl(out));
            vector<long long> port;
            for (int i = 0; i < 2 * n; ++i) port.push_back(out[(size_t) i]);
            g_cases++;
            cmp("b_wave_list", fmtv(nat_ov::wave_list(mask, staged, miss)), fmtv(port));
        }
    }
    for (int ok = 0; ok <= 1; ++ok)
        for (int hit = 0; hit <= 1; ++hit) {
            g_cases++;
            cmp("b_stage_verdict", std::to_string(nat_ov::stage_verdict(ok != 0, hit != 0)),
                std::to_string(ov_stage_verdict(ok, hit)));
        }
    for (int reset = 0; reset <= 1; ++reset) {
        vector<int64_t> out(4, -2);
        const vector<long long> nat = nat_ov::publish(reset != 0, 9, 4);
        ov_publish(reset, 9, 4, sl(out));
        g_cases++;
        vector<long long> portv = {out[0], out[1], out[2]};
        cmp("b_publish", fmtv(nat), fmtv(portv));
    }
}

static void case_waits() {
    for (long long done = 0; done <= 4; done += 2)
        for (long long nj = 0; nj <= 4; nj += 2)
            for (int stop = 0; stop <= 1; ++stop) {
                g_cases++;
                cmp("b_drain_done", std::to_string((int) nat_ov::drain_done(done, nj, stop != 0)),
                    std::to_string(ov_drain_done(done, nj, stop)));
            }
    for (int stop = 0; stop <= 1; ++stop)
        for (long long bg = 3; bg <= 4; ++bg)
            for (long long seen = 3; seen <= 4; ++seen)
                for (long long ni = 1; ni <= 2; ++ni)
                    for (long long nj = 1; nj <= 2; ++nj)
                        for (long long sn = 0; sn <= 1; ++sn) {
                            g_cases++;
                            cmp("b_worker_wait",
                                std::to_string((int) nat_ov::worker_wait(stop != 0, bg, seen, ni, nj, sn, 1)),
                                std::to_string(ov_worker_wait(stop, bg, seen, ni, nj, sn, 1)));
                        }
    for (long long bg = 2; bg <= 3; ++bg)
        for (long long ni = 0; ni <= 3; ++ni) {
            g_cases++;
            cmp("b_drain_more", std::to_string((int) nat_ov::drain_more(bg, 2, ni, 2)),
                std::to_string(ov_drain_more(bg, 2, ni, 2)));
        }
}

static void case_gates() {
    for (int il = 0; il <= 1; ++il)
        for (int cur = 0; cur <= 1; ++cur)
            for (int ex = -1; ex <= 3; ++ex) {
                g_cases++;
                cmp("b_gate_scope", std::to_string(nat_ov::gate_scope(il, cur, ex, 3)),
                    std::to_string(ov_gate_scope(il, cur, ex, 3)));
            }
    for (int ours = 0; ours <= 1; ++ours)
        for (int fault = 0; fault <= 1; ++fault)
            for (unsigned fg = 0; fg <= 7; fg += 7)
                for (unsigned want = 0; want <= 7; want += 7) {
                    g_cases++;
                    cmp("b_gate_path", std::to_string(nat_ov::gate_path(ours != 0, fault != 0, fg, want)),
                        std::to_string(ov_gate_path(ours, fault, (long long) fg, (long long) want)));
                }
    for (int hf = 0; hf <= 1; ++hf)
        for (int w = 0; w <= 2; ++w) {
            g_cases++;
            cmp("b_publish_mode", std::to_string(nat_ov::publish_mode(hf != 0, w)),
                std::to_string(ov_publish_mode(hf, w)));
        }
    for (int ov = 0; ov <= 1; ++ov) {
        g_cases++;
        cmp("b_read_failure", std::to_string(nat_ov::read_failure(ov != 0)), std::to_string(ov_read_failure(ov)));
        g_cases++;
        cmp("b_commit_failure", std::to_string(nat_ov::commit_failure(ov != 0)),
            std::to_string(ov_commit_failure(ov)));
        g_cases++;
        cmp("b_quiesce_arg", std::to_string(nat_ov::quiesce_arg(ov != 0, 7)), std::to_string(ov_quiesce_arg(ov, 7)));
    }
}

static void case_stall_union() {
    // A script of thread events: (thread, enter/exit, timestamp).
    struct Ev {
        int op; // 1 enter, 0 exit
        long long t;
    } script[] = {{1, 100}, {1, 120}, {0, 200}, {1, 260}, {0, 310}, {0, 400}, {0, 500},
                  {1, 600}, {1, 610}, {1, 620}, {0, 700}, {0, 750}, {0, 900}};
    nat_ov::Union nat;
    vector<int64_t> st = {0, 0, 0};
    int snapshot_at = 3;
    for (int i = 0; i < (int) (sizeof(script) / sizeof(script[0])); ++i) {
        const Ev & e = script[i];
        int nv, pv;
        if (e.op == 1) {
            nv = nat.enter_at(e.t);
            pv = (int) ov_stall_enter(sl(st), e.t);
        } else {
            nv = nat.exit_at(e.t);
            pv = (int) ov_stall_exit(sl(st), e.t);
        }
        g_cases++;
        cmp("b_stall_edge", fmti({nv, nat.stalled, nat.total_ns}), fmti({pv, st[0], st[2]}));
        if (i == snapshot_at || i == 6 || i == 10) {
            g_cases++;
            cmp("b_stall_total", std::to_string(nat.total_at(e.t + 50)), std::to_string(ov_stall_total(csl(st), e.t + 50)));
        }
    }
    g_cases++;
    cmp("b_stall_final", std::to_string(nat.total_at(1000)), std::to_string(ov_stall_total(csl(st), 1000)));
}

// ── slice C ──
static void index_case(std::initializer_list<std::pair<int, int>> chain) {
    vector<int64_t> kinds, types;
    vector<nat_rs::Node> nodes;
    for (const auto & pr : chain) {
        nat_rs::Node n;
        n.op = pr.first;
        n.i32 = pr.second != 0;
        nodes.push_back(n);
        kinds.push_back(pr.first);
        types.push_back(pr.second);
    }
    g_cases++;
    cmp("c_index", std::to_string((int) nat_rs::index_materialized(nodes)),
        std::to_string(rs_index_materialized(csl(kinds), csl(types))));
}

static void case_index() {
    index_case({{0, 1}});                                        // a graph input of I32
    index_case({{0, 0}});                                        // a graph input of something else
    index_case({});                                              // a null index
    index_case({{1, 0}, {0, 1}});                                // a view of an input
    index_case({{1, 0}, {2, 0}, {0, 1}});                        // a reshape of a view
    index_case({{3, 0}, {4, 0}, {0, 1}});                        // permute then transpose
    index_case({{5, 1}});                                        // a computed node
    index_case({{1, 0}, {5, 0}});                                // a view of a computed node
    index_case({{1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {0, 1}});  // eight hops: admitted
    index_case({{1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {0, 1}}); // nine: refused
    index_case({{2, 0}, {3, 0}, {4, 0}, {1, 0}, {0, 0}});        // a deep chain ending non-I32
    for (int a = 0; a <= 1; ++a)
        for (int b = 0; b <= 1; ++b) {
            g_cases++;
            cmp("c_leaf", std::to_string((int) nat_rs::leaf_admit(a != 0, b != 0)), std::to_string(rs_leaf_admit(a, b)));
        }
}

static void case_refs() {
    for (int gr = 0; gr <= 1; ++gr)
        for (int slot = -1; slot <= 2; ++slot)
            for (int iok = 0; iok <= 1; ++iok) {
                g_cases++;
                cmp("c_ref", std::to_string(nat_rs::ref_verdict(gr != 0, slot, iok != 0)),
                    std::to_string(rs_ref_verdict(gr, slot, iok)));
            }
    for (int g = 0; g <= 3; ++g)
        for (int d = 0; d <= 2; ++d) {
            g_cases++;
            cmp("c_table_verdict", std::to_string((int) nat_rs::table_verdict(g, d)),
                std::to_string(rs_table_verdict(g, d)));
        }
}

static void case_session() {
    for (int ex = 0; ex <= 1; ++ex)
        for (int ht = 0; ht <= 1; ++ht)
            for (int hb = 0; hb <= 1; ++hb)
                for (int ho = 0; ho <= 1; ++ho)
                    for (int gg = 0; gg <= 1; ++gg)
                        for (int rw = 0; rw <= 1; ++rw) {
                            g_cases++;
                            cmp("c_session_take",
                                std::to_string(nat_rs::session_take(ex != 0, ht != 0, hb != 0, ho != 0, gg != 0, rw != 0)),
                                std::to_string(rs_session_take(ex, ht, hb, ho, gg, rw)));
                        }
    for (long long mb : {-1, 0, 1, 64, 2048}) {
        g_cases++;
        cmp("c_row_budget", std::to_string(nat_rs::row_budget(mb)), std::to_string(rs_row_budget(mb)));
    }
}

static void case_takeover() {
    struct T {
        bool present;
        long long size;
        bool file_ok;
        long long row_bytes, n_rows;
        bool reserve_ok;
    } ts[] = {{false, 100, true, 4, 10, true},   {true, 0, true, 4, 10, true},
              {true, 100, false, 4, 10, true},   {true, 100, true, 0, 10, true},
              {true, 100, true, 4, 0, true},     {true, 100, true, 4, 10, false},
              {true, 100, true, 4, 10, true},    {true, -8, true, 4, 10, true},
              {true, 1, false, 0, 0, false}};
    for (const T & t : ts) {
        g_cases++;
        cmp("c_init_accept", std::to_string(nat_rs::init_accept(t.present, t.size, t.file_ok, t.row_bytes,
                                                               t.n_rows, t.reserve_ok)),
            std::to_string(rs_init_accept(t.present, t.size, t.file_ok, t.row_bytes, t.n_rows, t.reserve_ok)));
    }
    for (long long n : {0, 1, 3}) {
        g_cases++;
        cmp("c_policy_on", std::to_string((int) nat_rs::policy_on(n)), std::to_string(rs_policy_on(n)));
        g_cases++;
        cmp("c_mat_need", std::to_string(nat_rs::materialize_need(false, n)),
            std::to_string(rs_materialize_need(0, n)));
        g_cases++;
        cmp("c_mat_need_whole", std::to_string(nat_rs::materialize_need(true, n)),
            std::to_string(rs_materialize_need(1, n)));
    }
}

static void case_dispatch() {
    for (int gr = 0; gr <= 1; ++gr)
        for (int sv = 0; sv <= 1; ++sv)
            for (int ir = 0; ir <= 1; ++ir) {
                g_cases++;
                cmp("c_dispatch", std::to_string(nat_rs::dispatch(gr != 0, sv != 0, ir != 0)),
                    std::to_string(rs_dispatch(gr, sv, ir)));
            }
    for (int r = 0; r <= 1; ++r) {
        g_cases++;
        cmp("c_prep_fatal", std::to_string(nat_rs::prep_fatal(r != 0)), std::to_string(rs_prep_fatal(r)));
    }
    struct T {
        long long n;
        vector<uint8_t> lru;
    } ts[] = {{0, {}}, {4, {1, 1, 1, 1}}, {4, {1, 0, 1, 0}}, {4, {0, 0, 0, 1}}, {6, {1, 1, 1, 1}}};
    for (const T & t : ts) {
        g_cases++;
        cmp("c_whole_unlink", std::to_string(nat_rs::whole_unlink(t.n, t.lru)),
            std::to_string(rs_whole_unlink(t.n, cslu(t.lru))));
    }
}

int main() {
    goose_init(0, nullptr);
    int ck0 = g_checks, ca0 = g_cases, fl0 = g_fails;
    case_names();
    case_tails();
    case_weights_match();
    case_iso_and_arming();
    case_ctrace_and_ask();
    case_capture_defer();
    case_lifecycle();
    case_workload();
    case_topk_plans();
    const int ckA = g_checks - ck0, caA = g_cases - ca0, flA = g_fails - fl0;

    ck0 = g_checks;
    ca0 = g_cases;
    fl0 = g_fails;
    case_waves();
    case_staged();
    case_jobs();
    case_waits();
    case_gates();
    case_stall_union();
    const int ckB = g_checks - ck0, caB = g_cases - ca0, flB = g_fails - fl0;

    ck0 = g_checks;
    ca0 = g_cases;
    fl0 = g_fails;
    case_index();
    case_refs();
    case_session();
    case_takeover();
    case_dispatch();
    const int ckC = g_checks - ck0, caC = g_cases - ca0, flC = g_fails - fl0;

    std::printf("FG_P9_04_A cases=%d checks=%d failures=%d\n", caA, ckA, flA);
    std::printf("FG_P9_04_B cases=%d checks=%d failures=%d\n", caB, ckB, flB);
    std::printf("FG_P9_04_C cases=%d checks=%d failures=%d\n", caC, ckC, flC);
    std::printf("FG_P9_04_HOOKS cases=%d checks=%d failures=%d\n", g_cases, g_checks, g_fails);
    return g_fails ? 1 : 0;
}
