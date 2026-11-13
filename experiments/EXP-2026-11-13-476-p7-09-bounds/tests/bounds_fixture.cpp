// Differential fixture for integer bounds and const/enum rules (EXP476).
// Oracle: the real common/json-schema-to-grammar.cpp converter via build_grammar.
// Subject: experiments/EXP-2026-11-13-476-p7-09-bounds/source/bounds.goose.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <limits>

#include "bounds-gen.h"
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

static std::string ctx_i(int64_t a, int64_t b) {
    return "min=" + std::to_string(a) + " max=" + std::to_string(b);
}

int main(void) {
    goose_init(0, 0);
    std::vector<uint8_t> out(1 << 20), buf(1 << 20);

    const int64_t I64_MIN = std::numeric_limits<int64_t>::min();
    const int64_t I64_MAX = std::numeric_limits<int64_t>::max();

    const std::vector<int64_t> vals = {
        I64_MIN + 1, -1000000, -999, -100, -99, -10, -9, -2, -1,
        0, 1, 2, 9, 10, 11, 42, 99, 100, 101, 999, 1000, 1234,
        9999, 10000, 99999, 1000000, 123456789, I64_MAX - 1, I64_MAX,
    };

    auto check_int = [&](int64_t mn, int64_t mx) {
        auto s = std::make_unique<common_chat_schema_integer>();
        s->minimum = mn;
        s->maximum = mx;
        bool thrown = false;
        std::map<std::string, std::string> nr;
        try {
            nr = parse_rules(build_grammar([&](const common_grammar_builder & b) {
                b.add_schema("root", *s);
            }, { false }));
        } catch (const std::exception & e) {
            thrown = true;
        }
        ++cases;
        if (mn == I64_MIN && mx == I64_MAX) {
            // visit adds the integer primitive content under "root" (native PRIMITIVE_RULES)
            eq("integer primitive", nr.at("root"), "(\"-\"? integral-part)", ctx_i(mn, mx));
            return;
        }
        std::fill(out.begin(), out.end(), 0);
        std::fill(buf.begin(), buf.end(), 0);
        const int64_t len = int_rule(mn, mx, slv(out), slv(buf));
        if (thrown) {
            ++values;
            if (len >= 0) bad("expected native rejection", ctx_i(mn, mx) + " goose returned a rule");
            return;
        }
        eq("integer body", std::string((const char *) out.data(), (size_t) len), nr.at("root"), ctx_i(mn, mx));
    };

    for (int64_t v : vals) {
        check_int(v, I64_MAX);   // minimum only
        check_int(I64_MIN, v);   // maximum only
    }
    const std::vector<int64_t> pairs = {
        -1000, -100, -10, -1, 0, 1, 9, 10, 99, 100, 999, 1000, 1234, 9999, 1000000,
    };
    for (size_t i = 0; i < pairs.size(); ++i)
        for (size_t j = i; j < pairs.size(); ++j)
            check_int(pairs[i], pairs[j]);
    check_int(-I64_MAX, -1);
    check_int(I64_MIN + 1, I64_MAX - 1);

    // const rules from real JSON values
    const std::vector<std::string> consts = {
        "\"abc\"", "\"\"", "\"a\\\"b\"", "\"line\\nbreak\"", "\"caf\\u00e9\"",
        "42", "-7", "3.5", "1e10", "true", "false", "null",
        "[1,2]", "{\"a\":1}", "\"a\\\\b\"",
    };
    for (const auto & text : consts) {
        common_json v = common_json::parse(text);
        auto s = std::make_unique<common_chat_schema_const>(v);
        const auto nr = parse_rules(build_grammar([&](const common_grammar_builder & b) {
            b.add_schema("root", *s);
        }, { false }));
        ++cases;
        std::fill(out.begin(), out.end(), 0);
        const int64_t len = const_rule(sl(v.dump()), slv(out));
        eq("const body", std::string((const char *) out.data(), (size_t) len), nr.at("root"), text);
    }

    // enum rules
    const std::vector<std::string> enums = {
        "[]", "[\"a\"]", "[\"a\",\"b\"]", "[1,2,3]", "[\"a\",1,true,null]",
        "[\"a\\\"b\",\"c\\\\d\",\"e\\nf\"]", "[[1],[2]]", "[{\"a\":1},{\"b\":2}]",
    };
    for (const auto & text : enums) {
        common_json arr = common_json::parse(text);
        auto s = std::make_unique<common_chat_schema_enum>();
        for (size_t i = 0; i < arr.size(); ++i) s->values.push_back(arr[i]);
        const auto nr = parse_rules(build_grammar([&](const common_grammar_builder & b) {
            b.add_schema("root", *s);
        }, { false }));
        ++cases;
        std::vector<uint8_t> strs(1 << 16, 0);
        std::vector<int64_t> dir(2 * 64, 0);
        int64_t off = 0;
        for (size_t i = 0; i < arr.size(); ++i) {
            const std::string dump = arr[i].dump();
            dir[2 * i + 0] = off;
            dir[2 * i + 1] = (int64_t) dump.size();
            for (unsigned char c : dump) strs[off++] = c;
        }
        std::fill(out.begin(), out.end(), 0);
        const int64_t len = enum_rule(slv(strs), sli(dir), (int64_t) arr.size(), slv(out));
        eq("enum body", std::string((const char *) out.data(), (size_t) len), nr.at("root"), text);
    }

    printf("FG476_BOUNDS cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
