// Differential streaming gate: native common_chat_parse (the call
// common_chat_session::feed/finish makes) vs the Goose ported step
// chat_stream_step / chat_stream_open in base/runtime.goose.
//
// No weights, no template, no vocab: the PEG grammar is built in memory by
// chat-peg-parser.cpp, and the same parse (type, end, AST) is passed to both
// sides. The Goose side maps the AST with the accepted peg_map_msg, applies the
// ported feed/finish policy and publishes the message; the native side keeps
// the previous message exactly as common_chat_session does.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <cstdlib>
#include <ctime>

#include "chat.h"
#include "peg-parser.h"
#include "chat-peg-parser.h"
#include "gen.h"

// goose stdlib externs (same set the other chat fixtures link)
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

static int cases = 0, steps = 0, checks = 0, fails = 0;

static std::string vis(const std::string & s, size_t max = 160) {
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

// Flat AST copy plus the tag-id table, as in the accepted mapper gate.
struct Flat {
    std::vector<int64_t> a_tag, a_toff, a_tlen, a_cs, a_cc, achild, a_is, a_ic, inv, sdir;
    std::vector<uint8_t> strs;
    std::vector<int64_t> tid;
    std::map<std::string, int64_t> ids;

    int64_t sid(const std::string & s) {
        auto it = ids.find(s);
        if (it != ids.end()) return it->second;
        int64_t id = (int64_t) ids.size();
        ids[s] = id;
        sdir.push_back((int64_t) strs.size());
        sdir.push_back((int64_t) s.size());
        strs.insert(strs.end(), s.begin(), s.end());
        return id;
    }

    void build(const common_peg_ast_arena & arena) {
        a_tag.clear(); a_toff.clear(); a_tlen.clear(); a_cs.clear(); a_cc.clear();
        achild.clear(); a_is.clear(); a_ic.clear(); inv.clear(); strs.clear();
        sdir.clear(); ids.clear();
        const size_t n = arena.size();
        a_tag.assign(n, -1);
        a_toff.assign(n, 0);
        a_tlen.assign(n, 0);
        a_cs.assign(n, 0);
        a_cc.assign(n, 0);
        a_is.assign(n, 0);
        a_ic.assign(n, 0);
        for (size_t i = 0; i < n; i++) {
            const auto & node = arena.get((common_peg_ast_id) i);
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
        const char * names[12] = {
            "reasoning", "content", "tool-open", "tool-close", "tool-name", "tool-id",
            "tool-args", "tool-arg-open", "tool-arg-close", "tool-arg-name",
            "tool-arg-value", "tool-arg-string-value",
        };
        for (int i = 0; i < 12; i++) {
            auto it = ids.find(names[i]);
            if (it != ids.end()) tid[i] = it->second;
        }
    }
};

struct Step {
    std::string chunk;
    bool        is_final;
    int         expect_status; // -99: do not check, else the exact port status
};

struct Case {
    const char * name;
    std::vector<Step> steps;
};

static common_peg_arena make_stream_parser() {
    return build_chat_peg_parser([](common_chat_peg_builder & p) {
        auto tool = p.tool(
            p.tool_open(p.literal("<tool_call>")) +
            p.tool_name(p.chars("[a-zA-Z_]", 1, 64)) +
            p.tool_args(p.until("</tool_call>")) +
            p.tool_close(p.literal("</tool_call>")));
        auto reason = p.optional(
            p.literal("<think>") + p.reasoning(p.until("</think>")) + p.literal("</think>"));
        return p.start() + reason + p.content(p.until("<tool_call>")) + p.zero_or_more(tool) + p.end();
    });
}

static std::string bx(const std::vector<uint8_t> & v, int64_t off, int64_t len) {
    if (off < 0 || len < 0 || off + len > (int64_t) v.size()) return "<oob>";
    return std::string((const char *) v.data() + off, (size_t) len);
}

// Native message mirror of the session: feed/finish + "keep if empty".
struct Native {
    std::string text;
    common_chat_msg result;
    bool failed = false;
    Native() { result.role = "assistant"; }
};

static void run(const Case & c, const common_peg_arena & arena, const common_chat_parser_params & params) {
    ++cases;
    Native nat;

    std::vector<uint8_t> inbuf(1 << 16, 0), wcont(1 << 16, 0), wreas(1 << 16, 0), wtcb(1 << 16, 0);
    std::vector<uint8_t> rcont(1 << 16, 0), rreas(1 << 16, 0), rtcb(1 << 16, 0);
    std::vector<int64_t> mst(24, 0), rtab(6 * 64, 0), moutv(3 + 6 * 64, 0), out(3 + 6 * 64, 0), st(8, 0);

    sl_i64 st_s{st.data(), (int64_t) st.size()};
    chat_stream_open(st_s, 0);

    printf("CASE %d %s\n", cases, c.name);

    int step_i = 0;
    for (const Step & s : c.steps) {
        ++step_i;
        ++steps;

        // ---- native: the parse of the accumulated text, then common_chat_parse
        nat.text += s.chunk;
        common_peg_parse_context ctx(nat.text, COMMON_PEG_PARSE_FLAG_LENIENT);
        common_peg_parse_result res = arena.parse(ctx);

        bool nat_threw = false;
        if (!nat.failed) {
            try {
                common_chat_msg m = common_chat_parse(common_chat_input(nat.text), !s.is_final, params);
                if (!m.empty()) nat.result = m;
            } catch (const std::exception &) {
                nat_threw = true;
                nat.failed = true;
            }
        }

        // ---- goose: the same parse facts into the ported step
        Flat f;
        f.build(ctx.ast);
        std::vector<int64_t> roots;
        for (auto id : res.nodes) roots.push_back((int64_t) id);

        sl_i64 roots_s{roots.data(), (int64_t) roots.size()};
        sl_i64 a_tag{f.a_tag.data(), (int64_t) f.a_tag.size()};
        sl_i64 a_toff{f.a_toff.data(), (int64_t) f.a_toff.size()};
        sl_i64 a_tlen{f.a_tlen.data(), (int64_t) f.a_tlen.size()};
        sl_i64 a_cs{f.a_cs.data(), (int64_t) f.a_cs.size()};
        sl_i64 a_cc{f.a_cc.data(), (int64_t) f.a_cc.size()};
        sl_i64 achild{f.achild.data(), (int64_t) f.achild.size()};
        sl_i64 a_is{f.a_is.data(), (int64_t) f.a_is.size()};
        sl_i64 a_ic{f.a_ic.data(), (int64_t) f.a_ic.size()};
        sl_i64 inv{f.inv.data(), (int64_t) f.inv.size()};
        sl_i64 tid{f.tid.data(), (int64_t) f.tid.size()};
        sl_i64 mst_s{mst.data(), (int64_t) mst.size()};
        sl_u8 in_s{inbuf.data(), (int64_t) inbuf.size()};
        sl_u8 chunk_s{(uint8_t *) s.chunk.data(), (int64_t) s.chunk.size()};
        sl_u8 wcont_s{wcont.data(), (int64_t) wcont.size()};
        sl_u8 wreas_s{wreas.data(), (int64_t) wreas.size()};
        sl_u8 wtcb_s{wtcb.data(), (int64_t) wtcb.size()};
        sl_i64 mout_s{moutv.data(), (int64_t) moutv.size()};
        sl_u8 rcont_s{rcont.data(), (int64_t) rcont.size()};
        sl_u8 rreas_s{rreas.data(), (int64_t) rreas.size()};
        sl_u8 rtcb_s{rtcb.data(), (int64_t) rtcb.size()};
        sl_i64 rtab_s{rtab.data(), (int64_t) rtab.size()};
        sl_i64 out_s{out.data(), (int64_t) out.size()};

        int64_t status = chat_stream_step(
            st_s, in_s, chunk_s, s.is_final ? 1 : 0,
            (int64_t) res.type, (int64_t) res.end, roots_s,
            a_tag, a_toff, a_tlen, a_cs, a_cc, achild, a_is, a_ic, inv, tid,
            mst_s, wcont_s, wreas_s, wtcb_s, mout_s,
            rcont_s, rreas_s, rtcb_s, rtab_s, out_s);

        const std::string g_content = bx(rcont, 0, out[0]);
        const std::string g_reason  = bx(rreas, 0, out[1]);
        const int64_t g_n = out[2];

        ++checks;
        checks += 3;
        std::string diff;
        if (s.expect_status != -99 && status != s.expect_status) {
            diff += " status=" + std::to_string(status) + " want=" + std::to_string(s.expect_status);
        }
        // The state copy of the status must agree with the returned one.
        checks += 1;
        if (st[5] != status) diff += " st_status";
        if (g_content != nat.result.content) diff += " content";
        if (g_reason != nat.result.reasoning_content) diff += " reasoning";
        if (g_n != (int64_t) nat.result.tool_calls.size()) diff += " ncalls";
        for (int64_t i = 0; i < g_n && i < (int64_t) nat.result.tool_calls.size(); i++) {
            const int64_t b = 3 + 6 * i;
            checks += 3;
            if (bx(rtcb, out[b + 0], out[b + 1]) != nat.result.tool_calls[i].name) diff += " name" + std::to_string(i);
            if (bx(rtcb, out[b + 2], out[b + 3]) != nat.result.tool_calls[i].arguments) diff += " args" + std::to_string(i);
            if (bx(rtcb, out[b + 4], out[b + 5]) != nat.result.tool_calls[i].id) diff += " id" + std::to_string(i);
        }
        // A native throw must be the ported error status, and the previous
        // message must survive it.
        if (nat_threw && status != -3) diff += " noerror";

        printf("  step %d chunk=[%s] %s parse=%s end=%zu roots=%zu status=%lld"
               " content=[%s] reasoning=[%s] ncalls=%lld",
               step_i, vis(s.chunk).c_str(), s.is_final ? "finish" : "feed",
               common_peg_parse_result_type_name(res.type), res.end, res.nodes.size(),
               (long long) status, vis(g_content).c_str(), vis(g_reason).c_str(), (long long) g_n);
        for (int64_t i = 0; i < g_n; i++) {
            const int64_t b = 3 + 6 * i;
            printf(" call%lld name=[%s] args=[%s]", (long long) i,
                   vis(bx(rtcb, out[b + 0], out[b + 1])).c_str(),
                   vis(bx(rtcb, out[b + 2], out[b + 3])).c_str());
        }
        if (diff.empty()) {
            printf(" OK\n");
        } else {
            printf(" MISMATCH%s\n", diff.c_str());
            ++fails;
        }
    }
}

// Extra check, outside the ten sequences: a FAIL whose end is 0 has no
// partial branch at all, so feed() is the runtime_error path too, and feed()
// leaves the session open because native sets `finished` only in finish().
static void run_fail_zero(const common_peg_arena & arena, const common_chat_parser_params & params) {
    printf("EXTRA fail-at-zero: feed, feed, finish\n");
    std::vector<uint8_t> inbuf(1 << 16, 0), wcont(1 << 16, 0), wreas(1 << 16, 0), wtcb(1 << 16, 0);
    std::vector<uint8_t> rcont(1 << 16, 0), rreas(1 << 16, 0), rtcb(1 << 16, 0);
    std::vector<int64_t> mst(24, 0), rtab(6 * 64, 0), moutv(3 + 6 * 64, 0), out(3 + 6 * 64, 0), st(8, 0);
    sl_i64 st_s{st.data(), (int64_t) st.size()};
    chat_stream_open(st_s, 0);

    const char * chunks[3] = {"y", "", ""};
    const int finals[3] = {0, 0, 1};
    const int want[3] = {-3, -3, -3};
    std::string text;
    int bad = 0;
    for (int i = 0; i < 3; i++) {
        text += chunks[i];
        ++steps;
        ++checks;
        ++checks; // result content, reasoning and call count are all empty here
        checks += 2;
        common_peg_parse_context ctx(text, COMMON_PEG_PARSE_FLAG_LENIENT);
        common_peg_parse_result res = arena.parse(ctx);
        Flat f;
        f.build(ctx.ast);
        std::vector<int64_t> roots;
        for (auto id : res.nodes) roots.push_back((int64_t) id);
        sl_i64 roots_s{roots.data(), (int64_t) roots.size()};
        sl_i64 a_tag{f.a_tag.data(), (int64_t) f.a_tag.size()};
        sl_i64 a_toff{f.a_toff.data(), (int64_t) f.a_toff.size()};
        sl_i64 a_tlen{f.a_tlen.data(), (int64_t) f.a_tlen.size()};
        sl_i64 a_cs{f.a_cs.data(), (int64_t) f.a_cs.size()};
        sl_i64 a_cc{f.a_cc.data(), (int64_t) f.a_cc.size()};
        sl_i64 achild{f.achild.data(), (int64_t) f.achild.size()};
        sl_i64 a_is{f.a_is.data(), (int64_t) f.a_is.size()};
        sl_i64 a_ic{f.a_ic.data(), (int64_t) f.a_ic.size()};
        sl_i64 inv{f.inv.data(), (int64_t) f.inv.size()};
        sl_i64 tid{f.tid.data(), (int64_t) f.tid.size()};
        sl_i64 mst_s{mst.data(), (int64_t) mst.size()};
        sl_u8 in_s{inbuf.data(), (int64_t) inbuf.size()};
        sl_u8 chunk_s{(uint8_t *) chunks[i], (int64_t) strlen(chunks[i])};
        sl_u8 wcont_s{wcont.data(), (int64_t) wcont.size()};
        sl_u8 wreas_s{wreas.data(), (int64_t) wreas.size()};
        sl_u8 wtcb_s{wtcb.data(), (int64_t) wtcb.size()};
        sl_i64 mout_s{moutv.data(), (int64_t) moutv.size()};
        sl_u8 rcont_s{rcont.data(), (int64_t) rcont.size()};
        sl_u8 rreas_s{rreas.data(), (int64_t) rreas.size()};
        sl_u8 rtcb_s{rtcb.data(), (int64_t) rtcb.size()};
        sl_i64 rtab_s{rtab.data(), (int64_t) rtab.size()};
        sl_i64 out_s{out.data(), (int64_t) out.size()};
        int64_t status = chat_stream_step(st_s, in_s, chunk_s, finals[i],
            (int64_t) res.type, (int64_t) res.end, roots_s,
            a_tag, a_toff, a_tlen, a_cs, a_cc, achild, a_is, a_ic, inv, tid,
            mst_s, wcont_s, wreas_s, wtcb_s, mout_s, rcont_s, rreas_s, rtcb_s, rtab_s, out_s);
        bool threw = false;
        try {
            common_chat_parse(common_chat_input(text), finals[i] == 0, params);
        } catch (const std::exception &) {
            threw = true;
        }
        const int64_t fin_after = st[0];
        const int want_fin = (i == 2 || !threw) ? 1 : 0;
        bool ok = status == want[i] && out[0] == 0 && out[1] == 0 && out[2] == 0 &&
                  st[5] == status && threw && fin_after == want_fin;
        printf("  step %d parse=%s end=%zu status=%lld finished=%lld native_threw=%d %s\n",
               i + 1, common_peg_parse_result_type_name(res.type), res.end,
               (long long) status, (long long) fin_after, (int) threw, ok ? "OK" : "MISMATCH");
        if (!ok) ++bad;
    }
    if (bad) ++fails;
}

int main() {
    goose_init(0, 0);

    common_peg_arena arena = make_stream_parser();
    common_chat_parser_params params;
    params.format = COMMON_CHAT_FORMAT_PEG_SIMPLE;
    params.parser = arena;

    const int X = -99;

    // 1: plain content, an empty chunk at the start and in the middle, finish.
    run({"plain content, empty chunk, final finish", {
        {"", false, X}, {"Hel", false, X}, {"lo", false, X}, {" world", false, X},
        {"", false, X}, {"", true, X},
    }}, arena, params);

    // 2: split in the middle of the opening reasoning tag.
    run({"split inside the opening reasoning tag", {
        {"<thi", false, X}, {"nk>", false, X}, {"pla", false, X}, {"n", false, X},
        {"</think>", false, X}, {"Hi", false, X}, {"", true, X},
    }}, arena, params);

    // 3: split inside a multi-byte UTF-8 character.
    run({"split inside a multi-byte character", {
        {"<think>\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1", false, X},
        {std::string("\x82"), false, X},
        {"</think>", false, X},
        {"", true, X},
    }}, arena, params);

    // 4: split inside the closing reasoning tag.
    run({"split inside the closing reasoning tag", {
        {"<think>plan</thi", false, X}, {"nk", false, X}, {">", false, X},
        {"Hi", false, X}, {"", true, X},
    }}, arena, params);

    // 5: split inside the tool-call marker.
    run({"split inside the tool-call marker", {
        {"Hi<tool_c", false, X}, {"all", false, X}, {">", false, X}, {"get_wea", false, X},
        {"ther", true, X},
    }}, arena, params);

    // 6: name growth, an empty chunk, then a split inside the argument string.
    run({"name growth, empty chunk, split inside the quote", {
        {"Hi<tool_call>get_weather", false, X}, {"", false, X},
        {"{\"city\":\"Par", false, X}, {"is\"}", true, X},
    }}, arena, params);

    // 7: complete arguments, a split inside the tool closing tag, finish.
    run({"split inside the tool closing tag, finish", {
        {"Hi<tool_call>get_weather", false, X}, {"{\"city\":\"Paris\"}", false, X},
        {"</tool_ca", false, X}, {"ll>", true, X},
    }}, arena, params);

    // 8: finish after plain content.
    run({"finish after plain content", {
        {"Hello world", false, X}, {"", true, X},
    }}, arena, params);

    // 9: unparseable tail: feed freezes, finish is the native throw.
    run({"unparseable tail: partial FAIL then final error", {
        {"Hi<tool_call>f{\"a\":1}</tool_call>", false, X},
        {"oops", false, 1},
        {"", true, -3},
    }}, arena, params);

    // 10: protocol errors.
    run({"protocol: feed and finish after finish", {
        {"Hello", false, X}, {"", true, X}, {"", false, -2}, {"", true, -2},
    }}, arena, params);

    {
        common_peg_arena a0 = build_chat_peg_parser([](common_chat_peg_builder & p) {
            return p.start() + p.literal("<x>") + p.content(p.rest()) + p.end();
        });
        common_chat_parser_params p0;
        p0.format = COMMON_CHAT_FORMAT_PEG_SIMPLE;
        p0.parser = a0;
        run_fail_zero(a0, p0);
    }

    printf("FG568_STREAM cases=%d steps=%d values=%d failures=%d skipped=0\n",
           cases, steps, checks, fails);
    return fails ? 1 : 0;
}
