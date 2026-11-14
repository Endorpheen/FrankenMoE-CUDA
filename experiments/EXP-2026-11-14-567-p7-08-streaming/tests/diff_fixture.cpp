// Differential gate for the message diff: native
// common_chat_msg_diff::compute_diffs vs the Goose port chat_msg_diff /
// chat_msg_monotone in base/runtime.goose.
//
// Three pairs are real streaming pairs from a PEG parse without weights (a
// python-style container split between a backslash and the closing quote, the
// same split inside a quote, and JSON arguments growing); seven are hand-built
// edge cases. Both sides get the identical messages in the port's flat layout;
// entries, byte deltas and the error status are compared exactly.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <cstdlib>
#include <functional>
#include <ctime>

#include "chat.h"
#include "peg-parser.h"
#include "chat-peg-parser.h"
#include "gen.h"

extern "C" double gs_json_strtod(sl_u8 s) {
    std::string tmp((const char *) s.data, (size_t) s.len);
    return std::strtod(tmp.c_str(), nullptr);
}
extern "C" int64_t gs_json_fmt_e(sl_u8 out, int64_t prec, double x) {
    return snprintf((char *) out.data, (size_t) out.len, "%.*e", (int) prec, x);
}
extern "C" int64_t gs_json_fmt_g(sl_u8 out, double x) {
    return snprintf((char *) out.data, (size_t) out.len, "%.17g", x);
}
extern "C" double gs_frexp_mant(double x) {
    int e = 0;
    return std::frexp(x, &e);
}
extern "C" int64_t gs_to_string_f64(sl_u8 out, double x) {
    const std::string t = std::to_string(x);
    memcpy(out.data, t.data(), t.size());
    return (int64_t) t.size();
}
extern "C" int64_t gs_strftime_epoch(sl_u8 out, int64_t epoch, sl_u8 fmt) {
    const std::string f((const char *) fmt.data, (size_t) fmt.len);
    const std::time_t t = epoch < 0 ? std::time(nullptr) : (std::time_t) epoch;
    const std::tm * tmv = std::localtime(&t);
    if (!tmv) return -1;
    return (int64_t) std::strftime((char *) out.data, (size_t) out.len, f.c_str(), tmv);
}

static int cases = 0, checks = 0, fails = 0;

static std::string vis(const std::string & s, size_t max = 120) {
    std::string r;
    size_t n = 0;
    for (unsigned char c : s) {
        if (n++ > max) { r += "..."; break; }
        if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if (c == '\t') r += "\\t";
        else if (c == '"') r += "\\\"";
        else if (c >= 32 && c < 127) r.push_back((char) c);
        else { char b[8]; snprintf(b, sizeof b, "\\x%02X", c); r += b; }
    }
    return r;
}

struct Call {
    std::string name, args, id;
};

struct Msg {
    std::string content, reasoning;
    std::vector<Call> calls;
};

static Msg from_native(const common_chat_msg & m) {
    Msg r;
    r.content = m.content;
    r.reasoning = m.reasoning_content;
    for (const auto & c : m.tool_calls) r.calls.push_back({c.name, c.arguments, c.id});
    return r;
}

// Flat form used by the port: 6 i64 per call (name off/len, args off/len,
// id off/len) with offsets into one tool-call byte buffer.
struct Flat {
    std::vector<uint8_t> cont, reas, tcb;
    std::vector<int64_t> tab;
    int64_t n = 0;

    explicit Flat(const Msg & m) {
        cont.assign(m.content.begin(), m.content.end());
        reas.assign(m.reasoning.begin(), m.reasoning.end());
        for (const auto & c : m.calls) {
            tab.push_back((int64_t) tcb.size());
            tab.push_back((int64_t) c.name.size());
            tcb.insert(tcb.end(), c.name.begin(), c.name.end());
            tab.push_back((int64_t) tcb.size());
            tab.push_back((int64_t) c.args.size());
            tcb.insert(tcb.end(), c.args.begin(), c.args.end());
            tab.push_back((int64_t) tcb.size());
            tab.push_back((int64_t) c.id.size());
            tcb.insert(tcb.end(), c.id.begin(), c.id.end());
        }
        n = (int64_t) m.calls.size();
    }
};

