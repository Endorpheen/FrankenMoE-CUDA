// Parse-level gate: the arena built by the port is converted back into a native
// common_peg_arena and both are run through the native PEG machine on real tool
// answers. Compares success/fail/partial, consumed length and the whole AST.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <algorithm>

#include "chat.h"
#include "peg-parser.h"
#include "chat-peg-parser.h"
#include "runtime-gen.h"

// Host shims for the extern "gs_*" declarations the canonical runtime carries.
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
extern "C" int64_t gs_strftime_epoch(sl_u8 out, int64_t epoch, sl_u8 fmt) {
    const std::string f((const char *) fmt.data, (size_t) fmt.len);
    const std::time_t t = epoch < 0 ? std::time(nullptr) : (std::time_t) epoch;
    const std::tm * tmv = std::localtime(&t);
    if (!tmv) return -1;
    const size_t n = std::strftime((char *) out.data, (size_t) out.len, f.c_str(), tmv);
    return (int64_t) n;
}
extern "C" int64_t gs_to_string_f64(sl_u8 out, double x) {
    const std::string s = std::to_string(x);
    memcpy(out.data, s.data(), s.size());
    return (int64_t) s.size();
}

struct Case {
    const char * id;
    bool json;
    std::map<std::string, std::string> markers;
    std::string start, end, name_key, args_key, call_id_key, gen_call_id_key;
    bool array_wrapped = false, function_is_key = false, accept_wrapper = false;
    std::vector<std::string> params_order;
    std::vector<std::string> names;
    std::vector<std::vector<std::string>> props;
    std::vector<bool> has_fn;
    bool parallel = false, force = false;
    std::string input;
    bool lenient = false;
};

struct Port {
    std::vector<int64_t> kind, cfirst, ccount, children, sidx, dstart, dcount, mn, mx, st;
    std::vector<uint8_t> pool, sb;
    Port() : kind(8192, 0), cfirst(8192, 0), ccount(8192, 0), children(131072, 0), sidx(8192, 0),
             dstart(16384, 0), dcount(16384, 0), mn(8192, 0), mx(8192, 0), st(128, 0),
             pool(524288, 0), sb(65536, 0) {}
    sl_i64 s(std::vector<int64_t> & v) { return sl_i64{v.data(), (int64_t) v.size()}; }
};

static void join(const std::vector<std::string> & v, std::string & blob,
                 std::vector<int64_t> & off, std::vector<int64_t> & len) {
    off.clear();
    len.clear();
    for (const auto & s : v) {
        off.push_back((int64_t) blob.size());
        len.push_back((int64_t) s.size());
        blob += s;
    }
}

static std::string build_tools_json(const Case & c) {
    std::string s = "[";
    for (size_t i = 0; i < c.names.size(); i++) {
        if (i) s += ",";
        if (!c.has_fn[i]) { s += "{\"type\":\"function\"}"; continue; }
        s += "{\"type\":\"function\",\"function\":{\"name\":\"" + c.names[i] +
             "\",\"parameters\":{\"type\":\"object\",\"properties\":{";
        for (size_t j = 0; j < c.props[i].size(); j++) {
            if (j) s += ",";
            s += "\"" + c.props[i][j] + "\":{\"type\":\"string\"}";
        }
        s += "}}}}";
    }
    s += "]";
    return s;
}

static common_peg_arena build_native(const Case & c) {
    common_json tools = common_json::parse(build_tools_json(c));
    common_chat_peg_builder p;
    if (!c.json) {
        p.set_root(p.standard_constructed_tools(c.markers, tools, c.parallel, c.force));
    } else {
        p.set_root(p.standard_json_tools(c.start, c.end, tools, c.parallel, c.force,
                                         c.name_key, c.args_key, c.array_wrapped, c.function_is_key,
                                         c.call_id_key, c.gen_call_id_key, c.params_order,
                                         c.accept_wrapper));
    }
    return p.build();
}

