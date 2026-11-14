// Differential fixture: builds the same hand-made ASTs as probe.cpp in the
// flat arena ABI of draft.goose, runs peg_map_gemma4 / peg_map_minimax, and
// compares the dumped message with the native reference strings.
#include "runtime-gen.h"
#include "json.h"

#include <iostream>
#include <string>
#include <ctime>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <vector>

using namespace std;

static int cases = 0;

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


struct Node {
    int64_t         rule, tag;
    size_t          off, len;
    bool            part;
    vector<int64_t> ch;
};

struct A {
    string         in;
    vector<Node>   ns;
    vector<int64_t> roots;

    size_t at(const string & s) const {
        size_t p = in.find(s);
        if (p == string::npos) {
            cerr << "text not found: [" << s << "]\n";
            exit(2);
        }
        return p;
    }
    int64_t n_at(int64_t rule, int64_t tag, size_t start, size_t len, vector<int64_t> ch, bool part = false) {
        ns.push_back({ rule, tag, start, len, part, std::move(ch) });
        return (int64_t) ns.size() - 1;
    }
    int64_t n(int64_t rule, int64_t tag, const string & text, vector<int64_t> ch, bool part = false) {
        return n_at(rule, tag, at(text), text.size(), std::move(ch), part);
    }
};

// gemma4 ids
enum { G_REASONING, G_CONTENT, G_TOOL, G_TOOL_NAME, G_TOOL_ARGS, G_TAGS,
       X_G_TOOL_OPEN, X_G_TOOL_ID };
enum { GR_NUMBER, GR_BOOL, GR_NULL, GR_STRING_CONTENT, GR_STRING, GR_ARRAY, GR_KEY_NAME, GR_KEY, GR_KV, GR_DICT, GR_VALUE };
// minimax ids
enum { M_REASONING, M_CONTENT, M_TOOL, M_TOOL_NAME, M_ARG, M_ARG_NAME, M_ARG_VALUE, M_ARG_STRING_VALUE, M_ARG_OBJECT,
       M_ARG_ARRAY, M_ARG_ITEM, M_TAGS_N,
       X_TOOL_OPEN, X_TOOL_CLOSE, X_TOOL_ID, X_ARG_OPEN, X_ARG_CLOSE };

static string dump(const string & cont, const string & reas, const vector<string> & calls) {
    common_json m = common_json::object();
    m["role"] = "assistant";
    if (!cont.empty()) {
        m["content"] = cont;
    }
    if (!reas.empty()) {
        m["reasoning_content"] = reas;
    }
    common_json a = common_json::array();
    for (const auto & c : calls) {
        // name \x1f arguments \x1f id
        size_t p1 = c.find('\x1f');
        size_t p2 = c.find('\x1f', p1 + 1);
        common_json o = common_json::object();
        o["name"]      = c.substr(0, p1);
        o["arguments"] = c.substr(p1 + 1, p2 - p1 - 1);
        o["id"]        = c.substr(p2 + 1);
        a.push_back(o);
    }
    m["tool_calls"] = a;
    return m.dump();
}

static sl_i64 S(vector<int64_t> & v) { return sl_i64{ v.data(), (int64_t) v.size() }; }
static sl_u8 U(vector<uint8_t> & v) { return sl_u8{ v.data(), (int64_t) v.size() }; }

