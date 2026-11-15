// Staged S1.1 smoke gate: the selected grammar path against the frozen oracle.
//
// No model, no GPU, no server: the gate needs only libllama-common.
//
// In an FG_GOOSE_JSON_SCHEMA=ON build json_schema_to_grammar() dispatches to
// the Goose module through common/fg-goose-schema-bridge.cpp.
// json_schema_to_grammar_oracle() below is the frozen copy of the native body
// that P11-04 D1 deleted from json-schema-to-grammar.cpp. An OFF build carries
// no converter and must answer the empty grammar, which the gate pins; the
// output line says which mode was measured and how many calls reached the Goose
// module, so an ON build cannot pass by silently keeping a native path.
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

// P11-04 D1 (EXP665): the native converter body is deleted from
// common/json-schema-to-grammar.cpp. What it did is reproduced here, frozen, so
// the gate keeps an independent oracle: build_grammar() is public API, and the
// three steps are the whole removed body.
static std::string json_schema_to_grammar_oracle(const common_json & schema, bool force_gbnf) {
#ifdef LLAMA_USE_LLGUIDANCE
    if (!force_gbnf) {
        return "%llguidance {}\nstart: %json " + schema.dump();
    }
#else
    (void) force_gbnf;
#endif
    return build_grammar([&](const common_grammar_builder & callbacks) {
        auto copy = schema;
        callbacks.resolve_refs(copy);
        callbacks.add_schema("", copy);
    });
}

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
        expected = json_schema_to_grammar_oracle(common_json::parse(schema_text), true);
    } catch (const std::exception & e) {
        report_fail(name, std::string("oracle threw: ") + e.what());
        return;
    }
    try {
        actual = json_schema_to_grammar(common_json::parse(schema_text), true);
    } catch (const std::exception & e) {
        report_fail(name, std::string("selected path threw: ") + e.what());
        return;
    }

#ifndef FG_GOOSE_JSON_SCHEMA
    // P11-04 D1: the isolation build carries no converter at all. Pin the empty
    // answer here, and still count the rules the oracle produced so the case
    // list keeps its size.
    rules += (int) parse_rules(expected).size();
    if (!actual.empty()) {
        report_fail(name, "isolation build answered a non-empty grammar");
    }
    return;
#endif

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