static int64_t build_port(const Case & c, Port & P) {
    std::string nblob, pblob, kblob, vblob;
    std::vector<int64_t> noff, nlen, poff, plen, pfirst, pcount;
    join(c.names, nblob, noff, nlen);
    for (size_t i = 0; i < c.props.size(); i++) {
        std::vector<std::string> v = c.props[i];
        std::vector<int64_t> o, l;
        std::string tmp;
        join(v, tmp, o, l);
        pfirst.push_back((int64_t) poff.size());
        pcount.push_back((int64_t) v.size());
        for (size_t j = 0; j < v.size(); j++) {
            poff.push_back((int64_t) pblob.size() + o[j]);
            plen.push_back(l[j]);
        }
        pblob += tmp;
    }
    std::vector<std::string> mk, mv;
    for (const auto & kv : c.markers) { mk.push_back(kv.first); mv.push_back(kv.second); }
    std::vector<int64_t> koff, klen2, voff, vlen;
    join(mk, kblob, koff, klen2);
    join(mv, vblob, voff, vlen);
    std::vector<int64_t> hf;
    for (bool b : c.has_fn) hf.push_back(b ? 1 : 0);

    if (!c.json) {
        return chat_standard_constructed_tools(
            sl_i64{koff.data(), (int64_t) koff.size()}, sl_i64{klen2.data(), (int64_t) klen2.size()},
            sl_u8{(uint8_t *) kblob.data(), (int64_t) kblob.size()}, (int64_t) mk.size(),
            sl_i64{voff.data(), (int64_t) voff.size()}, sl_i64{vlen.data(), (int64_t) vlen.size()},
            sl_u8{(uint8_t *) vblob.data(), (int64_t) vblob.size()},
            sl_i64{noff.data(), (int64_t) noff.size()}, sl_i64{nlen.data(), (int64_t) nlen.size()},
            sl_u8{(uint8_t *) nblob.data(), (int64_t) nblob.size()},
            sl_i64{hf.data(), (int64_t) hf.size()}, (int64_t) c.names.size(),
            sl_i64{pfirst.data(), (int64_t) pfirst.size()}, sl_i64{pcount.data(), (int64_t) pcount.size()},
            sl_i64{poff.data(), (int64_t) poff.size()}, sl_i64{plen.data(), (int64_t) plen.size()},
            sl_u8{(uint8_t *) pblob.data(), (int64_t) pblob.size()},
            c.parallel ? 1 : 0, c.force ? 1 : 0,
            P.s(P.kind), P.s(P.cfirst), P.s(P.ccount), P.s(P.children), P.s(P.sidx),
            P.s(P.dstart), P.s(P.dcount), sl_u8{P.pool.data(), (int64_t) P.pool.size()},
            P.s(P.mn), P.s(P.mx), sl_u8{P.sb.data(), (int64_t) P.sb.size()}, P.s(P.st));
    }
    std::string pob;
    std::vector<int64_t> po_off, po_len;
    join(c.params_order, pob, po_off, po_len);
    sl_u8 ss{(uint8_t *) c.start.data(), (int64_t) c.start.size()};
    sl_u8 se{(uint8_t *) c.end.data(), (int64_t) c.end.size()};
    sl_u8 nk{(uint8_t *) c.name_key.data(), (int64_t) c.name_key.size()};
    sl_u8 ak{(uint8_t *) c.args_key.data(), (int64_t) c.args_key.size()};
    sl_u8 ck{(uint8_t *) c.call_id_key.data(), (int64_t) c.call_id_key.size()};
    sl_u8 gk{(uint8_t *) c.gen_call_id_key.data(), (int64_t) c.gen_call_id_key.size()};
    sl_u8 sk{(uint8_t *) "object", 6};
    return chat_standard_json_tools(
        ss, se,
        sl_i64{noff.data(), (int64_t) noff.size()}, sl_i64{nlen.data(), (int64_t) nlen.size()},
        sl_u8{(uint8_t *) nblob.data(), (int64_t) nblob.size()},
        sl_i64{hf.data(), (int64_t) hf.size()}, (int64_t) c.names.size(), sk,
        c.parallel ? 1 : 0, c.force ? 1 : 0, nk, ak,
        c.array_wrapped ? 1 : 0, c.function_is_key ? 1 : 0, ck, gk,
        sl_i64{po_off.data(), (int64_t) po_off.size()}, sl_i64{po_len.data(), (int64_t) po_len.size()},
        sl_u8{(uint8_t *) pob.data(), (int64_t) pob.size()}, (int64_t) c.params_order.size(),
        c.accept_wrapper ? 1 : 0,
        P.s(P.kind), P.s(P.cfirst), P.s(P.ccount), P.s(P.children), P.s(P.sidx),
        P.s(P.dstart), P.s(P.dcount), sl_u8{P.pool.data(), (int64_t) P.pool.size()},
        P.s(P.mn), P.s(P.mx), sl_u8{P.sb.data(), (int64_t) P.sb.size()}, P.s(P.st));
}

