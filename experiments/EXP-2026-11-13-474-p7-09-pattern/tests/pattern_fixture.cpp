// Differential fixture for the pattern/format conversion (EXP474).
// Oracle: the real common/json-schema-to-grammar.cpp converter, linked from the
// prebuilt llama-common objects, driven through build_grammar.
// Subject: experiments/EXP-2026-11-13-474-p7-09-pattern/source/pattern.goose.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <string>
#include <vector>
#include <map>
#include <random>
#include <stdexcept>

#include "pattern-gen.h"
#include "json-schema-to-grammar.h"
#include "json-schema.h"

// ---- common.cpp helpers the linked objects reference -----------------------
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

static sl_u8 sl(const std::string & s) { return sl_u8{ (uint8_t *) s.data(), (int64_t) s.size() }; }
static sl_u8 slv(std::vector<uint8_t> & v) { return sl_u8{ v.data(), (int64_t) v.size() }; }
static sl_i64 sli(std::vector<int64_t> & v) { return sl_i64{ v.data(), (int64_t) v.size() }; }

// ---- native oracle ---------------------------------------------------------

struct NativeResult {
    int status = 0;               // 0 ok, 1 unsupported (fallback), 2 invalid
    std::string reason;
    std::map<std::string, std::string> rules;
};

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

static NativeResult native_convert(const std::string & pattern, bool dotall,
                                   const std::vector<std::pair<std::string, std::string>> & seeds) {
    NativeResult r;
    auto s = std::make_unique<common_chat_schema_string>();
    s->pattern = pattern;

    fflush(stderr);
    FILE * f = tmpfile();
    const int saved = dup(2);
    if (f) dup2(fileno(f), 2);

    std::string grammar;
    try {
        grammar = build_grammar([&](const common_grammar_builder & b) {
            for (const auto & kv : seeds) b.add_rule(kv.first, kv.second);
            b.add_schema("root", *s);
        }, { dotall });
    } catch (const std::exception & e) {
        r.status = 2;
        r.reason = e.what();
    }

    fflush(stderr);
    if (f) {
        dup2(saved, 2);
        close(saved);
        rewind(f);
        char buf[4096];
        size_t n = fread(buf, 1, sizeof buf - 1, f);
        buf[n] = 0;
        if (strstr(buf, "accepting any string")) r.status = 1;
        fclose(f);
    } else {
        close(saved);
    }

    if (r.status != 2) r.rules = parse_rules(grammar);
    return r;
}

// ---- goose driver ----------------------------------------------------------

struct Goose {
    std::vector<uint8_t> tb, kb, nb, reason, rule, name;
    std::vector<int64_t> to, tl, ti, ko, ki, subs, st;
    Goose() : tb(1 << 20), kb(1 << 16), nb(512), reason(4096), rule(1 << 16), name(512),
              to(8192), tl(8192), ti(8192), ko(1024), ki(1024), subs(16384), st(32) {}
};

static int goose_convert(Goose & g, const std::string & pattern, bool dotall,
                         const std::vector<std::pair<std::string, std::string>> & seeds,
                         std::string & rule, std::string & name, std::string & reason) {
    std::fill(g.st.begin(), g.st.end(), 0);
    int64_t off = 0;
    g.st[2] = (int64_t) seeds.size();     // S_KEYS
    for (size_t i = 0; i < seeds.size(); ++i) {
        g.ko[i] = off;
        g.ki[i] = 0;
        for (unsigned char c : seeds[i].first) g.kb[off++] = c;
    }
    g.st[3] = off;                        // S_KB
    std::fill(g.rule.begin(), g.rule.end(), 0);
    std::fill(g.name.begin(), g.name.end(), 0);
    std::fill(g.reason.begin(), g.reason.end(), 0);
    std::fill(g.nb.begin(), g.nb.end(), 0);

    const int status = (int) pattern_rule(sl(pattern), sl("root"), dotall ? 1 : 0,
        slv(g.tb), sli(g.to), sli(g.tl), sli(g.ti),
        slv(g.kb), sli(g.ko), sli(g.ki), sli(g.subs), sli(g.st),
        slv(g.nb), slv(g.reason), slv(g.rule), slv(g.name));

    rule.assign((const char *) g.rule.data(), (size_t) g.st[14]);
    name.assign((const char *) g.name.data(), (size_t) g.st[13]);
    reason.assign((const char *) g.reason.data(), (size_t) g.st[9]);
    return status;
}

