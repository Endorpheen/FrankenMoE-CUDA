// Contract gate: the published state is validated before it is read (-5, no
// trap), every status has a byte-exact name, and a successful step does not
// hide an earlier failure.
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

static int cases = 0, steps = 0, checks = 0, fails = 0;

struct Call { std::string name, args, id; };
struct Msg { std::string content, reasoning; std::vector<Call> calls; };

struct Env {
    std::vector<uint8_t> inbuf, wcont, wreas, wtcb, rcont, rreas, rtcb;
    std::vector<int64_t> mst, rtab, mout, out, st;
    Env()
        : inbuf(1 << 16, 0), wcont(1 << 16, 0), wreas(1 << 16, 0), wtcb(1 << 16, 0),
          rcont(1 << 16, 0), rreas(1 << 16, 0), rtcb(1 << 16, 0), mst(24, 0),
          rtab(6 * 64, 0), mout(3 + 6 * 64, 0), out(3 + 6 * 64, 0), st(9, 0) {}
};

static std::string bx(const std::vector<uint8_t> & v, int64_t off, int64_t len) {
    if (off < 0 || len < 0 || off + len > (int64_t) v.size()) return "<oob>";
    return std::string((const char *) v.data() + off, (size_t) len);
}

static void seed_state(Env & e, const Msg & m) {
    e.rcont.assign(1 << 16, 0);
    std::copy(m.content.begin(), m.content.end(), e.rcont.begin());
    e.rreas.assign(1 << 16, 0);
    std::copy(m.reasoning.begin(), m.reasoning.end(), e.rreas.begin());
    e.rtcb.assign(1 << 16, 0);
    std::fill(e.rtab.begin(), e.rtab.end(), 0);
    for (size_t i = 0; i < m.calls.size(); i++) {
        const auto & c = m.calls[i];
        e.rtab[i * 6 + 0] = (int64_t) e.rtcb.size(); // always 0 at first, then offset
        e.rtab[i * 6 + 1] = (int64_t) c.name.size();
        e.rtab[i * 6 + 2] = e.rtab[i * 6 + 0] + (int64_t) c.name.size();
        e.rtab[i * 6 + 3] = (int64_t) c.args.size();
        e.rtab[i * 6 + 4] = e.rtab[i * 6 + 2] + (int64_t) c.args.size();
        e.rtab[i * 6 + 5] = (int64_t) c.id.size();
    }
    // Rebuild the tool buffer in the order the table describes.
    size_t at = 0;
    for (size_t i = 0; i < m.calls.size(); i++) {
        const auto & c = m.calls[i];
        e.rtab[i * 6 + 0] = (int64_t) at;
        e.rtab[i * 6 + 1] = (int64_t) c.name.size();
        std::copy(c.name.begin(), c.name.end(), e.rtcb.begin() + at);
        at += c.name.size();
        e.rtab[i * 6 + 2] = (int64_t) at;
        e.rtab[i * 6 + 3] = (int64_t) c.args.size();
        std::copy(c.args.begin(), c.args.end(), e.rtcb.begin() + at);
        at += c.args.size();
        e.rtab[i * 6 + 4] = (int64_t) at;
        e.rtab[i * 6 + 5] = (int64_t) c.id.size();
        std::copy(c.id.begin(), c.id.end(), e.rtcb.begin() + at);
        at += c.id.size();
    }
    e.st[2] = (int64_t) m.content.size();
    e.st[3] = (int64_t) m.reasoning.size();
    e.st[4] = (int64_t) m.calls.size();
}

static Msg read_state(const Env & e) {
    Msg m;
    m.content = bx(e.rcont, 0, e.st[2]);
    m.reasoning = bx(e.rreas, 0, e.st[3]);
    for (int64_t i = 0; i < e.st[4]; i++) {
        m.calls.push_back({bx(e.rtcb, e.rtab[i * 6 + 0], e.rtab[i * 6 + 1]),
                           bx(e.rtcb, e.rtab[i * 6 + 2], e.rtab[i * 6 + 3]),
                           bx(e.rtcb, e.rtab[i * 6 + 4], e.rtab[i * 6 + 5])});
    }
    return m;
}