// ---- port arena -> native arena -------------------------------------------
// The port keeps (kind, children range, string index, min/max). This converter
// replays that graph through common_peg_parser_builder, memoised per port id, so
// the reconstructed arena has the same structure. Rules go through rule(name,
// fn): the builder registers the name before calling fn, which breaks the
// json-value/json-object/json-array recursion exactly like the native build.

static std::string port_str(const Port & P, int64_t sid) {
    if (sid < 0) return std::string();
    return std::string((const char *) P.pool.data() + P.dstart[sid], (size_t) P.dcount[sid]);
}

namespace {
struct Converter {
    common_peg_parser_builder & b;
    const Port & P;
    std::map<int64_t, int64_t> memo;
    std::string err;

    Converter(common_peg_parser_builder & b_, const Port & P_) : b(b_), P(P_) {}

    common_peg_parser wrap(int64_t id) { return common_peg_parser((size_t) id, b); }

    std::vector<common_peg_parser> kids(int64_t pid) {
        std::vector<common_peg_parser> v;
        for (int64_t i = 0; i < P.ccount[pid]; i++) v.push_back(run(P.children[P.cfirst[pid] + i]));
        return v;
    }

    common_peg_parser run(int64_t pid) {
        auto it = memo.find(pid);
        if (it != memo.end()) return wrap(it->second);
        int64_t k = P.kind[pid];
        common_peg_parser out = b.eps();
        if (k == 0) {
            out = b.eps();
        } else if (k == 2) {
            out = b.end();
        } else if (k == 3) {
            out = b.literal(port_str(P, P.sidx[pid]));
        } else if (k == 4) {
            out = b.sequence(kids(pid));
        } else if (k == 5) {
            out = b.choice(kids(pid));
        } else if (k == 6) {
            out = b.repeat(run(P.children[P.cfirst[pid]]), (int) P.mn[pid], (int) P.mx[pid]);
        } else if (k == 8) {
            out = b.negate(run(P.children[P.cfirst[pid]]));
        } else if (k == 9) {
            out = b.any();
        } else if (k == 13) {
            const std::string d = port_str(P, P.sidx[pid]);
            out = d.empty() ? b.rest() : b.until(d);
        } else if (k == 14) {                       // Rule
            const std::string name = port_str(P, P.sidx[pid]);
            memo[pid] = (int64_t) b.rule(name, [&]() {
                return run(P.children[P.cfirst[pid]]);
            }).id();
            return wrap(memo[pid]);
        } else if (k == 18) {                       // Schema
            const std::string kind = port_str(P, P.sidx[pid]);
            if (kind != "object") { err = "schema kind " + kind + " not modelled"; return b.eps(); }
            common_json js = common_json::parse("{\"type\":\"object\"}");
            out = b.schema(run(P.children[P.cfirst[pid]]), "schema-" + std::to_string(pid), js, false);
        } else if (k == 16) {
            out = b.atomic(run(P.children[P.cfirst[pid]]));
        } else if (k == 17) {
            out = b.tag(port_str(P, P.sidx[pid]), run(P.children[P.cfirst[pid]]));
        } else if (k == 10) {
            out = b.space();
        } else if (k == 11) {
            out = b.chars(port_str(P, P.sidx[pid]), (int) P.mn[pid], (int) P.mx[pid]);
        } else if (k == 12) {
            const std::string d = port_str(P, P.sidx[pid]);
            if (d.empty()) { err = "empty String delimiter"; return b.eps(); }
            out = b.string_content(d[0]);
        } else {
            err = "port kind " + std::to_string(k) + " not convertible";
            return b.eps();
        }
        memo[pid] = (int64_t) out.id();
        return out;
    }
};
}  // namespace

