// Differential fixture for _visit_all_of (EXP478).
// Oracle: the real converter, twice. First the allOf schema's root rule; then a
// flat object/enum schema built from the Goose merge result. If the merge is
// right, both native bodies are identical.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

#include "allof-gen.h"
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

enum PKind { P_STRING, P_BOOL, P_NUMBER, P_NULL, P_ANY };
static std::unique_ptr<common_chat_schema> prim(PKind k) {
    switch (k) {
        case P_STRING: return std::make_unique<common_chat_schema_string>();
        case P_BOOL: return std::make_unique<common_chat_schema_boolean>();
        case P_NUMBER: return std::make_unique<common_chat_schema_number>();
        case P_NULL: return std::make_unique<common_chat_schema_null>();
        default: return std::make_unique<common_chat_schema_any>();
    }
}

struct Prop { std::string name; PKind kind; };
struct Comp { int kind; std::vector<int> items; };  // 0 obj, 1 enum, 2 anyOf, 3 ref
struct Case { std::vector<Comp> comps; std::vector<int> top; std::vector<Prop> props; std::vector<std::string> vals; };

static void check(const Case & c, const std::string & label) {
    // native allOf
    std::vector<std::unique_ptr<common_chat_schema>> keep;
    std::function<std::unique_ptr<common_chat_schema>(int)> build = [&](int ci) -> std::unique_ptr<common_chat_schema> {
        const Comp & comp = c.comps[ci];
        if (comp.kind == 0) {
            auto o = std::make_unique<common_chat_schema_object>();
            for (int pi : comp.items) {
                common_chat_schema_property p;
                p.name = c.props[pi].name;
                p.schema = prim(c.props[pi].kind);
                p.required = false;
                o->properties.push_back(std::move(p));
            }
            return o;
        }
        if (comp.kind == 1) {
            auto e = std::make_unique<common_chat_schema_enum>();
            for (int vi : comp.items) e->values.push_back(common_json::parse(c.vals[vi]));
            return e;
        }
        if (comp.kind == 2) {
            auto a = std::make_unique<common_chat_schema_any_of>();
            for (int ch : comp.items) a->children.push_back(build(ch));
            return a;
        }
        auto t = build(comp.items[0]);
        auto r = std::make_unique<common_chat_schema_ref>("#/t");
        r->target = t.get();
        keep.push_back(std::move(t));
        return r;
    };
    std::map<std::string, std::string> nr;
    try {
        auto root = std::make_unique<common_chat_schema_all_of>();
        for (int ci : c.top) root->children.push_back(build(ci));
        nr = parse_rules(build_grammar([&](const common_grammar_builder & b) {
            b.add_schema("root", *root);
        }, { false }));
    } catch (const std::exception & e) {
        bad("native throw", label + ": " + e.what());
        return;
    }

    // flatten
    std::vector<int64_t> ckind, cfirst, ccount, citems;
    for (const auto & comp : c.comps) {
        ckind.push_back(comp.kind);
        cfirst.push_back((int64_t) citems.size());
        ccount.push_back((int64_t) comp.items.size());
        for (int it : comp.items) citems.push_back(it);
    }
    std::vector<uint8_t> pnames(1 << 16, 0);
    std::vector<int64_t> pdir(2 * 64, 0), pkind(64, 0);
    int64_t off = 0;
    for (size_t i = 0; i < c.props.size(); ++i) {
        pdir[2 * i + 0] = off;
        pdir[2 * i + 1] = (int64_t) c.props[i].name.size();
        for (unsigned char ch : c.props[i].name) pnames[off++] = ch;
        pkind[i] = c.props[i].kind;
    }
    std::vector<uint8_t> vdumps(1 << 16, 0);
    std::vector<int64_t> vdir(2 * 64, 0);
    off = 0;
    for (size_t i = 0; i < c.vals.size(); ++i) {
        vdir[2 * i + 0] = off;
        vdir[2 * i + 1] = (int64_t) c.vals[i].size();
        for (unsigned char ch : c.vals[i]) vdumps[off++] = ch;
    }
    std::vector<int64_t> out_props(256, -1), out_req(256, 0), out_vals(256, -1), tinfo(4 * 256, 0), st(8, 0);
    std::vector<uint8_t> abuf(1 << 18, 0);

    ++cases;
    std::vector<int64_t> top(c.top.begin(), c.top.end());
    const int64_t kind = allof_merge(sli(ckind), sli(cfirst), sli(ccount), sli(citems),
        sli(top), (int64_t) top.size(),
        slv(pnames), sli(pdir), slv(vdumps), sli(vdir),
        sli(out_props), sli(out_req), sli(out_vals), sli(tinfo), slv(abuf), sli(st));

    std::map<std::string, std::string> fr;
    try {
        if (kind == 0) {
            auto e = std::make_unique<common_chat_schema_enum>();
            for (int64_t i = 0; i < st[1]; ++i) e->values.push_back(common_json::parse(c.vals[out_vals[i]]));
            fr = parse_rules(build_grammar([&](const common_grammar_builder & b) {
                b.add_schema("root", *e);
            }, { false }));
        } else {
            auto o = std::make_unique<common_chat_schema_object>();
            for (int64_t i = 0; i < st[0]; ++i) {
                const int pi = (int) out_props[i];
                common_chat_schema_property p;
                p.name = c.props[pi].name;
                p.schema = prim(c.props[pi].kind);
                p.required = out_req[i] != 0;
                o->properties.push_back(std::move(p));
            }
            fr = parse_rules(build_grammar([&](const common_grammar_builder & b) {
                b.add_schema("root", *o);
            }, { false }));
        }
    } catch (const std::exception & e) {
        bad("flat native throw", label + ": " + e.what());
        return;
    }
    // The flat native object applies the required set itself, which masks the
    // merge's own promotion; check the emitted flags directly for that case.
    if (label == "required set promotes every a") {
        ++values;
        if (st[0] != 3 || out_req[0] != 1 || out_req[1] != 1 || out_req[2] != 0)
            bad("required set", label + " flags are wrong");
    }
    eq("allOf body", fr.at("root"), nr.at("root"), label);
}

