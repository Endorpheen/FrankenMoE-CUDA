// Differential gate for monotone publication: chat_stream_step must refuse a
// candidate that common_chat_msg_diff::compute_diffs would reject against the
// last published message, keep the previous message, and report -6.
//
// No weights: the grammar is built in memory, the candidate comes from
// common_chat_parse, and the acceptance decision comes from the native diff.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <map>
#include <algorithm>

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

static int cases = 0, steps_n = 0, checks = 0, fails = 0;

static std::string vis(const std::string & s, size_t max = 90) {
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

struct Call { std::string name, args, id; };
struct Msg { std::string content, reasoning; std::vector<Call> calls; };

static Msg from_native(const common_chat_msg & m) {
    Msg r;
    r.content = m.content;
    r.reasoning = m.reasoning_content;
    for (const auto & c : m.tool_calls) r.calls.push_back({c.name, c.arguments, c.id});
    return r;
}

static common_chat_msg to_native(const Msg & m) {
    common_chat_msg r;
    r.role = "assistant";
    r.content = m.content;
    r.reasoning_content = m.reasoning;
    for (const auto & c : m.calls) r.tool_calls.push_back({c.name, c.args, c.id});
    return r;
}

static bool native_monotone(const Msg & a, const Msg & b) {
    try {
        common_chat_msg_diff::compute_diffs(to_native(a), to_native(b));
    } catch (const std::exception &) {
        return false;
    }
    return true;
}

struct Step { std::string chunk; bool is_final; };

static std::string bx(const std::vector<uint8_t> & v, int64_t off, int64_t len) {
    if (off < 0 || len < 0 || off + len > (int64_t) v.size()) return "<oob>";
    return std::string((const char *) v.data() + off, (size_t) len);
}

static void run_seq(const char * title, const common_peg_arena & arena, const Msg & seed,
                    const std::vector<Step> & steps, const common_chat_parser_params & params) {
    ++cases;
    printf("CASE %d %s\n", cases, title);

    std::vector<uint8_t> inbuf(1 << 16, 0), wcont(1 << 16, 0), wreas(1 << 16, 0), wtcb(1 << 16, 0);
    std::vector<uint8_t> rcont(1 << 16, 0), rreas(1 << 16, 0), rtcb(1 << 16, 0);
    std::vector<int64_t> mst(24, 0), rtab(6 * 64, 0), mout(3 + 6 * 64, 0), out(3 + 6 * 64, 0), st(8, 0);

    sl_i64 st_s{st.data(), (int64_t) st.size()};
    chat_stream_open(st_s, 0);

    // Seed the published message exactly as a previous step would leave it.
    rcont.assign(1 << 16, 0);
    std::copy(seed.content.begin(), seed.content.end(), rcont.begin());
    rreas.assign(1 << 16, 0);
    std::copy(seed.reasoning.begin(), seed.reasoning.end(), rreas.begin());
    rtcb.clear();
    rtab.assign(6 * 64, 0);
    for (size_t i = 0; i < seed.calls.size(); i++) {
        const auto & c = seed.calls[i];
        rtab[i * 6 + 0] = (int64_t) rtcb.size();
        rtab[i * 6 + 1] = (int64_t) c.name.size();
        rtcb.insert(rtcb.end(), c.name.begin(), c.name.end());
        rtab[i * 6 + 2] = (int64_t) rtcb.size();
        rtab[i * 6 + 3] = (int64_t) c.args.size();
        rtcb.insert(rtcb.end(), c.args.begin(), c.args.end());
        rtab[i * 6 + 4] = (int64_t) rtcb.size();
        rtab[i * 6 + 5] = (int64_t) c.id.size();
        rtcb.insert(rtcb.end(), c.id.begin(), c.id.end());
    }
    rtcb.resize(1 << 16, 0);
    st[2] = (int64_t) seed.content.size();
    st[3] = (int64_t) seed.reasoning.size();
    st[4] = (int64_t) seed.calls.size();

    Msg prev = seed;
    std::string text;

    int n = 0;
    for (const Step & s : steps) {
        ++n;
        ++steps_n;
        text += s.chunk;

        common_peg_parse_context ctx(text, COMMON_PEG_PARSE_FLAG_LENIENT);
        common_peg_parse_result res = arena.parse(ctx);

        // Native expectation: parse, then the diff decides acceptance.
        Msg expect;
        int64_t want_status;
        bool native_threw = false;
        bool candidate_ok = false;
        bool candidate_empty = false;
        try {
            common_chat_msg m = common_chat_parse(common_chat_input(text), !s.is_final, params);
            if (m.empty()) {
                candidate_empty = true;
                want_status = 1;
                expect = prev;
            } else {
                Msg cand = from_native(m);
                if (!native_monotone(prev, cand)) {
                    want_status = -6;
                    expect = prev;
                } else {
                    want_status = 0;
                    expect = cand;
                    candidate_ok = true;
                }
            }
        } catch (const std::exception &) {
            native_threw = true;
            want_status = -3;
            expect = prev;
        }

        // Flat AST for the ported step (same conversion as the other gates).
        std::vector<int64_t> a_tag, a_toff, a_tlen, a_cs, a_cc, achild, a_is, a_ic, inv, sdir, tid;
        std::vector<uint8_t> strs;
        std::map<std::string, int64_t> ids;
        auto sid = [&](const std::string & s) {
            auto it = ids.find(s);
            if (it != ids.end()) return it->second;
            int64_t id = (int64_t) ids.size();
            ids[s] = id;
            sdir.push_back((int64_t) strs.size());
            sdir.push_back((int64_t) s.size());
            strs.insert(strs.end(), s.begin(), s.end());
            return id;
        };
        const size_t nn = ctx.ast.size();
        a_tag.assign(nn, -1); a_toff.assign(nn, 0); a_tlen.assign(nn, 0);
        a_cs.assign(nn, 0); a_cc.assign(nn, 0); a_is.assign(nn, 0); a_ic.assign(nn, 0);
        for (size_t i = 0; i < nn; i++) {
            const auto & node = ctx.ast.get((common_peg_ast_id) i);
            if (!node.tag.empty()) a_tag[i] = sid(node.tag);
            a_toff[i] = (int64_t) node.start;
            a_tlen[i] = (int64_t) (node.end - node.start);
            a_cs[i] = (int64_t) achild.size();
            for (auto ch : node.children) achild.push_back((int64_t) ch);
            a_cc[i] = (int64_t) node.children.size();
            a_is[i] = (int64_t) inv.size() / 2;
            a_ic[i] = (int64_t) node.invalid_utf8.size();
            for (const auto & r : node.invalid_utf8) {
                inv.push_back((int64_t) r.pos);
                inv.push_back((int64_t) r.len);
            }
        }
        tid.assign(12, -1);
        const char * tnames[12] = {"reasoning", "content", "tool-open", "tool-close", "tool-name",
                                   "tool-id", "tool-args", "tool-arg-open", "tool-arg-close",
                                   "tool-arg-name", "tool-arg-value", "tool-arg-string-value"};
        for (int i = 0; i < 12; i++) {
            auto it = ids.find(tnames[i]);
            if (it != ids.end()) tid[i] = it->second;
        }
        std::vector<int64_t> roots;
        for (auto id : res.nodes) roots.push_back((int64_t) id);

        sl_i64 roots_s{roots.data(), (int64_t) roots.size()};
        sl_i64 a_tag_s{a_tag.data(), (int64_t) a_tag.size()};
        sl_i64 a_toff_s{a_toff.data(), (int64_t) a_toff.size()};
        sl_i64 a_tlen_s{a_tlen.data(), (int64_t) a_tlen.size()};
        sl_i64 a_cs_s{a_cs.data(), (int64_t) a_cs.size()};
        sl_i64 a_cc_s{a_cc.data(), (int64_t) a_cc.size()};
        sl_i64 achild_s{achild.data(), (int64_t) achild.size()};
        sl_i64 a_is_s{a_is.data(), (int64_t) a_is.size()};
        sl_i64 a_ic_s{a_ic.data(), (int64_t) a_ic.size()};
        sl_i64 inv_s{inv.data(), (int64_t) inv.size()};
        sl_i64 tid_s{tid.data(), (int64_t) tid.size()};
        sl_i64 mst_s{mst.data(), (int64_t) mst.size()};
        sl_u8 in_s{inbuf.data(), (int64_t) inbuf.size()};
        sl_u8 chunk_s{(uint8_t *) s.chunk.data(), (int64_t) s.chunk.size()};
        sl_u8 wcont_s{wcont.data(), (int64_t) wcont.size()};
        sl_u8 wreas_s{wreas.data(), (int64_t) wreas.size()};
        sl_u8 wtcb_s{wtcb.data(), (int64_t) wtcb.size()};
        sl_i64 mout_s{mout.data(), (int64_t) mout.size()};
        sl_u8 rcont_s{rcont.data(), (int64_t) rcont.size()};
        sl_u8 rreas_s{rreas.data(), (int64_t) rreas.size()};
        sl_u8 rtcb_s{rtcb.data(), (int64_t) rtcb.size()};
        sl_i64 rtab_s{rtab.data(), (int64_t) rtab.size()};
        sl_i64 out_s{out.data(), (int64_t) out.size()};

        int64_t status = chat_stream_step(
            st_s, in_s, chunk_s, s.is_final ? 1 : 0,
            (int64_t) res.type, (int64_t) res.end, roots_s,
            a_tag_s, a_toff_s, a_tlen_s, a_cs_s, a_cc_s, achild_s, a_is_s, a_ic_s, inv_s, tid_s,
            mst_s, wcont_s, wreas_s, wtcb_s, mout_s,
            rcont_s, rreas_s, rtcb_s, rtab_s, out_s);

        // The published message, read from the state the step left behind.
        Msg got;
        got.content = bx(rcont, 0, st[2]);
        got.reasoning = bx(rreas, 0, st[3]);
        for (int64_t i = 0; i < st[4]; i++) {
            got.calls.push_back({bx(rtcb, rtab[i * 6 + 0], rtab[i * 6 + 1]),
                                 bx(rtcb, rtab[i * 6 + 2], rtab[i * 6 + 3]),
                                 bx(rtcb, rtab[i * 6 + 4], rtab[i * 6 + 5])});
        }

        // out is the published message in the port's ABI layout.
        Msg got_out;
        got_out.content = bx(rcont, 0, out[0]);
        got_out.reasoning = bx(rreas, 0, out[1]);
        for (int64_t i = 0; i < out[2]; i++) {
            const int64_t b = 3 + i * 6;
            got_out.calls.push_back({bx(rtcb, out[b + 0], out[b + 1]),
                                     bx(rtcb, out[b + 2], out[b + 3]),
                                     bx(rtcb, out[b + 4], out[b + 5])});
        }

        std::string diff;
        ++checks;
        if (status != want_status) diff += " status=" + std::to_string(status) + " want=" + std::to_string(want_status);
        ++checks;
        if ((want_status == -6) != (st[6] < 0)) diff += " st6=" + std::to_string(st[6]);
        if (st[0] != ((s.is_final || status == -3) ? 1 : 0)) diff += " finished=" + std::to_string(st[0]);
        ++checks;
        if (got.content != expect.content) diff += " content";
        ++checks;
        if (got.reasoning != expect.reasoning) diff += " reasoning";
        ++checks;
        if (got.calls.size() != expect.calls.size()) diff += " ncalls";
        for (size_t i = 0; i < got.calls.size() && i < expect.calls.size(); i++) {
            checks += 3;
            if (got.calls[i].name != expect.calls[i].name) diff += " name" + std::to_string(i);
            if (got.calls[i].args != expect.calls[i].args) diff += " args" + std::to_string(i);
            if (got.calls[i].id != expect.calls[i].id) diff += " id" + std::to_string(i);
        }
        ++checks;
        if (st[5] != status) diff += " st_status";
        ++checks;
        if (got_out.content != got.content) diff += " out_content";
        ++checks;
        if (got_out.reasoning != got.reasoning) diff += " out_reasoning";
        ++checks;
        if (got_out.calls.size() != got.calls.size()) diff += " out_ncalls";
        for (size_t i = 0; i < got_out.calls.size() && i < got.calls.size(); i++) {
            checks += 3;
            if (got_out.calls[i].name != got.calls[i].name) diff += " out_name" + std::to_string(i);
            if (got_out.calls[i].args != got.calls[i].args) diff += " out_args" + std::to_string(i);
            if (got_out.calls[i].id != got.calls[i].id) diff += " out_id" + std::to_string(i);
        }

        printf("  step %d chunk=[%s] %s parse=%s native=%s status=%lld diff=%lld"
               " content=[%s] reasoning=[%s] ncalls=%lld",
               n, vis(s.chunk).c_str(), s.is_final ? "finish" : "feed",
               common_peg_parse_result_type_name(res.type),
               native_threw ? "throw" : (candidate_empty ? "empty" : (candidate_ok ? "accept" : "reject")),
               (long long) status, (long long) st[6], vis(got.content).c_str(),
               vis(got.reasoning).c_str(), (long long) got.calls.size());
        for (size_t i = 0; i < got.calls.size(); i++) {
            printf(" call%zu name=[%s] args=[%s]", i, vis(got.calls[i].name).c_str(),
                   vis(got.calls[i].args).c_str());
        }
        if (diff.empty()) {
            printf(" OK\n");
        } else {
            printf(" MISMATCH%s\n", diff.c_str());
            ++fails;
        }
        prev = got;
    }
}

static common_peg_arena json_parser() {
    return build_chat_peg_parser([](common_chat_peg_builder & p) {
        auto tool = p.tool(p.tool_open(p.literal("<tool_call>")) +
                           p.tool_name(p.chars("[a-zA-Z_]", 1, 64)) +
                           p.tool_args(p.until("</tool_call>")) +
                           p.tool_close(p.literal("</tool_call>")));
        return p.start() + p.content(p.until("<tool_call>")) + p.zero_or_more(tool) + p.end();
    });
}

static common_peg_arena reasoning_parser() {
    return build_chat_peg_parser([](common_chat_peg_builder & p) {
        auto reason = p.optional(p.literal("<think>") + p.reasoning(p.until("<｜end▁of▁thinking｜>")) +
                                 p.literal("<｜end▁of▁thinking｜>"));
        return p.start() + reason + p.content(p.until("<tool_call>")) + p.end();
    });
}

static common_peg_arena py_parser() {
    return build_chat_peg_parser([](common_chat_peg_builder & p) {
        auto arg = p.tool_arg(p.tool_arg_open(p.literal("<p>")) + p.tool_arg_name(p.until("=")) +
                              p.literal("=") + p.tool_arg_value(p.until(";")) +
                              p.tool_arg_close(p.literal(";")));
        return p.start() + p.content(p.until("<tool>")) +
               p.zero_or_more(p.tool(p.tool_open(p.literal("<tool>")) + p.tool_name(p.until("|")) +
                                     p.literal("|") + p.tool_args(p.zero_or_more(arg)) +
                                     p.tool_close(p.literal("</tool>")))) +
               p.end();
    });
}

int main() {
    goose_init(0, 0);

    auto run = [](const char * t, const common_peg_arena & a, const Msg & seed,
                  std::vector<Step> steps) {
        common_chat_parser_params p;
        p.format = COMMON_CHAT_FORMAT_PEG_SIMPLE;
        p.parser = a;
        run_seq(t, a, seed, steps, p);
    };

    const Msg empty;

    // 1: JSON arguments grow; every candidate is monotone.
    run("json args grow", json_parser(), empty, {
        {"Hi<tool_call>get_weather", false}, {"{\"city\":\"Par", false},
        {"is\"}", false}, {"</tool_call>", false}, {"", true},
    });

    // 2: python container split between backslash and quote: the second
    // candidate is not a prefix, so it is refused and the first message stays.
    run("python escape: rejected candidate, then finish", py_parser(), empty, {
        {"<tool>search|<p>arg={'a':'x\\", false}, {"'};</tool>", false}, {"", true},
    });

    // 3: the monotone python split inside the quote.
    run("python quote split: published", py_parser(), empty, {
        {"<tool>search|<p>arg={'a':'x", false}, {"y\"};</tool>", false}, {"", true},
    });

    // 4: seeded content is not a prefix of any candidate.
    run("seeded content mismatch", json_parser(), Msg{"zzz", "", {}}, {
        {"Hi", false}, {"!", false},
    });

    // 5: seeded call name is not a prefix of the candidate name.
    run("seeded name mismatch", json_parser(), Msg{"", "", {{"set_x", "", ""}}}, {
        {"Hi<tool_call>get_weather", false},
    });

    // 6: the candidate has fewer calls than the published message.
    run("seeded call count shrinks", json_parser(), Msg{"", "", {{"f", "", ""}, {"g", "", ""}}}, {
        {"Hi<tool_call>get_weather", false},
    });

    // 7: seeded reasoning is not a prefix of the candidate reasoning.
    run("seeded reasoning mismatch", reasoning_parser(), Msg{"", "zzz", {}}, {
        {"<think>plan", false},
    });

    // 8: an empty candidate keeps the published message (status 1, not a diff).
    run("empty candidate keeps the message", json_parser(), Msg{"Hi", "", {}}, {
        {"", false},
    });

    // 9: plain content, finish republishes the same message.
    run("plain content, finish", json_parser(), empty, {
        {"Hello", false}, {"", true},
    });

    // 10: a seeded prefix grows: the publication is allowed.
    run("seeded prefix grows", json_parser(), Msg{"H", "", {}}, {
        {"Hello", false}, {"", true},
    });

    printf("FG568_MONO cases=%d steps=%d values=%d failures=%d skipped=0\n", cases, steps_n, checks, fails);
    return fails ? 1 : 0;
}
