// Differential fixture for the remaining non-AST PEG variants (EXP483).
// Oracle: common_peg_arena::parse.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <variant>

#include "pegparse2-gen.h"
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

static sl_u8 sl(const std::string & s) { return sl_u8{ (uint8_t *) s.data(), (int64_t) s.size() }; }
static sl_u8 slv(std::vector<uint8_t> & v) { return sl_u8{ v.data(), (int64_t) v.size() }; }
static sl_i64 sli(std::vector<int64_t> & v) { return sl_i64{ v.data(), (int64_t) v.size() }; }

enum K { K_LIT = 3, K_SEQ = 4, K_CHOICE = 5, K_REP = 6, K_AND = 7, K_NOT = 8, K_ANY = 9,
         K_SPACE = 10, K_CHARS = 11, K_STRING = 12, K_UNTIL = 13, K_ATOMIC = 16, K_GBNF = 18, K_AC = 19 };
struct Spec {
    int kind;
    std::vector<int> ch;
    std::string s;
    std::vector<std::string> delims;
    int mn = 0, mx = 0;
    bool neg = false;
};
struct Interner {
    std::vector<std::string> strings;
    std::map<std::string, int> idx;
    int add(const std::string & s) {
        auto it = idx.find(s);
        if (it != idx.end()) return it->second;
        int i = (int) strings.size();
        strings.push_back(s); idx[s] = i; return i;
    }
};

static void run_tree(const std::vector<Spec> & specs, int root, const std::string & label,
                     const std::vector<std::string> & inputs, const std::vector<int> & starts,
                     const std::vector<common_peg_parse_flags> & flags) {
    common_peg_parser_builder b;
    std::vector<common_peg_parser> ps;
    for (const auto & s : specs) {
        common_peg_parser p = b.eps();
        switch (s.kind) {
            case K_LIT: p = b.literal(s.s); break;
            case K_SEQ: { auto q = b.sequence(); for (int c : s.ch) q += ps[c]; p = q; break; }
            case K_CHOICE: { auto q = b.choice(); for (int c : s.ch) q |= ps[c]; p = q; break; }
            case K_REP: p = b.repeat(ps[s.ch[0]], s.mn, s.mx); break;
            case K_AND: p = b.peek(ps[s.ch[0]]); break;
            case K_NOT: p = b.negate(ps[s.ch[0]]); break;
            case K_ANY: p = b.any(); break;
            case K_SPACE: p = b.space(); break;
            case K_CHARS: p = b.chars(s.s, s.mn, s.mx); break;
            case K_STRING: p = b.string_content(s.s.empty() ? '"' : s.s[0]); break;
            case K_UNTIL: p = b.until(s.s); break;
            case K_ATOMIC: p = b.atomic(ps[s.ch[0]]); break;
            case K_GBNF: p = b.gbnf(ps[s.ch[0]], s.s); break;
            case K_AC: p = b.ac(ps[s.ch[0]], s.delims); break;
        }
        ps.push_back(p);
    }
    auto arena = b.build();
    for (size_t i = 0; i < specs.size(); ++i) {
        if (specs[i].kind == K_CHARS && specs[i].neg) {
            std::get<common_peg_chars_parser>(arena.get(ps[i].id())).negated = true;
        }
        if (specs[i].kind == K_UNTIL && !specs[i].delims.empty()) {
            auto & u = std::get<common_peg_until_parser>(arena.get(ps[i].id()));
            for (const auto & d : specs[i].delims) u.delimiters.push_back(d);
        }
    }

    Interner in;
    std::vector<int64_t> kind, cfirst, ccount, children, sidx, mn, mx;
    std::vector<int64_t> rstart, rcount, rngs, rnge, neg, dstart, dcount, delims;
    for (const auto & s : specs) {
        kind.push_back(s.kind);
        cfirst.push_back((int64_t) children.size());
        ccount.push_back((int64_t) s.ch.size());
        for (int c : s.ch) children.push_back(c);
        sidx.push_back(s.s.empty() ? in.add("") : in.add(s.s));
        mn.push_back(s.mn);
        mx.push_back(s.mx);
        rstart.push_back(0); rcount.push_back(0); neg.push_back(0);
        dstart.push_back((int64_t) delims.size());
        if (s.kind == K_UNTIL) {
            int64_t n = (s.s.empty() ? 0 : 1) + (int64_t) s.delims.size();
            dcount.push_back(n);
            if (!s.s.empty()) delims.push_back(in.add(s.s));
            for (const auto & d : s.delims) delims.push_back(in.add(d));
        } else {
            dcount.push_back(0);
        }
    }
    // chars ranges come from the built native variants
    for (size_t i = 0; i < specs.size(); ++i) {
        if (specs[i].kind != K_CHARS) continue;
        const auto & cp = std::get<common_peg_chars_parser>(arena.get(ps[i].id()));
        rstart[i] = (int64_t) rngs.size();
        rcount[i] = (int64_t) cp.ranges.size();
        neg[i] = cp.negated ? 1 : 0;
        for (const auto & r : cp.ranges) { rngs.push_back(r.start); rnge.push_back(r.end); }
    }
    std::vector<uint8_t> strs(1 << 16, 0);
    std::vector<int64_t> sdir(2 * 1024, 0);
    int64_t off = 0;
    for (size_t i = 0; i < in.strings.size(); ++i) {
        sdir[2 * i + 0] = off;
        sdir[2 * i + 1] = (int64_t) in.strings[i].size();
        for (unsigned char c : in.strings[i]) strs[off++] = c;
    }
    const int64_t root_id = ps[root].id();

    for (const auto & input : inputs) {
        for (int start : starts) {
            if (start > (int) input.size() + 1) continue;
            for (auto fl : flags) {
                common_peg_parse_context ctx(input, fl);
                const auto nr = arena.parse(root_id, ctx, (size_t) start);
                std::vector<int64_t> out(4, -1);
                ++cases;
                const int64_t gt = peg_parse2(root, sl(input), start, (fl & COMMON_PEG_PARSE_FLAG_LENIENT) ? 1 : 0,
                    sli(kind), sli(cfirst), sli(ccount), sli(children), sli(sidx), sli(mn), sli(mx),
                    slv(strs), sli(sdir), sli(rstart), sli(rcount), sli(rngs), sli(rnge), sli(neg),
                    sli(dstart), sli(dcount), sli(delims), sli(out));
                std::string c = label + " input=[" + input + "] start=" + std::to_string(start) +
                                " lenient=" + std::to_string((fl & COMMON_PEG_PARSE_FLAG_LENIENT) ? 1 : 0);
                ++values;
                if (gt != (int64_t) nr.type || out[1] != (int64_t) nr.start || out[2] != (int64_t) nr.end) {
                    bad("result", c + " native=(" + std::to_string((int) nr.type) + "," + std::to_string(nr.start) + "," +
                                    std::to_string(nr.end) + ") goose=(" + std::to_string(gt) + "," +
                                    std::to_string(out[1]) + "," + std::to_string(out[2]) + ")");
                }
            }
        }
    }
}

