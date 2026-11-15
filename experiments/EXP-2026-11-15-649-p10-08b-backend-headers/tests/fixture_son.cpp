// Differential gate for P10-08 S6 (SONAME, versioning and the export surface).
//
// Native arms, all re-derived by tests/extract_native.py on every build:
//   - the three version variables from ggml/CMakeLists.txt and CMakeLists.txt;
//   - SONAME, DT_NEEDED and the symlink chain read out of the reference .so
//     files with readelf and readlink;
//   - the dynamic symbol tables with nm;
//   - the public declarations parsed out of the five headers;
//   - the export-macro expansion produced by running the real preprocessor over
//     the real headers with and without the shared flags;
//   - the counts in docs/goose-migration/abi-ledger.json.
//
// The reconciliation rule under test: every declaration must be exported by one
// of the libraries its header belongs to, except the enumerated allowance, and
// every llama_* export without a declaration must be in the enumerated legacy
// set. Neither side can shrink silently.
//
// Port arm: the son_ exports of source/soname-exports.goose.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "son.h"
#include "native_abi.h"

static int cases = 0;
static int values = 0;
static int fails = 0;

static void eq_int(const char * what, int64_t want, int64_t got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-46s want=%lld got=%lld\n", what, (long long) want, (long long) got);
    }
}

static void eq_str(const char * what, const std::string & want, const std::string & got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-46s want=[%s] got=[%s]\n", what, want.c_str(), got.c_str());
    }
}

static std::string port_str(int64_t (*fn)(int64_t, sl_u8), int64_t key) {
    char buf[256];
    memset(buf, 0x5a, sizeof buf);
    int64_t n = fn(key, sl_u8{(uint8_t *) buf, (int64_t) sizeof buf - 1});
    if (n < 0) {
        return "!";
    }
    return std::string(buf, (size_t) n);
}

static std::vector<std::string> split(const std::string & s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}

static std::string port_needed(int64_t lib, int64_t i) {
    char buf[128];
    memset(buf, 0x5a, sizeof buf);
    int64_t n = son_needed_at(lib, i, sl_u8{(uint8_t *) buf, (int64_t) sizeof buf - 1});
    if (n < 0) return "!";
    return std::string(buf, (size_t) n);
}

// "libllama.so.0.3.0" -> "llama"
static std::string stem_of(const std::string & file) {
    const size_t so = file.rfind(".so.");
    if (file.rfind("lib", 0) != 0 || so == std::string::npos) return "!";
    return file.substr(3, so - 3);
}

static std::vector<std::string> syms(int lib) {
    return split(NAT_LIB_SYM[lib], '|');
}

static bool has(const std::vector<std::string> & v, const std::string & s) {
    for (auto & x : v) {
        if (x == s) return true;
    }
    return false;
}

