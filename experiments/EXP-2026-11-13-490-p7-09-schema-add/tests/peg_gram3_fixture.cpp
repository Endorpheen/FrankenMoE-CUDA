// Differential fixture for PEG -> GBNF build_grammar (EXP486).
// Oracle: common_peg_arena::build_grammar with a capturing builder.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <functional>

#include "peggram3-gen.h"
#include "peg-parser.h"
#include "json-schema.h"
#include "json-schema-to-grammar.h"

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
void common_log_add(struct common_log *, enum ggml_log_level, const char *, ...) {}
struct common_log * common_log_main() { return nullptr; }
int common_log_get_verbosity_thold() { return 0; }

static int cases = 0, values = 0, fails = 0;
static void bad(const char * what, const std::string & detail) {
    ++fails;
    if (fails <= 12) printf("FAIL %s: %s\n", what, detail.c_str());
}
static sl_u8 sl(const std::string & s) { return sl_u8{ (uint8_t *) s.data(), (int64_t) s.size() }; }
static sl_u8 slv(std::vector<uint8_t> & v) { return sl_u8{ v.data(), (int64_t) v.size() }; }
static sl_i64 sli(std::vector<int64_t> & v) { return sl_i64{ v.data(), (int64_t) v.size() }; }

enum { K_CHARS = 11, K_STRING = 12, K_SCHEMA = 20 };
struct GNode { int kind; std::vector<int> ch; std::string s; int mn = 0, mx = 0; int add = 0; };
struct CaseBuilder {
    common_peg_parser_builder b;
    common_peg_parser root;
    std::vector<std::string> rule_names;
    CaseBuilder() : b(), root(b.eps()) {}
    common_peg_parser rule(const std::string & n, const common_peg_parser & p) {
        rule_names.push_back(n);
        return b.rule(n, p);
    }
};