static std::string name_of(int64_t code) {
    std::vector<uint8_t> b(64, 0);
    sl_u8 s{b.data(), (int64_t) b.size()};
    int64_t n = chat_stream_error_name(code, s);
    return std::string((const char *) b.data(), (size_t) n);
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

struct FlatAst {
    std::vector<int64_t> a_tag, a_toff, a_tlen, a_cs, a_cc, achild, a_is, a_ic, inv, sdir, tid;
    std::vector<uint8_t> strs;
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

    void build(const common_peg_ast_arena & a) {
        const size_t n = a.size();
        a_tag.assign(n, -1); a_toff.assign(n, 0); a_tlen.assign(n, 0);
        a_cs.assign(n, 0); a_cc.assign(n, 0); a_is.assign(n, 0); a_ic.assign(n, 0);
        for (size_t i = 0; i < n; i++) {
            const auto & node = a.get((common_peg_ast_id) i);
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
        const char * names[12] = {"reasoning", "content", "tool-open", "tool-close", "tool-name",
                                  "tool-id", "tool-args", "tool-arg-open", "tool-arg-close",
                                  "tool-arg-name", "tool-arg-value", "tool-arg-string-value"};
        for (int i = 0; i < 12; i++) {
            auto it = ids.find(names[i]);
            if (it != ids.end()) tid[i] = it->second;
        }
    }
};

static const common_chat_parser_params * G_PARAMS = nullptr;
static std::string G_TEXT;

// One step with the current G_TEXT; the state is whatever the caller set up.
static int64_t step(Env & e, const std::string & chunk, bool is_final,
                    const common_peg_parse_result & res, FlatAst & f) {
    ++steps;
    std::vector<int64_t> roots;
    for (auto id : res.nodes) roots.push_back((int64_t) id);
    sl_i64 st_s{e.st.data(), (int64_t) e.st.size()};
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
    sl_i64 mst_s{e.mst.data(), (int64_t) e.mst.size()};
    sl_u8 in_s{e.inbuf.data(), (int64_t) e.inbuf.size()};
    sl_u8 chunk_s{(uint8_t *) chunk.data(), (int64_t) chunk.size()};
    sl_u8 wcont_s{e.wcont.data(), (int64_t) e.wcont.size()};
    sl_u8 wreas_s{e.wreas.data(), (int64_t) e.wreas.size()};
    sl_u8 wtcb_s{e.wtcb.data(), (int64_t) e.wtcb.size()};
    sl_i64 mout_s{e.mout.data(), (int64_t) e.mout.size()};
    sl_u8 rcont_s{e.rcont.data(), (int64_t) e.rcont.size()};
    sl_u8 rreas_s{e.rreas.data(), (int64_t) e.rreas.size()};
    sl_u8 rtcb_s{e.rtcb.data(), (int64_t) e.rtcb.size()};
    sl_i64 rtab_s{e.rtab.data(), (int64_t) e.rtab.size()};
    sl_i64 out_s{e.out.data(), (int64_t) e.out.size()};
    return chat_stream_step(st_s, in_s, chunk_s, is_final ? 1 : 0,
                            (int64_t) res.type, (int64_t) res.end, roots_s,
                            a_tag, a_toff, a_tlen, a_cs, a_cc, achild, a_is, a_ic, inv, tid,
                            mst_s, wcont_s, wreas_s, wtcb_s, mout_s,
                            rcont_s, rreas_s, rtcb_s, rtab_s, out_s);
}

static void check(const char * title, bool ok, const std::string & detail) {
    ++cases;
    if (!ok) {
        ++fails;
        printf("CASE %d FAIL %s%s\n", cases, title, detail.c_str());
    } else {
        printf("CASE %d OK %s\n", cases, title);
    }
}

// Manual-state case: corrupt one slot, expect -5 and no change to the message.
static void bad_state_case(const char * title, const Msg & seed,
                           const std::function<void(Env &)> & corrupt) {
    Env e;
    seed_state(e, seed);
    corrupt(e);
    const int64_t before_st[5] = {e.st[0], e.st[1], e.st[2], e.st[3], e.st[4]};
    const int64_t before_tab[6] = {e.rtab[0], e.rtab[1], e.rtab[2], e.rtab[3], e.rtab[4], e.rtab[5]};
    std::string text = "Hi";
    common_peg_parse_context ctx(text, COMMON_PEG_PARSE_FLAG_LENIENT);
    auto res = G_PARAMS->parser.parse(ctx);
    FlatAst f;
    f.build(ctx.ast);
    int64_t status = step(e, text, false, res, f);
    std::string d;
    ++checks;
    if (status != -5) d += " status=" + std::to_string(status);
    ++checks;
    if (e.st[7] != -5) d += " err=" + std::to_string(e.st[7]);
    ++checks;
    if (name_of(status) != "bad_state") d += " name=" + name_of(status);
    ++checks;
    for (int i = 0; i < 5; i++) {
        if (e.st[i] != before_st[i]) { d += " st" + std::to_string(i) + "_changed"; }
    }
    for (int i = 0; i < 6; i++) {
        if (e.rtab[i] != before_tab[i]) { d += " tab" + std::to_string(i) + "_changed"; }
    }
    check(title, d.empty(), d);
}

int main() {
    goose_init(0, 0);
    common_peg_arena arena = json_parser();
    common_chat_parser_params params;
    params.format = COMMON_CHAT_FORMAT_PEG_SIMPLE;
    params.parser = arena;
    G_PARAMS = &params;

    // ---- the name table is the contract, byte for byte.
    {
        std::string d;
        struct { int64_t code; const char * name; } tbl[] = {
            {0, "published"}, {1, "kept"}, {-1, "input_full"}, {-2, "after_finish"},
            {-3, "unparsed_output"}, {-4, "output_too_small"}, {-5, "bad_state"},
            {-6, "not_monotone"}, {-7, "unknown"}, {42, "unknown"},
        };
        for (auto & t : tbl) {
            ++checks;
            if (name_of(t.code) != t.name) {
                d += " [" + std::to_string(t.code) + "=" + name_of(t.code) + "]";
            }
        }
        check("status name table", d.empty(), d);
    }

    // ---- manual state: bad lengths, bad count, bad offsets, bad lengths.
    const Msg one_call{"", "", {{"f", "{}", "c1"}}};
    bad_state_case("content length beyond its buffer", one_call,
                   [](Env & e) { e.st[2] = (int64_t) e.rcont.size() + 1; });
    bad_state_case("reasoning length beyond its buffer", one_call,
                   [](Env & e) { e.st[3] = 70000; });
    bad_state_case("call table shorter than the count", Msg{"", "", {{"f", "{}", "c1"}, {"g", "{}", "c2"}}},
                   [](Env & e) { e.st[4] = 64 * 6 / 6 + 1; });
    bad_state_case("name offset beyond the tool buffer", one_call,
                   [](Env & e) { e.rtab[0] = 65530; e.rtab[1] = 100; });
    bad_state_case("arguments offset is negative", one_call,
                   [](Env & e) { e.rtab[2] = -1; });
    bad_state_case("id length is negative", one_call,
                   [](Env & e) { e.rtab[5] = -3; });
    bad_state_case("input length beyond its buffer", one_call,
                   [](Env & e) { e.st[1] = 70000; });
    bad_state_case("the second call's arguments are beyond the tool buffer",
                   Msg{"", "", {{"f", "{}", "c1"}, {"g", "{}", "c2"}}},
                   [](Env & e) { e.rtab[6 + 2] = 65000; e.rtab[6 + 3] = 1000; });

    // ---- a longer table than the count is legal.
    {
        Env e;
        seed_state(e, Msg{"", "", {{"get_", "", ""}}});
        // Junk beyond the call count must never be read.
        for (int i = 6; i < 12; i++) e.rtab[i] = (i % 2 == 0) ? -777 : 999999;
        std::string text = "Hi<tool_call>get_weather";
        common_peg_parse_context ctx(text, COMMON_PEG_PARSE_FLAG_LENIENT);
        auto res = params.parser.parse(ctx);
        FlatAst f;
        f.build(ctx.ast);
        int64_t status = step(e, text, false, res, f);
        std::string d;
        ++checks;
        if (status != 0) d += " status=" + std::to_string(status);
        ++checks;
        if (e.st[7] != 0) d += " err=" + std::to_string(e.st[7]);
        check("a longer table than the count is accepted", d.empty(), d);
    }

    // ---- repeated call after -6, then a success that must not hide it.
    {
        Env e;
        G_TEXT.clear();
        sl_i64 st_s{e.st.data(), (int64_t) e.st.size()};
        chat_stream_open(st_s, 0);
        seed_state(e, Msg{"zzz", "", {}});
        std::string d;

        auto one = [&](const std::string & chunk, bool fin, int64_t want) {
            G_TEXT += chunk;
            common_peg_parse_context ctx(G_TEXT, COMMON_PEG_PARSE_FLAG_LENIENT);
            auto res = params.parser.parse(ctx);
            FlatAst f;
            f.build(ctx.ast);
            int64_t status = step(e, chunk, fin, res, f);
            ++checks;
            if (status != want) d += " [" + chunk + " status=" + std::to_string(status) +
                                      " want=" + std::to_string(want) + "]";
            ++checks;
            if (e.st[5] != status) d += " st_status";
            return status;
        };

        one("Hi", false, -6);
        one("!", false, -6);
        ++checks;
        if (e.st[7] != -6) d += " err_after_repeats=" + std::to_string(e.st[7]);
        ++checks;
        if (name_of(e.st[7]) != "not_monotone") d += " err_name";

        // Repair the published state by hand and publish a monotone candidate:
        // the step reports 0 but the earlier failure stays recorded.
        e.st[2] = 0;
        e.st[3] = 0;
        e.st[4] = 0;
        one("Hi", false, 0);
        ++checks;
        if (e.st[5] != 0) d += " st_status_after_success";
        ++checks;
        if (e.st[7] != -6) d += " masked=" + std::to_string(e.st[7]);
        check("repeats after -6, success does not mask it", d.empty(), d);
    }

    // ---- finish after -6: still -6, finished set, then the protocol guard.
    {
        Env e;
        sl_i64 st_s{e.st.data(), (int64_t) e.st.size()};
        chat_stream_open(st_s, 0);
        seed_state(e, Msg{"zzz", "", {}});
        std::string d;
        std::string text = "Hi";
        common_peg_parse_context ctx(text, COMMON_PEG_PARSE_FLAG_LENIENT);
        auto res = params.parser.parse(ctx);
        FlatAst f;
        f.build(ctx.ast);
        ++checks;
        if (step(e, text, false, res, f) != -6) d += " feed";
        std::string fin;
        common_peg_parse_context ctx2(text, COMMON_PEG_PARSE_FLAG_LENIENT);
        auto res2 = params.parser.parse(ctx2);
        FlatAst f2;
        f2.build(ctx2.ast);
        ++checks;
        if (step(e, fin, true, res2, f2) != -6) d += " finish";
        ++checks;
        if (e.st[0] != 1) d += " finished=" + std::to_string(e.st[0]);
        ++checks;
        if (e.st[7] != -6) d += " err=" + std::to_string(e.st[7]);
        ++checks;
        if (step(e, "", false, res2, f2) != -2) d += " after_finish";
        ++checks;
        if (name_of(e.st[7]) != "after_finish") d += " err_name=" + name_of(e.st[7]);
        check("finish after -6, then the protocol guard", d.empty(), d);
    }

    // ---- open() clears the persistent error and every slot.
    {
        Env e;
        seed_state(e, Msg{"zzz", "", {}});
        e.st[7] = -6;
        e.st[0] = 1;
        sl_i64 st_s{e.st.data(), (int64_t) e.st.size()};
        chat_stream_open(st_s, 0);
        e.st[2] = 0;
        std::string d;
        ++checks;
        if (e.st[7] != 0 || e.st[0] != 0 || e.st[2] != 0 || e.st[3] != 0 || e.st[4] != 0 ||
            e.st[1] != 0 || e.st[5] != 0 || e.st[6] != 0) {
            d += " not_cleared";
        }
        check("open() clears the persistent error", d.empty(), d);
    }

    printf("FG568_CONTRACT cases=%d steps=%d values=%d failures=%d skipped=0\n",
           cases, steps, checks, fails);
    return fails ? 1 : 0;
}
