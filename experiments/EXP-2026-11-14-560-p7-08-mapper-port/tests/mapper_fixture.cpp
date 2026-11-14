// Differential fixture: native common_chat_peg_mapper vs the Goose port in
// draft.goose, over hand-built grammars. No weights, no server: the arena and
// the parse result come from common/peg-parser.cpp, the mapper from
// common/chat-peg-parser.cpp, both inside libllama-common.so.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdlib>
#include <ctime>

#include "chat.h"
#include "peg-parser.h"
#include "chat-peg-parser.h"
#include "runtime-gen.h"

using json = common_json;

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

static std::string vis(const std::string & s, size_t max = 300) {
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

// Flat copy of one arena plus its tag-id table.
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

static std::string bx(const std::vector<uint8_t> & v, int64_t off, int64_t len) {
    if (off < 0 || len < 0 || off + len > (int64_t) v.size()) return "<oob>";
    return std::string((const char *) v.data() + off, (size_t) len);
}


struct Case {
    const char * name;
    std::function<common_peg_parser(common_chat_peg_builder &)> build;
    std::string input;
};

static json tool_def(const std::string & name, json props) {
    json fn;
    fn["name"] = name;
    fn["description"] = "";
    fn["parameters"] = json{{"type", "object"}, {"properties", props}};
    return json{{"type", "function"}, {"function", fn}};
}

static void run(const Case & c) {
    common_peg_arena arena = build_chat_peg_parser(c.build);
    common_peg_parse_context ctx(c.input);
    common_peg_parse_result res = arena.parse(ctx);

    common_chat_msg native;
    native.role = "assistant";
    {
        common_chat_peg_mapper mapper(native);
        mapper.from_ast(ctx.ast, res);
    }

    Flat f;
    f.build(ctx.ast);

    if (getenv("DBG")) {
        for (size_t i = 0; i < ctx.ast.size(); i++) {
            const auto & n = ctx.ast.get((common_peg_ast_id) i);
            printf("  node %zu rule=[%s] tag=[%s] [%zu,%zu) kids=%zu\n", i, n.rule.c_str(), n.tag.c_str(),
                   n.start, n.end, n.children.size());
        }
        printf("  tid:");
        for (int i = 0; i < 12; i++) printf(" %lld", (long long) f.tid[i]);
        printf("\n");
    }
    std::vector<uint8_t> cont(1 << 16, 0), reas(1 << 16, 0), tcb(1 << 16, 0);
    std::vector<int64_t> st(24, 0), out(3 + 6 * 8, 0);
    std::vector<int64_t> roots;
    for (auto id : res.nodes) roots.push_back((int64_t) id);

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
    sl_i64 roots_s{roots.data(), (int64_t) roots.size()};
    sl_i64 st_s{st.data(), (int64_t) st.size()};
    sl_i64 out_s{out.data(), (int64_t) out.size()};
    sl_u8 cont_s{cont.data(), (int64_t) cont.size()};
    sl_u8 reas_s{reas.data(), (int64_t) reas.size()};
    sl_u8 tcb_s{tcb.data(), (int64_t) tcb.size()};
    sl_u8 in_s{(uint8_t *) c.input.data(), (int64_t) c.input.size()};

    peg_map_msg(roots_s, (int64_t) roots.size(), in_s, a_tag, a_toff, a_tlen, a_cs, a_cc, achild,
                a_is, a_ic, inv, tid, cont_s, reas_s, tcb_s, st_s, out_s);

    if (getenv("DBG3")) {
        std::function<void(common_peg_ast_id, int)> rec = [&](common_peg_ast_id id, int d) {
            const auto & n = ctx.ast.get(id);
            printf("  %*sid=%d tag=[%s] [%zu,%zu)\n", d * 2, "", (int) id, n.tag.c_str(), n.start, n.end);
            for (auto ch : n.children) rec(ch, d + 1);
        };
        for (auto r : res.nodes) rec(r, 0);
    }
    if (getenv("DBG2")) {
        printf("  st:");
        for (size_t i = 0; i < st.size(); i++) printf(" %lld", (long long) st[i]);
        printf("\n  out:");
        for (size_t i = 0; i < 15; i++) printf(" %lld", (long long) out[i]);
        printf("\n");
    }
    std::string g_content = bx(cont, 0, out[0]);
    std::string g_reason  = bx(reas, 0, out[1]);
    int64_t g_n = out[2];

    ++cases;
    checks += 3 + 3 * (int64_t) native.tool_calls.size();
    std::string diff;
    if (g_content != native.content) diff += " content";
    if (g_reason != native.reasoning_content) diff += " reasoning";
    if (g_n != (int64_t) native.tool_calls.size()) diff += " ncalls";
    for (int64_t i = 0; i < g_n && i < (int64_t) native.tool_calls.size(); i++) {
        const int64_t b = 3 + 6 * i;
        if (bx(tcb, out[b + 0], out[b + 1]) != native.tool_calls[i].name) diff += " name" + std::to_string(i);
        if (bx(tcb, out[b + 2], out[b + 3]) != native.tool_calls[i].arguments) diff += " args" + std::to_string(i);
        if (bx(tcb, out[b + 4], out[b + 5]) != native.tool_calls[i].id) diff += " id" + std::to_string(i);
    }

    printf("CASE %d %s\n", cases, c.name);
    printf("  result=%s start=%zu end=%zu nodes=%zu\n",
           common_peg_parse_result_type_name(res.type), res.start, res.end, res.nodes.size());
    printf("  native_json=%s\n", native.to_json_oaicompat().dump().c_str());
    printf("  goose content=[%s] reasoning=[%s] ncalls=%lld\n", vis(g_content).c_str(), vis(g_reason).c_str(),
           (long long) g_n);
    for (int64_t i = 0; i < g_n; i++) {
        const int64_t b = 3 + 6 * i;
        printf("  goose call%lld name=[%s] args=[%s] id=[%s]\n", (long long) i,
               vis(bx(tcb, out[b + 0], out[b + 1])).c_str(),
               vis(bx(tcb, out[b + 2], out[b + 3])).c_str(),
               vis(bx(tcb, out[b + 4], out[b + 5])).c_str());
    }
    if (diff.empty()) {
        printf("  status=OK\n");
    } else {
        printf("  status=MISMATCH%s\n", diff.c_str());
        ++fails;
    }
}

int main() {
    goose_init(0, 0);
    const char * XOPEN = "<tool_call>", * XEND = "</tool_call>";

    // 1: content only.
    run({"content only", [](common_chat_peg_builder & b) {
            return b.content(b.zero_or_more(b.any()));
         },
         "hello!"});

    // 2: reasoning, then content.
    run({"reasoning then content", [](common_chat_peg_builder & b) {
            return b.reasoning(b.until("</r>")) + b.literal("</r>") + b.content(b.zero_or_more(b.any()));
         },
         "step by step</r>The answer is 42."});

    // 3: whitespace-only reasoning is dropped at the end.
    run({"whitespace reasoning", [](common_chat_peg_builder & b) {
            return b.reasoning(b.until("|")) + b.literal("|") + b.content(b.zero_or_more(b.any()));
         },
         " \n\t|answer"});

    // 13/14: trim() removes at most one leading and every trailing space from an
    // argument name, so the key is clean on both sides.
    run({"arg name with a leading space", [](common_chat_peg_builder & b) {
            auto arg = b.tool_arg(b.tool_arg_open(b.literal("<p>")) + b.tool_arg_name(b.until("=")) +
                                 b.literal("=") + b.tool_arg_value(b.until(";")) +
                                 b.tool_arg_close(b.literal(";")));
            return b.tool(b.tool_open(b.literal("<tool>")) + b.tool_name(b.until("|")) + b.literal("|") +
                          b.tool_args(b.zero_or_more(arg)) + b.tool_close(b.literal("</tool>")));
         },
         "<tool>search|<p> city=Paris;</tool>"});

    run({"arg name with a trailing space", [](common_chat_peg_builder & b) {
            auto arg = b.tool_arg(b.tool_arg_open(b.literal("<p>")) + b.tool_arg_name(b.until("=")) +
                                 b.literal("=") + b.tool_arg_value(b.until(";")) +
                                 b.tool_arg_close(b.literal(";")));
            return b.tool(b.tool_open(b.literal("<tool>")) + b.tool_name(b.until("|")) + b.literal("|") +
                          b.tool_args(b.zero_or_more(arg)) + b.tool_close(b.literal("</tool>")));
         },
         "<tool>search|<p>city =Paris;</tool>"});

    // 15/16: Python-style value normalization and a quoted tool id.
    run({"python value normalized", [](common_chat_peg_builder & b) {
            auto arg = b.tool_arg(b.tool_arg_open(b.literal("<p>")) + b.tool_arg_name(b.until("=")) +
                                 b.literal("=") + b.tool_arg_value(b.until(";")) +
                                 b.tool_arg_close(b.literal(";")));
            return b.tool(b.tool_open(b.literal("<tool>")) + b.tool_name(b.until("|")) + b.literal("|") +
                          b.tool_args(b.zero_or_more(arg)) + b.tool_close(b.literal("</tool>")));
         },
         "<tool>search|<p>arg={'a': True};</tool>"});

    run({"tool name with a trailing space", [](common_chat_peg_builder & b) {
            return b.tool(b.tool_open(b.literal("<tool>")) + b.tool_name(b.until("|")) + b.literal("|") +
                          b.tool_args(b.zero_or_more(b.tool_arg(b.tool_arg_open(b.literal("<p>")) + b.tool_arg_name(b.until("=")) + b.literal("=") + b.tool_arg_value(b.until(";")) + b.tool_arg_close(b.literal(";"))))) +
                          b.tool_close(b.literal("</tool>")));
         },
         "<tool>search |<p>x=1;</tool>"});

    // 4: XML constructed tools, two arguments (tool-arg-name/value/close).
    run({"xml one call", [&](common_chat_peg_builder & b) {
            json tools = json::array({tool_def("get_weather", json{{"city", {{"type", "string"}}},
                                                                   {"days", {{"type", "integer"}}}})});
            return b.standard_constructed_tools({}, tools, false, true);
         },
         std::string(XOPEN) + "<function=get_weather><param=city>Paris</param><param=days>3</param></function>" + XEND});

    // 5: two calls in one section (a second tool-open after a push).
    run({"xml two calls", [&](common_chat_peg_builder & b) {
            json tools = json::array({tool_def("get_weather", json{{"city", {{"type", "string"}}}}),
                                      tool_def("get_time", json{{"zone", {{"type", "string"}}}})});
            return b.standard_constructed_tools({}, tools, true, true);
         },
         std::string(XOPEN) + "<function=get_weather><param=city>Oslo</param></function> "
         "<function=get_time><param=zone>CET</param></function>" + XEND});

    // 6: JSON flat keys, name + arguments + string id.
    run({"json flat id", [&](common_chat_peg_builder & b) {
            json tools = json::array({tool_def("get_weather", json{{"city", {{"type", "string"}}}})});
            return b.standard_json_tools("", "", tools, false, true, "name", "arguments", false, false,
                                         "id", "", {"name", "arguments", "id"}, false);
         },
         R"({"name":"get_weather","arguments":{"city":"Paris"},"id":"call_1"})"});

    // 7: an unbalanced object value: tool-close adds the missing braces.
    run({"xml unbalanced braces", [&](common_chat_peg_builder & b) {
            json tools = json::array({tool_def("put", json{{"extra", {{"type", "object"}}}})});
            return b.standard_constructed_tools({}, tools, false, true);
         },
         std::string(XOPEN) + "<function=put><param=extra>{\"a\":{\"b\":1</param></function>" + XEND});

    // 8: nested keys (function.name/function.arguments) with an integer id.
    run({"json nested gen id", [&](common_chat_peg_builder & b) {
            json tools = json::array({tool_def("get_weather", json{{"city", {{"type", "string"}}}})});
            return b.standard_json_tools("", "", tools, false, true, "function.name", "function.arguments",
                                         false, false, "", "tool_call_id", {}, false);
         },
         R"({"tool_call_id":7,"function":{"name":"get_weather","arguments":{"city":"Rome"}}})"});

    // 9: function-is-key layout with a string id.
    run({"json function key", [&](common_chat_peg_builder & b) {
            json tools = json::array({tool_def("get_weather", json{{"city", {{"type", "string"}}}})});
            return b.standard_json_tools("", "", tools, false, true, "", "", false, true, "id", "", {}, false);
         },
         R"({"get_weather":{"id":"c9",{"city":"Bern"}}})"});

    // 10: arguments arrive before the name (args_buffer flush at tool-name).
    run({"json args before name", [&](common_chat_peg_builder & b) {
            json tools = json::array({tool_def("get_weather", json{{"city", {{"type", "string"}}}})});
            return b.standard_json_tools("", "", tools, false, true, "name", "arguments", false, false,
                                         "", "", {"arguments", "name"}, false);
         },
         R"({"arguments":{"city":"Kyiv"},"name":"get_weather"})"});

    // 11: schema-declared string value (tool-arg-string-value) with a closing
    // quote at arg-close.
    run({"string value tag", [](common_chat_peg_builder & b) {
            auto arg = b.tool_arg(b.tool_arg_open(b.literal("<p>")) + b.tool_arg_name(b.until("=")) +
                                 b.literal("=") + b.tool_arg_string_value(b.until(";")) +
                                 b.tool_arg_close(b.literal(";")));
            return b.tool(b.tool_open(b.literal("<tool>")) + b.tool_name(b.until("|")) + b.literal("|") +
                          b.tool_args(b.zero_or_more(arg)) + b.tool_close(b.literal("</tool>")));
         },
         "<tool>search|<p>q=he said \"hi\";<p>n=42;</tool>"});

    // 12: two adjacent content nodes concatenate with no separator.
    run({"two content nodes", [](common_chat_peg_builder & b) {
            return b.content(b.until("x")) + b.literal("x") + b.content(b.zero_or_more(b.any()));
         },
         "firstxsecond"});

    printf("FG560_RUNTIME_MAPPER cases=%d values=%d failures=%d skipped=0\n", cases, checks, fails);
    return fails ? 1 : 0;
}
