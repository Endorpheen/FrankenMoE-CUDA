// Differential fixture for the visit() dispatch, rule map and $ref targets
// (EXP479). Oracle: json_schema_to_grammar on the JSON form of the same schema.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

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
    if (fails <= 12) printf("FAIL %s: %s\n", what, detail.c_str());
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

enum Kind { K_ANY, K_REF, K_ANYOF, K_OBJ, K_STR, K_BOOL, K_NUM, K_NULL };
struct N {
    int kind;
    std::vector<std::tuple<std::string, N *, bool>> props;
    std::vector<N *> children;
    std::string refname;
    N * target = nullptr;
    bool add_any = false;
    explicit N(int k) : kind(k) {}
};
static std::vector<N*> all_nodes;
static N * mk(int k) { N * n = new N(k); all_nodes.push_back(n); return n; }

static std::string json_of(N * n) {
    switch (n->kind) {
        case K_ANY: return "{}";
        case K_REF: return "{\"$ref\":\"#/definitions/" + n->refname + "\"}";
        case K_ANYOF: {
            std::string s = "{\"anyOf\":[";
            for (size_t i = 0; i < n->children.size(); ++i) { if (i) s += ","; s += json_of(n->children[i]); }
            return s + "]}";
        }
        case K_OBJ: {
            std::string props, req;
            for (auto & [name, child, required] : n->props) {
                if (!props.empty()) props += ",";
                props += common_json(name).dump() + ":" + json_of(child);
                if (required) { if (!req.empty()) req += ","; req += common_json(name).dump(); }
            }
            std::string out = "{\"type\":\"object\"";
            if (!props.empty()) out += ",\"properties\":{" + props + "}";
            if (!req.empty()) out += ",\"required\":[" + req + "]";
            if (n->add_any) out += ",\"additionalProperties\":true";
            return out + "}";
        }
        case K_STR: return "{\"type\":\"string\"}";
        case K_BOOL: return "{\"type\":\"boolean\"}";
        case K_NUM: return "{\"type\":\"number\"}";
        default: return "{\"type\":\"null\"}";
    }
}

static std::string json_doc(N * root, const std::map<std::string, N *> & defs) {
    std::string s = json_of(root);
    if (!defs.empty()) {
        std::string d;
        for (auto & [name, node] : defs) { if (!d.empty()) d += ","; d += "\"" + name + "\":" + json_of(node); }
        s = s.substr(0, s.size() - 1) + ",\"definitions\":{" + d + "}}";
    }
    return s;
}

// primitive ids: 0 value,1 object,2 array,3 string,4 number,5 boolean,6 null,
// 7 char,8 integral-part,9 decimal-part
struct PrimTable { std::vector<std::string> names, contents; std::vector<std::vector<int>> deps; std::string space; };

static PrimTable build_prim_table() {
    PrimTable t;
    t.names = {"value", "object", "array", "string", "number", "boolean", "null", "char", "integral-part", "decimal-part"};
    t.deps = {{1,2,3,4,5,6}, {3,0}, {0}, {7}, {8,9}, {}, {}, {}, {}, {}};
    auto rules_of = [](const std::string & schema) {
        return parse_rules(json_schema_to_grammar(common_json::parse(schema)));
    };
    const auto any_rules = rules_of("{}");
    const auto obj_rules = rules_of("{\"type\":\"object\",\"additionalProperties\":true}");
    const auto arr_rules = rules_of("{\"type\":\"array\"}");
    const auto str_rules = rules_of("{\"type\":\"string\"}");
    const auto num_rules = rules_of("{\"type\":\"number\"}");
    const auto bool_rules = rules_of("{\"type\":\"boolean\"}");
    const auto null_rules = rules_of("{\"type\":\"null\"}");
    t.contents.resize(10);
    t.contents[0] = any_rules.at("value");
    t.contents[1] = obj_rules.at("object");
    t.contents[2] = arr_rules.at("root");
    t.contents[3] = str_rules.at("root");
    t.contents[4] = num_rules.at("root");
    t.contents[5] = bool_rules.at("root");
    t.contents[6] = null_rules.at("root");
    t.contents[7] = str_rules.at("char");
    t.contents[8] = num_rules.at("integral-part");
    t.contents[9] = num_rules.at("decimal-part");
    t.space = any_rules.at("space");
    return t;
}