static common_peg_arena convert_port(const Port & P, int64_t root, std::string & err) {
    common_peg_parser_builder b;
    Converter c(b, P);
    auto r = c.run(root);
    err = c.err;
    b.set_root(r);
    return b.build();
}

// ---- AST comparison --------------------------------------------------------

static void ast_walk(const common_peg_ast_arena & ast, common_peg_ast_id id, int depth, std::string & out) {
    const auto & n = ast.get(id);
    out += std::to_string(depth);
    out += "|" + n.rule + "|" + n.tag + "|" + std::to_string(n.start) + "-" + std::to_string(n.end);
    out += "|" + std::string(n.text);
    out += n.is_partial ? "|partial" : "";
    out += "\n";
    for (auto ch : n.children) ast_walk(ast, ch, depth + 1, out);
}

static const char * type_name(common_peg_parse_result_type t) {
    switch (t) {
        case COMMON_PEG_PARSE_RESULT_FAIL: return "FAIL";
        case COMMON_PEG_PARSE_RESULT_SUCCESS: return "SUCCESS";
        case COMMON_PEG_PARSE_RESULT_NEED_MORE_INPUT: return "PARTIAL";
    }
    return "?";
}

struct Outcome {
    common_peg_parse_result_type type;
    size_t end;
    std::string ast;
};

static Outcome run_parse(const common_peg_arena & a, const Case & c) {
    common_peg_parse_context ctx(c.input, c.lenient ? COMMON_PEG_PARSE_FLAG_LENIENT : COMMON_PEG_PARSE_FLAG_NONE);
    common_peg_parse_result r = a.parse(ctx);
    Outcome o;
    o.type = r.type;
    o.end = r.end;
    for (auto id : r.nodes) ast_walk(ctx.ast, id, 0, o.ast);
    return o;
}

static void show(const Outcome & o) {
    size_t from = 0;
    int lines = 0;
    while (from < o.ast.size() && lines < 6) {
        size_t nl = o.ast.find('\n', from);
        if (nl == std::string::npos) nl = o.ast.size();
        printf("    %s\n", o.ast.substr(from, nl - from).c_str());
        from = nl + 1;
        lines++;
    }
}