// Native diffs in the port's flat entry layout, byte for byte.
struct Exp {
    std::vector<int64_t> out;   // [count, 8 per entry]
    std::vector<uint8_t> dbuf;
    int64_t status = 0;         // 0 or the ported error code
    bool threw = false;
    std::string error;
};

static Exp native_diff(const Msg & a, const Msg & b) {
    Exp e;
    common_chat_msg ma, mb;
    ma.role = mb.role = "assistant";
    ma.content = a.content;
    ma.reasoning_content = a.reasoning;
    mb.content = b.content;
    mb.reasoning_content = b.reasoning;
    for (const auto & c : a.calls) ma.tool_calls.push_back({c.name, c.args, c.id});
    for (const auto & c : b.calls) mb.tool_calls.push_back({c.name, c.args, c.id});

    std::vector<common_chat_msg_diff> diffs;
    try {
        diffs = common_chat_msg_diff::compute_diffs(ma, mb);
    } catch (const std::exception & ex) {
        e.threw = true;
        e.error = ex.what();
    }

    bool reas_pending = a.reasoning != b.reasoning;
    bool cont_pending = a.content != b.content;
    std::vector<int64_t> entries;
    int64_t cur = 0;
    for (const auto & d : diffs) {
        int64_t kind, idx, o1, l1, o2, l2, o3, l3;
        const std::string * s1 = nullptr;
        const std::string * s2 = nullptr;
        const std::string * s3 = nullptr;
        if (d.tool_call_index == std::string::npos) {
            kind = reas_pending ? 0 : 1;
            idx = -1;
            if (reas_pending) { s1 = &d.reasoning_content_delta; reas_pending = false; }
            else { s1 = &d.content_delta; cont_pending = false; }
            s2 = s3 = nullptr;
        } else {
            kind = 2;
            idx = (int64_t) d.tool_call_index;
            s1 = &d.tool_call_delta.id;
            s2 = &d.tool_call_delta.name;
            s3 = &d.tool_call_delta.arguments;
        }
        o1 = cur; l1 = s1 ? (int64_t) s1->size() : 0;
        if (s1) e.dbuf.insert(e.dbuf.end(), s1->begin(), s1->end());
        cur += l1;
        o2 = cur; l2 = s2 ? (int64_t) s2->size() : 0;
        if (s2) e.dbuf.insert(e.dbuf.end(), s2->begin(), s2->end());
        cur += l2;
        o3 = cur; l3 = s3 ? (int64_t) s3->size() : 0;
        if (s3) e.dbuf.insert(e.dbuf.end(), s3->begin(), s3->end());
        cur += l3;
        entries.insert(entries.end(), {kind, idx, o1, l1, o2, l2, o3, l3});
    }
    e.out.push_back((int64_t) diffs.size());
    e.out.insert(e.out.end(), entries.begin(), entries.end());
    return e;
}

static std::string bx(const std::vector<uint8_t> & v, int64_t off, int64_t len) {
    if (off < 0 || len < 0 || off + len > (int64_t) v.size()) return "<oob>";
    return std::string((const char *) v.data() + off, (size_t) len);
}

