// Deep and large corpus for the remaining PEG variants (EXP501).
// Oracle: common_peg_arena::parse; subject:
// experiments/EXP-2026-11-13-483-p7-09-peg-parse2/source/pegparse2.goose.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <functional>

#include "peg2-gen.h"
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

static void check(common_peg_arena & arena, size_t pid,
                  const std::vector<std::string> & inputs, const std::string & label) {

    // encode the tree from the built arena: the kinds under test plus any/seq/rep
    std::map<size_t, int> idx;
    std::vector<int64_t> kind, cf, cc, child, sidx, mn, mx, rstart, rcount, rngs, rnge, neg,
        dstart, dcount, delims;
    std::map<std::string, int> sidmap;
    std::vector<std::string> sstr;
    auto sid = [&](const std::string & s) {
        auto it = sidmap.find(s);
        if (it != sidmap.end()) return it->second;
        int i = (int) sstr.size(); sstr.push_back(s); sidmap[s] = i; return i;
    };

    std::function<int(size_t)> enc = [&](size_t id) -> int {
        auto it = idx.find(id);
        if (it != idx.end()) return it->second;
        const int me = (int) kind.size();
        idx[id] = me;
        kind.push_back(-1); cf.push_back(0); cc.push_back(0); sidx.push_back(0);
        mn.push_back(0); mx.push_back(0); rstart.push_back(0); rcount.push_back(0);
        neg.push_back(0); dstart.push_back(0); dcount.push_back(0);
        std::visit([&](auto & p) {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, common_peg_literal_parser>) {
                kind[me] = 3; sidx[me] = sid(p.literal);
            } else if constexpr (std::is_same_v<T, common_peg_sequence_parser>) {
                kind[me] = 4;
                std::vector<int> tmp; for (auto c : p.children) tmp.push_back(enc(c));
                cf[me] = (int64_t) child.size();
                for (int c : tmp) child.push_back(c);
                cc[me] = (int64_t) tmp.size();
            } else if constexpr (std::is_same_v<T, common_peg_repetition_parser>) {
                kind[me] = 6; cf[me] = (int64_t) child.size(); child.push_back(enc(p.child)); cc[me] = 1;
                mn[me] = p.min_count; mx[me] = p.max_count;
            } else if constexpr (std::is_same_v<T, common_peg_any_parser>) {
                kind[me] = 9;
            } else if constexpr (std::is_same_v<T, common_peg_space_parser>) {
                kind[me] = 10;
            } else if constexpr (std::is_same_v<T, common_peg_chars_parser>) {
                kind[me] = 11; sidx[me] = sid(p.pattern);
                rstart[me] = (int64_t) rngs.size();
                for (const auto & r : p.ranges) { rngs.push_back(r.start); rnge.push_back(r.end); }
                rcount[me] = (int64_t) p.ranges.size();
                neg[me] = p.negated ? 1 : 0;
                mn[me] = p.min_count; mx[me] = p.max_count;
            } else if constexpr (std::is_same_v<T, common_peg_string_parser>) {
                kind[me] = 12; sidx[me] = sid(std::string(1, p.delimiter));
            } else if constexpr (std::is_same_v<T, common_peg_until_parser>) {
                kind[me] = 13;
                dstart[me] = (int64_t) delims.size();
                for (const auto & d : p.delimiters) delims.push_back(sid(d));
                dcount[me] = (int64_t) p.delimiters.size();
            } else if constexpr (std::is_same_v<T, common_peg_ac_parser>) {
                kind[me] = 19; cf[me] = (int64_t) child.size(); child.push_back(enc(p.child)); cc[me] = 1;
                dstart[me] = (int64_t) delims.size();
                for (const auto & d : p.delimiters) delims.push_back(sid(d));
                dcount[me] = (int64_t) p.delimiters.size();
            } else if constexpr (std::is_same_v<T, common_peg_gbnf_parser>) {
                kind[me] = 18; cf[me] = (int64_t) child.size(); child.push_back(enc(p.child)); cc[me] = 1;
                sidx[me] = sid(p.grammar);
            } else if constexpr (std::is_same_v<T, common_peg_atomic_parser>) {
                kind[me] = 16; cf[me] = (int64_t) child.size(); child.push_back(enc(p.child)); cc[me] = 1;
            } else {
                kind[me] = 99;
            }
        }, arena.get(id));
        return me;
    };
    const int groot = enc(pid);
    if (kind[(size_t) groot] == 99) { bad("unsupported kind", label); return; }

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
        common_peg_parse_context ctx(input, COMMON_PEG_PARSE_FLAG_NONE);
        const auto nr = arena.parse(pid, ctx, 0);
        std::vector<int64_t> out(4, -1);
        ++cases;
        const int64_t g = peg_parse2(groot, sl(input), 0, 0, sli(kind), sli(cf), sli(cc), sli(child),
            sli(sidx), sli(mn), sli(mx), slv(strs), sli(sdir), sli(rstart), sli(rcount), sli(rngs), sli(rnge),
            sli(neg), sli(dstart), sli(dcount), sli(delims), sli(out));
        const std::string c = label + " input len=" + std::to_string(input.size());
        ++values;
        if (g != (int64_t) nr.type || out[1] != (int64_t) nr.start || out[2] != (int64_t) nr.end) {
            bad("result", c + " native=(" + std::to_string((int) nr.type) + "," + std::to_string(nr.start) + "," +
                            std::to_string(nr.end) + ") goose=(" + std::to_string(g) + "," +
                            std::to_string(out[1]) + "," + std::to_string(out[2]) + ")");
        }
    }
}

int main(void) {
    goose_init(0, 0);

    { // until with 200 delimiters over a 200k input
        common_peg_parser_builder b;
        std::vector<std::string> dl;
        for (int i = 0; i < 200; ++i) dl.push_back("</d" + std::to_string(i) + ">");
        common_peg_parser p = b.until("</d199>");
        b.set_root(p);
        auto arena = b.build();
        // the builder takes one delimiter; widen the built parser to 200
        auto & up = std::get<common_peg_until_parser>(arena.get(p.id()));
        up.delimiters = dl;
        check(arena, p.id(), {std::string(200000, 'x'), std::string(200000, 'x') + "</d199>", ""}, "until 200 delims x200k");
    }
    { // chars over 200k
        common_peg_parser_builder b;
        common_peg_parser p = b.chars("a-z", 0, -1);
        b.set_root(p);
        auto arena = b.build();
        check(arena, p.id(), {std::string(200000, 'a'), std::string(200000, 'a') + "1", ""}, "chars a-z x200k");
    }
    { // a 100k string content
        common_peg_parser_builder b;
        common_peg_parser p = b.string_content('"');
        b.set_root(p);
        std::string inner(100000, 'q');
        auto arena = b.build();
        check(arena, p.id(), {"\"" + inner + "\"", "\"" + inner, "\"" + std::string(100000, '\\') + "\""}, "string 100k");
    }
    { // ac over 200k
        common_peg_parser_builder b;
        std::vector<std::string> dl = {"</a>", "</b>"};
        common_peg_parser p = b.ac(b.any(), dl);
        b.set_root(p);
        auto arena = b.build();
        check(arena, p.id(), {std::string(200000, 'x'), std::string(200000, 'x') + "</b>"}, "ac any x200k");
    }

    printf("FG501_PEG2 cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