static void check(CaseBuilder & cb, const std::string & label) {
    auto arena = cb.b.build();
    const size_t root_id = cb.root.id();

    // native grammar
    std::map<std::string, std::string> nmap;
    common_grammar_builder gb;
    gb.add_rule = [&](const std::string & n, const std::string & r) { nmap[n] = r; return n; };
    gb.add_schema = [&](const std::string & n, const common_chat_schema &) { nmap[n] = "<schema>"; return n; };
    arena.build_grammar(gb, false);

    // encode graph
    std::vector<GNode> g;
    std::map<size_t, int> seen;
    std::function<int(size_t)> enc = [&](size_t id) -> int {
        auto it = seen.find(id);
        if (it != seen.end()) return it->second;
        int idx = (int) g.size();
        seen[id] = idx;
        g.push_back({0, {}, "", 0, 0});
        std::visit([&](auto & p) {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, common_peg_epsilon_parser>) { g[idx].kind = 0; }
            else if constexpr (std::is_same_v<T, common_peg_start_parser>) { g[idx].kind = 1; }
            else if constexpr (std::is_same_v<T, common_peg_end_parser>) { g[idx].kind = 2; }
            else if constexpr (std::is_same_v<T, common_peg_literal_parser>) { g[idx].kind = 3; g[idx].s = p.literal; }
            else if constexpr (std::is_same_v<T, common_peg_sequence_parser>) {
                g[idx].kind = 4;
                for (auto c : p.children) { int ci = enc(c); g[idx].ch.push_back(ci); }
            } else if constexpr (std::is_same_v<T, common_peg_choice_parser>) {
                g[idx].kind = 5;
                for (auto c : p.children) { int ci = enc(c); g[idx].ch.push_back(ci); }
            } else if constexpr (std::is_same_v<T, common_peg_repetition_parser>) {
                g[idx].kind = 6; g[idx].ch = {enc(p.child)}; g[idx].mn = p.min_count; g[idx].mx = p.max_count;
            } else if constexpr (std::is_same_v<T, common_peg_and_parser>) { g[idx].kind = 7; g[idx].ch = {enc(p.child)}; }
            else if constexpr (std::is_same_v<T, common_peg_not_parser>) { g[idx].kind = 8; g[idx].ch = {enc(p.child)}; }
            else if constexpr (std::is_same_v<T, common_peg_any_parser>) { g[idx].kind = 9; }
            else if constexpr (std::is_same_v<T, common_peg_space_parser>) { g[idx].kind = 10; }
            else if constexpr (std::is_same_v<T, common_peg_rule_parser>) { g[idx].kind = 14; g[idx].s = p.name; g[idx].ch = {enc(p.child)}; }
            else if constexpr (std::is_same_v<T, common_peg_ref_parser>) { g[idx].kind = 15; g[idx].s = p.name; }
            else if constexpr (std::is_same_v<T, common_peg_atomic_parser>) { g[idx].kind = 16; g[idx].ch = {enc(p.child)}; }
            else if constexpr (std::is_same_v<T, common_peg_tag_parser>) { g[idx].kind = 17; g[idx].s = p.tag; g[idx].ch = {enc(p.child)}; }
            else if constexpr (std::is_same_v<T, common_peg_chars_parser>) { g[idx].kind = 11; g[idx].s = p.pattern; g[idx].mn = p.min_count; g[idx].mx = p.max_count; }
            else if constexpr (std::is_same_v<T, common_peg_string_parser>) { g[idx].kind = 12; g[idx].s = std::string(1, p.delimiter); }
            else if constexpr (std::is_same_v<T, common_peg_schema_parser>) { g[idx].kind = 20; g[idx].s = p.name; g[idx].ch = {enc(p.child)}; g[idx].add = (p.node && !(p.raw && p.node->may_be_string())) ? 1 : 0; }
            else { g[idx].kind = 99; }
        }, arena.get(id));
        return idx;
    };
    const int groot = enc(root_id);

    std::map<std::string, int> sidx;
    std::vector<std::string> sstr;
    auto sid = [&](const std::string & s) {
        auto it = sidx.find(s);
        if (it != sidx.end()) return it->second;
        int i = (int) sstr.size(); sstr.push_back(s); sidx[s] = i; return i;
    };
    std::vector<int64_t> rname, rid;
    for (const auto & n : cb.rule_names) {
        rname.push_back(sid(n));
        rid.push_back(enc(arena.get_rule(n)));
    }
    std::vector<int64_t> kind, cfirst, ccount, children, sidxv, mn, mx, schema_add;
    for (auto & n : g) {
        kind.push_back(n.kind);
        cfirst.push_back((int64_t) children.size());
        ccount.push_back((int64_t) n.ch.size());
        for (int c : n.ch) children.push_back(c);
        sidxv.push_back(sid(n.s));
        mn.push_back(n.mn);
        mx.push_back(n.mx);
        schema_add.push_back(n.add);
    }
    std::vector<uint8_t> strs(1 << 16, 0);
    std::vector<int64_t> sdir(2 * 1024, 0);
    int64_t off = 0;
    for (size_t i = 0; i < sstr.size(); ++i) {
        sdir[2 * i + 0] = off;
        sdir[2 * i + 1] = (int64_t) sstr[i].size();
        for (unsigned char c : sstr[i]) strs[off++] = c;
    }
    std::vector<uint8_t> out(1 << 16, 0);
    std::vector<int64_t> mname(256, -1), mval(256, 0), mlen(256, 0), st(8, 0), seenv(4096, 0), marks(256, 0);
    ++cases;
    const int64_t n = build_grammar(groot, sli(kind), sli(cfirst), sli(ccount), sli(children), sli(sidxv), sli(mn), sli(mx),
        slv(strs), sli(sdir), sli(rname), sli(rid), (int64_t) rname.size(), sli(schema_add), slv(out), sli(mname), sli(mval), sli(mlen), sli(st), sli(seenv), sli(marks));
    std::map<std::string, std::string> got;
    for (int64_t i = 0; i < n; ++i) {
        std::string name = mname[i] < 0 ? std::string("root") : sstr[mname[i]];
        got[name] = std::string((const char *) out.data() + mval[i], (size_t) mlen[i]);
    }
    ++values;
    if (got != nmap) {
        std::string diff;
        for (auto & [k, v] : nmap) {
            auto it = got.find(k);
            if (it == got.end()) diff += " missing " + k;
            else if (it->second != v) diff += " text " + k + " got=[" + it->second + "] want=[" + v + "]";
        }
        for (auto & [k, v] : got) if (!nmap.count(k)) diff += " extra " + k;
        bad("grammar", label + ":" + diff);
    }
}