struct Interner {
    std::vector<std::string> strings;
    std::map<std::string, int> index;
    int add(const std::string & s) {
        auto it = index.find(s);
        if (it != index.end()) return it->second;
        const int i = (int) strings.size();
        strings.push_back(s);
        index[s] = i;
        return i;
    }
};

static void check(N * root, const std::map<std::string, N *> & defs, const PrimTable & pt, const std::string & label) {
    const std::string doc = json_doc(root, defs);
    std::map<std::string, std::string> native;
    bool thrown = false;
    try {
        native = parse_rules(json_schema_to_grammar(common_json::parse(doc)));
    } catch (const std::exception &) {
        thrown = true;
    }

    Interner in;
    std::vector<int64_t> kind, pfirst, pcount, cfirst, ccount, add_kind, target, refstr;
    std::vector<int64_t> pname, pschema, preq, pdump, children;
    std::map<N *, int> idx;

    std::function<void(N *)> assign = [&](N * n) {
        if (idx.count(n)) return;
        const int me = (int) kind.size();
        idx[n] = me;
        kind.push_back(n->kind);
        pfirst.push_back(0); pcount.push_back(0);
        cfirst.push_back(0); ccount.push_back(0);
        add_kind.push_back(n->kind == K_OBJ && n->add_any ? 1 : 0); target.push_back(-1); refstr.push_back(0);
        if (n->kind == K_OBJ) {
            std::vector<std::tuple<int, int, int>> tmp;
            for (auto & [name, child, required] : n->props) {
                assign(child);
                tmp.emplace_back(in.add(name), idx[child], required ? 1 : 0);
            }
            pfirst[me] = (int64_t) pname.size();
            for (auto & [pn, sc, rq] : tmp) {
                pname.push_back(pn);
                pschema.push_back(sc);
                preq.push_back(rq);
                pdump.push_back(in.add(common_json(in.strings[pn]).dump()));
            }
            pcount[me] = (int64_t) pname.size() - pfirst[me];
        } else if (n->kind == K_ANYOF) {
            std::vector<int> kids;
            for (N * ch : n->children) { assign(ch); kids.push_back(idx[ch]); }
            cfirst[me] = (int64_t) children.size();
            for (int ch : kids) children.push_back(ch);
            ccount[me] = (int64_t) kids.size();
        } else if (n->kind == K_REF) {
            refstr[me] = in.add("#/definitions/" + n->refname);
            if (n->target) { assign(n->target); target[me] = idx[n->target]; }
        }
    };
    assign(root);

    std::vector<int64_t> ptname, ptcontent, ptdeps;
    for (size_t i = 0; i < pt.names.size(); ++i) {
        ptname.push_back(in.add(pt.names[i]));
        ptcontent.push_back(in.add(pt.contents[i]));
        for (int d = 0; d < 6; ++d) ptdeps.push_back(d < (int) pt.deps[i].size() ? pt.deps[i][d] : -1);
    }
    const int64_t space_sid = in.add(pt.space);

    std::vector<uint8_t> strs(1 << 16, 0);
    std::vector<int64_t> sdir(2 * 1024, 0);
    int64_t off = 0;
    for (size_t i = 0; i < in.strings.size(); ++i) {
        sdir[2 * i + 0] = off;
        sdir[2 * i + 1] = (int64_t) in.strings[i].size();
        for (unsigned char c : in.strings[i]) strs[off++] = c;
    }

    const int max_entries = 512;
    std::vector<int64_t> mo(max_entries, 0), ml(max_entries, 0), vo(max_entries, 0), vl(max_entries, 0), st(8, 0), rref(128, 0);
    std::vector<uint8_t> mb(1 << 20, 0);
    std::vector<uint8_t> tb(1 << 20, 0);
    std::vector<uint8_t> ob(1 << 20, 0);
    ++cases;
    const int64_t count = convert(idx[root], sli(kind), sli(pfirst), sli(pcount), sli(cfirst), sli(ccount), sli(children),
        sli(add_kind), sli(target), sli(refstr), sli(pname), sli(pschema), sli(preq), sli(pdump),
        slv(strs), sli(sdir), sli(ptname), sli(ptcontent), sli(ptdeps),
        sli(mo), sli(ml), sli(vo), sli(vl), slv(mb), slv(tb), slv(ob), sli(st), sli(rref), space_sid);

    if (thrown) {
        ++values;
        if (st[4] == 0) bad("expected native rejection", label + " goose reported no error");
        return;
    }
    std::map<std::string, std::string> got;
    for (int64_t i = 0; i < count; ++i) {
        got[std::string((const char *) mb.data() + mo[i], (size_t) ml[i])] =
            std::string((const char *) mb.data() + vo[i], (size_t) vl[i]);
    }
    ++values;
    if (got != native) {
        std::string diff;
        for (auto & [k, v] : native) {
            auto it = got.find(k);
            if (it == got.end()) diff += " missing " + k;
            else if (it->second != v) diff += " text " + k + " got=[" + it->second + "] want=[" + v + "]";
        }
        for (auto & [k, v] : got) if (!native.count(k)) diff += " extra " + k;
        bad("rule map", label + ":" + diff);
        if (label == "root any") {
            for (auto & [k, v] : native) printf("  native %s = [%s]\n", k.c_str(), v.c_str());
            for (auto & [k, v] : got) printf("  goose  %s = [%s]\n", k.c_str(), v.c_str());
        }
    }
}

