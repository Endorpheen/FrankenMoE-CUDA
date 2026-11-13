// Differential fixture for PEG chars pattern parsing (EXP498).
// Oracle: common_peg_parser_builder::chars, which stores the parsed ranges and
// the negation flag in the built common_peg_chars_parser.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <variant>

#include "chars-gen.h"
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

static void check(const std::string & pattern) {
    common_peg_parser_builder b;
    common_peg_parser p = b.chars(pattern, 1, -1);
    auto arena = b.build();
    const auto & cp = std::get<common_peg_chars_parser>(arena.get(p.id()));

    std::vector<int64_t> starts(256, 0), ends(256, 0), st(8, 0);
    ++cases;
    const int64_t n = chars_parse(sl_u8{ (uint8_t *) pattern.data(), (int64_t) pattern.size() },
        sli(starts), sli(ends), sli(st));

    const std::string ctx = "pattern=[" + visible(pattern) + "]";
    ++values;
    if (st[0] != (cp.negated ? 1 : 0)) {
        bad("negated", ctx + " goose=" + std::to_string(st[0]) + " native=" + std::to_string((int) cp.negated));
    }
    ++values;
    if ((size_t) n != cp.ranges.size()) {
        std::string native;
        for (const auto & r : cp.ranges) native += " [" + std::to_string(r.start) + "," + std::to_string(r.end) + "]";
        bad("range count", ctx + " goose=" + std::to_string(n) + " native=" + std::to_string(cp.ranges.size()) + native);
        return;
    }
    for (int64_t i = 0; i < n; ++i) {
        ++values;
        if (starts[i] != (int64_t) cp.ranges[(size_t) i].start || ends[i] != (int64_t) cp.ranges[(size_t) i].end) {
            bad("range", ctx + " index " + std::to_string(i) +
                " goose=[" + std::to_string(starts[i]) + "," + std::to_string(ends[i]) + "]" +
                " native=[" + std::to_string(cp.ranges[(size_t) i].start) + "," + std::to_string(cp.ranges[(size_t) i].end) + "]");
        }
    }
}

int main(void) {
    goose_init(0, 0);

    const std::vector<std::string> patterns = {
        "a-z",
        "a-z0-9_",
        "[a-z]",
        "[^a-z]",
        "[a-zA-Z0-9_]",
        "abc",
        "a-",
        "-a",
        "a-b-c",
        "[a-z-]",
        "[-a]",
        "[]",
        "]",
        "[^]",
        "\\x41-\\x5A",
        "\\u00e9",
        "\\U0001F600",
        "\\x41",
        "\\n",
        "\\t",
        "\\r",
        "\\\\",
        "\\]",
        "\\[",
        "\\-",
        "\\q",
        "\\x4",
        "\\u12",
        "\\U0001",
        "a\\-z",
        "[a\\-z]",
        "z-a",
        "0-9a-fA-F",
        "^^",
        "a^b",
        "[^^]",
        "caf\xc3\xa9",
        "\xc3\xa9",
        "\xe4\xb8\xad",
        "[\\x00-\\x1f]",
        "~",
        " ",
    };
    for (const auto & p : patterns) check(p);

    printf("FG498_CHARS cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
