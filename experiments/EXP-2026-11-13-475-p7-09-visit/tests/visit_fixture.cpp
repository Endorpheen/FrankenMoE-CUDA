// Differential fixture for the visit() rule-text assembly (EXP475).
// Oracle: the real common/json-schema-to-grammar.cpp converter via build_grammar.
// Subject: experiments/EXP-2026-11-13-475-p7-09-visit/source/visit.goose.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>

#include "visit-gen.h"
#include "json-schema-to-grammar.h"
#include "json-schema.h"

std::string string_join(const std::vector<std::string> & v, const std::string & sep) {
    std::string r;
    for (size_t i = 0; i < v.size(); ++i) { if (i) r += sep; r += v[i]; }
    return r;
}
std::vector<std::string> string_split(const std::string & s, const std::string & d) {
    std::vector<std::string> o; size_t p = 0;
    while (true) { size_t q = s.find(d, p); if (q == std::string::npos) { o.push_back(s.substr(p)); break; } o.push_back(s.substr(p, q - p)); p = q + d.size(); }
    return o;
}
std::string string_repeat(const std::string & s, size_t n) { std::string r; while (n--) r += s; return r; }

static int cases = 0, values = 0, fails = 0;
static void bad(const char * what, const std::string & detail) {
    ++fails;
    if (fails <= 15) printf("FAIL %s: %s\n", what, detail.c_str());
}
static void eq(const char * what, const std::string & got, const std::string & want, const std::string & ctx) {
    ++values;
    if (got != want) bad(what, ctx + "\n  got =[" + got + "]\n  want=[" + want + "]");
}

static sl_u8 sl(const std::string & s) { return sl_u8{ (uint8_t *) s.data(), (int64_t) s.size() }; }
static sl_u8 slv(std::vector<uint8_t> & v) { return sl_u8{ v.data(), (int64_t) v.size() }; }
static sl_i64 sli(std::vector<int64_t> & v) { return sl_i64{ v.data(), (int64_t) v.size() }; }

static std::map<std::string, std::string> parse_rules(const std::string & g) {
    std::map<std::string, std::string> m;
    size_t p = 0;
    while (p < g.size()) {
        size_t e = g.find('\n', p);
        if (e == std::string::npos) e = g.size();
        const std::string line = g.substr(p, e - p);
        const size_t sep = line.find(" ::= ");
        if (sep != std::string::npos) m[line.substr(0, sep)] = line.substr(sep + 5);
        p = e + 1;
    }
    return m;
}

// rule-name sanitizer, used only to look up native kv/rest names in the fixture
static std::string esc(const std::string & s) {
    std::string r;
    bool run = false;
    for (unsigned char c : s) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-';
        if (ok) { r.push_back((char) c); run = false; }
        else if (!run) { r.push_back('-'); run = true; }
    }
    return r;
}

enum Kind { K_STRING, K_BOOLEAN, K_NUMBER, K_NULL, K_ANY };
static const char * prim_name(Kind k) {
    switch (k) {
        case K_STRING: return "string";
        case K_BOOLEAN: return "boolean";
        case K_NUMBER: return "number";
        case K_NULL: return "null";
        default: return "value";
    }
}
static std::unique_ptr<common_chat_schema> prim(Kind k) {
    switch (k) {
        case K_STRING: return std::make_unique<common_chat_schema_string>();
        case K_BOOLEAN: return std::make_unique<common_chat_schema_boolean>();
        case K_NUMBER: return std::make_unique<common_chat_schema_number>();
        case K_NULL: return std::make_unique<common_chat_schema_null>();
        default: return std::make_unique<common_chat_schema_any>();
    }
}

struct Goose {
    std::vector<uint8_t> strs, out, abuf, rtext;
    std::vector<int64_t> dir, req, ast, kvdir, opt, rp, rst;
    Goose() : strs(1 << 16), out(1 << 16), abuf(1 << 20), rtext(1 << 16),
              dir(6 * 64), req(64), ast(1), kvdir(2 * 65), opt(65), rp(4 * 65), rst(2) {}
    void reset_inputs() {
        std::fill(out.begin(), out.end(), 0);
        std::fill(abuf.begin(), abuf.end(), 0);
        std::fill(rtext.begin(), rtext.end(), 0);
        ast[0] = 0;
        std::fill(rst.begin(), rst.end(), 0);
    }
};

struct Prop { std::string name; Kind kind; bool required; };

