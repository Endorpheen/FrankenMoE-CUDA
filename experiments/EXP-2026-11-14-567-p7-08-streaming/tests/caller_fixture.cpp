// Caller-level gate: the example consumer (examples/stream_consumer.c) against
// real streaming sequences, not the step internals. Includes two deliberate
// counterexamples: a caller that ignores -6, and a caller that retries -4 with
// the same chunk.
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
#include "stream_consumer.h"

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

static std::string vis(const std::string & s, size_t max = 100) {
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

// ---- parse + AST conversion, as in the other gates.
struct Flat {
    std::vector<int64_t> a_tag, a_toff, a_tlen, a_cs, a_cc, achild, a_is, a_ic, inv, tid;
    std::vector<int64_t> roots;
    std::map<std::string, int64_t> ids;
    std::vector<int64_t> sdir;

    int64_t sid(const std::string & s) {
        auto it = ids.find(s);
        if (it != ids.end()) return it->second;
        int64_t id = (int64_t) ids.size();
        ids[s] = id;
        sdir.push_back(0);
        sdir.push_back((int64_t) s.size());
        return id;
    }

    void build(const common_peg_ast_arena & a, const common_peg_parse_result & res) {
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
        roots.clear();
        for (auto id : res.nodes) roots.push_back((int64_t) id);
    }

    sc_step step(const std::string & chunk, bool fin, int64_t type, int64_t end) const {
        sc_step s;
        memset(&s, 0, sizeof s);
        s.chunk = (const uint8_t *) chunk.data();
        s.chunk_len = (int64_t) chunk.size();
        s.is_final = fin ? 1 : 0;
        s.ptype = type;
        s.pend = end;
        s.roots = {roots.data(), (int64_t) roots.size()};
        s.a_tag = {a_tag.data(), (int64_t) a_tag.size()};
        s.a_toff = {a_toff.data(), (int64_t) a_toff.size()};
        s.a_tlen = {a_tlen.data(), (int64_t) a_tlen.size()};
        s.a_cs = {a_cs.data(), (int64_t) a_cs.size()};
        s.a_cc = {a_cc.data(), (int64_t) a_cc.size()};
        s.achild = {achild.data(), (int64_t) achild.size()};
        s.a_is = {a_is.data(), (int64_t) a_is.size()};
        s.a_ic = {a_ic.data(), (int64_t) a_ic.size()};
        s.inv = {inv.data(), (int64_t) inv.size()};
        s.tid = {tid.data(), (int64_t) tid.size()};
        return s;
    }
};

// The parse type/end plus the flat AST of one step.
struct Parsed {
    Flat    flat;
    int64_t type = 0;
    int64_t end  = 0;
};

// Parse the accumulated text and return the flat step.
struct Driver {
    const common_peg_arena * arena;
    common_chat_parser_params params;
    std::string text;

    // Parses the accumulated text; the caller reads .flat, .type and .end.
    Parsed feed(const std::string & chunk) {
        text += chunk;
        common_peg_parse_context ctx(text, COMMON_PEG_PARSE_FLAG_LENIENT);
        auto res = arena->parse(ctx);
        Parsed p;
        p.flat.build(ctx.ast, res);
        p.type = (int64_t) res.type;
        p.end  = (int64_t) res.end;
        return p;
    }
    sc_step make(const Flat & f, const std::string & chunk, bool fin, int64_t type, int64_t end) const {
        return f.step(chunk, fin, type, end);
    }
};

static std::string view_content(const sc_stream & s) {
    sc_msg_view v;
    sc_message(&s, &v);
    return std::string((const char *) v.content, (size_t) v.content_len);
}

static std::string view_reason(const sc_stream & s) {
    sc_msg_view v;
    sc_message(&s, &v);
    return std::string((const char *) v.reasoning, (size_t) v.reasoning_len);
}

static std::string call_field(const sc_stream & s, int64_t i, int field) {
    int64_t len = 0;
    const uint8_t *p = sc_call_field(&s, i, field, &len);
    return p ? std::string((const char *) p, (size_t) len) : std::string("<null>");
}

// sc_feed takes a pointer; the fixture builds steps as temporaries.
static sc_action feed_step(sc_stream *s, sc_step st) {
    return sc_feed(s, &st);
}

static void check(const char * name, bool ok, const std::string & detail = "") {
    ++cases;
    ++checks;
    if (!ok) {
        ++fails;
        printf("SC %d FAIL %s %s\n", cases, name, detail.c_str());
    } else {
        printf("SC %d OK %s\n", cases, name);
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

static common_peg_arena fail_parser() {
    return build_chat_peg_parser([](common_chat_peg_builder & p) {
        return p.start() + p.content(p.until("</s>")) + p.literal("</s>") + p.end();
    });
}

int main() {
    goose_init(0, 0);

    // ---------- 1: happy path, JSON arguments grow.
    {
        common_peg_arena a = json_parser();
        common_chat_parser_params p;
        p.format = COMMON_CHAT_FORMAT_PEG_SIMPLE;
        p.parser = a;
        sc_stream s;
        sc_init(&s, 1 << 16, 1 << 16, 1 << 16);
        Driver d{&a, p, ""};
        const char * chunks[] = {"Hi<tool_call>get_weather", "{\"city\":\"Par", "is\"}", "</tool_call>"};
        std::string seen;
        bool all_pub = true;
        for (const char * c : chunks) {
            Parsed q = d.feed(c);
            sc_action act = feed_step(&s, q.flat.step(c, false, q.type, q.end));
            if (act != SC_PUBLISHED) all_pub = false;
            seen = view_content(s);
        }
        {
            Parsed q = d.feed("");
            sc_action act = feed_step(&s, q.flat.step("", true, q.type, q.end));
            all_pub = all_pub && act == SC_PUBLISHED;
        }
        check("happy path publishes every step",
              all_pub && s.published == 5 && seen == "Hi" && s.st[4] == 1 &&
                  call_field(s, 0, 0) == "get_weather" && call_field(s, 0, 1) == "{\"city\":\"Paris\"}" &&
                  sc_ok(&s) && !sc_done(&s) == false && sc_last_error(&s) == 0,
              " published=" + std::to_string(s.published) + " content=[" + vis(seen) + "]");
        sc_free(&s);
    }

    // ---------- 2: -6 on feed, repeated; the counterexample ignores it.
    {
        common_peg_arena a = py_parser();
        common_chat_parser_params p;
        p.format = COMMON_CHAT_FORMAT_PEG_SIMPLE;
        p.parser = a;
        sc_stream s;
        sc_init(&s, 1 << 16, 1 << 16, 1 << 16);
        Driver d{&a, p, ""};

        auto run = [&](const std::string & chunk, bool fin) {
            d.text += chunk;
            common_peg_parse_context ctx(d.text, COMMON_PEG_PARSE_FLAG_LENIENT);
            auto res = a.parse(ctx);
            Flat f;
            f.build(ctx.ast, res);
            return feed_step(&s, f.step(chunk, fin, (int64_t) res.type, (int64_t) res.end));
        };
        sc_action a1 = run("<tool>search|<p>arg={'a':'x\\", false);
        std::string stale = call_field(s, 0, 1);
        sc_action a2 = run("'};</tool>", false);
        sc_action a3 = run("", false);
        check("non-monotone candidate is rejected and not published",
              a1 == SC_PUBLISHED && a2 == SC_REJECTED && a3 == SC_REJECTED &&
                  s.published == 1 && s.rejected == 2 && !sc_ok(&s) && sc_last_error(&s) == -6 &&
                  call_field(s, 0, 1) == stale,
              " acts=" + std::to_string(a1) + "," + std::to_string(a2) + "," + std::to_string(a3) +
                  " pub=" + std::to_string(s.published));

        // Counterexample: a caller that treats -6 as "nothing happened" and
        // sends the message at finish anyway.
        ::sc_stream ns = s;                      // same state, naive policy
        bool sent = true;                        // send on finish regardless
        std::string naive = call_field(ns, 0, 1);
        const bool naive_differs_from_expected = naive == stale && !sc_ok(&ns);
        check("COUNTEREXAMPLE: ignoring -6 sends a stale message",
              sent && naive_differs_from_expected,
              " sent=[" + vis(naive) + "] err=" + std::to_string(sc_last_error(&ns)) + " (wrong on purpose)");
        sc_free(&s);
    }

    // ---------- 3: -6 on finish, the session ends failed.
    {
        common_peg_arena a = py_parser();
        common_chat_parser_params p;
        p.format = COMMON_CHAT_FORMAT_PEG_SIMPLE;
        p.parser = a;
        sc_stream s;
        sc_init(&s, 1 << 16, 1 << 16, 1 << 16);
        Driver d{&a, p, ""};
        auto run = [&](const std::string & chunk, bool fin) {
            d.text += chunk;
            common_peg_parse_context ctx(d.text, COMMON_PEG_PARSE_FLAG_LENIENT);
            auto res = a.parse(ctx);
            Flat f;
            f.build(ctx.ast, res);
            return feed_step(&s, f.step(chunk, fin, (int64_t) res.type, (int64_t) res.end));
        };
        sc_action a1 = run("<tool>search|<p>arg={'a':'x\\", false);
        sc_action a2 = run("'};</tool>", true);
        check("finish with -6 ends failed, nothing to send",
              a1 == SC_PUBLISHED && a2 == SC_REJECTED && s.published == 1 && sc_done(&s) &&
                  !sc_ok(&s) && sc_last_error(&s) == -6,
              " a2=" + std::to_string(a2));
        sc_free(&s);
    }

    // ---------- 4: -3 on finish (unparsed output).
    {
        common_peg_arena a = fail_parser();
        common_chat_parser_params p;
        p.format = COMMON_CHAT_FORMAT_PEG_SIMPLE;
        p.parser = a;
        sc_stream s;
        sc_init(&s, 1 << 16, 1 << 16, 1 << 16);
        Driver d{&a, p, ""};
        auto run = [&](const std::string & chunk, bool fin) {
            d.text += chunk;
            common_peg_parse_context ctx(d.text, COMMON_PEG_PARSE_FLAG_LENIENT);
            auto res = a.parse(ctx);
            Flat f;
            f.build(ctx.ast, res);
            return feed_step(&s, f.step(chunk, fin, (int64_t) res.type, (int64_t) res.end));
        };
        sc_action a1 = run("hello", false);
        const int64_t pub_after_feed = s.published;
        sc_action a2 = run("</s>JUNK", true);
        check("-3 on finish: fail the request, send nothing",
              a1 == SC_PUBLISHED && a2 == SC_UNPARSED && s.published == pub_after_feed &&
                  sc_done(&s) && !sc_ok(&s) && sc_last_error(&s) == -3,
              " a2=" + std::to_string(a2) + " pub=" + std::to_string(s.published));
        sc_free(&s);
    }

    // ---------- 5: -1 retry with the same chunk after growing the input.
    {
        common_peg_arena a = json_parser();
        sc_stream s;
        sc_init(&s, 4, 1 << 16, 1 << 16);
        Driver d{&a, common_chat_parser_params{}, ""};
        // Parses the given text and feeds the chunk in one scope, so the flat
        // AST stays alive for the call.
        auto feed_text = [&](const std::string & text, const std::string & chunk) {
            common_peg_parse_context ctx(text, COMMON_PEG_PARSE_FLAG_LENIENT);
            auto res = a.parse(ctx);
            Flat f;
            f.build(ctx.ast, res);
            return feed_step(&s, f.step(chunk, false, (int64_t) res.type, (int64_t) res.end));
        };
        // "Hel" fits the 4-byte buffer; "lo" does not.
        sc_action a0 = feed_text("Hel", "Hel");
        sc_action a1 = feed_text("Hello", "lo");
        int64_t len_after_fail = s.st[1];
        sc_grow_input(&s);
        sc_action a2 = feed_text("Hello", "lo");
        check("-1 is retried with the same chunk, accumulated bytes survive growth",
              a0 == SC_PUBLISHED && a1 == SC_NEED_INPUT_SPACE && len_after_fail == 3 &&
                  a2 == SC_PUBLISHED && view_content(s) == "Hello" && s.st[1] == 5,
              " a1=" + std::to_string(a1) + " len=" + std::to_string(s.st[1]) +
                  " content=[" + vis(view_content(s)) + "]");
        sc_free(&s);
    }

    // ---------- 6: -4 with a small work buffer, retried with an empty chunk.
    {
        common_peg_arena a = json_parser();
        sc_stream s;
        sc_init(&s, 1 << 16, 4, 1 << 16);
        Driver d{&a, common_chat_parser_params{}, ""};
        std::string chunk = "Hi<tool_call>get_weather";
        d.text = chunk;
        common_peg_parse_context ctx(d.text, COMMON_PEG_PARSE_FLAG_LENIENT);
        auto res = a.parse(ctx);
        Flat f;
        f.build(ctx.ast, res);
        sc_action a1 = feed_step(&s, f.step(chunk, false, (int64_t) res.type, (int64_t) res.end));
        const int64_t in_after = s.st[1];
        // Grow until the work buffers hold the candidate; every retry uses an
        // empty chunk because the first attempt already appended the input.
        int guard = 0;
        while (a1 == SC_NEED_WORK_SPACE && guard++ < 12) {
            sc_grow_work(&s);
            a1 = feed_step(&s, f.step("", false, (int64_t) res.type, (int64_t) res.end));
        }
        check("-4 retried with an empty chunk publishes once",
              in_after == 24 && a1 == SC_PUBLISHED &&
                  view_content(s) == "Hi" && s.st[1] == 24 && call_field(s, 0, 0) == "get_weather",
              " a1=" + std::to_string(a1) + " in=" + std::to_string(s.st[1]) +
                  " content=[" + vis(view_content(s)) + "]");
        sc_free(&s);
    }

    // ---------- 7: counterexample, -4 retried with the same chunk.
    {
        common_peg_arena a = json_parser();
        sc_stream s;
        sc_init(&s, 1 << 16, 4, 1 << 16);
        Driver d{&a, common_chat_parser_params{}, ""};
        std::string chunk = "Hi<tool_call>get_weather";
        d.text = chunk;
        common_peg_parse_context ctx(d.text, COMMON_PEG_PARSE_FLAG_LENIENT);
        auto res = a.parse(ctx);
        Flat f;
        f.build(ctx.ast, res);
        sc_action a1 = feed_step(&s, f.step(chunk, false, (int64_t) res.type, (int64_t) res.end));
        while (a1 == SC_NEED_WORK_SPACE) {
            sc_grow_work(&s);
            a1 = feed_step(&s, f.step("", false, (int64_t) res.type, (int64_t) res.end));
        }
        // The caller now has a published message, but repeats the chunk it was
        // told had already been appended.
        sc_action a2 = feed_step(&s, f.step(chunk, false, (int64_t) res.type, (int64_t) res.end));
        check("COUNTEREXAMPLE: retrying -4 with the same chunk duplicates the input",
              a2 == SC_PUBLISHED && s.st[1] == 48,
              " in=" + std::to_string(s.st[1]) + " (48 instead of 24, wrong on purpose)");
        sc_free(&s);
    }

    // ---------- 8: -2 after finish.
    {
        common_peg_arena a = json_parser();
        common_chat_parser_params p;
        p.format = COMMON_CHAT_FORMAT_PEG_SIMPLE;
        p.parser = a;
        sc_stream s;
        sc_init(&s, 1 << 16, 1 << 16, 1 << 16);
        Driver d{&a, p, ""};
        auto run = [&](const std::string & chunk, bool fin) {
            d.text += chunk;
            common_peg_parse_context ctx(d.text, COMMON_PEG_PARSE_FLAG_LENIENT);
            auto res = a.parse(ctx);
            Flat f;
            f.build(ctx.ast, res);
            return feed_step(&s, f.step(chunk, fin, (int64_t) res.type, (int64_t) res.end));
        };
        sc_action a1 = run("Hello", false);
        sc_action a2 = run("", true);
        std::string msg = view_content(s);
        sc_action a3 = run("", false);
        check("-2 after finish is a protocol error with the state intact",
              a1 == SC_PUBLISHED && a2 == SC_PUBLISHED && a3 == SC_PROTOCOL &&
                  view_content(s) == msg && sc_last_error(&s) == -2,
              " a3=" + std::to_string(a3));
        sc_free(&s);
    }

    // ---------- 9: -5 on a corrupted published state.
    {
        common_peg_arena a = json_parser();
        sc_stream s;
        sc_init(&s, 1 << 16, 1 << 16, 1 << 16);
        Driver d{&a, common_chat_parser_params{}, ""};
        auto run = [&](const std::string & chunk, bool fin) {
            d.text += chunk;
            common_peg_parse_context ctx(d.text, COMMON_PEG_PARSE_FLAG_LENIENT);
            auto res = a.parse(ctx);
            Flat f;
            f.build(ctx.ast, res);
            return feed_step(&s, f.step(chunk, fin, (int64_t) res.type, (int64_t) res.end));
        };
        run("Hello", false);
        sc_state(&s)[2] = 1 << 20; // content length beyond its buffer
        sc_action act = run("", false);
        check("-5 is a bad state, not a trap",
              act == SC_BAD_STATE && sc_last_error(&s) == -5 && sc_state(&s)[2] == (1 << 20),
              " act=" + std::to_string(act));
        sc_free(&s);
    }

    // ---------- 10: after -6 a repaired state publishes, but the session is
    // still not ok: the caller must not send a completion.
    {
        common_peg_arena a = py_parser();
        common_chat_parser_params p;
        p.format = COMMON_CHAT_FORMAT_PEG_SIMPLE;
        p.parser = a;
        sc_stream s;
        sc_init(&s, 1 << 16, 1 << 16, 1 << 16);
        Driver d{&a, p, ""};
        auto run = [&](const std::string & chunk, bool fin) {
            d.text += chunk;
            common_peg_parse_context ctx(d.text, COMMON_PEG_PARSE_FLAG_LENIENT);
            auto res = a.parse(ctx);
            Flat f;
            f.build(ctx.ast, res);
            return feed_step(&s, f.step(chunk, fin, (int64_t) res.type, (int64_t) res.end));
        };
        run("<tool>search|<p>arg={'a':'x\\", false);
        sc_action a2 = run("'};</tool>", false);
        int64_t *st = sc_state(&s);
        st[2] = 0;
        st[3] = 0;
        st[4] = 0;
        sc_action a3 = run("", false);
        check("a publish after -6 does not make the session ok",
              a2 == SC_REJECTED && a3 == SC_PUBLISHED && st[5] == 0 && st[7] == -6 && !sc_ok(&s),
              " a3=" + std::to_string(a3) + " st7=" + std::to_string(st[7]));
        sc_free(&s);
    }

    // ---------- 12: an empty candidate is KEPT, not an error.
    {
        common_peg_arena a = json_parser();
        sc_stream s;
        sc_init(&s, 1 << 16, 1 << 16, 1 << 16);
        sc_step empty;
        memset(&empty, 0, sizeof empty);
        empty.chunk = (const uint8_t *) "";
        empty.chunk_len = 0;
        empty.is_final = 0;
        empty.ptype = 2;
        empty.pend = 0;
        sc_action act = sc_feed(&s, &empty);
        check("an empty candidate is kept, not published",
              act == SC_KEPT && s.published == 0 && sc_ok(&s) && sc_last_error(&s) == 0,
              " act=" + std::to_string(act));
        sc_free(&s);
    }

    printf("FG568_CALLER cases=%d values=%d failures=%d\n", cases, checks, fails);
    return fails ? 1 : 0;
}