static bool run(const string & name, A & a, bool gemma, const string & want) {
    vector<int64_t> a_rule, a_tag, a_toff, a_tlen, a_part, a_cs, a_cc, achild, a_is, a_ic, inv;
    for (auto & x : a.ns) {
        a_rule.push_back(x.rule);
        a_tag.push_back(x.tag);
        a_toff.push_back((int64_t) x.off);
        a_tlen.push_back((int64_t) x.len);
        a_part.push_back(x.part ? 1 : 0);
        a_cs.push_back((int64_t) achild.size());
        a_cc.push_back((int64_t) x.ch.size());
        for (auto c : x.ch) {
            achild.push_back(c);
        }
        a_is.push_back(0);
        a_ic.push_back(0);
    }
    // the port indexes state and out without a size check for the fixed slots
    vector<uint8_t> cont(1 << 16), reas(1 << 16), tcb(1 << 18);
    vector<int64_t> st(8, 0), out(3 + 6 * 256, 0);
    vector<int64_t> rid, tid;
    vector<uint8_t> inb(a.in.begin(), a.in.end());
    if (gemma) {
        rid = { GR_NUMBER, GR_BOOL, GR_NULL, GR_STRING_CONTENT, GR_STRING, GR_ARRAY, GR_KEY_NAME, GR_KEY, GR_KV, GR_DICT, GR_VALUE };
        tid = { G_REASONING, G_CONTENT, G_TOOL, G_TOOL_NAME, G_TOOL_ARGS };
        sl_i64 ri = S(rid), ti = S(tid);
        peg_map_gemma4(S(a.roots), (int64_t) a.roots.size(), U(inb), S(a_tag), S(a_rule), S(a_toff), S(a_tlen),
                       S(a_part), S(a_cs), S(a_cc), S(achild), S(a_is), S(a_ic), S(inv), ri, ti, U(cont), U(reas),
                       U(tcb), S(st), S(out));
    } else {
        tid = { M_REASONING, M_CONTENT, M_TOOL, M_TOOL_NAME, M_ARG, M_ARG_NAME, M_ARG_VALUE, M_ARG_STRING_VALUE,
                M_ARG_OBJECT, M_ARG_ARRAY, M_ARG_ITEM };
        sl_i64 ti = S(tid);
        peg_map_minimax(S(a.roots), (int64_t) a.roots.size(), U(inb), S(a_tag), S(a_toff), S(a_tlen), S(a_part),
                        S(a_cs), S(a_cc), S(achild), S(a_is), S(a_ic), S(inv), ti, U(cont), U(reas), U(tcb),
                        S(st), S(out));
    }
    string c((char *) cont.data(), out[0]);
    string r((char *) reas.data(), out[1]);
    vector<string> calls;
    for (int64_t i = 0; i < out[2]; i++) {
        int64_t b = 3 + 6 * i;
        calls.push_back(string((char *) tcb.data() + out[b + 0], out[b + 1]) + "\x1f" +
                        string((char *) tcb.data() + out[b + 2], out[b + 3]) + "\x1f" +
                        string((char *) tcb.data() + out[b + 4], out[b + 5]));
    }
    string got = dump(c, r, calls);
    bool ok = got == want;
    ++cases;
    cout << name << (ok ? " PASS " : " FAIL ") << got << "\n";
    if (!ok) {
        cout << name << " want " << want << "\n";
    }
    return ok;
}

struct g4 {
    A & b;
    int64_t str(const string & inner, const string & full, bool partial = false) {
        auto c = b.n(GR_STRING_CONTENT, -1, inner, {}, partial);
        auto s = b.n(GR_STRING, -1, full, { c }, partial);
        return b.n(GR_VALUE, -1, full, { s }, partial);
    }
    int64_t num(const string & t) { return b.n(GR_NUMBER, -1, t, {}); }
    int64_t boo(const string & t) { return b.n(GR_BOOL, -1, t, {}); }
    int64_t arr(const string & t, vector<int64_t> v, bool partial = false) { return b.n(GR_ARRAY, -1, t, std::move(v), partial); }
    int64_t kv(const string & key, const string & t, vector<int64_t> ch, bool partial = false, bool kp = false) {
        auto kn = b.n(GR_KEY_NAME, -1, key, {});
        auto k  = b.n(GR_KEY, -1, key + ":", { kn }, kp);
        vector<int64_t> kids = { k };
        for (auto x : ch) {
            kids.push_back(x);
        }
        return b.n(GR_KV, -1, t, std::move(kids), partial);
    }
    int64_t dict(const string & t, vector<int64_t> kvs, bool partial = false) { return b.n(GR_DICT, -1, t, std::move(kvs), partial); }
    int64_t tool(const string & name, int64_t d, const string & args_text, bool np = false, const string & id = "") {
        auto nm = b.n(-1, G_TOOL_NAME, name, {}, np);
        vector<int64_t> kids = { b.n(-1, X_G_TOOL_OPEN, name, { nm }) };
        if (!id.empty()) {
            kids.push_back(b.n(-1, X_G_TOOL_ID, id, {}));
        }
        if (d >= 0) {
            kids.push_back(b.n(-1, G_TOOL_ARGS, args_text, { d }));
        } else {
            kids.push_back(b.n_at(-1, G_TOOL_ARGS, b.at(name) + name.size(), 0, {}));
        }
        return b.n(-1, G_TOOL, name + args_text, std::move(kids));
    }
};

