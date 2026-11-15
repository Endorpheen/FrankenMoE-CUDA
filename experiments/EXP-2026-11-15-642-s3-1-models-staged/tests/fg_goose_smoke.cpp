// Staged S1.1 smoke gate: the selected grammar path against the native oracle.
//
// No model, no GPU, no server: the gate needs only libllama-common.
//
// In an FG_GOOSE_JSON_SCHEMA=ON build json_schema_to_grammar() dispatches to
// the Goose module through common/fg-goose-schema-bridge.cpp, and
// json_schema_to_grammar_native() is the oracle. In an OFF build both sides are
// the native body, which proves the switch-off path still builds and behaves;
// the output line says which mode was measured and how many calls reached the
// Goose module, so an ON build cannot pass by silently keeping the native path.
//
// Output: FG629_SMOKE mode=<goose|native> cases=N rules=M failures=K goose_calls=C

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "json-schema-to-grammar.h"
#include "json.h"

#include "fg_goose_cases.h"

static std::map<std::string, std::string> parse_rules(const std::string & g) {
    std::map<std::string, std::string> m;
    size_t p = 0;
    while (p < g.size()) {
        size_t e = g.find('\n', p);
        if (e == std::string::npos) {
            e = g.size();
        }
        const std::string line = g.substr(p, e - p);
        const size_t sep = line.find(" ::= ");
        if (sep != std::string::npos) {
            m[line.substr(0, sep)] = line.substr(sep + 5);
        }
        p = e + 1;
    }
    return m;
}

static int cases = 0;
static int rules = 0;
static int failures = 0;

static void report_fail(const char * name, const std::string & detail) {
    ++failures;
    if (failures <= 12) {
        printf("FAIL %s: %s\n", name, detail.c_str());
    }
}

static void run_case(const char * name, const char * schema_text) {
    ++cases;

    std::string expected;
    std::string actual;
    try {
        expected = json_schema_to_grammar_native(common_json::parse(schema_text), true);
    } catch (const std::exception & e) {
        report_fail(name, std::string("native oracle threw: ") + e.what());
        return;
    }
    try {
        actual = json_schema_to_grammar(common_json::parse(schema_text), true);
    } catch (const std::exception & e) {
        report_fail(name, std::string("selected path threw: ") + e.what());
        return;
    }

    const auto native = parse_rules(expected);
    const auto ported = parse_rules(actual);
    rules += (int) native.size();

    if (ported == native) {
        return;
    }

    std::string detail;
    for (const auto & kv : native) {
        auto it = ported.find(kv.first);
        if (it == ported.end()) {
            detail = "missing rule " + kv.first;
            break;
        }
        if (it->second != kv.second) {
            const std::string & a = kv.second;
            const std::string & b = it->second;
            size_t d = 0;
            while (d < a.size() && d < b.size() && a[d] == b[d]) {
                ++d;
            }
            char buf[160];
            snprintf(buf, sizeof buf, " len(a)=%zu len(b)=%zu first=%zu a=%d b=%d",
                     a.size(), b.size(), d,
                     d < a.size() ? (unsigned char) a[d] : -1,
                     d < b.size() ? (unsigned char) b[d] : -1);
            detail = "rule " + kv.first + "\n  native: " + a + "\n  selected: " + b + buf;
            break;
        }
    }
    if (detail.empty()) {
        for (const auto & kv : ported) {
            if (native.find(kv.first) == native.end()) {
                detail = "extra rule " + kv.first + " ::= " + kv.second;
                break;
            }
        }
    }
    if (detail.empty()) {
        detail = "same keys, different order or text";
    }
    report_fail(name, detail);
}

int main() {
    for (const auto & c : fg_schema_cases) {
        run_case(c[0], c[1]);
    }

#ifdef FG_GOOSE_JSON_SCHEMA
    const char * mode = "goose";
    const long calls = fg_goose_json_schema_calls();
#else
    const char * mode = "native";
    const long calls = 0;
#endif

    printf("FG629_SMOKE mode=%s cases=%d rules=%d failures=%d goose_calls=%ld\n",
           mode, cases, rules, failures, calls);

#ifdef FG_GOOSE_JSON_SCHEMA
    // An ON build that never entered the bridge would be the native path with
    // extra objects; treat it as a failure of the stage, not of the module.
    if (calls != cases) {
        printf("FAIL dispatch: goose_calls=%ld expected %d\n", calls, cases);
        return 2;
    }
#endif
    return failures == 0 ? 0 : 1;
}
