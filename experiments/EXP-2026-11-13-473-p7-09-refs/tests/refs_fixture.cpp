// Differential fixture for the $ref / rule-name decisions (EXP473).
// Right arm: transcriptions of _resolve_ref, _add_rule, visit, is_reserved_name
// and INVALID_RULE_CHARS_RE from common/json-schema-to-grammar.cpp.
// Left arm: the Goose module in source/refs.goose.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <unordered_set>
#include <random>
#include <regex>

#include "refs-gen.h"

static int cases = 0, values = 0, fails = 0;
static void bad(const char * what, const std::string & detail) {
    ++fails;
    if (fails <= 12) printf("FAIL %s: %s\n", what, detail.c_str());
}
static void eqstr(const char * what, const std::string & got, const std::string & want) {
    ++values;
    if (got != want) bad(what, "got=[" + got + "] want=[" + want + "]");
}

// ---- native transcriptions -------------------------------------------------

static std::string esc(const std::string & name) {
    static const std::regex re("[^a-zA-Z0-9-]+");
    return std::regex_replace(name, re, "-");
}

static std::string ref_fragment(const std::string & ref) {
    const auto it = ref.find('#');
    return it != std::string::npos ? ref.substr(it + 1) : ref;
}

static std::string native_ref_name(const std::string & ref) {
    return "ref" + esc(ref_fragment(ref));
}

static const std::unordered_set<std::string> & reserved() {
    static const std::unordered_set<std::string> s = {
        "root", "boolean", "decimal-part", "integral-part", "number", "integer",
        "value", "object", "array", "uuid", "char", "string", "null",
        "date", "time", "date-time", "date-string", "time-string",
        "date-time-string",
    };
    return s;
}

static std::string native_rule_name(const std::string & name) {
    return reserved().count(name) ? name + "-" : name.empty() ? "root" : name;
}

static std::string native_sub_name(const std::string & name) {
    return name + (name.empty() ? "" : "-");
}

// rules map key -> rule text; the text is an interned "#id" string
static std::string native_add_rule(std::map<std::string, std::string> & rules,
                                   const std::string & name, const std::string & rule) {
    const std::string esc_name = esc(name);
    if (rules.find(esc_name) == rules.end() || rules[esc_name] == rule) {
        rules[esc_name] = rule;
        return esc_name;
    }
    int i = 0;
    while (rules.find(esc_name + std::to_string(i)) != rules.end() &&
           rules[esc_name + std::to_string(i)] != rule) {
        i++;
    }
    const std::string key = esc_name + std::to_string(i);
    rules[key] = rule;
    return key;
}

// ---- goose helpers ---------------------------------------------------------

static sl_u8 sl(const std::string & s) {
    return sl_u8{ (uint8_t *) s.data(), (int64_t) s.size() };
}
static std::string from_out(const std::vector<uint8_t> & out, int64_t n) {
    return std::string((const char *) out.data(), (size_t) n);
}