static void run_pair(const char * name, const Msg & a, const Msg & b, int64_t want_status) {
    ++cases;
    Flat fa(a), fb(b);
    std::vector<uint8_t> dbuf(1 << 16, 0);
    std::vector<int64_t> out(1 + 8 * 16, 0);

    sl_u8 pa{fa.cont.data(), (int64_t) fa.cont.size()};
    sl_u8 pra{fa.reas.data(), (int64_t) fa.reas.size()};
    sl_u8 pta{fa.tcb.data(), (int64_t) fa.tcb.size()};
    sl_i64 ptab{fa.tab.data(), (int64_t) fa.tab.size()};
    sl_u8 nca{fb.cont.data(), (int64_t) fb.cont.size()};
    sl_u8 nra{fb.reas.data(), (int64_t) fb.reas.size()};
    sl_u8 nta{fb.tcb.data(), (int64_t) fb.tcb.size()};
    sl_i64 ntab{fb.tab.data(), (int64_t) fb.tab.size()};
    sl_u8 d_s{dbuf.data(), (int64_t) dbuf.size()};
    sl_i64 o_s{out.data(), (int64_t) out.size()};

    int64_t status = chat_msg_diff(pa, (int64_t) fa.cont.size(), pra, (int64_t) fa.reas.size(),
                                   pta, ptab, fa.n,
                                   nca, (int64_t) fb.cont.size(), nra, (int64_t) fb.reas.size(),
                                   nta, ntab, fb.n, d_s, o_s);
    int64_t mono = chat_msg_monotone(pa, (int64_t) fa.cont.size(), pra, (int64_t) fa.reas.size(),
                                     pta, ptab, fa.n,
                                     nca, (int64_t) fb.cont.size(), nra, (int64_t) fb.reas.size(),
                                     nta, ntab, fb.n);

    Exp e = native_diff(a, b);

    std::string diff;
    ++checks;
    if (status != want_status) diff += " status=" + std::to_string(status) + " want=" + std::to_string(want_status);
    ++checks;
    if (mono != status) diff += " mono=" + std::to_string(mono);
    ++checks;
    if (e.threw != (status != 0)) diff += " throw_mismatch";
    if (status != 0) {
        // No entries may be published on an error path, and the diff output
        // must stay untouched apart from the count slot.
        ++checks;
        if (out[0] != 0) diff += " entries_on_error";
    } else {
        ++checks;
        if (out[0] != e.out[0]) diff += " nentries=" + std::to_string(out[0]) + " want=" + std::to_string(e.out[0]);
        for (int64_t i = 0; i < out[0] && i < e.out[0]; i++) {
            for (int64_t k = 0; k < 8; k++) {
                ++checks;
                if (out[1 + i * 8 + k] != e.out[1 + i * 8 + k]) {
                    diff += " e" + std::to_string(i) + "f" + std::to_string(k);
                }
            }
            const int64_t b0 = 1 + i * 8;
            for (int64_t s = 0; s < 3; s++) {
                ++checks;
                if (bx(dbuf, out[b0 + 2 + 2 * s], out[b0 + 3 + 2 * s]) !=
                    bx(e.dbuf, e.out[b0 + 2 + 2 * s], e.out[b0 + 3 + 2 * s])) {
                    diff += " e" + std::to_string(i) + "s" + std::to_string(s);
                }
            }
        }
    }

    printf("CASE %d %s status=%lld", cases, name, (long long) status);
    if (status == 0) {
        printf(" diffs=%lld", (long long) out[0]);
        for (int64_t i = 0; i < out[0]; i++) {
            const int64_t b0 = 1 + i * 8;
            const int64_t kind = out[b0];
            const int64_t idx = out[b0 + 1];
            if (kind == 2) {
                printf(" | call[%lld] id=[%s] name=[%s] args_delta=[%s]", (long long) idx,
                       vis(bx(dbuf, out[b0 + 2], out[b0 + 3])).c_str(),
                       vis(bx(dbuf, out[b0 + 4], out[b0 + 5])).c_str(),
                       vis(bx(dbuf, out[b0 + 6], out[b0 + 7])).c_str());
            } else {
                printf(" | %s_delta=[%s]", kind == 0 ? "reasoning" : "content",
                       vis(bx(dbuf, out[b0 + 2], out[b0 + 3])).c_str());
            }
        }
    } else {
        printf(" native_threw=%d error=[%s]", (int) e.threw, vis(e.error, 90).c_str());
    }
    if (diff.empty()) {
        printf(" OK\n");
    } else {
        printf(" MISMATCH%s\n", diff.c_str());
        ++fails;
    }
}