static void check_object(Goose & g, const std::string & label, const std::vector<Prop> & props,
                         int add_kind, Kind add_value) {
    auto o = std::make_unique<common_chat_schema_object>();
    for (const auto & p : props) {
        common_chat_schema_property cp;
        cp.name = p.name;
        cp.schema = prim(p.kind);
        cp.required = p.required;
        o->properties.push_back(std::move(cp));
    }
    if (add_kind == 1) o->additional_properties = prim(K_ANY);
    if (add_kind == 2) o->additional_properties = prim(add_value);

    std::map<std::string, std::string> nr;
    try {
        nr = parse_rules(build_grammar([&](const common_grammar_builder & b) {
            b.add_schema("root", *o);
        }, { false }));
    } catch (const std::exception & e) {
        bad("native throw", label + ": " + e.what());
        return;
    }

    ++cases;
    const int64_t n = (int64_t) props.size();
    if (add_kind == 1 && props.empty()) {
        ++values;
        const int64_t branch = object_branch(0, 1);
        if (branch != 1) bad("object_branch", label + " expected 1");
        eq("empty+any maps to object", nr["root"], "object", label);
        return;
    }
    ++values;
    if (object_branch(n, add_kind) != 2) bad("object_branch", label + " expected 2");

    g.reset_inputs();
    int64_t off = 0;
    for (int64_t i = 0; i < n; ++i) {
        const std::string dump = common_json(props[i].name).dump();
        const std::string child = prim_name(props[i].kind);
        g.dir[6 * i + 0] = off; g.dir[6 * i + 1] = (int64_t) props[i].name.size();
        for (unsigned char c : props[i].name) g.strs[off++] = c;
        g.dir[6 * i + 2] = off; g.dir[6 * i + 3] = (int64_t) dump.size();
        for (unsigned char c : dump) g.strs[off++] = c;
        g.dir[6 * i + 4] = off; g.dir[6 * i + 5] = (int64_t) child.size();
        for (unsigned char c : child) g.strs[off++] = c;
        g.req[i] = props[i].required ? 1 : 0;
    }
    const std::string add_name = prim_name(add_value);
    const int64_t len = object_rule(sl(""), slv(g.strs), sli(g.dir), sli(g.req), n,
        add_kind, sl(add_name), slv(g.out), slv(g.abuf), sli(g.ast), sli(g.kvdir), sli(g.opt),
        slv(g.rtext), sli(g.rp), sli(g.rst));
    const std::string body((const char *) g.out.data(), (size_t) len);
    eq("object body", body, nr["root"], label);

    // property kv texts
    for (const auto & p : props) {
        const std::string dump = common_json(p.name).dump();
        std::vector<uint8_t> kvout(4096, 0);
        const int64_t kl = kv_rule(sl(dump), sl(prim_name(p.kind)), slv(kvout));
        const std::string got((const char *) kvout.data(), (size_t) kl);
        const std::string key = esc(p.name + "-kv");
        auto it = nr.find(key);
        if (it == nr.end()) bad("missing kv", label + " key=" + key);
        else eq("kv text", got, it->second, label + " key=" + key);
    }

    // additional kv
    if (add_kind != 0) {
        const std::string key_rule = props.empty() ? "string" : "additional-k";
        const std::string value_rule = add_kind == 1 ? "value" : add_name;
        std::vector<uint8_t> kvout(8192, 0);
        const int64_t kl = additional_kv(sl(key_rule), sl(value_rule), slv(kvout));
        const std::string got((const char *) kvout.data(), (size_t) kl);
        auto it = nr.find("additional-kv");
        if (it == nr.end()) bad("missing additional-kv", label);
        else eq("additional kv", got, it->second, label);
    }

    // rest rules emitted by the recursive optional chain
    int64_t native_rests = 0;
    for (const auto & kv : nr) if (kv.first.size() >= 5 && kv.first.compare(kv.first.size() - 5, 5, "-rest") == 0) ++native_rests;
    ++values;
    if (g.rst[0] != native_rests) bad("rest count", label + " goose=" + std::to_string(g.rst[0]) + " native=" + std::to_string(native_rests));
    for (int64_t i = 0; i < g.rst[0]; ++i) {
        const std::string name((const char *) g.rtext.data() + g.rp[4 * i + 0], (size_t) g.rp[4 * i + 1]);
        const std::string text((const char *) g.rtext.data() + g.rp[4 * i + 2], (size_t) g.rp[4 * i + 3]);
        auto it = nr.find(name);
        if (it == nr.end()) bad("missing rest", label + " name=" + name);
        else eq("rest text", text, it->second, label + " name=" + name);
    }
}

static void check_tuple(Goose & g, const std::string & label, const std::vector<Kind> & items) {
    auto t = std::make_unique<common_chat_schema_tuple>();
    for (Kind k : items) t->items.push_back(prim(k));
    const auto nr = parse_rules(build_grammar([&](const common_grammar_builder & b) {
        b.add_schema("root", *t);
    }, { false }));
    ++cases;
    g.reset_inputs();
    int64_t off = 0;
    for (size_t i = 0; i < items.size(); ++i) {
        const std::string name = prim_name(items[i]);
        g.dir[2 * i + 0] = off;
        g.dir[2 * i + 1] = (int64_t) name.size();
        for (unsigned char c : name) g.strs[off++] = c;
    }
    const int64_t len = tuple_rule(slv(g.strs), sli(g.dir), (int64_t) items.size(), slv(g.out));
    eq("tuple body", std::string((const char *) g.out.data(), (size_t) len), nr.at("root"), label);
}

