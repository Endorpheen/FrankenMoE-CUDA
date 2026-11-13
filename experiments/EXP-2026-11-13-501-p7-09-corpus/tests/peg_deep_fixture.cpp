// Deep and large corpus for the PEG matching engine (EXP501).
// Oracle: common_peg_arena::parse; subject:
// experiments/EXP-2026-11-13-482-p7-09-peg-parse/source/pegparse.goose.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>

#include "peg1-gen.h"
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

struct Spec { int kind; std::vector<int> ch; std::string s; int mn = 0, mx = 0; };
enum { K_LIT = 3, K_SEQ = 4, K_CHOICE = 5, K_REP = 6, K_ANY = 9, K_SPACE = 10 };

struct CaseBuilder {
    common_peg_parser_builder b;
    common_peg_parser root;
    CaseBuilder() : b(), root(b.eps()) {}
};

static void run(CaseBuilder & cb, const std::vector<Spec> & specs, const std::string & label,
                const std::vector<std::string> & inputs, const std::vector<int> & starts,
                const std::vector<common_peg_parse_flags> & flags) {
    // rebuild the tree through the shared builder is not possible after build(),
    // so the caller passes a builder whose tree is the specs; encode and compare
    auto arena = cb.b.build();
    const size_t root_id = cb.root.id();

    std::map<std::string, int> sidx;
    std::vector<std::string> sstr;
    auto sid = [&](const std::string & s) {
        auto it = sidx.find(s);
        if (it != sidx.end()) return it->second;
        int i = (int) sstr.size(); sstr.push_back(s); sidx[s] = i; return i;
    };
    std::vector<int64_t> kind, cfirst, ccount, children, sidxv, mn, mx;
    for (const auto & s : specs) {
        kind.push_back(s.kind);
        cfirst.push_back((int64_t) children.size());
        ccount.push_back((int64_t) s.ch.size());
        for (int c : s.ch) children.push_back(c);
        sidxv.push_back(sid(s.s));
        mn.push_back(s.mn);
        mx.push_back(s.mx);
    }
    size_t total = 1;
    for (const auto & s : sstr) total += s.size();
    std::vector<uint8_t> strs(total, 0);
    std::vector<int64_t> sdir(2 * sstr.size() + 2, 0);
    int64_t off = 0;
    for (size_t i = 0; i < sstr.size(); ++i) {
        sdir[2 * i + 0] = off;
        sdir[2 * i + 1] = (int64_t) sstr[i].size();
        for (unsigned char c : sstr[i]) strs[off++] = c;
    }

    for (const auto & input : inputs) {
        for (int start : starts) {
            if (start > (int) input.size() + 1) continue;
            for (auto fl : flags) {
                common_peg_parse_context ctx(input, fl);
                const auto nr = arena.parse(root_id, ctx, (size_t) start);
                std::vector<int64_t> out2(4, -1);
                ++cases;
                const int64_t rootidx = (int64_t) specs.size() - 1;
                const int64_t g2 = peg_parse(rootidx, sl(input), start,
                    (fl & COMMON_PEG_PARSE_FLAG_LENIENT) ? 1 : 0,
                    sli(kind), sli(cfirst), sli(ccount), sli(children), sli(sidxv), sli(mn), sli(mx),
                    slv(strs), sli(sdir), sli(out2));
                std::string c = label + " input len=" + std::to_string(input.size()) +
                                " start=" + std::to_string(start) +
                                " lenient=" + std::to_string((fl & COMMON_PEG_PARSE_FLAG_LENIENT) ? 1 : 0);
                ++values;
                if (g2 != (int64_t) nr.type || out2[1] != (int64_t) nr.start || out2[2] != (int64_t) nr.end) {
                    bad("result", c + " native=(" + std::to_string((int) nr.type) + "," + std::to_string(nr.start) + "," +
                                    std::to_string(nr.end) + ") goose=(" + std::to_string(g2) + "," +
                                    std::to_string(out2[1]) + "," + std::to_string(out2[2]) + ")");
                }
            }
        }
    }
}

int main(void) {
    goose_init(0, 0);
    const std::vector<common_peg_parse_flags> flags = { COMMON_PEG_PARSE_FLAG_NONE, COMMON_PEG_PARSE_FLAG_LENIENT };
    const std::vector<int> starts = { 0 };

    { // 600 nested repetitions of any: the executor recurses per level
        CaseBuilder cb;
        std::vector<Spec> specs = {{K_ANY, {}, "", 0, 0}};
        common_peg_parser p = cb.b.any();
        for (int i = 0; i < 600; ++i) {
            p = cb.b.repeat(p, 1, 1);
            specs.push_back({K_REP, {i}, "", 1, 1});
        }
        cb.root = p;
        run(cb, specs, "deep 600 repeats", {std::string(600, 'x'), std::string(599, 'x'), ""}, starts, flags);
    }
    { // a large repetition over 200000 bytes
        CaseBuilder cb;
        std::vector<Spec> specs = {{K_ANY, {}, "", 0, 0}, {K_REP, {0}, "", 0, -1}};
        cb.root = cb.b.repeat(cb.b.any(), 0, -1);
        run(cb, specs, "rep any x200k", {std::string(200000, 'a'), std::string(1, 'a'), ""}, starts, flags);
    }
    { // a 100000-byte literal
        CaseBuilder cb;
        const std::string lit(100000, 'a');
        std::vector<Spec> specs = {{K_LIT, {}, lit, 0, 0}};
        cb.root = cb.b.literal(lit);
        run(cb, specs, "100k literal", {lit, std::string(99999, 'a'), std::string(100001, 'a')}, starts, flags);
    }
    { // a 50000-byte literal followed by a repetition
        CaseBuilder cb;
        const std::string lit(50000, 'b');
        std::vector<Spec> specs = {{K_LIT, {}, lit, 0, 0}, {K_ANY, {}, "", 0, 0}, {K_REP, {1}, "", 0, -1},
                                   {K_SEQ, {0, 2}, "", 0, 0}};
        auto seq = cb.b.sequence();
        seq += cb.b.literal(lit);
        seq += cb.b.repeat(cb.b.any(), 0, -1);
        cb.root = seq;
        run(cb, specs, "long lit + star", {lit + std::string(50000, 'c'), lit}, starts, flags);
    }
    { // 400 alternatives
        CaseBuilder cb;
        std::vector<Spec> specs;
        for (int i = 0; i < 400; ++i) specs.push_back({K_LIT, {}, std::string(1, (char) ('a' + i % 26)), 0, 0});
        std::vector<int> all;
        for (int i = 0; i < 400; ++i) all.push_back(i);
        specs.push_back({K_CHOICE, all, "", 0, 0});
        auto c = cb.b.choice();
        for (int i = 0; i < 400; ++i) c |= cb.b.literal(std::string(1, (char) ('a' + i % 26)));
        cb.root = c;
        run(cb, specs, "400 alternatives", {"a", "z", "1"}, starts, flags);
    }

    printf("FG501_PEG cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