struct mm {
    A & b;
    string ns = "]<]minimax[>[";
    int64_t sv(const string & t, bool p = false) { return b.n(-1, M_ARG_STRING_VALUE, t, {}, p); }
    int64_t jv(const string & t, bool p = false) { return b.n(-1, M_ARG_VALUE, t, {}, p); }
    int64_t member(const string & name, const string & opent, const string & vt, int64_t v, const string & cl,
                   bool partial = false, bool wc = true) {
        auto nm = b.n(-1, M_ARG_NAME, name, {});
        auto op = b.n(-1, X_ARG_OPEN, opent, { nm });
        vector<int64_t> kids = { op };
        if (v >= 0) {
            kids.push_back(v);
        }
        string whole = opent + vt;
        if (wc) {
            kids.push_back(b.n(-1, X_ARG_CLOSE, cl, {}));
            whole += cl;
        }
        return b.n(-1, M_ARG, whole, std::move(kids), partial);
    }
    int64_t nameless(const string & vt, int64_t v) { return b.n(-1, M_ARG, vt, { v }); }
    int64_t obj(const string & t, vector<int64_t> ch, bool p = false) { return b.n(-1, M_ARG_OBJECT, t, std::move(ch), p); }
    int64_t ary(const string & t, vector<int64_t> ch, bool p = false) { return b.n(-1, M_ARG_ARRAY, t, std::move(ch), p); }
    int64_t item(const string & t, int64_t v, bool p = false) { return b.n(-1, M_ARG_ITEM, t, { v }, p); }
    int64_t tool(const string & name, const string & io, const string & body, vector<int64_t> mem, const string & cl,
                 bool partial = false, const string & id = "", bool wc = true) {
        auto nm = b.n(-1, M_TOOL_NAME, name, {});
        vector<int64_t> kids = { b.n(-1, X_TOOL_OPEN, io, { nm }) };
        if (!id.empty()) {
            kids.push_back(b.n(-1, X_TOOL_ID, id, {}));
        }
        for (auto m : mem) {
            kids.push_back(m);
        }
        string whole = io + body;
        if (wc) {
            kids.push_back(b.n(-1, X_TOOL_CLOSE, cl, {}));
            whole += cl;
        }
        return b.n(-1, M_TOOL, whole, std::move(kids), partial);
    }
};