static void check_array(Goose & g, const std::string & label, Kind item, int mn, int mx) {
    auto a = std::make_unique<common_chat_schema_array>();
    a->items = prim(item);
    a->min_items = mn;
    a->max_items = mx;
    const auto nr = parse_rules(build_grammar([&](const common_grammar_builder & b) {
        b.add_schema("root", *a);
    }, { false }));
    ++cases;
    g.reset_inputs();
    const std::string name = prim_name(item);
    const int64_t len = array_rule(sl(name), mn, mx, slv(g.out), slv(g.abuf));
    eq("array body", std::string((const char *) g.out.data(), (size_t) len), nr.at("root"), label);
}

static void check_union(Goose & g, const std::string & label, const std::vector<Kind> & alts) {
    auto u = std::make_unique<common_chat_schema_any_of>();
    for (Kind k : alts) u->children.push_back(prim(k));
    const auto nr = parse_rules(build_grammar([&](const common_grammar_builder & b) {
        b.add_schema("root", *u);
    }, { false }));
    ++cases;
    g.reset_inputs();
    int64_t off = 0;
    for (size_t i = 0; i < alts.size(); ++i) {
        const std::string name = prim_name(alts[i]);
        g.dir[2 * i + 0] = off;
        g.dir[2 * i + 1] = (int64_t) name.size();
        for (unsigned char c : name) g.strs[off++] = c;
    }
    const int64_t len = union_rule(slv(g.strs), sli(g.dir), (int64_t) alts.size(), slv(g.out));
    eq("union body", std::string((const char *) g.out.data(), (size_t) len), nr.at("root"), label);
}

int main(void) {
    goose_init(0, 0);
    Goose g;

    check_object(g, "empty", {}, 0, K_NUMBER);
    check_object(g, "empty+any", {}, 1, K_ANY);
    check_object(g, "empty+number", {}, 2, K_NUMBER);
    check_object(g, "one required", {{"a", K_STRING, true}}, 0, K_NUMBER);
    check_object(g, "req+opt", {{"a", K_STRING, true}, {"b", K_BOOLEAN, false}}, 0, K_NUMBER);
    check_object(g, "req+opt+opt", {{"a", K_STRING, true}, {"b", K_BOOLEAN, false}, {"c", K_NUMBER, false}}, 0, K_NUMBER);
    check_object(g, "three optional", {{"a", K_STRING, false}, {"b", K_BOOLEAN, false}, {"c", K_NULL, false}}, 0, K_NUMBER);
    check_object(g, "all required", {{"a", K_STRING, true}, {"b", K_BOOLEAN, true}, {"c", K_NUMBER, true}}, 0, K_NUMBER);
    check_object(g, "req+number", {{"a", K_STRING, true}}, 2, K_NUMBER);
    check_object(g, "opt+opt+string", {{"a", K_STRING, false}, {"b", K_BOOLEAN, false}}, 2, K_STRING);
    check_object(g, "req+opt+any", {{"a", K_STRING, true}, {"b", K_BOOLEAN, false}}, 1, K_ANY);
    check_object(g, "req+opt+bool", {{"a", K_STRING, true}, {"b", K_BOOLEAN, false}}, 2, K_BOOLEAN);
    check_object(g, "weird names", {{"a.b", K_STRING, true}, {"c d", K_BOOLEAN, false}, {"9x", K_NUMBER, false}}, 0, K_NUMBER);
    check_object(g, "unicode name", {{"caf\xc3\xa9", K_STRING, true}, {"x-y", K_BOOLEAN, false}}, 0, K_NUMBER);

    check_tuple(g, "tuple 0", {});
    check_tuple(g, "tuple 1", {K_STRING});
    check_tuple(g, "tuple 2", {K_STRING, K_BOOLEAN});
    check_tuple(g, "tuple 3", {K_STRING, K_BOOLEAN, K_NULL});

    for (int mn = 0; mn <= 3; ++mn)
        for (int mx = -1; mx <= 5; ++mx)
            check_array(g, "array " + std::to_string(mn) + ".." + std::to_string(mx), K_STRING, mn, mx);

    check_union(g, "anyOf 1", {K_STRING});
    check_union(g, "anyOf 2", {K_STRING, K_BOOLEAN});
    check_union(g, "anyOf 3", {K_STRING, K_BOOLEAN, K_NULL});

    printf("FG475_VISIT cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
