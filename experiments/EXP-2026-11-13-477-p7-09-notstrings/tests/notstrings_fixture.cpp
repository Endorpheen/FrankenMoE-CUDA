// Differential fixture for _not_strings (EXP477).
// Oracle: the real common/json-schema-to-grammar.cpp converter via build_grammar;
// the subject is the "additional-k" rule of an object with typed additional
// properties, which is exactly _not_strings(property names).
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <random>

#include "notstrings-gen.h"
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

static void check(const std::vector<std::string> & names) {
    auto o = std::make_unique<common_chat_schema_object>();
    for (size_t i = 0; i < names.size(); ++i) {
        common_chat_schema_property p;
        p.name = names[i];
        p.schema = std::make_unique<common_chat_schema_string>();
        p.required = (i % 2) == 0;
        o->properties.push_back(std::move(p));
    }
    o->additional_properties = std::make_unique<common_chat_schema_integer>();

    std::map<std::string, std::string> nr;
    try {
        nr = parse_rules(build_grammar([&](const common_grammar_builder & b) {
            b.add_schema("root", *o);
        }, { false }));
    } catch (const std::exception & e) {
        bad("native throw", e.what());
        return;
    }
    auto it = nr.find("additional-k");
    if (it == nr.end()) { bad("missing additional-k", names.empty() ? "<empty>" : names[0]); return; }

    std::vector<uint8_t> strs(1 << 16, 0);
    std::vector<int64_t> dir(2 * 64, 0);
    int64_t off = 0;
    for (size_t i = 0; i < names.size(); ++i) {
        dir[2 * i + 0] = off;
        dir[2 * i + 1] = (int64_t) names[i].size();
        for (unsigned char c : names[i]) strs[off++] = c;
    }
    std::vector<int64_t> head(4096, -1), ecpt(8192, 0), eto(8192, 0), enext(8192, 0), st(8, 0), rst(4, 0);
    std::vector<uint8_t> out(1 << 16, 0), rbuf(1 << 18, 0);

    ++cases;
    const int64_t len = not_strings(slv(strs), sli(dir), (int64_t) names.size(), sl("char"),
        sli(head), sli(ecpt), sli(eto), sli(enext), sli(st), slv(out), slv(rbuf), sli(rst));
    std::string ctx;
    for (const auto & n : names) ctx += "[" + n + "]";
    eq("not_strings", std::string((const char *) out.data(), (size_t) len), it->second, ctx);
}

int main(void) {
    goose_init(0, 0);

    const std::vector<std::vector<std::string>> fixed = {
        {"a"},
        {"and", "also"},
        {"ab", "ac", "b"},
        {"a", "ab", "abc"},
        {"abc", "abd", "aec"},
        {"caf\xc3\xa9", "cafe"},
        {"\xc3\xa9", "\xc3\xa8"},
        {"0", "9", "a", "z", "_"},
        {""},
        {"", "a"},
        {"x", "xy", "xyz", "xyzz"},
        {"a", "a"},
        {"ab", "ab", "abc"},
        {"aa", "aab", "aac", "ab", "b"},
        {"-", ".", "0", "9", "A", "Z", "a", "z"},
        {"\xe4\xb8\xad", "\xe4\xb8\xad\xe6\x96\x87", "a"},
    };
    for (const auto & s : fixed) check(s);

    std::mt19937 rng(477001);
    const char * alpha[] = {"a", "b", "c", "ab", "ba", "aa", "0", "\xc3\xa9", "-", "_"};
    for (int i = 0; i < 60; ++i) {
        std::vector<std::string> names;
        const int n = 1 + (int) (rng() % 6);
        for (int k = 0; k < n; ++k) names.push_back(alpha[rng() % 10]);
        check(names);
    }

    printf("FG477_NOTSTRINGS cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