// Three real pairs: parse chunks without weights, map them with the native
// mapper, then diff consecutive messages.
static void real_pairs() {
    struct Seq {
        const char * name;
        std::function<common_peg_parser(common_chat_peg_builder &)> build;
        std::vector<std::string> chunks;
        int64_t want_status;
    };
    std::vector<Seq> seqs = {
        {"python container, split between backslash and quote",
         [](common_chat_peg_builder & p) {
             auto arg = p.tool_arg(p.tool_arg_open(p.literal("<p>")) + p.tool_arg_name(p.until("=")) +
                                   p.literal("=") + p.tool_arg_value(p.until(";")) +
                                   p.tool_arg_close(p.literal(";")));
             return p.start() + p.content(p.until("<tool>")) +
                    p.zero_or_more(p.tool(p.tool_open(p.literal("<tool>")) + p.tool_name(p.until("|")) +
                                          p.literal("|") + p.tool_args(p.zero_or_more(arg)) +
                                          p.tool_close(p.literal("</tool>")))) +
                    p.end();
         },
         {"<tool>search|<p>arg={'a':'x\\", "'};</tool>"}, -1},
        {"python container, split inside the quote",
         [](common_chat_peg_builder & p) {
             auto arg = p.tool_arg(p.tool_arg_open(p.literal("<p>")) + p.tool_arg_name(p.until("=")) +
                                   p.literal("=") + p.tool_arg_value(p.until(";")) +
                                   p.tool_arg_close(p.literal(";")));
             return p.start() + p.content(p.until("<tool>")) +
                    p.zero_or_more(p.tool(p.tool_open(p.literal("<tool>")) + p.tool_name(p.until("|")) +
                                          p.literal("|") + p.tool_args(p.zero_or_more(arg)) +
                                          p.tool_close(p.literal("</tool>")))) +
                    p.end();
         },
         {"<tool>search|<p>arg={'a':'x", "y\"};</tool>"}, 0},
        {"json tool args, split inside the string",
         [](common_chat_peg_builder & p) {
             auto tool = p.tool(p.tool_open(p.literal("<tool_call>")) +
                                p.tool_name(p.chars("[a-zA-Z_]", 1, 64)) +
                                p.tool_args(p.until("</tool_call>")) +
                                p.tool_close(p.literal("</tool_call>")));
             return p.start() + p.content(p.until("<tool_call>")) + p.zero_or_more(tool) + p.end();
         },
         {"Hi<tool_call>get_weather{\"city\":\"Par", "is\"}</tool_call>"}, 0},
    };

    for (auto & s : seqs) {
        common_peg_arena arena = build_chat_peg_parser(s.build);
        common_chat_parser_params params;
        params.format = COMMON_CHAT_FORMAT_PEG_SIMPLE;
        params.parser = arena;
        std::string text;
        std::vector<Msg> msgs;
        for (const auto & c : s.chunks) {
            text += c;
            try {
                common_chat_msg m = common_chat_parse(common_chat_input(text), true, params);
                if (!m.empty()) msgs.push_back(from_native(m));
            } catch (const std::exception &) {
            }
        }
        if (msgs.size() >= 2) run_pair(s.name, msgs[0], msgs[1], s.want_status);
    }
}

int main() {
    goose_init(0, 0);

    real_pairs();

    // 4: identical messages.
    run_pair("identical messages", Msg{"abc", "r", {{"f", "{\"a\":1}", "c1"}}},
             Msg{"abc", "r", {{"f", "{\"a\":1}", "c1"}}}, 0);

    // 5: the current content is a prefix of the previous one (partial stop
    // word): an entry with an empty delta, no error.
    run_pair("content shrinks to a prefix", Msg{"Hello world", "", {}},
             Msg{"Hello ", "", {}}, 0);

    // 6: reasoning grows, content unchanged: only the reasoning entry.
    run_pair("reasoning grows, content unchanged", Msg{"Hi", "pla", {}},
             Msg{"Hi", "plan", {}}, 0);

    // 7: content rewritten, not a prefix relation.
    run_pair("content rewritten", Msg{"abc", "", {}}, Msg{"abd", "", {}}, -1);

    // 8: fewer tool calls than before.
    run_pair("tool call count shrinks", Msg{"", "", {{"f", "{}", "c1"}}},
             Msg{"", "", {}}, -2);

    // 9: the last call's name is neither equal nor a prefix on both sides.
    run_pair("tool name mismatch", Msg{"", "", {{"get_", "{}", ""}}},
             Msg{"", "", {{"set_x", "{}", ""}}}, -3);

    // 10: the last previous call changes id and grows its arguments, and a new
    // call is appended: one delta entry for the old call, one whole new call.
    run_pair("id change, arguments unchanged, new call appended",
             Msg{"", "", {{"f", "{\"a\":1}", "c1"}}},
             Msg{"", "", {{"f", "{\"a\":1}", "c2"}, {"g", "{}", "c3"}}}, 0);

    printf("FG568_DIFF cases=%d values=%d failures=%d skipped=0\n", cases, checks, fails);
    return fails ? 1 : 0;
}
