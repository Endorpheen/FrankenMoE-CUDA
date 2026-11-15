// P10-07 differential gate: one JSON consumer, two carriers, same corpus.
//
//   OFF build: hf_pick() is the nlohmann path. The dump is the canon answer.
//   ON build : hf_pick() is the Goose path; the gate also runs the always
//              compiled nlohmann body as the oracle and compares every field.
//
// It also checks that the dispatch is real: the call counters must show the
// oracle calls and the dispatched calls separately.
//
// Output: one deterministic line per case, then a summary line.

#include <cstdio>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "hf-pick.h"

static std::string esc(const std::string & s) {
    static const char * hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if (c >= 0x20 && c < 0x7f && c != '\\' && c != '|' && c != '=') {
            out.push_back((char) c);
        } else {
            out.push_back('\\');
            out.push_back('x');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 15]);
        }
    }
    return out;
}

static std::string field_list(const hf_pick_result & r) {
    std::string s = std::to_string(r.matches.size());
    for (const auto & m : r.matches) {
        s += "|";
        s += esc(m);
    }
    return s;
}

#if defined(FG_GOOSE_JSON_REPLACE) && FG_GOOSE_JSON_REPLACE
static bool same(const hf_pick_result & a, const hf_pick_result & b, std::string & why) {
    if (a.error != b.error) {
        why = "error " + std::to_string(a.error) + " vs " + std::to_string(b.error);
        return false;
    }
    if (a.chosen != b.chosen) {
        why = "chosen '" + esc(a.chosen) + "' vs '" + esc(b.chosen) + "'";
        return false;
    }
    if (a.split_prefix != b.split_prefix) {
        why = "prefix '" + esc(a.split_prefix) + "' vs '" + esc(b.split_prefix) + "'";
        return false;
    }
    if (a.matches != b.matches) {
        why = "matches " + field_list(a) + " vs " + field_list(b);
        return false;
    }
    return true;
}
#endif

static std::string read_all(const std::string & path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main(int argc, char ** argv) {
    std::string cases_dir = "tests/cases";
    bool dump = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--dump") {
            dump = true;
        } else if (a == "--cases" && i + 1 < argc) {
            cases_dir = argv[++i];
        }
    }

    std::ifstream manifest(cases_dir + "/../cases.tsv");
    if (!manifest) {
        manifest.open("tests/cases.tsv");
    }
    if (!manifest) {
        fprintf(stderr, "gate: cannot open cases.tsv under %s\n", cases_dir.c_str());
        return 2;
    }

    int cases = 0;
    int failures = 0;
    int diffs = 0;

    std::string line;
    while (std::getline(manifest, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream ls(line);
        std::string name, quant;
        std::getline(ls, name, '\t');
        std::getline(ls, quant, '\t');

        const std::string body = read_all(cases_dir + "/" + name + ".json");
        ++cases;

        const hf_pick_result got = hf_pick(body, quant);

        std::string out = "case=" + name + " err=" + std::to_string(got.error) +
                          " chosen=" + esc(got.chosen) +
                          " prefix=" + esc(got.split_prefix) +
                          " matches=" + field_list(got);
        printf("%s\n", out.c_str());

#if defined(FG_GOOSE_JSON_REPLACE) && FG_GOOSE_JSON_REPLACE
        const hf_pick_result oracle = hf_pick_native(body, quant);
        std::string why;
        if (!same(got, oracle, why)) {
            ++diffs;
            printf("DIFF case=%s %s\n", name.c_str(), why.c_str());
        }
#endif
    }

    if (!dump) {
#if defined(FG_GOOSE_JSON_REPLACE) && FG_GOOSE_JSON_REPLACE
        printf("FG665_GATE mode=goose cases=%d failures=%d native_vs_goose_diffs=%d"
               " native_calls=%d goose_calls=%d\n",
               cases, failures, diffs, hf_pick_calls_native, hf_pick_calls_goose);
        if (diffs != 0) {
            ++failures;
        }
#else
        printf("FG665_GATE mode=native cases=%d failures=%d native_vs_goose_diffs=0"
               " native_calls=%d goose_calls=%d\n",
               cases, failures, hf_pick_calls_native, hf_pick_calls_goose);
#endif
    }

    return (failures == 0 && diffs == 0) ? 0 : 1;
}