// ---- comparison ------------------------------------------------------------

static void one(Goose & g, const std::string & pattern, bool dotall,
                const std::vector<std::pair<std::string, std::string>> & seeds) {
    if (pattern.empty()) return;  // visit() only calls the converter for a non-empty pattern
    std::string rule, name, reason;
    const int gs = goose_convert(g, pattern, dotall, seeds, rule, name, reason);
    const NativeResult nr = native_convert(pattern, dotall, seeds);
    ++cases;

    ++values;
    if (gs != nr.status) {
        bad("status", "pattern=[" + pattern + "] goose=" + std::to_string(gs) +
                      " native=" + std::to_string(nr.status) + " reason=[" + reason + "]");
        return;
    }

    if (gs == 2) {
        const std::string head = "JSON schema conversion failed:\nInvalid pattern " + pattern + ": ";
        std::string want = nr.reason;
        if (want.rfind(head, 0) == 0) want = want.substr(head.size());
        ++values;
        if (reason != want) bad("invalid reason", "pattern=[" + pattern + "] got=[" + reason + "] want=[" + want + "]");
        ++values;
        if (!name.empty()) bad("invalid name", "expected empty, got [" + name + "]");
        return;
    }

    auto it = nr.rules.find(name);
    ++values;
    if (it == nr.rules.end()) {
        std::string have;
        for (const auto & kv : nr.rules) have += kv.first + ";";
        bad("missing rule", "pattern=[" + pattern + "] name=[" + name + "] have=[" + have + "]");
        return;
    }
    ++values;
    if (it->second != rule) {
        bad("rule text", "pattern=[" + pattern + "] name=[" + name + "]\n  got =[" + rule + "]\n  want=[" + it->second + "]");
    }
}

