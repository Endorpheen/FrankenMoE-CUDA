// Differential fixture for the PEG matching engine (EXP482).
// Oracle: common_peg_arena::parse for literal/sequence/choice/repetition/any/space.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>

#include "pegparse-gen.h"
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

enum K { K_LIT = 3, K_SEQ = 4, K_CHOICE = 5, K_REP = 6, K_ANY = 9, K_SPACE = 10 };
struct Spec { int kind; std::vector<int> ch; std::string s; int mn = 0, mx = 0; };
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

static void run_tree(const std::vector<Spec> & specs, int root,
                     const std::vector<std::string> & inputs, const std::vector<int> & starts,
                     const std::vector<common_peg_parse_flags> & flags, const std::string & label) {
    common_peg_parser_builder b;
    std::vector<common_peg_parser> ps;
    for (const auto & s : specs) {
        common_peg_parser p = b.eps();
        switch (s.kind) {
            case K_LIT: p = b.literal(s.s); break;
            case K_SEQ: { auto q = b.sequence(); for (int c : s.ch) q += ps[c]; p = q; break; }
            case K_CHOICE: { auto q = b.choice(); for (int c : s.ch) q |= ps[c]; p = q; break; }
            case K_REP: p = b.repeat(ps[s.ch[0]], s.mn, s.mx); break;
            case K_ANY: p = b.any(); break;
            case K_SPACE: p = b.space(); break;
        }
        ps.push_back(p);
    }
    auto arena = b.build();

    Interner in;
    std::vector<int64_t> kind, cfirst, ccount, children, sidx, mn, mx;
    for (const auto & s : specs) {
        kind.push_back(s.kind);
        cfirst.push_back((int64_t) children.size());
        ccount.push_back((int64_t) s.ch.size());
        for (int c : s.ch) children.push_back(c);
        sidx.push_back(in.add(s.s));
        mn.push_back(s.mn);
        mx.push_back(s.mx);
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
                const int64_t gt = peg_parse(root, sl(input), start, (fl & COMMON_PEG_PARSE_FLAG_LENIENT) ? 1 : 0,
                    sli(kind), sli(cfirst), sli(ccount), sli(children), sli(sidx), sli(mn), sli(mx),
                    slv(strs), sli(sdir), sli(out));
                std::string c = label + " input=[" + input + "] start=" + std::to_string(start) +
                                " lenient=" + std::to_string((fl & COMMON_PEG_PARSE_FLAG_LENIENT) ? 1 : 0);
                ++values;
                if (gt != (int64_t) nr.type || out[1] != (int64_t) nr.start || out[2] != (int64_t) nr.end) {
                    bad("result", c + " native=(" + std::to_string((int) nr.type) + "," + std::to_string(nr.start) + "," +
                                    std::to_string(nr.end) + ") goose=(" + std::to_string(gt) + "," +
                                    std::to_string(out[1]) + "," + std::to_string(out[2]) + ")");
                }
                ++values;
                if (!nr.nodes.empty()) bad("nodes", c + " native nodes are not empty");
            }
        }
    }
}

int main(void) {
    goose_init(0, 0);
    const std::vector<std::string> inputs = {
        "", "a", "ab", "abc", "ac", "aaa", "aaaa", "aaaaa", "aab", "bab",
        "  x", " \t\nx", "\xc3\xa9", "\xc3\xa9" "a", "\xe4\xb8\xad",
        "\xff", "\xc3", "\xc3" "a", "a\xc3",
    };
    const std::vector<int> starts = {0, 1, 2, 3};
    const std::vector<common_peg_parse_flags> flags = {
        COMMON_PEG_PARSE_FLAG_NONE, COMMON_PEG_PARSE_FLAG_LENIENT,
    };

    run_tree({{K_LIT, {}, "abc", 0, 0}}, 0, inputs, starts, flags, "lit abc");
    run_tree({{K_LIT, {}, "a", 0, 0}}, 0, inputs, starts, flags, "lit a");
    run_tree({{K_LIT, {}, "a", 0, 0}, {K_LIT, {}, "b", 0, 0}, {K_SEQ, {0, 1}, "", 0, 0}},
             2, inputs, starts, flags, "seq a b");
    run_tree({{K_LIT, {}, "a", 0, 0}, {K_LIT, {}, "c", 0, 0}, {K_SEQ, {0, 1}, "", 0, 0}},
             2, inputs, starts, flags, "seq a c");
    run_tree({{K_LIT, {}, "a", 0, 0}, {K_LIT, {}, "b", 0, 0}, {K_CHOICE, {0, 1}, "", 0, 0}},
             2, inputs, starts, flags, "choice a b");
    run_tree({{K_LIT, {}, "a", 0, 0}, {K_REP, {0}, "", 0, -1}}, 1, inputs, starts, flags, "rep a*");
    run_tree({{K_LIT, {}, "a", 0, 0}, {K_REP, {0}, "", 2, -1}}, 1, inputs, starts, flags, "rep a{2,}");
    run_tree({{K_LIT, {}, "a", 0, 0}, {K_REP, {0}, "", 2, 3}}, 1, inputs, starts, flags, "rep a{2,3}");
    run_tree({{K_ANY, {}, "", 0, 0}}, 0, inputs, starts, flags, "any");
    run_tree({{K_ANY, {}, "", 0, 0}, {K_REP, {0}, "", 1, -1}}, 1, inputs, starts, flags, "rep any+");
    run_tree({{K_SPACE, {}, "", 0, 0}}, 0, inputs, starts, flags, "space");
    run_tree({{K_SPACE, {}, "", 0, 0}, {K_LIT, {}, "x", 0, 0}, {K_SEQ, {0, 1}, "", 0, 0}},
             2, inputs, starts, flags, "space x");
    run_tree({{K_LIT, {}, "a", 0, 0}, {K_LIT, {}, "b", 0, 0}, {K_SEQ, {0, 1}, "", 0, 0},
              {K_LIT, {}, "a", 0, 0}, {K_LIT, {}, "c", 0, 0}, {K_SEQ, {3, 4}, "", 0, 0},
              {K_CHOICE, {2, 5}, "", 0, 0}}, 6, inputs, starts, flags, "choice (a b) (a c)");
    run_tree({{K_LIT, {}, "a", 0, 0}, {K_LIT, {}, "b", 0, 0}, {K_CHOICE, {0, 1}, "", 0, 0},
              {K_REP, {2}, "", 0, -1}}, 3, inputs, starts, flags, "rep (a|b)*");
    run_tree({{K_LIT, {}, "a", 0, 0}, {K_ANY, {}, "", 0, 0}, {K_SEQ, {0, 1}, "", 0, 0},
              {K_REP, {2}, "", 0, -1}}, 3, inputs, starts, flags, "rep (a .)*");

    printf("FG482_PEGPARSE cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