int main(int argc, char ** argv) {
    goose_init(0, 0);
    std::vector<Case> cases;
    {
        Case c{};
        c.id = "P1-json-single"; c.json = true;
        c.start = "<tool_call>"; c.end = "</tool_call>";
        c.names = {"get_weather"}; c.props = {{"location", "unit"}}; c.has_fn = {true};
        c.force = true;
        c.input = "<tool_call>{\"name\":\"get_weather\",\"arguments\":{\"location\":\"Paris\",\"unit\":\"c\"}}</tool_call>";
        cases.push_back(c);
    }
    {
        Case c{};
        c.id = "P2-json-two-calls"; c.json = true;
        c.start = "<tool_call>"; c.end = "</tool_call>";
        c.names = {"get_weather"}; c.props = {{"location"}}; c.has_fn = {true};
        c.parallel = true; c.force = true;
        c.input = "<tool_call>{\"name\":\"get_weather\",\"arguments\":{\"location\":\"Paris\"}}, {\"name\":\"get_weather\",\"arguments\":{\"location\":\"Rome\"}}</tool_call>";
        cases.push_back(c);
    }
    {
        // arguments before the name: parameters_order puts arguments first
        Case c{};
        c.id = "P3-json-args-first"; c.json = true;
        c.start = "<tool_call>"; c.end = "</tool_call>";
        c.names = {"get_weather"}; c.props = {{"location"}}; c.has_fn = {true};
        c.params_order = {"arguments", "name"};
        c.force = true;
        c.input = "<tool_call>{\"arguments\":{\"location\":\"Paris\"},\"name\":\"get_weather\"}</tool_call>";
        cases.push_back(c);
    }
    {
        // truncated inside the arguments string, lenient -> partial
        Case c{};
        c.id = "P4-json-partial"; c.json = true;
        c.start = "<tool_call>"; c.end = "</tool_call>";
        c.names = {"get_weather"}; c.props = {{"location"}}; c.has_fn = {true};
        c.force = true; c.lenient = true;
        c.input = "<tool_call>{\"name\":\"get_weather\",\"arguments\":{\"location\":\"Par";
        cases.push_back(c);
    }
    {
        // garbage after a complete call: success, end < input.size()
        Case c{};
        c.id = "P5-json-trailing-junk"; c.json = true;
        c.start = "<tool_call>"; c.end = "</tool_call>";
        c.names = {"get_weather"}; c.props = {{"location"}}; c.has_fn = {true};
        c.force = true;
        c.input = "<tool_call>{\"name\":\"get_weather\",\"arguments\":{\"location\":\"Paris\"}}</tool_call> and then some junk";
        cases.push_back(c);
    }
    {
        Case c{};
        c.id = "P6-xml-single"; c.json = false;
        c.names = {"get_weather"}; c.props = {{"location", "unit"}}; c.has_fn = {true};
        c.force = true;
        c.input = "<tool_call><function=get_weather><param=location>Paris</param></function></tool_call>";
        cases.push_back(c);
    }
    {
        Case c{};
        c.id = "P7-xml-two-calls"; c.json = false;
        c.names = {"get_weather"}; c.props = {{"location"}}; c.has_fn = {true};
        c.parallel = true; c.force = true;
        c.input = "<tool_call><function=get_weather><param=location>Paris</param></function> <function=get_weather><param=location>Rome</param></function></tool_call>";
        cases.push_back(c);
    }
    {
        // truncated inside the argument value, lenient -> partial
        Case c{};
        c.id = "P8-xml-partial"; c.json = false;
        c.names = {"get_weather"}; c.props = {{"location"}}; c.has_fn = {true};
        c.force = true; c.lenient = true;
        c.input = "<tool_call><function=get_weather><param=location>Par";
        cases.push_back(c);
    }

    {
        // invalid JSON exponent: native refuses "1e", a port with a zero-min
        // exponent digit class would accept it (M15 class of defect)
        Case c{};
        c.id = "P9-json-bad-exp"; c.json = true;
        c.start = "<tool_call>"; c.end = "</tool_call>";
        c.names = {"get_weather"}; c.props = {{"location"}}; c.has_fn = {true};
        c.force = true;
        c.input = "<tool_call>{\"name\":\"get_weather\",\"arguments\":{\"location\":1e}}</tool_call>";
        cases.push_back(c);
    }

    int fails = 0;
    for (const Case & c : cases) {
        common_peg_arena native = build_native(c);
        Port P;
        int64_t proot = build_port(c, P);
        std::string cerr;
        common_peg_arena converted = convert_port(P, proot, cerr);

        Outcome n = run_parse(native, c);
        Outcome p = run_parse(converted, c);

        bool ok = !cerr.empty() ? false : (n.type == p.type && n.end == p.end && n.ast == p.ast);
        if (ok) {
            printf("%-22s ok   %-8s end=%zu ast_nodes=%zu\n", c.id, type_name(n.type), n.end,
                   (size_t) std::count(n.ast.begin(), n.ast.end(), '\n'));
        } else {
            printf("%-22s FAIL\n", c.id);
            if (!cerr.empty()) printf("  convert: %s\n", cerr.c_str());
            if (n.type != p.type) printf("  type native=%s port=%s\n", type_name(n.type), type_name(p.type));
            if (n.end != p.end) printf("  end native=%zu port=%zu\n", n.end, p.end);
            if (n.ast != p.ast) {
                printf("  native ast:\n"); show(n);
                printf("  port ast:\n"); show(p);
            }
            fails++;
        }
    }
    printf("parse_cases=%zu failures=%d\n", cases.size(), fails);
    return fails ? 1 : 0;
}
