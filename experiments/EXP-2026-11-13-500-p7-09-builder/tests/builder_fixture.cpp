// Differential fixture for the PEG builder decisions (EXP500).
// Oracle: common_peg_parser_builder's rule_name, sequence/choice flattening and
// the rule() overloads, observed through the built arena.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <variant>

#include "builder-gen.h"
#include "peg-parser.h"

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
static sl_u8 slv(std::vector<uint8_t> & v) { return sl_u8{ v.data(), (int64_t) v.size() }; }
static sl_i64 sli(std::vector<int64_t> & v) { return sl_i64{ v.data(), (int64_t) v.size() }; }

static std::string visible(const std::string & s) {
    std::string r;
    for (unsigned char c : s) {
        if (c >= 32 && c < 127) r.push_back((char) c);
        else { char b[8]; snprintf(b, sizeof b, "\\x%02X", c); r += b; }
    }
    return r;
}

// the cleaned name of the rule parser that the registry points at
static std::string clean_name_of(const common_peg_arena & a, size_t rule_id) {
    const auto & v = a.get(rule_id);
    if (auto p = std::get_if<common_peg_rule_parser>(&v)) return p->name;
    return "<not a rule>";
}

static void check_rule_name(const std::string & raw) {
    common_peg_parser_builder b;
    auto p = b.rule(raw, b.literal("x"));
    auto arena = b.build();
    (void) p;

    std::vector<uint8_t> out(256, 0);
    const int64_t n = rule_name(sl_u8{ (uint8_t *) raw.data(), (int64_t) raw.size() }, slv(out));
    const std::string got((const char *) out.data(), (size_t) n);

    // the native clean name: the last rule parser carries it
    std::string want;
    for (size_t i = 0; i < arena.size(); ++i) {
        if (auto rp = std::get_if<common_peg_rule_parser>(&arena.get(i))) want = rp->name;
    }
    ++cases;
    ++values;
    if (got != want) bad("rule_name", "raw=[" + visible(raw) + "] got=[" + got + "] want=[" + want + "]");

    // the registry resolves the clean name to a rule parser with that name
    ++values;
    if (!arena.has_rule(got)) { bad("registered", "raw=[" + visible(raw) + "] clean=[" + got + "]"); return; }
    ++values;
    const std::string reg = clean_name_of(arena, arena.get_rule(got));
    if (reg != got) bad("registry name", "clean=[" + got + "] registry=[" + reg + "]");
    (void) want;
}

static void check_flatten(int want_kind, const std::string & label) {
    // build: inner = sequence/choice(A, B); outer = sequence/choice(inner, C)
    common_peg_parser_builder b;
    auto A = b.literal("a");
    auto B = b.literal("b");
    auto C = b.literal("c");
    common_peg_parser inner = (want_kind == 0) ? b.sequence({A, B}) : b.choice({A, B});
    common_peg_parser outer = (want_kind == 0) ? b.sequence({inner, C}) : b.choice({inner, C});
    b.set_root(outer);
    auto arena = b.build();

    // encode the arena so the port can see the inner parser's children
    const size_t n = arena.size();
    std::vector<int64_t> kind(n, -1), cf(n, 0), cc(n, 0), children;
    for (size_t i = 0; i < n; ++i) {
        const auto & v = arena.get(i);
        if (auto s = std::get_if<common_peg_sequence_parser>(&v)) {
            kind[i] = 0; cf[i] = (int64_t) children.size();
            for (auto c : s->children) children.push_back((int64_t) c);
            cc[i] = (int64_t) s->children.size();
        } else if (auto c = std::get_if<common_peg_choice_parser>(&v)) {
            kind[i] = 1; cf[i] = (int64_t) children.size();
            for (auto ch : c->children) children.push_back((int64_t) ch);
            cc[i] = (int64_t) c->children.size();
        }
    }

    // the outer parser's native children
    std::vector<size_t> native;
    if (want_kind == 0) native = std::get<common_peg_sequence_parser>(arena.get(outer.id())).children;
    else native = std::get<common_peg_choice_parser>(arena.get(outer.id())).children;

    // the port: flatten the inputs the builder saw
    std::vector<int64_t> ids = { (int64_t) inner.id(), (int64_t) C.id() };
    std::vector<int64_t> out(64, -1);
    ++cases;
    const int64_t cnt = flatten(sli(ids), 2, (int64_t) want_kind, sli(kind), sli(cf), sli(cc), sli(children), sli(out));
    ++values;
    if (cnt != (int64_t) native.size()) {
        bad("flatten count", label + " goose=" + std::to_string(cnt) + " native=" + std::to_string(native.size()));
        return;
    }
    for (int64_t i = 0; i < cnt; ++i) {
        ++values;
        if (out[(size_t) i] != (int64_t) native[(size_t) i]) {
            bad("flatten", label + " index " + std::to_string(i) +
                " goose=" + std::to_string(out[(size_t) i]) + " native=" + std::to_string(native[(size_t) i]));
        }
    }
}

