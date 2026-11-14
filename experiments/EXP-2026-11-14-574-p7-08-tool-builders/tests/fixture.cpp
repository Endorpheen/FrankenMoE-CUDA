// Differential gate: native standard tool builders vs the port in runtime.goose.
// Both sides build the same grammar and are compared through the textual arena
// dump; no weights, no server.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <string>
#include <vector>
#include <map>
#include <variant>

#include "chat.h"
#include "json-schema-to-grammar.h"
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
};

static std::string build_tools_json(const Case & c) {
    std::string s = "[";
    for (size_t i = 0; i < c.names.size(); i++) {
        if (i) s += ",";
        if (!c.has_fn[i]) {
            s += "{\"type\":\"function\"}";
            continue;
        }
        s += "{\"type\":\"function\",\"function\":{\"name\":\"" + c.names[i] + "\",\"parameters\":{\"type\":\"object\",\"properties\":{";
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

struct Port {
    std::vector<int64_t> kind, cfirst, ccount, children, sidx, dstart, dcount, mn, mx, st;
    std::vector<uint8_t> pool, sb;
    Port() : kind(4096, 0), cfirst(4096, 0), ccount(4096, 0), children(65536, 0), sidx(4096, 0),
             dstart(8192, 0), dcount(8192, 0), mn(4096, 0), mx(4096, 0), st(128, 0),
             pool(262144, 0), sb(65536, 0) {}
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

static std::string build_port(const Case & c, int64_t & root_out, Port & P_out) {
    Port P;
    // st starts zeroed, which is what peg_reset does for st[60..63]; the tool
    // builders initialise their own scratch and json-cache slots.

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
    std::vector<int64_t> koff, klen2;
    join(mk, kblob, koff, klen2);
    std::vector<int64_t> voff, vlen;
    join(mv, vblob, voff, vlen);
    std::vector<int64_t> hf;
    for (bool b : c.has_fn) hf.push_back(b ? 1 : 0);

    int64_t root;
    if (!c.json) {
        root = chat_standard_constructed_tools(
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
    } else {
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
        root = chat_standard_json_tools(
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
    std::vector<uint8_t> out(262144, 0);
    std::vector<int64_t> vis2(65536, 0);
    int64_t n = peg_dump_basic(root,
                         sl_i64{vis2.data(), (int64_t) vis2.size()},
                         sl_i64{vis2.data(), (int64_t) vis2.size()},
                         sl_i64{P.kind.data(), (int64_t) P.kind.size()}, sl_i64{P.cfirst.data(), (int64_t) P.cfirst.size()},
                         sl_i64{P.ccount.data(), (int64_t) P.ccount.size()}, sl_i64{P.children.data(), (int64_t) P.children.size()},
                         sl_i64{P.sidx.data(), (int64_t) P.sidx.size()}, sl_i64{P.dstart.data(), (int64_t) P.dstart.size()},
                         sl_i64{P.dcount.data(), (int64_t) P.dcount.size()}, sl_i64{P.mn.data(), (int64_t) P.mn.size()},
                         sl_i64{P.mx.data(), (int64_t) P.mx.size()}, sl_u8{P.pool.data(), (int64_t) P.pool.size()},
                         sl_u8{out.data(), (int64_t) out.size()}, sl_i64{P.st.data(), (int64_t) P.st.size()});
    root_out = root;
    P_out = std::move(P);
    return std::string((const char *) out.data(), (size_t) n);
}

static void first_diff(const std::string & a, const std::string & b) {
    size_t i = 0;
    while (i < a.size() && i < b.size() && a[i] == b[i]) i++;
    size_t from = i > 120 ? i - 120 : 0;
    printf("  at %zu\n  native: ...%s\n  port  : ...%s\n",
           i, a.substr(from, 240).c_str(), b.substr(from, 240).c_str());
}

// ---- identity gate ---------------------------------------------------------
// The textual dump prints "[cycle]" for every repeated node, so two different
// nodes with the same content are indistinguishable there (this is exactly the
// digits/digits0 blind spot). This gate walks the native arena and the port
// arena in lockstep and requires a graph isomorphism: same kind, same payload,
// same child order, and the same sharing (a native id used twice must be one
// port id used twice, and the reverse).
enum NK {
    NK_EPS, NK_START, NK_END, NK_LIT, NK_SEQ, NK_CHOICE, NK_REP, NK_AND, NK_NOT,
    NK_ANY, NK_SPACE, NK_CHARS, NK_STRING, NK_UNTIL, NK_SCHEMA, NK_RULE, NK_REF,
    NK_ATOMIC, NK_TAG, NK_GBNF, NK_AC
};

// Port kind numbers, as in runtime.goose.
static int port_nk(int64_t k) {
    switch (k) {
        case 0: return NK_EPS;
        case 1: return NK_START;
        case 2: return NK_END;
        case 3: return NK_LIT;
        case 4: return NK_SEQ;
        case 5: return NK_CHOICE;
        case 6: return NK_REP;
        case 8: return NK_NOT;
        case 9: return NK_ANY;
        case 13: return NK_UNTIL;
        case 7: return NK_AND;
        case 10: return NK_SPACE;
        case 11: return NK_CHARS;
        case 12: return NK_STRING;
        case 14: return NK_RULE;
        case 16: return NK_ATOMIC;
        case 17: return NK_TAG;
        case 18: return NK_SCHEMA;
        case 20: return NK_AC;
        default: return -1;
    }
}

static int native_nk(const common_peg_parser_variant & v) {
    if (std::get_if<common_peg_epsilon_parser>(&v)) return NK_EPS;
    if (std::get_if<common_peg_start_parser>(&v)) return NK_START;
    if (std::get_if<common_peg_end_parser>(&v)) return NK_END;
    if (std::get_if<common_peg_literal_parser>(&v)) return NK_LIT;
    if (std::get_if<common_peg_sequence_parser>(&v)) return NK_SEQ;
    if (std::get_if<common_peg_choice_parser>(&v)) return NK_CHOICE;
    if (std::get_if<common_peg_repetition_parser>(&v)) return NK_REP;
    if (std::get_if<common_peg_and_parser>(&v)) return NK_AND;
    if (std::get_if<common_peg_not_parser>(&v)) return NK_NOT;
    if (std::get_if<common_peg_any_parser>(&v)) return NK_ANY;
    if (std::get_if<common_peg_space_parser>(&v)) return NK_SPACE;
    if (std::get_if<common_peg_chars_parser>(&v)) return NK_CHARS;
    if (std::get_if<common_peg_string_parser>(&v)) return NK_STRING;
    if (std::get_if<common_peg_until_parser>(&v)) return NK_UNTIL;
    if (std::get_if<common_peg_schema_parser>(&v)) return NK_SCHEMA;
    if (std::get_if<common_peg_rule_parser>(&v)) return NK_RULE;
    if (std::get_if<common_peg_ref_parser>(&v)) return NK_REF;
    if (std::get_if<common_peg_atomic_parser>(&v)) return NK_ATOMIC;
    if (std::get_if<common_peg_tag_parser>(&v)) return NK_TAG;
    if (std::get_if<common_peg_gbnf_parser>(&v)) return NK_GBNF;
    return NK_AC;
}

static const char * nk_name(int k) {
    static const char * n[] = {"Epsilon", "Start", "End", "Literal", "Sequence", "Choice",
                               "Repetition", "And", "Not", "Any", "Space", "CharRepeat",
                               "String", "Until", "Schema", "Rule", "Ref", "Atomic", "Tag",
                               "Gbnf", "Ac"};
    return (k >= 0 && k < 21) ? n[k] : "?";
}

static std::string port_str(const Port & P, int64_t sid) {
    if (sid < 0) return std::string();
    return std::string((const char *) P.pool.data() + P.dstart[sid], (size_t) P.dcount[sid]);
}

static bool native_children(const common_peg_parser_variant & v, std::vector<common_peg_parser_id> & out) {
    if (auto * p = std::get_if<common_peg_sequence_parser>(&v)) { out = p->children; return true; }
    if (auto * p = std::get_if<common_peg_choice_parser>(&v)) { out = p->children; return true; }
    if (auto * p = std::get_if<common_peg_repetition_parser>(&v)) { out = {p->child}; return true; }
    if (auto * p = std::get_if<common_peg_not_parser>(&v)) { out = {p->child}; return true; }
    if (auto * p = std::get_if<common_peg_and_parser>(&v)) { out = {p->child}; return true; }
    if (auto * p = std::get_if<common_peg_atomic_parser>(&v)) { out = {p->child}; return true; }
    if (auto * p = std::get_if<common_peg_tag_parser>(&v)) { out = {p->child}; return true; }
    if (auto * p = std::get_if<common_peg_rule_parser>(&v)) { out = {p->child}; return true; }
    if (auto * p = std::get_if<common_peg_schema_parser>(&v)) { out = {p->child}; return true; }
    if (auto * p = std::get_if<common_peg_gbnf_parser>(&v)) { out = {p->child}; return true; }
    if (auto * p = std::get_if<common_peg_ac_parser>(&v)) { out = {p->child}; return true; }
    return false;
}

static bool payload_eq(const common_peg_parser_variant & v, int nk, const Port & P, int64_t pid,
                       std::string & why) {
    if (auto * p = std::get_if<common_peg_literal_parser>(&v)) {
        if (p->literal != port_str(P, P.sidx[pid])) { why = "literal"; return false; }
    } else if (auto * p = std::get_if<common_peg_rule_parser>(&v)) {
        if (p->name != port_str(P, P.sidx[pid])) { why = "rule name"; return false; }
    } else if (auto * p = std::get_if<common_peg_tag_parser>(&v)) {
        if (p->tag != port_str(P, P.sidx[pid])) { why = "tag name"; return false; }
    } else if (auto * p = std::get_if<common_peg_chars_parser>(&v)) {
        if (p->pattern != port_str(P, P.sidx[pid]) || p->min_count != P.mn[pid] ||
            p->max_count != P.mx[pid]) { why = "CharRepeat pattern/bounds"; return false; }
    } else if (auto * p = std::get_if<common_peg_repetition_parser>(&v)) {
        if (p->min_count != P.mn[pid] || p->max_count != P.mx[pid]) { why = "Repetition bounds"; return false; }
    } else if (auto * p = std::get_if<common_peg_string_parser>(&v)) {
        const std::string d(1, p->delimiter);
        if (d != port_str(P, P.sidx[pid])) { why = "String delimiter"; return false; }
    } else if (auto * p = std::get_if<common_peg_until_parser>(&v)) {
        std::string joined;
        for (size_t i = 0; i < p->delimiters.size(); i++) {
            if (i) joined += " | ";
            joined += p->delimiters[i];
        }
        if (joined != port_str(P, P.sidx[pid])) { why = "Until delimiters"; return false; }
    } else if (auto * p = std::get_if<common_peg_schema_parser>(&v)) {
        const std::string k = p->node ? common_chat_schema::kind_name(p->node->kind()) : "null";
        if (k != port_str(P, P.sidx[pid])) { why = "Schema kind"; return false; }
    }
    (void) nk;
    return true;
}

static bool iso_walk(const common_peg_arena & a, int64_t nid, const Port & P, int64_t pid,
                     std::map<int64_t, int64_t> & n2p, std::map<int64_t, int64_t> & p2n,
                     std::string & why) {
    auto in = n2p.find(nid);
    auto ip = p2n.find(pid);
    if (in != n2p.end() || ip != p2n.end()) {
        if (in == n2p.end() || ip == p2n.end() || in->second != pid) {
            why = "sharing differs at native id " + std::to_string(nid) + " / port id " +
                  std::to_string(pid) + " (" + nk_name(native_nk(a.get(nid))) + ")";
            return false;
        }
        return true;
    }
    n2p[nid] = pid;
    p2n[pid] = nid;

    const auto & v = a.get(nid);
    int nk = native_nk(v);
    int pk = port_nk(P.kind[pid]);
    if (nk != pk) {
        why = std::string("kind ") + nk_name(nk) + " vs " + nk_name(pk);
        return false;
    }
    if (!payload_eq(v, nk, P, pid, why)) return false;

    std::vector<common_peg_parser_id> nc;
    native_children(v, nc);
    if ((int64_t) nc.size() != P.ccount[pid]) {
        why = std::string("child count in ") + nk_name(nk);
        return false;
    }
    for (size_t i = 0; i < nc.size(); i++) {
        if (!iso_walk(a, nc[i], P, P.children[P.cfirst[pid] + (int64_t) i], n2p, p2n, why)) return false;
    }
    return true;
}

static std::map<std::string, std::string> parse_rules(const std::string & text) {
    std::map<std::string, std::string> m;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        std::string line = text.substr(pos, nl - pos);
        size_t sep = line.find(" ::= ");
        if (sep != std::string::npos) {
            m[line.substr(0, sep)] = line.substr(sep + 5);
        }
        pos = nl + 1;
    }
    return m;
}

static std::string native_grammar(const common_peg_arena & a) {
    common_grammar_builder b;
    std::vector<std::pair<std::string, std::string>> rules;
    b.add_rule = [&](const std::string & n, const std::string & body) {
        rules.emplace_back(n, body);
        return n;
    };
    b.add_schema = [&](const std::string & n, const common_chat_schema &) { return n; };
    a.build_grammar(b, false);
    std::string out;
    for (auto & [n, body] : rules) out += n + " ::= " + body + "\n";
    return out;
}

static std::string port_grammar(const Port & P, int64_t root) {
    static char out[1 << 20];
    static uint8_t scratch[1 << 20];
    std::vector<int64_t> meta(8, 0), vis(4096, 0), rids(4096, 0), hids(4096, 0);
    std::vector<int64_t> roff(4096, 0), rlen(4096, 0), boff(4096, 0), blen(4096, 0);
    sl_i64 mi{meta.data(), (int64_t) meta.size()};
    sl_i64 vi{vis.data(), (int64_t) vis.size()};
    sl_i64 ri{rids.data(), (int64_t) rids.size()};
    sl_i64 hi{hids.data(), (int64_t) hids.size()};
    sl_i64 ro{roff.data(), (int64_t) roff.size()};
    sl_i64 rl{rlen.data(), (int64_t) rlen.size()};
    sl_i64 bo{boff.data(), (int64_t) boff.size()};
    sl_i64 bl{blen.data(), (int64_t) blen.size()};
    int64_t len = autoparser_arena_gbnf(root, 0, 4096,
        sl_i64{(int64_t *) P.kind.data(), (int64_t) P.kind.size()},
        sl_i64{(int64_t *) P.cfirst.data(), (int64_t) P.cfirst.size()},
        sl_i64{(int64_t *) P.ccount.data(), (int64_t) P.ccount.size()},
        sl_i64{(int64_t *) P.children.data(), (int64_t) P.children.size()},
        sl_i64{(int64_t *) P.sidx.data(), (int64_t) P.sidx.size()},
        sl_i64{(int64_t *) P.dstart.data(), (int64_t) P.dstart.size()},
        sl_i64{(int64_t *) P.dcount.data(), (int64_t) P.dcount.size()},
        sl_i64{(int64_t *) P.mn.data(), (int64_t) P.mn.size()},
        sl_i64{(int64_t *) P.mx.data(), (int64_t) P.mx.size()},
        sl_u8{(uint8_t *) (void *) P.pool.data(), (int64_t) P.pool.size()},
        mi, vi, ri, hi, ro, rl, bo, bl,
        sl_u8{scratch, sizeof scratch}, sl_u8{(uint8_t *) out, sizeof out});
    return std::string(out, (size_t) len);
}

static std::map<int64_t, int64_t> g_n2p;

static std::string identity_check(const common_peg_arena & a, const Port & P, int64_t root,
                                  bool & ok) {
    std::map<int64_t, int64_t> n2p, p2n;
    std::string why;
    ok = iso_walk(a, a.root(), P, root, n2p, p2n, why);
    g_n2p = n2p;
    return ok ? std::string() : why;
}

static std::vector<Case> make_cases() {
    std::vector<Case> cases;
    {
        Case c{};
        c.id = "CT-1"; c.json = false;
        c.names = {"get_weather"}; c.props = {{"location", "unit"}}; c.has_fn = {true};
        c.parallel = false; c.force = true;
        cases.push_back(c);
    }
    {
        Case c{};
        c.id = "CT-2"; c.json = false;
        c.markers = {
            {"tool_call_start_marker", "<|tool_call_begin|>"},
            {"tool_call_end_marker", "<|tool_call_end|>"},
            {"function_opener", "<|function:"},
            {"function_name_suffix", "|>"},
            {"function_closer", "<|/function|>"},
            {"parameter_key_prefix", "<|param:"},
            {"parameter_key_suffix", "|>"},
            {"parameter_closer", "<|/param|>"},
        };
        c.names = {"get_weather", "ping"}; c.props = {{"location", "unit"}, {}}; c.has_fn = {true, true};
        c.parallel = true; c.force = false;
        cases.push_back(c);
    }
    {
        Case c{};
        c.id = "CT-3"; c.json = false;
        c.parallel = false; c.force = true;
        cases.push_back(c);
    }
    {
        Case c{};
        c.id = "CT-4"; c.json = false;
        c.names = {"", ""}; c.props = {{}, {}}; c.has_fn = {false, false};
        c.parallel = false; c.force = true;
        cases.push_back(c);
    }
    {
        Case c{};
        c.id = "CT-5"; c.json = false;
        c.markers = {{"function_opener", "<fn:"}, {"parameter_closer", "<|/p|>"}};
        c.names = {"a", "b", "c"}; c.props = {{"x"}, {}, {"y", "z"}}; c.has_fn = {true, true, true};
        c.parallel = true; c.force = true;
        cases.push_back(c);
    }
    {
        Case c{};
        c.id = "J-1"; c.json = true;
        c.start = "<tool_call>"; c.end = "</tool_call>";
        c.names = {"get_weather"}; c.props = {{"location", "unit"}}; c.has_fn = {true};
        c.parallel = false; c.force = true;
        cases.push_back(c);
    }
    {
        Case c{};
        c.id = "J-2"; c.json = true;
        c.start = "<|START_ACTION|>"; c.end = "<|END_ACTION|>";
        c.name_key = "tool_name"; c.args_key = "parameters"; c.gen_call_id_key = "tool_call_id";
        c.params_order = {"tool_call_id", "tool_name", "parameters"};
        c.names = {"get_weather"}; c.props = {{"location", "unit"}}; c.has_fn = {true};
        c.parallel = true; c.force = false;
        cases.push_back(c);
    }
    {
        Case c{};
        c.id = "J-3"; c.json = true;
        c.start = "<tool_calls>"; c.end = "</tool_calls>";
        c.array_wrapped = true; c.accept_wrapper = true; c.call_id_key = "id";
        c.names = {"get_weather"}; c.props = {{"location", "unit"}}; c.has_fn = {true};
        c.parallel = true; c.force = true;
        cases.push_back(c);
    }
    {
        Case c{};
        c.id = "J-4"; c.json = true;
        c.start = "<tool_calls>"; c.end = "</tool_calls>";
        c.args_key = "args"; c.function_is_key = true; c.call_id_key = "id";
        c.names = {"get_weather"}; c.props = {{"location", "unit"}}; c.has_fn = {true};
        c.parallel = true; c.force = false;
        cases.push_back(c);
    }
    {
        Case c{};
        c.id = "J-5"; c.json = true;
        c.start = "<tool_call>"; c.end = "</tool_call>";
        c.name_key = "function.name"; c.args_key = "function.arguments"; c.call_id_key = "id";
        c.names = {"get_weather"}; c.props = {{"location", "unit"}}; c.has_fn = {true};
        c.parallel = true; c.force = false;
        cases.push_back(c);
    }
    {
        // function_is_key, no args wrapper, no id fields: inner is the args parser alone
        Case c{};
        c.id = "T-1"; c.json = true;
        c.start = "<tool_calls>"; c.end = "</tool_calls>";
        c.function_is_key = true;
        c.names = {"get_weather"}; c.props = {{"location", "unit"}}; c.has_fn = {true};
        c.parallel = true; c.force = false;
        cases.push_back(c);
    }
    {
        // function_is_key with a generated numeric id only
        Case c{};
        c.id = "T-2"; c.json = true;
        c.start = "<tool_calls>"; c.end = "</tool_calls>";
        c.function_is_key = true; c.args_key = "args"; c.gen_call_id_key = "tool_call_id";
        c.names = {"get_weather"}; c.props = {{"location", "unit"}}; c.has_fn = {true};
        c.parallel = true; c.force = true;
        cases.push_back(c);
    }
    {
        // flat with a string call id and no parameters_order (insertion order)
        Case c{};
        c.id = "T-3"; c.json = true;
        c.start = "<tool_call>"; c.end = "</tool_call>";
        c.call_id_key = "id";
        c.names = {"get_weather"}; c.props = {{"location", "unit"}}; c.has_fn = {true};
        c.parallel = false; c.force = true;
        cases.push_back(c);
    }
    return cases;
}

int main(int argc, char ** argv) {
    goose_init(0, 0);
    std::vector<Case> cases = make_cases();
    const char * only = argc > 1 ? argv[1] : nullptr;
    int fails = 0, ran = 0;
    for (const Case & c : cases) {
        if (only && strcmp(only, c.id) != 0) continue;
        ran++;
        common_peg_arena a = build_native(c);
        std::string n = a.dump(a.root());
        int64_t port_root = 0;
        Port P;
        std::string p = build_port(c, port_root, P);
        if (getenv("DUMP")) printf("%s\n", p.c_str());
        bool id_ok = false;
        std::string why = identity_check(a, P, port_root, id_ok);
        auto nr = parse_rules(native_grammar(a));
        auto pr = parse_rules(port_grammar(P, port_root));
        nr.erase("space"); pr.erase("space");
        bool gbnf_ok = nr == pr;
        if (!gbnf_ok && getenv("GBNF_DIFF")) {
            int shown = 0;
            for (auto & [k, v] : nr) {
                auto it = pr.find(k);
                if (it == pr.end()) {
                    printf("  only-native %s ::= %s\n", k.c_str(), v.substr(0, 80).c_str());
                    if (++shown >= 4) break;
                } else if (it->second != v) {
                    printf("  body-diff %s\n    native: %s\n    port  : %s\n", k.c_str(),
                           v.substr(0, 90).c_str(), it->second.substr(0, 90).c_str());
                    if (++shown >= 4) break;
                }
            }
            if (getenv("GBNF_DIFF") && !strcmp(getenv("GBNF_DIFF"), "3")) {
                for (size_t i = 0; i < 20 && i < a.size(); i++) {
                    const auto & v = a.get(i);
                    int nk = -1;
                    if (std::holds_alternative<common_peg_epsilon_parser>(v)) nk = 0;
                    else if (std::holds_alternative<common_peg_start_parser>(v)) nk = 1;
                    else if (std::holds_alternative<common_peg_end_parser>(v)) nk = 2;
                    else if (std::holds_alternative<common_peg_literal_parser>(v)) nk = 3;
                    else if (std::holds_alternative<common_peg_sequence_parser>(v)) nk = 4;
                    else if (std::holds_alternative<common_peg_choice_parser>(v)) nk = 5;
                    else if (std::holds_alternative<common_peg_repetition_parser>(v)) nk = 6;
                    else if (std::holds_alternative<common_peg_any_parser>(v)) nk = 9;
                    else if (std::holds_alternative<common_peg_space_parser>(v)) nk = 10;
                    else if (std::holds_alternative<common_peg_until_parser>(v)) nk = 13;
                    else if (std::holds_alternative<common_peg_rule_parser>(v)) nk = 14;
                    else if (std::holds_alternative<common_peg_tag_parser>(v)) nk = 17;
                    else if (std::holds_alternative<common_peg_ac_parser>(v)) nk = 20;
                    printf("  node %2zu native=%2d port=%2lld\n", i, nk, (long long) P.kind[i]);
                }
            }
            if (getenv("GBNF_DIFF") && !strcmp(getenv("GBNF_DIFF"), "2")) {
                for (size_t ni = 0; ni < a.size(); ni++) {
                    const auto & v = a.get(ni);
                    bool h = std::holds_alternative<common_peg_until_parser>(v) ||
                             std::holds_alternative<common_peg_ac_parser>(v);
                    if (h) {
                        auto it = g_n2p.find((int64_t) ni);
                        printf("  id native=%zu port=%lld\n", ni,
                               it == g_n2p.end() ? -1LL : (long long) it->second);
                    }
                }
            }
            for (auto & [k, v] : pr) {
                if (nr.find(k) == nr.end()) {
                    printf("  only-port %s ::= %s\n", k.c_str(), v.substr(0, 80).c_str());
                    if (++shown >= 8) break;
                }
            }
        }
        if (n == p && id_ok) {
            printf("%-5s ok   nodes=%zu bytes=%zu identity=ok gbnf=%s\n", c.id, a.size(), n.size(),
                   gbnf_ok ? "ok" : "DIFF");
        } else if (n != p) {
            printf("%-5s FAIL dump native=%zu port=%zu\n", c.id, n.size(), p.size());
            first_diff(n, p);
            fails++;
        } else {
            printf("%-5s FAIL identity: %s\n", c.id, why.c_str());
            fails++;
        }
    }
    printf("cases=%d failures=%d\n", ran, fails);
    return fails ? 1 : 0;
}