int main(void) {
    goose_init(0, 0);

    { CaseBuilder cb; auto s = cb.b.sequence(); s += cb.b.literal("a"); s += cb.b.literal("b"); cb.root = s; cb.b.set_root(s); check(cb, "seq a b"); }
    { CaseBuilder cb; auto s = cb.b.sequence(); s += cb.b.literal("a\"b\\c\n"); s += cb.b.literal("b"); cb.root = s; cb.b.set_root(s); check(cb, "literal escapes"); }
    { CaseBuilder cb; auto c = cb.b.choice(); c |= cb.b.literal("a"); c |= cb.b.literal("b"); cb.root = c; cb.b.set_root(c); check(cb, "choice a b"); }
    { CaseBuilder cb; auto r = cb.b.repeat(cb.b.literal("a"), 0, -1); cb.root = r; cb.b.set_root(r); check(cb, "a*"); }
    { CaseBuilder cb; auto r = cb.b.repeat(cb.b.literal("a"), 1, -1); cb.root = r; cb.b.set_root(r); check(cb, "a+"); }
    { CaseBuilder cb; auto r = cb.b.repeat(cb.b.literal("a"), 0, 1); cb.root = r; cb.b.set_root(r); check(cb, "a?"); }
    { CaseBuilder cb; auto r = cb.b.repeat(cb.b.literal("a"), 2, 2); cb.root = r; cb.b.set_root(r); check(cb, "a{2}"); }
    { CaseBuilder cb; auto r = cb.b.repeat(cb.b.literal("a"), 2, 4); cb.root = r; cb.b.set_root(r); check(cb, "a{2,4}"); }
    { CaseBuilder cb; auto r = cb.b.repeat(cb.b.literal("a"), 2, -1); cb.root = r; cb.b.set_root(r); check(cb, "a{2,}"); }
    { CaseBuilder cb; auto c = cb.b.choice(); c |= cb.b.literal("a"); c |= cb.b.literal("b");
      auto s = cb.b.sequence(); s += c; s += cb.b.literal("c"); cb.root = s; cb.b.set_root(s); check(cb, "seq (a|b) c"); }
    { CaseBuilder cb; auto s = cb.b.sequence(); s += cb.b.literal("a"); s += cb.b.literal("b");
      auto c = cb.b.choice(); c |= s; c |= cb.b.literal("c"); cb.root = c; cb.b.set_root(c); check(cb, "choice (a b)|c"); }
    { CaseBuilder cb; auto s = cb.b.sequence(); s += cb.b.tag("t", cb.b.literal("a")); s += cb.b.atomic(cb.b.literal("b"));
      cb.root = s; cb.b.set_root(s); check(cb, "tag atomic"); }
    { CaseBuilder cb; auto s = cb.b.sequence(); s += cb.b.space(); s += cb.b.any(); cb.root = s; cb.b.set_root(s); check(cb, "space any"); }
    { CaseBuilder cb; auto s = cb.b.sequence(); s += cb.b.eps(); s += cb.b.literal("a"); cb.root = s; cb.b.set_root(s); check(cb, "seq eps a"); }
    { CaseBuilder cb; auto r = cb.rule("main", cb.b.literal("a")); cb.root = r; cb.b.set_root(r); check(cb, "rule root"); }
    { CaseBuilder cb; auto r2 = cb.rule("b", cb.b.literal("x"));
      auto s = cb.b.sequence(); s += cb.b.literal("a"); s += r2;
      auto r1 = cb.rule("a", s); cb.root = r1; cb.b.set_root(r1); check(cb, "two rules"); }
    { CaseBuilder cb; auto c = cb.b.choice(); c |= cb.b.literal("a"); c |= cb.b.literal("b");
      auto rep = cb.b.repeat(c, 0, -1); cb.root = rep; cb.b.set_root(rep); check(cb, "rep choice"); }
    { CaseBuilder cb; auto s = cb.b.sequence(); s += cb.b.literal("a"); s += cb.b.literal("b");
      auto c = cb.b.choice(); c |= s; c |= cb.b.eps(); cb.root = c; cb.b.set_root(c); check(cb, "choice with eps"); }

    { CaseBuilder cb; auto inner = cb.b.choice(); inner |= cb.b.literal("a"); inner |= cb.b.literal("b");
      auto c = cb.b.choice(); c |= inner; c |= cb.b.literal("c"); cb.root = c; cb.b.set_root(c); check(cb, "choice of choice"); }
    { CaseBuilder cb; auto inner = cb.b.choice(); inner |= cb.b.literal("a"); inner |= cb.b.literal("b");
      auto s = cb.b.sequence(); s += cb.b.tag("t", inner); s += cb.b.literal("c"); cb.root = s; cb.b.set_root(s); check(cb, "seq tag(choice)"); }
    { CaseBuilder cb; auto inner = cb.b.choice(); inner |= cb.b.literal("a"); inner |= cb.b.literal("b");
      auto r = cb.b.repeat(cb.b.atomic(inner), 0, -1); cb.root = r; cb.b.set_root(r); check(cb, "rep atomic(choice)"); }

    { CaseBuilder cb; auto r = cb.b.repeat(cb.b.chars("[a-z]", 0, -1), 0, -1); cb.root = r; cb.b.set_root(r); check(cb, "chars star"); }
    { CaseBuilder cb; auto c = cb.b.chars("[a-z]", 2, 3); cb.root = c; cb.b.set_root(c); check(cb, "chars {2,3}"); }
    { CaseBuilder cb; auto c = cb.b.chars("[^a]", 1, -1); cb.root = c; cb.b.set_root(c); check(cb, "chars negated"); }
    { CaseBuilder cb; auto s = cb.b.string_content('"'); cb.root = s; cb.b.set_root(s); check(cb, "string quote"); }
    { CaseBuilder cb; auto s = cb.b.string_content('\''); cb.root = s; cb.b.set_root(s); check(cb, "string squote"); }
    { CaseBuilder cb; auto seq = cb.b.sequence(); seq += cb.b.string_content('"'); seq += cb.b.chars("[a-z]", 0, -1); cb.root = seq; cb.b.set_root(seq); check(cb, "string then chars"); }
    { CaseBuilder cb; auto sch = cb.b.schema(cb.b.literal("a"), "s", common_json::parse("{\"type\":\"string\"}"), true); cb.root = sch; cb.b.set_root(sch); check(cb, "schema delegate"); }
    { CaseBuilder cb; auto seq = cb.b.sequence(); seq += cb.b.schema(cb.b.literal("a"), "s", common_json::parse("{\"type\":\"string\"}"), true); seq += cb.b.literal("b"); cb.root = seq; cb.b.set_root(seq); check(cb, "schema then lit"); }
    { CaseBuilder cb; auto sch = cb.b.schema(cb.b.literal("a"), "s", common_json::parse("{\"type\":\"string\"}"), false); cb.root = sch; cb.b.set_root(sch); check(cb, "schema add"); }
    { CaseBuilder cb; auto seq = cb.b.sequence(); seq += cb.b.schema(cb.b.literal("a"), "s", common_json::parse("{\"type\":\"string\"}"), false); seq += cb.b.literal("b"); cb.root = seq; cb.b.set_root(seq); check(cb, "schema add then lit"); }

    printf("FG490_PEGSCHEMA cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