static void check_rule_plan(const std::string & name, int mode, const std::string & label) {
    common_peg_parser_builder b;
    size_t returned = 0;
    if (mode == 0) {
        auto p = b.rule(name, b.literal("x"));
        returned = p.id();
    } else {
        auto p = b.rule(name, [&]() { return b.sequence({b.literal("x"), b.literal("y")}); });
        returned = p.id();
    }
    auto arena = b.build();

    std::vector<uint8_t> clean(256, 0);
    const int64_t cn = rule_name(sl_u8{ (uint8_t *) name.data(), (int64_t) name.size() }, slv(clean));
    const std::string cname((const char *) clean.data(), (size_t) cn);

    int64_t rules = 0;
    size_t last_rule = SIZE_MAX;
    for (size_t i = 0; i < arena.size(); ++i) {
        if (auto rp = std::get_if<common_peg_rule_parser>(&arena.get(i))) {
            if (rp->name == cname) { rules++; last_rule = i; }
        }
    }
    const int64_t registrations = (rules > 0) ? 1 : 0;

    std::vector<int64_t> out(4, -1);
    ++cases;
    rule_plan(0, (int64_t) mode, sli(out));
    ++values;
    if (out[0] != rules) bad("rule parsers", label + " goose=" + std::to_string(out[0]) + " native=" + std::to_string(rules));
    ++values;
    if (out[1] != registrations) bad("registrations", label + " goose=" + std::to_string(out[1]) + " native=" + std::to_string(registrations));
    ++values;
    if (!arena.has_rule(cname)) { bad("registry missing", label + " clean=[" + cname + "]"); return; }
    if (arena.get_rule(cname) != last_rule) bad("registry target", label + " registry is not the last rule parser");
    ++values;
    const int64_t is_ref = std::holds_alternative<common_peg_ref_parser>(arena.get(returned)) ? 1 : 0;
    if (is_ref != out[2]) bad("returned kind", label + " goose=" + std::to_string(out[2]) + " native=" + std::to_string(is_ref));
}

int main(void) {
    goose_init(0, 0);

    const std::vector<std::string> names = {
        "simple", "with space", "a.b", "a/b/c", "root-1", "UPPER", "9lives",
        "caf\xc3\xa9", "a__b", "  ", "-already-", "x!y?z", "a\tb",
    };
    for (const auto & n : names) check_rule_name(n);

    check_flatten(0, "sequence");
    check_flatten(1, "choice");

    check_rule_plan("plain", 0, "rule(name, parser)");
    check_rule_plan("with space", 0, "rule(name, parser) cleaned");
    check_rule_plan("plain", 1, "rule(name, fn)");
    check_rule_plan("with space", 1, "rule(name, fn) cleaned");

    // the second overload returns an existing rule's ref without adding a parser
    {
        common_peg_parser_builder b;
        auto first = b.rule("dup", b.literal("x"));
        const size_t before = 0;
        (void) first;
        auto again = b.rule("dup", [&]() { return b.literal("y"); });
        (void) again;
        auto arena = b.build();
        int64_t rules = 0;
        for (size_t i = 0; i < arena.size(); ++i) {
            if (auto rp = std::get_if<common_peg_rule_parser>(&arena.get(i))) {
                if (rp->name == "dup") rules++;
            }
        }
        std::vector<int64_t> out(4, -1);
        ++cases;
        rule_plan(1, 1, sli(out));
        ++values;
        if (rules != out[0] + 1) {
            bad("existing rule", "native rules=" + std::to_string(rules) + " expected first + " + std::to_string(out[0]));
        }
        (void) before;
    }

    printf("FG500_BUILDER cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