int main(void) {
    goose_init(0, 0);

    // case 1: two objects, duplicate name, all top-level props required
    {
        Case c;
        c.props = {{"a", P_STRING}, {"b", P_BOOL}};
        c.comps.push_back({0, {0}});
        c.comps.push_back({0, {0, 1}});
        c.top = {0, 1};
        check(c, "obj a + obj a,b");
    }
    // case 2: three objects, order permutation
    {
        Case c;
        c.props = {{"a", P_STRING}, {"b", P_BOOL}, {"c", P_NUMBER}};
        c.comps.push_back({0, {0, 1}});
        c.comps.push_back({0, {2, 0}});
        c.comps.push_back({0, {1}});
        c.top = {0, 1, 2};
        check(c, "obj a,b + obj c,a + obj b");
    }
    // case 3: empty
    {
        Case c;
        check(c, "no children");
    }
    // case 4: empty object child
    {
        Case c;
        c.comps.push_back({0, {}});
        c.top = {0};
        check(c, "empty object");
    }
    // case 5: enum intersection
    {
        Case c;
        c.vals = {"\"a\"", "\"b\"", "\"c\""};
        c.comps.push_back({1, {0, 1}});
        c.comps.push_back({1, {0, 2}});
        c.top = {0, 1};
        check(c, "enum a,b + a,c");
    }
    // case 6: intersection sorted by rendered text
    {
        Case c;
        c.vals = {"\"b\"", "\"a\""};
        c.comps.push_back({1, {0, 1}});
        c.comps.push_back({1, {1, 0}});
        c.top = {0, 1};
        check(c, "enum b,a + a,b");
    }
    // case 7: disjoint enums -> empty object
    {
        Case c;
        c.vals = {"\"a\"", "\"c\""};
        c.comps.push_back({1, {0}});
        c.comps.push_back({1, {1}});
        c.top = {0, 1};
        check(c, "enum a + c");
    }
    // case 8: duplicate value inflates the count
    {
        Case c;
        c.vals = {"\"a\""};
        c.comps.push_back({1, {0, 0}});
        c.comps.push_back({1, {0}});
        c.top = {0, 1};
        check(c, "enum a,a + a");
    }
    // case 9: object + enum, no intersection
    {
        Case c;
        c.props = {{"x", P_STRING}};
        c.vals = {"\"x\""};
        c.comps.push_back({0, {0}});
        c.comps.push_back({1, {0}});
        c.top = {0, 1};
        check(c, "obj x + enum x");
    }
    // case 10: anyOf alternatives are optional
    {
        Case c;
        c.props = {{"a", P_STRING}, {"b", P_BOOL}};
        c.comps.push_back({0, {0}});
        c.comps.push_back({0, {1}});
        c.comps.push_back({2, {0, 1}});
        c.top = {2};
        check(c, "anyOf obj a | obj b");
    }
    // case 11: anyOf of two enums, count 2 vs 1 child
    {
        Case c;
        c.vals = {"\"a\""};
        c.comps.push_back({1, {0}});
        c.comps.push_back({1, {0}});
        c.comps.push_back({2, {0, 1}});
        c.top = {2};
        check(c, "anyOf enum a | enum a");
    }
    // case 12: anyOf object + enum -> enum wins
    {
        Case c;
        c.props = {{"a", P_STRING}};
        c.vals = {"\"a\""};
        c.comps.push_back({0, {0}});
        c.comps.push_back({1, {0}});
        c.comps.push_back({2, {0, 1}});
        c.top = {2};
        check(c, "anyOf obj a | enum a");
    }
    // case 13: ref to an object
    {
        Case c;
        c.props = {{"a", P_STRING}, {"b", P_BOOL}};
        c.comps.push_back({0, {0, 1}});
        c.comps.push_back({3, {0}});
        c.top = {1};
        check(c, "ref -> obj a,b");
    }
    // case 14: ref to an enum plus a sibling enum
    {
        Case c;
        c.vals = {"\"a\"", "\"b\""};
        c.comps.push_back({1, {0, 1}});
        c.comps.push_back({1, {0}});
        c.comps.push_back({3, {0}});
        c.top = {2, 1};
        check(c, "ref -> enum a,b + enum a");
    }
    // case 15: object plus ref to an object
    {
        Case c;
        c.props = {{"p", P_STRING}, {"q", P_NUMBER}};
        c.comps.push_back({0, {0}});
        c.comps.push_back({0, {1}});
        c.comps.push_back({3, {1}});
        c.top = {0, 2};
        check(c, "obj p + ref -> obj q");
    }
    // case 16: ref to anyOf is ignored
    {
        Case c;
        c.props = {{"a", P_STRING}};
        c.comps.push_back({0, {0}});
        c.comps.push_back({2, {0}});
        c.comps.push_back({3, {1}});
        c.top = {2};
        check(c, "ref -> anyOf obj a");
    }
    // case 17: enum + anyOf of enums
    {
        Case c;
        c.vals = {"\"x\"", "\"y\""};
        c.comps.push_back({1, {0}});
        c.comps.push_back({1, {0}});
        c.comps.push_back({1, {1}});
        c.comps.push_back({2, {1, 2}});
        c.top = {0, 3};
        check(c, "enum x + anyOf enum x | enum y");
    }
    // case 18: escaped and unicode enum values
    {
        Case c;
        c.vals = {"\"a\\\\\\\"b\"", "\"caf\\u00e9\""};
        c.comps.push_back({1, {0, 1}});
        c.comps.push_back({1, {0}});
        c.top = {0, 1};
        check(c, "escaped enum intersection");
    }
    // case 19: numeric enum values sort as text
    {
        Case c;
        c.vals = {"1", "2", "3", "4"};
        c.comps.push_back({1, {0, 1, 2}});
        c.comps.push_back({1, {1, 2, 3}});
        c.top = {0, 1};
        check(c, "numeric enum intersection");
    }
    // case 20: many duplicates of one name
    {
        Case c;
        c.props = {{"a", P_STRING}};
        c.comps.push_back({0, {0}});
        c.comps.push_back({0, {0}});
        c.comps.push_back({0, {0}});
        c.top = {0, 1, 2};
        check(c, "obj a x3");
    }
    // case 21: anyOf optional props plus a required object
    {
        Case c;
        c.props = {{"a", P_STRING}, {"b", P_BOOL}, {"c", P_NULL}};
        c.comps.push_back({0, {0}});
        c.comps.push_back({0, {1}});
        c.comps.push_back({2, {0, 1}});
        c.comps.push_back({0, {2}});
        c.top = {2, 3};
        check(c, "anyOf a|b + obj c");
    }
    // case 22: empty property name
    {
        Case c;
        c.props = {{"", P_STRING}};
        c.comps.push_back({0, {0}});
        c.top = {0};
        check(c, "empty property name");
    }
    // case 23: boolean enum intersection
    {
        Case c;
        c.vals = {"true", "false"};
        c.comps.push_back({1, {0, 1}});
        c.comps.push_back({1, {0}});
        c.top = {0, 1};
        check(c, "bool enum intersection");
    }

    // case 24: the same name once required (object) and once optional (anyOf)
    {
        Case c;
        c.props = {{"a", P_STRING}, {"b", P_BOOL}};
        c.comps.push_back({0, {0}});
        c.comps.push_back({0, {0}});
        c.comps.push_back({0, {1}});
        c.comps.push_back({2, {1, 2}});
        c.top = {0, 3};
        check(c, "required set promotes every a");
    }

    printf("FG478_ALLOF cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