int main() {
    goose_init(0, nullptr);

    // 1. versions, SONAMEs and the symlink chain.
    eq_int("library count", NAT_LIB_N, son_lib_count());
    for (int i = 0; i < NAT_LIB_N && i < (int) son_lib_count(); i++) {
        cases++;
        const std::string stem = port_str(son_lib_stem, i);
        const bool llama_family = stem == "llama" || stem == "llama-common" || stem == "mtmd";
        eq_str(("stem " + std::to_string(i)).c_str(), stem_of(NAT_LIB_FILE[i]), stem);
        eq_str(("version " + stem).c_str(), llama_family ? NAT_LLAMA_VERSION : NAT_GGML_VERSION,
               port_str(son_lib_version, i));
        eq_int(("soversion " + stem).c_str(), llama_family ? NAT_LLAMA_SOVERSION : NAT_GGML_SOVERSION,
               son_lib_soversion(i));
        eq_str(("soname " + stem).c_str(), NAT_LIB_SONAME[i], port_str(son_soname, i));
        // libX.so -> libX.so.<sov> -> libX.so.<version>
        const std::string short_link = std::string("lib") + stem + ".so";
        const std::string mid_link = std::string("lib") + stem + ".so." + std::to_string(son_lib_soversion(i));
        // readlink gives the target each link points at
        eq_str(("short symlink target " + stem).c_str(), NAT_LIB_CHAIN_SHORT[i], mid_link);
        eq_str(("mid symlink target " + stem).c_str(), NAT_LIB_CHAIN_MID[i],
               std::string("lib") + stem + ".so." + port_str(son_lib_version, i));
        const std::string chain = short_link + " -> " + mid_link + " -> lib" + stem + ".so." +
                                  port_str(son_lib_version, i);
        eq_str(("chain " + stem).c_str(), chain, port_str(son_chain, i));
    }

    // 2. DT_NEEDED: required entries present, the forbidden one absent.
    for (int i = 0; i < NAT_LIB_N; i++) {
        cases++;
        const std::vector<std::string> needed = split(NAT_LIB_NEEDED[i], '|');
        const int64_t n = son_needed_count(i);
        for (int64_t k = 0; k < n; k++) {
            const std::string want = port_needed(i, k);
            values++;
            if (!has(needed, want)) {
                fails++;
                printf("FAIL lib%d requires %s but does not link it\n", i, want.c_str());
            }
        }
        const std::string forbid = port_str(son_forbidden_needed, i);
        values++;
        if (has(needed, forbid)) {
            fails++;
            printf("FAIL lib%d links the forbidden %s\n", i, forbid.c_str());
        }
    }

    // 3. every public declaration must be exported by its libraries.
    {
        std::vector<std::string> unresolved;
        for (int i = 0; i < NAT_DECL_N; i++) {
            const std::string header = NAT_DECL[i].header;
            int hid = -1;
            for (int64_t h = 0; h < son_header_count(); h++) {
                if (port_str(son_header_name, h) == header) hid = (int) h;
            }
            values++;
            if (hid < 0) {
                fails++;
                printf("FAIL header %s is unknown to the port\n", header.c_str());
                continue;
            }
            const int64_t mask = son_header_libs(hid);
            bool found = false;
            for (int lib = 0; lib < NAT_LIB_N; lib++) {
                if ((mask & (1 << lib)) != 0 && has(syms(lib), NAT_DECL[i].name)) {
                    found = true;
                }
            }
            cases++;
            if (!found) unresolved.push_back(NAT_DECL[i].name);
        }
        std::vector<std::string> allow;
        for (int64_t i = 0; i < son_allowance_count(); i++) allow.push_back(port_str(son_allowance_name, i));
        eq_int("unresolved count", (int64_t) allow.size(), (int64_t) unresolved.size());
        for (auto & u : unresolved) {
            eq_int(("unresolved " + u).c_str(), has(allow, u) ? 1 : 0, 1);
        }
        // and the allowance is exactly the oracle's set
        for (int i = 0; i < NAT_UNRESOLVED_N; i++) {
            eq_str("allowance name", NAT_UNRESOLVED[i], i < (int) allow.size() ? allow[(size_t) i] : "!");
        }
    }

    // 4. the undeclared llama_* exports.
    {
        std::vector<std::string> legacy;
        for (int64_t i = 0; i < son_legacy_count(); i++) legacy.push_back(port_str(son_legacy_name, i));
        eq_int("legacy count", NAT_LEGACY_N, (int64_t) legacy.size());
        for (int i = 0; i < NAT_LEGACY_N; i++) {
            cases++;
            eq_str("legacy export", NAT_LEGACY[i], i < (int) legacy.size() ? legacy[(size_t) i] : "!");
        }
        eq_int("no extra legacy symbol", 0,
               (int64_t) (NAT_LEGACY_N - (int64_t) legacy.size()));
    }

    // 5. the export macros, expanded by the real preprocessor.
    eq_int("macro count", NAT_MACRO_N / 2, son_macro_count());
    for (int i = 0; i < NAT_MACRO_N; i++) {
        cases++;
        eq_int(("macro outcome " + std::to_string(NAT_MACRO[i].macro_id) + "/" +
                std::to_string(NAT_MACRO[i].shared)).c_str(),
               NAT_MACRO[i].code, son_macro_outcome(NAT_MACRO[i].macro_id, NAT_MACRO[i].shared));
    }
    for (int64_t m = 0; m < son_macro_count() && m < 3; m++) {
        eq_str("macro name", NAT_MACRO_NAME[m], port_str(son_macro_name, m));
    }
    eq_int("macro_outcome(-1)", -1, son_macro_outcome(-1, 1));

    // 6. the ledger counts and the identity of its library.
    eq_int("ledger exports", NAT_LEDGER_EXPORTS, son_ledger_exports());
    eq_int("ledger declarations", NAT_LEDGER_DECLS, son_ledger_declarations());
    eq_int("ledger imports", NAT_LEDGER_IMPORTS, son_ledger_imports());
    eq_int("ledger structs", NAT_LEDGER_STRUCTS, son_ledger_structs());
    {
        cases++;
        char buf[128];
        const int64_t n = son_ledger_library(sl_u8{(uint8_t *) buf, 127});
        eq_str("ledger library", NAT_LEDGER_LIB, std::string(buf, (size_t) n));
    }

    // 7. SONAME parsing: the ledger library carries a stem and a patch version
    // that upstream never publishes, so it cannot match the upstream SONAME.
    struct parse_case {
        const char * name;
        const char * want;  // empty means refusal
    };
    const std::vector<parse_case> parse = {
        {"libllama.so.0", "llama\n0\n0"},
        {"libggml-base.so.0.22.0", "ggml-base\n0.22.0\n0"},
        {"libllama-exp371.so.0.7.0", "llama-exp371\n0.7.0\n0"},
        {"libmtmd.so.0.3.0", "mtmd\n0.3.0\n0"},
        {"llama.so.0", ""},
        {"libllama.so", ""},
        {"lib.so.0", ""},
        {"", ""},
    };
    for (auto & c : parse) {
        cases++;
        char buf[256];
        const int64_t n = son_split_soname(
            sl_u8{(uint8_t *) c.name, (int64_t) strlen(c.name)}, sl_u8{(uint8_t *) buf, 255});
        const std::string got = n < 0 ? std::string() : std::string(buf, (size_t) n);
        eq_str(("split " + std::string(c.name[0] ? c.name : "<empty>")).c_str(), c.want, got);
    }

    // 8. identity: only the upstream SONAME is a match.
    cases++;
    eq_int("identity upstream", 1, son_identity_matches(sl_u8{(uint8_t *) "libllama.so.0", 12}));
    eq_int("identity ledger", 0,
           son_identity_matches(sl_u8{(uint8_t *) NAT_LEDGER_LIB, (int64_t) strlen(NAT_LEDGER_LIB)}));
    eq_int("identity short", 0, son_identity_matches(sl_u8{(uint8_t *) "libllama.so", 11}));
    eq_int("identity long", 0, son_identity_matches(sl_u8{(uint8_t *) "libllama.so.00", 13}));

    // 9. boundaries.
    eq_int("stem(-1)", -1, son_lib_stem(-1, sl_u8{(uint8_t *) nullptr, 0}));
    eq_int("soversion(-1)", -1, son_lib_soversion(-1));
    eq_int("header_libs(-1)", -1, son_header_libs(-1));
    eq_int("needed_at(out of range)", -1, son_needed_at(0, 99, sl_u8{(uint8_t *) nullptr, 0}));

    printf("FG651_SON cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