int main(void) {
    goose_init(0, 0);
    Goose g;

    const std::vector<std::string> fixed = {
        "^a$", "^abc$", "^a.b$", "^a|b$", "^(ab)$", "^(?:ab)$", "^()$",
        "^[a-z]+$", "^[^a]$", "^[]]$", "^[a\\]]$", "^[a\\-z]$", "^[\\x41-\\x5A]$",
        "^\\n$", "^\\\\$", "^\\x41$", "^\\u0041$", "^\\U00000041$", "^\\t$",
        "^\\d$", "^\\.$", "^\\[$", "^a\\-b$", "^a\\|b$", "^a\\(b$",
        "^a*$", "^a+$", "^a?$", "^a{2}$", "^a{2,}$", "^a{2,3}$", "^a{0}$", "^a{0,1}$", "^a{1,1}$", "^a{0,0}$", "^a{0,2}$",
        "^a{,}$", "^a{,3}$", "^a{2,3}?$", "^(ab){2,3}$", "^(ab)*$", "^(a|b){2}$",
        "^(ab){2}(cd){3}$", "^a**$", "^a+?$", "^a{ 2}$", "^a{+2}$", "^a{-1,2}$",
        "abc", "^abc", "abc$", "^$", "^a^b$", "^a$b$", "^(a$", "^a)$", "^[$",
        "^[a$", "^*$", "^+$", "^?$", "^a{2$", "^a{1,2,3}$", "^a{x}$",
        "^a{99999999999}$", "^a\\", "^\\q$", "^a\"b$", "^\"$", "^a b$",
        "^a.b.c$", "^(a.b){2}$", "^[.]$", "^[\\.]$", "^a\\{2$",
    };

    const std::vector<std::pair<std::string, std::string>> seed_sets[] = {
        {},
        {{"dot", "SEEDDOT"}},
        {{"root", "SEEDROOT"}},
        {{"root-1", "SEED1"}},
        {{"dot", "SEEDDOT"}, {"root-1", "SEED1"}, {"root", "SEEDROOT"}},
    };

    for (const auto & p : fixed) {
        for (const auto & seeds : seed_sets) one(g, p, false, seeds);
        one(g, p, true, {});
    }

    // randomised patterns over the converter's own alphabet
    std::mt19937 rng(474001);
    const char * alpha = "abAB01.()[]{}|*+?^$\\- \":,{}=_";
    const size_t nalpha = strlen(alpha);
    for (int i = 0; i < 900; ++i) {
        std::string p;
        const int n = (int) (rng() % 16);
        for (int k = 0; k < n; ++k) p.push_back(alpha[rng() % nalpha]);
        if (rng() % 3) p = "^" + p + "$";
        one(g, p, (rng() % 4) == 0, (rng() % 5) == 0 ? seed_sets[1] : seed_sets[0]);
    }

    // dot rule text, both dotall settings
    for (int d = 0; d < 2; ++d) {
        std::vector<uint8_t> out(256, 0);
        const int64_t n = dot_rule(d, slv(out));
        std::string got((const char *) out.data(), (size_t) n);
        const NativeResult nr = native_convert("^.$", d != 0, {});
        ++cases; ++values;
        const std::string want = nr.rules.count("dot") ? nr.rules.at("dot") : std::string("<none>");
        if (want != got) bad("dot rule", "got=[" + got + "] want=[" + want + "]");
    }

    // string shape selection (table transcription) and the length rule (oracle)
    const int64_t shap[][5] = {
        // has_pattern, format, min, max, want
        {1, 0, 0, -1, 0}, {1, 1, 0, -1, 0},
        {0, 1, 0, -1, 1}, {0, 2, 0, -1, 2}, {0, 3, 0, -1, 3}, {0, 4, 0, -1, 4},
        {0, 0, 1, -1, 5}, {0, 0, 0, 3, 5}, {0, 0, 2, 4, 5},
        {0, 0, 0, -1, 6}, {0, 1, 2, 4, 1}, {0, 2, 2, 4, 2},
    };
    for (const auto & c : shap) {
        ++cases; ++values;
        const int64_t got = string_shape(c[0], c[1], c[2], c[3]);
        if (got != c[4]) bad("string_shape", "got=" + std::to_string(got) + " want=" + std::to_string(c[4]));
    }

    for (int mn = 0; mn <= 3; ++mn)
    for (int mx = -1; mx <= 5; ++mx) {
        std::vector<uint8_t> out(256, 0);
        const int64_t n = length_rule(mn, mx, slv(out));
        const std::string got((const char *) out.data(), (size_t) n);
        auto s = std::make_unique<common_chat_schema_string>();
        s->min_length = mn;
        s->max_length = mx;
        const std::string grammar = build_grammar([&](const common_grammar_builder & b) {
            b.add_schema("root", *s);
        }, { false });
        const auto rules = parse_rules(grammar);
        ++cases; ++values;
        if (rules.at("root") != got) {
            bad("length_rule", "min=" + std::to_string(mn) + " max=" + std::to_string(mx) +
                               " got=[" + got + "] want=[" + rules.at("root") + "]");
        }
    }

    // format_literal against the public native helper
    {
        std::vector<std::string> lits = {"", "a", "abc", "-", "]", "a-b]c",
            "quote\"here", "back\\slash", "cr\rlf\n", "\r\n\"\\"};
        for (int b = 0; b < 256; ++b) lits.push_back(std::string(1, (char) (unsigned char) b));
        std::mt19937 r(474002);
        const char * la = "ab09-_./: \"\\\r\n\t]";
        const size_t nla = strlen(la);
        for (int i = 0; i < 300; ++i) {
            std::string s2;
            const int n = (int) (r() % 12);
            for (int k = 0; k < n; ++k) s2.push_back(la[r() % nla]);
            lits.push_back(s2);
        }
        for (const auto & l : lits) {
            std::vector<uint8_t> out(1024, 0);
            const int64_t n = format_literal(sl(l), slv(out));
            const std::string got((const char *) out.data(), (size_t) n);
            const std::string want = gbnf_format_literal(l);
            ++cases; ++values;
            if (got != want) bad("format_literal", "got=[" + got + "] want=[" + want + "]");
        }
    }

    printf("FG474_PATTERN cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