int main(void) {
    goose_init(0, 0);

    // name corpus: every reserved name, near misses, every single byte, and
    // mixed strings with invalid runs
    std::vector<std::string> names;
    for (const auto & r : reserved()) names.push_back(r);
    for (const char * extra : {
            "", "root!", "root ", "Root", "boolean2", "decimal_part", "a",
            "A", "-", "--", "---", "-a-", "/definitions/Foo", "#/$defs/a b",
            "With Space", "tab\there", "dot.dot", "colon:colon", "under_score",
            "caf\xc3\xa9", "\xe9\xe9\xe9", "a\x7f" "b", "a\x00" "b", "9lives",
            "a.b-c_d/e", "x" "y" "z", "##", "  ", "\t\n", "a-", "-a", "0-",
        }) {
        names.push_back(extra);
    }
    for (int b = 0; b < 256; ++b) names.push_back(std::string(1, (char) (unsigned char) b));

    // 1. sanitize
    for (const auto & n : names) {
        std::vector<uint8_t> out(n.size() * 2 + 8, 0xAA);
        const int64_t got = sanitize(sl(n), sl_u8{ out.data(), (int64_t) out.size() });
        ++cases;
        eqstr("sanitize", from_out(out, got), esc(n));
    }

    // 2. ref fragment and ref name
    std::vector<std::string> refs = {
        "", "#", "#/definitions/Foo", "#/definitions/a b", "no-hash",
        "a#b#c", "#a#b", "#/x_y", "#/$defs/Name", "x#", "#-#", "#. .",
        "/definitions/Foo", "#/definitions/caf\xc3\xa9",
    };
    for (const auto & n : names) refs.push_back(n);
    std::mt19937 rng(473001);
    for (int i = 0; i < 400; ++i) {
        std::string s;
        const int len = (int) (rng() % 14);
        for (int k = 0; k < len; ++k) s.push_back("aZ09-_./:# \xc3\xa9"[rng() % 13]);
        refs.push_back(s);
    }
    for (const auto & r : refs) {
        std::vector<uint8_t> out(r.size() * 2 + 8, 0xAA);
        const int64_t got = ref_name(sl(r), sl_u8{ out.data(), (int64_t) out.size() });
        ++cases;
        eqstr("ref_name", from_out(out, got), native_ref_name(r));
        // ref_cut is the fragment offset
        const auto it = r.find('#');
        const int64_t want_cut = it != std::string::npos ? (int64_t) it + 1 : 0;
        ++values;
        if (ref_cut(sl(r)) != want_cut) bad("ref_cut", r);
    }

    // 3. rule_name and sub_name
    for (const auto & n : names) {
        std::vector<uint8_t> o1(n.size() * 2 + 8, 0xAA), o2(n.size() * 2 + 8, 0xAA);
        const int64_t l1 = rule_name(sl(n), sl_u8{ o1.data(), (int64_t) o1.size() });
        const int64_t l2 = sub_name(sl(n), sl_u8{ o2.data(), (int64_t) o2.size() });
        ++cases;
        eqstr("rule_name", from_out(o1, l1), native_rule_name(n));
        ++values;
        if (from_out(o2, l2) != native_sub_name(n)) bad("sub_name", n);
    }

    // 4. add_rule over insertion sequences
    {
        std::map<std::string, std::string> native;
        std::vector<std::string> keys;
        std::vector<int64_t> ids;
        std::map<std::string, int64_t> pos;

        auto run_step = [&](const std::string & name, int64_t rule_id) {
            std::string buf;
            std::vector<int64_t> offs;
            for (const auto & k : keys) {
                offs.push_back((int64_t) buf.size());
                buf += k;
            }
            std::vector<uint8_t> out(name.size() * 2 + 32, 0xAA);
            const int64_t got = add_rule(sl(name), rule_id,
                                         sl(buf),
                                         sl_i64{ offs.data(), (int64_t) offs.size() },
                                         sl_i64{ ids.data(), (int64_t) ids.size() },
                                         sl_u8{ out.data(), (int64_t) out.size() });
            const std::string got_key = from_out(out, got);
            const std::string want_key = native_add_rule(native, name, "#" + std::to_string(rule_id));
            ++cases;
            eqstr("add_rule", got_key, want_key);
            if (pos.find(got_key) == pos.end()) {
                pos[got_key] = (int64_t) keys.size();
                keys.push_back(got_key);
                ids.push_back(rule_id);
            }
        };

        // deterministic collision ladder
        run_step("a.b", 0);
        run_step("a_b", 1);
        run_step("a-b", 0);
        run_step("a-b", 1);
        run_step("a b", 0);
        run_step("a-b0", 2);
        run_step("a-b", 2);
        run_step("a.b", 0);
        run_step("", 0);
        run_step("root", 0);
        run_step("root", 1);
        run_step("root-", 1);

        // random insertions over a pool that collides heavily after sanitizing
        const char * pool[] = {"a.b", "a_b", "a b", "a-b", "x", "x-", "x0",
                               "root", "string", "caf\xc3\xa9", "cafe", "a/b", "a:b"};
        for (int i = 0; i < 900; ++i) {
            const std::string name = pool[rng() % 13];
            const int64_t rule_id = (int64_t) (rng() % 4);
            run_step(name, rule_id);
        }
    }

    // 5. resolve decision
    for (int re = 0; re < 2; ++re)
    for (int rs = 0; rs < 2; ++rs)
    for (int ht = 0; ht < 2; ++ht) {
        uint8_t out[2] = {0xAA, 0xAA};
        const int64_t got = resolve_action(re, rs, ht, sl_u8{ out, 2 });
        int64_t want = 0;
        if (!re && !rs) want = ht ? 2 : 1;
        ++cases; ++values;
        if (got != want) bad("resolve_action", "flags");
        ++values;
        if (out[0] != (uint8_t) want || out[1] != (uint8_t) (want == 1 ? 1 : 0))
            bad("resolve_action out", "flags");
    }

    printf("FG473_REFS cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