int main() {
    goose_init(0, nullptr);
    int fails = 0;
    {
        A b;
        b.in = "Hello<|tool_call>call:get_weather{city:<|\"|>Paris<|\"|>}<tool_call|>";
        g4 g{ b };
        auto c  = b.n(-1, G_CONTENT, "Hello", {});
        auto v  = g.str("Paris", "<|\"|>Paris<|\"|>");
        auto kv = g.kv("city", "city:<|\"|>Paris<|\"|>", { v });
        auto d  = g.dict("{city:<|\"|>Paris<|\"|>}", { kv });
        auto t  = g.tool("get_weather", d, "{city:<|\"|>Paris<|\"|>}");
        b.roots = { c, t };
        fails += !run("G1", b, true,
            "{\"role\":\"assistant\",\"content\":\"Hello\",\"tool_calls\":[{\"name\":\"get_weather\",\"arguments\":\"{\\\"city\\\":\\\"Paris\\\"}\",\"id\":\"\"}]}");
    }
    {
        // G8: the tool name and args live one level deeper than the tool node.
        A b;
        b.in = "<|tool_call>call:get_weather{city:<|\"|>Paris<|\"|>}<tool_call|>";
        g4 g{ b };
        auto v  = g.str("Paris", "<|\"|>Paris<|\"|>");
        auto kv = g.kv("city", "city:<|\"|>Paris<|\"|>", { v });
        auto d  = g.dict("{city:<|\"|>Paris<|\"|>}", { kv });
        auto nm = b.n(-1, G_TOOL_NAME, "get_weather", {});
        auto ar = b.n(-1, G_TOOL_ARGS, "{city:<|\"|>Paris<|\"|>}", { d });
        auto wr = b.n(-1, -1, "", { nm, ar });
        auto t  = b.n(-1, G_TOOL, "get_weather{city:<|\"|>Paris<|\"|>}", { wr });
        b.roots = { t };
        fails += !run("G8", b, true,
            "{\"role\":\"assistant\",\"tool_calls\":[{\"name\":\"get_weather\",\"arguments\":\"{\\\"city\\\":\\\"Paris\\\"}\",\"id\":\"\"}]}");
    }
    {
        // G9: a value node with an empty text, which native skips entirely.
        A b;
        b.in = "<|tool_call>call:noop{empty:}<tool_call|>";
        g4 g{ b };
        auto e  = b.n(GR_STRING, -1, "", {});
        auto kv = g.kv("empty", "empty:", { e });
        auto d  = g.dict("{empty:}", { kv });
        auto t  = g.tool("noop", d, "{empty:}");
        b.roots = { t };
        fails += !run("G9", b, true,
            "{\"role\":\"assistant\",\"tool_calls\":[{\"name\":\"noop\",\"arguments\":\"{\\\"empty\\\":}\",\"id\":\"\"}]}");
    }
    {
        // G10: a partial array inside the arguments must not be closed.
        A b;
        b.in = "<|tool_call>call:set_items{items:[41";
        g4 g{ b };
        auto a   = g.arr("[41", { g.num("41") }, true);
        auto kv  = g.kv("items", "items:[41", { a }, true);
        auto d   = g.dict("{items:[41", { kv }, true);
        auto t   = g.tool("set_items", d, "{items:[41");
        b.roots  = { t };
        fails += !run("G10", b, true,
            "{\"role\":\"assistant\",\"tool_calls\":[{\"name\":\"set_items\",\"arguments\":\"{\\\"items\\\":[41\",\"id\":\"\"}]}");
    }
    {
        A b;
        b.in = "<|tool_call>call:set_items{items:[41,42],ok:true}<tool_call|>";
        g4 g{ b };
        auto a   = g.arr("[41,42]", { g.num("41"), g.num("42") });
        auto kv1 = g.kv("items", "items:[41,42]", { a });
        auto kv2 = g.kv("ok", "ok:true", { g.boo("true") });
        auto d   = g.dict("{items:[41,42],ok:true}", { kv1, kv2 });
        auto t   = g.tool("set_items", d, "{items:[41,42],ok:true}");
        b.roots  = { t };
        fails += !run("G2", b, true,
            "{\"role\":\"assistant\",\"tool_calls\":[{\"name\":\"set_items\",\"arguments\":\"{\\\"items\\\":[41,42],\\\"ok\\\":true}\",\"id\":\"\"}]}");
    }
    {
        A b;
        b.in = "<|tool_call>call:get_weather{city:<|\"|>Par";
        g4 g{ b };
        auto v  = g.str("Par", "<|\"|>Par", true);
        auto kv = g.kv("city", "city:<|\"|>Par", { v });
        auto d  = g.dict("{city:<|\"|>Par", { kv }, true);
        auto t  = g.tool("get_weather", d, "{city:<|\"|>Par");
        b.roots = { t };
        fails += !run("G3", b, true,
            "{\"role\":\"assistant\",\"tool_calls\":[{\"name\":\"get_weather\",\"arguments\":\"{\\\"city\\\":\\\"Par\",\"id\":\"\"}]}");
    }
    {
        A b;
        b.in = "<|tool_call>call:get_city{cit";
        g4 g{ b };
        auto kn = b.n(GR_KEY_NAME, -1, "cit", {});
        auto k  = b.n(GR_KEY, -1, "cit", { kn }, true);
        auto kv = b.n(GR_KV, -1, "cit", { k }, true);
        auto d  = g.dict("{cit", { kv }, true);
        auto t  = g.tool("get_city", d, "{cit");
        b.roots = { t };
        fails += !run("G4", b, true,
            "{\"role\":\"assistant\",\"tool_calls\":[{\"name\":\"get_city\",\"arguments\":\"{\\\"cit\",\"id\":\"\"}]}");
    }
    {
        A b;
        b.in = "<|channel>thought\n  <channel|>Hi<|tool_call>call:get_clock";
        g4 g{ b };
        size_t rs = b.at("\n  <channel|>") + 1;
        auto r    = b.n_at(-1, G_REASONING, rs, 2, {});
        auto c    = b.n(-1, G_CONTENT, "Hi", {});
        auto t    = g.tool("get_clock", -1, "", true);
        b.roots   = { r, c, t };
        fails += !run("G5", b, true,
            "{\"role\":\"assistant\",\"content\":\"Hi\",\"reasoning_content\":\"  \",\"tool_calls\":[]}");
    }
    {
        A b;
        b.in = "<|tool_call>call:ping<tool_call|>";
        g4 g{ b };
        b.roots = { g.tool("ping", -1, "") };
        fails += !run("G6", b, true,
            "{\"role\":\"assistant\",\"tool_calls\":[{\"name\":\"ping\",\"arguments\":\"\",\"id\":\"\"}]}");
    }
    {
        A b;
        b.in = "call_7<|tool_call>call:get_time{tz:<|\"|>UTC<|\"|>}<tool_call|>";
        g4 g{ b };
        auto v  = g.str("UTC", "<|\"|>UTC<|\"|>");
        auto kv = g.kv("tz", "tz:<|\"|>UTC<|\"|>", { v });
        auto d  = g.dict("{tz:<|\"|>UTC<|\"|>}", { kv });
        auto t  = g.tool("get_time", d, "{tz:<|\"|>UTC<|\"|>}", false, "call_7");
        b.roots = { t };
        fails += !run("G7", b, true,
            "{\"role\":\"assistant\",\"tool_calls\":[{\"name\":\"get_time\",\"arguments\":\"{\\\"tz\\\":\\\"UTC\\\"}\",\"id\":\"\"}]}");
    }
    {
        A b;
        string ns = "]<]minimax[>[";
        string so = ns + "<steps>", sc = ns + "</steps>";
        string fo = ns + "<fast>", fc = ns + "</fast>";
        string oo = ns + "<opts>", oc = ns + "</opts>";
        string to = ns + "<tags>", tc = ns + "</tags>";
        string io = ns + "<item>", ic = ns + "</item>";
        string io2 = ns + "<invoke name=\"plan\">", icl = ns + "</invoke>";
        string fast_body = fo + "true" + fc;
        string tags_body = io + "red" + ic + io + "blue" + ic;
        string body      = so + "2" + sc + oo + fast_body + oc + to + tags_body + tc;
        b.in             = "Plan:" + ns + "<tool_call>" + io2 + body + icl + ns + "</tool_call>";
        mm m{ b };
        auto content = b.n(-1, M_CONTENT, "Plan:", {});
        auto m_steps = m.member("steps", so, "2", m.jv("2"), sc);
        auto m_fast  = m.member("fast", fo, "true", m.jv("true"), fc);
        auto opts    = m.obj(fast_body, { m_fast });
        auto m_opts  = m.member("opts", oo, fast_body, opts, oc);
        auto tags    = m.ary(tags_body, { m.item(io + "red", m.sv("red")), m.item(io + "blue", m.sv("blue")) });
        auto m_tags  = m.member("tags", to, tags_body, tags, tc);
        auto t       = m.tool("plan", io2, body, { m_steps, m_opts, m_tags }, icl);
        b.roots      = { content, t };
        fails += !run("M1", b, false,
            "{\"role\":\"assistant\",\"content\":\"Plan:\",\"tool_calls\":[{\"name\":\"plan\",\"arguments\":\"{\\\"steps\\\":2,\\\"opts\\\":{\\\"fast\\\":true},\\\"tags\\\":[\\\"red\\\",\\\"blue\\\"]}\",\"id\":\"\"}]}");
    }
    {
        A b;
        string ns = "]<]minimax[>[";
        string so = ns + "<steps>", sc = ns + "</steps>";
        string to = ns + "<tags>", tc = ns + "</tags>";
        string io = ns + "<item>", ic = ns + "</item>";
        string io2 = ns + "<invoke name=\"plan\">", icl = ns + "</invoke>";
        string body = so + "2" + sc + to + io + "red";
        b.in        = ns + "<tool_call>" + io2 + body;
        mm m{ b };
        auto m_steps = m.member("steps", so, "2", m.jv("2"), sc);
        auto tags    = m.ary(io + "red", { m.item(io + "red", m.sv("red", true), true) }, true);
        auto m_tags  = m.member("tags", to, io + "red", tags, tc, true, false);
        auto t       = m.tool("plan", io2, body, { m_steps, m_tags }, icl, true, "", false);
        b.roots      = { t };
        fails += !run("M2", b, false,
            "{\"role\":\"assistant\",\"tool_calls\":[{\"name\":\"plan\",\"arguments\":\"{\\\"steps\\\":2,\\\"tags\\\":[\\\"red\",\"id\":\"\"}]}");
    }
    {
        A b;
        string ns = "]<]minimax[>[";
        string so = ns + "<steps>", sc = ns + "</steps>";
        string io2 = ns + "<invoke name=\"plan\">", icl = ns + "</invoke>";
        string body = so + "2" + sc + "7";
        b.in        = "call_9" + ns + "<tool_call>" + io2 + body + icl + ns + "</tool_call>";
        mm m{ b };
        auto m_steps = m.member("steps", so, "2", m.jv("2"), sc);
        auto t       = m.tool("plan", io2, body, { m_steps, m.nameless("7", m.jv("7")) }, icl, false, "call_9");
        b.roots      = { t };
        fails += !run("M3", b, false,
            "{\"role\":\"assistant\",\"tool_calls\":[{\"name\":\"plan\",\"arguments\":\"{\\\"steps\\\":2}\",\"id\":\"\"}]}");
    }
    cout << "FG562_RUNTIME_SUBCLASS cases=" << cases << " values=" << cases * 5 << " failures=" << fails << " skipped=0\n";
    return fails != 0;
}