int main(void) {
    goose_init(0, 0);
    const std::vector<std::string> inputs = {
        "", "a", "ab", "abc", "aaa", "aaaa", "aab", "  x",
        "\"abc\"", "\"a\\\"b\"", "\"a\\\\b\"", "\"a\\u0041\"", "\"a\\u004Z\"", "\"a\\q\"", "\"a",
        "abcEND", "xENDy", "xEN", "aENDEND", "caf\xc3\xa9", "\xff", "\xc3",
    };
    const std::vector<int> starts = {0, 1, 2};
    const std::vector<common_peg_parse_flags> flags = {
        COMMON_PEG_PARSE_FLAG_NONE, COMMON_PEG_PARSE_FLAG_LENIENT,
    };

    run_tree({{K_LIT, {}, "a", {}, 0, 0}, {K_AND, {0}, "", {}, 0, 0}}, 1, "and lit", inputs, starts, flags);
    run_tree({{K_LIT, {}, "a", {}, 0, 0}, {K_NOT, {0}, "", {}, 0, 0}}, 1, "not lit", inputs, starts, flags);
    run_tree({{K_LIT, {}, "ab", {}, 0, 0}, {K_NOT, {0}, "", {}, 0, 0}}, 1, "not lit ab", inputs, starts, flags);
    run_tree({{K_LIT, {}, "a", {}, 0, 0}, {K_ATOMIC, {0}, "", {}, 0, 0}}, 1, "atomic lit", inputs, starts, flags);
    run_tree({{K_LIT, {}, "a", {}, 0, 0}, {K_GBNF, {0}, "g", {}, 0, 0}}, 1, "gbnf lit", inputs, starts, flags);
    run_tree({{K_ANY, {}, "", {}, 0, 0}, {K_AC, {0}, "", {"</a>"}, 0, 0}}, 1, "ac any", inputs, starts, flags);
    run_tree({{K_CHARS, {}, "a-z", {}, 1, -1}}, 0, "chars a-z+", inputs, starts, flags);
    run_tree({{K_CHARS, {}, "a-z", {}, 2, 3}}, 0, "chars a-z{2,3}", inputs, starts, flags);
    run_tree({{K_CHARS, {}, "a-z", {}, 0, -1}}, 0, "chars a-z*", inputs, starts, flags);
    run_tree({{K_CHARS, {}, "0-9", {}, 1, -1}}, 0, "chars digits+", inputs, starts, flags);
    run_tree({{K_CHARS, {}, "a-z", {}, 0, -1, true}}, 0, "chars not a-z*", inputs, starts, flags);
    run_tree({{K_CHARS, {}, "a-z", {}, 3, 2}}, 0, "chars min>max", inputs, starts, flags);
    run_tree({{K_STRING, {}, "\"", {}, 0, 0}}, 0, "string quote", inputs, starts, flags);
    run_tree({{K_UNTIL, {}, "END", {}, 0, 0}}, 0, "until END", inputs, starts, flags);
    run_tree({{K_UNTIL, {}, "END", {"STOP"}, 0, 0}}, 0, "until END/STOP", inputs, starts, flags);
    run_tree({{K_CHARS, {}, "a-z", {}, 1, -1}, {K_STRING, {}, "\"", {}, 0, 0}, {K_SEQ, {0, 1}, "", {}, 0, 0}},
             2, "chars + string", inputs, starts, flags);

    printf("FG483_PEGPARSE2 cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