int main(void) {
    goose_init(0, 0);
    const PrimTable pt = build_prim_table();

    // root object with boolean/null properties, nested object
    {
        N * o = mk(K_OBJ);
        N * inner = mk(K_OBJ);
        inner->props.push_back({"b", mk(K_BOOL), true});
        o->props.push_back({"a", mk(K_BOOL), true});
        o->props.push_back({"n", mk(K_NULL), false});
        o->props.push_back({"o", inner, false});
        check(o, {}, pt, "object a,n,o(b)");
    }
    // three levels of objects
    {
        N * l3 = mk(K_OBJ); l3->props.push_back({"x", mk(K_NULL), true});
        N * l2 = mk(K_OBJ); l2->props.push_back({"l3", l3, true});
        N * l1 = mk(K_OBJ); l1->props.push_back({"l2", l2, true});
        check(l1, {}, pt, "three levels");
    }
    // root ref to an object
    {
        N * a = mk(K_OBJ); a->props.push_back({"b", mk(K_BOOL), true});
        N * ref = mk(K_REF); ref->refname = "A"; ref->target = a;
        check(ref, {{"A", a}}, pt, "root ref A");
    }
    // two refs to the same object
    {
        N * a = mk(K_OBJ); a->props.push_back({"b", mk(K_BOOL), true});
        N * r1 = mk(K_REF); r1->refname = "A"; r1->target = a;
        N * r2 = mk(K_REF); r2->refname = "A"; r2->target = a;
        N * o = mk(K_OBJ);
        o->props.push_back({"x", r1, true});
        o->props.push_back({"y", r2, false});
        check(o, {{"A", a}}, pt, "two refs to A");
    }
    // ref cycle A -> B -> A
    {
        N * a = mk(K_OBJ);
        N * b = mk(K_OBJ);
        N * ra = mk(K_REF); ra->refname = "A"; ra->target = a;
        N * rb = mk(K_REF); rb->refname = "B"; rb->target = b;
        a->props.push_back({"b", rb, true});
        b->props.push_back({"a", ra, false});
        N * root = mk(K_REF); root->refname = "A"; root->target = a;
        check(root, {{"A", a}, {"B", b}}, pt, "ref cycle A<->B");
    }
    // anyOf of two objects
    {
        N * a = mk(K_OBJ); a->props.push_back({"p", mk(K_BOOL), true});
        N * b = mk(K_OBJ); b->props.push_back({"q", mk(K_NULL), false});
        N * u = mk(K_ANYOF); u->children = {a, b};
        check(u, {}, pt, "anyOf obj | obj");
    }
    // anyOf nested in an object property
    {
        N * a = mk(K_BOOL);
        N * b = mk(K_NULL);
        N * u = mk(K_ANYOF); u->children = {a, b};
        N * o = mk(K_OBJ);
        o->props.push_back({"v", u, true});
        check(o, {}, pt, "object with anyOf property");
    }
    // anyOf of anyOf
    {
        N * a = mk(K_BOOL);
        N * b = mk(K_NULL);
        N * inner = mk(K_ANYOF); inner->children = {a, b};
        N * u = mk(K_ANYOF); u->children = {inner, mk(K_BOOL)};
        check(u, {}, pt, "anyOf of anyOf");
    }
    // ref to an anyOf
    {
        N * u = mk(K_ANYOF);
        u->children = {mk(K_BOOL), mk(K_NULL)};
        N * r = mk(K_REF); r->refname = "U"; r->target = u;
        N * o = mk(K_OBJ);
        o->props.push_back({"u", r, false});
        check(o, {{"U", u}}, pt, "ref to anyOf");
    }
    // root any, primitives
    check(mk(K_ANY), {}, pt, "root any");
    check(mk(K_STR), {}, pt, "root string");
    check(mk(K_BOOL), {}, pt, "root boolean");
    check(mk(K_NUM), {}, pt, "root number");
    check(mk(K_NULL), {}, pt, "root null");
    // object with a number and a string property
    {
        N * o = mk(K_OBJ);
        o->props.push_back({"n", mk(K_NUM), true});
        o->props.push_back({"s", mk(K_STR), false});
        check(o, {}, pt, "object number+string");
    }
    // unresolved ref
    {
        N * r = mk(K_REF); r->refname = "Missing"; r->target = nullptr;
        check(r, {}, pt, "unresolved ref");
    }
    // object property names that need sanitization
    {
        N * o = mk(K_OBJ);
        o->props.push_back({"a.b", mk(K_BOOL), true});
        o->props.push_back({"c d", mk(K_NULL), false});
        o->props.push_back({"a..b", mk(K_STR), false});
        check(o, {}, pt, "weird property names");
    }

    // two required properties (comma chain) plus an optional
    {
        N * o = mk(K_OBJ);
        o->props.push_back({"a", mk(K_BOOL), true});
        o->props.push_back({"b", mk(K_NULL), true});
        o->props.push_back({"c", mk(K_STR), false});
        check(o, {}, pt, "two required + optional");
    }
    // four optional properties: three rest rules, chains of length > 1
    {
        N * o = mk(K_OBJ);
        o->props.push_back({"a", mk(K_BOOL), false});
        o->props.push_back({"b", mk(K_NULL), false});
        o->props.push_back({"c", mk(K_STR), false});
        o->props.push_back({"d", mk(K_NUM), false});
        check(o, {}, pt, "four optionals");
    }
    // reserved property names
    {
        N * o = mk(K_OBJ);
        o->props.push_back({"string", mk(K_BOOL), true});
        o->props.push_back({"root", mk(K_NULL), true});
        check(o, {}, pt, "reserved property names");
    }
    // empty object with additionalProperties: true
    {
        N * o = mk(K_OBJ);
        o->add_any = true;
        check(o, {}, pt, "empty object + additional any");
    }

    for (N * n : all_nodes) delete n;
    all_nodes.clear();
    printf("FG479_VISIT cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
