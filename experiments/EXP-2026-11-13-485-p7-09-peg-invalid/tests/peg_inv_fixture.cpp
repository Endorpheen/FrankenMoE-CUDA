// Differential fixture for PEG schema passthrough and invalid-UTF-8 tracking
// (EXP485). Oracle: common_peg_arena::parse + common_peg_ast_arena.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <functional>

#include "peginv-gen.h"
#include "peg-parser.h"
#include "json-schema.h"

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

struct GNode { int kind; std::vector<int> ch; std::string s; std::vector<std::string> delims; int mn = 0, mx = 0; };
struct CaseBuilder {
    common_peg_parser_builder b;
    common_peg_parser root;
    CaseBuilder() : b(), root(b.eps()) {}
};

static void check(CaseBuilder & cb, const std::string & label,
                  const std::vector<std::string> & inputs, const std::vector<int> & starts,
                  const std::vector<common_peg_parse_flags> & flags) {
    auto arena = cb.b.build();
    const size_t root_id = cb.root.id();

    std::vector<GNode> g;
    std::map<size_t, int> seen;
    std::function<int(size_t)> enc = [&](size_t id) -> int {
        auto it = seen.find(id);
        if (it != seen.end()) return it->second;
        int idx = (int) g.size();
        seen[id] = idx;
        g.push_back({0, {}, "", {}, 0, 0});
        std::visit([&](auto & p) {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, common_peg_literal_parser>) { g[idx].kind = 3; g[idx].s = p.literal; }
            else if constexpr (std::is_same_v<T, common_peg_sequence_parser>) {
                g[idx].kind = 4;
                for (auto c : p.children) { int ci = enc(c); g[idx].ch.push_back(ci); }
            } else if constexpr (std::is_same_v<T, common_peg_rule_parser>) { g[idx].kind = 14; g[idx].s = p.name; g[idx].ch = {enc(p.child)}; }
            else if constexpr (std::is_same_v<T, common_peg_tag_parser>) { g[idx].kind = 17; g[idx].s = p.tag; g[idx].ch = {enc(p.child)}; }
            else if constexpr (std::is_same_v<T, common_peg_until_parser>) { g[idx].kind = 13; g[idx].delims = p.delimiters; }
            else if constexpr (std::is_same_v<T, common_peg_schema_parser>) { g[idx].kind = 20; g[idx].ch = {enc(p.child)}; }
            else if constexpr (std::is_same_v<T, common_peg_ref_parser>) { g[idx].kind = 98; g[idx].s = p.name; }
            else { g[idx].kind = 99; }
        }, arena.get(id));
        return idx;
    };
    int groot = enc(root_id);
    if (g[groot].kind == 98) groot = enc(arena.get_rule(g[groot].s));
    if (g[groot].kind == 99 || g[groot].kind == 98) { bad("unsupported kind", label); return; }

    std::map<std::string, int> sidx;
    std::vector<std::string> sstr;
    auto sid = [&](const std::string & s) {
        auto it = sidx.find(s);
        if (it != sidx.end()) return it->second;
        int i = (int) sstr.size(); sstr.push_back(s); sidx[s] = i; return i;
    };
    std::vector<int64_t> kind, cfirst, ccount, children, sidxv, dstart, dcount, delims;
    for (auto & n : g) {
        kind.push_back(n.kind);
        cfirst.push_back((int64_t) children.size());
        ccount.push_back((int64_t) n.ch.size());
        for (int c : n.ch) children.push_back(c);
        sidxv.push_back(sid(n.s));
        dstart.push_back((int64_t) delims.size());
        dcount.push_back((int64_t) n.delims.size());
        for (const auto & d : n.delims) delims.push_back(sid(d));
    }
    std::vector<uint8_t> strs(1 << 16, 0);
    std::vector<int64_t> sdir(2 * 1024, 0);
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
                std::vector<int64_t> a_rule(512, -1), a_tag(512, -1), a_start(512, 0), a_end(512, 0),
                    a_toff(512, 0), a_tlen(512, 0), a_cs(512, 0), a_cc(512, 0), a_partial(512, 0),
                    a_is(512, 0), a_ic(512, 0), achild(4096, 0), nres(4096, 0), invp(4096, 0), aninv(4096, 0),
                    st(16, 0), out(8, -1);
                ++cases;
                const int64_t gt = peg_parse_ast2(groot, sl(input), start, (fl & COMMON_PEG_PARSE_FLAG_LENIENT) ? 1 : 0,
                    sli(kind), sli(cfirst), sli(ccount), sli(children), sli(sidxv), slv(strs), sli(sdir),
                    sli(dstart), sli(dcount), sli(delims),
                    sli(a_rule), sli(a_tag), sli(a_start), sli(a_end), sli(a_toff), sli(a_tlen), sli(a_cs), sli(a_cc),
                    sli(a_partial), sli(a_is), sli(a_ic), sli(achild), sli(nres), sli(invp), sli(aninv), sli(st), sli(out));
                std::string c = label + " input=[" + input + "] start=" + std::to_string(start) +
                                " lenient=" + std::to_string((fl & COMMON_PEG_PARSE_FLAG_LENIENT) ? 1 : 0);
                ++values;
                if (gt != (int64_t) nr.type || out[1] != (int64_t) nr.start || out[2] != (int64_t) nr.end) {
                    bad("result", c + " native=(" + std::to_string((int) nr.type) + "," + std::to_string(nr.start) + "," +
                                    std::to_string(nr.end) + ") goose=(" + std::to_string(gt) + "," +
                                    std::to_string(out[1]) + "," + std::to_string(out[2]) + ")");
                    continue;
                }
                ++values;
                if ((int64_t) nr.invalid_utf8.size() != out[6]) {
                    bad("result invalid count", c + " native=" + std::to_string(nr.invalid_utf8.size()) + " goose=" + std::to_string(out[6]));
                    continue;
                }
                bool ok = true;
                for (int64_t i = 0; i < out[6] && ok; ++i) {
                    ++values;
                    if (nr.invalid_utf8[i].pos != (size_t) invp[(out[5] + i) * 2 + 0] ||
                        nr.invalid_utf8[i].len != (size_t) invp[(out[5] + i) * 2 + 1]) {
                        bad("result invalid run", c);
                        ok = false;
                    }
                }
                if (!ok) continue;
                ++values;
                if ((int64_t) ctx.ast.size() != st[0]) { bad("node count", c); continue; }
                for (int64_t i = 0; i < st[0] && ok; ++i) {
                    const auto & nn = ctx.ast.get(i);
                    ++values;
                    if (nn.rule != (a_rule[i] >= 0 ? sstr[a_rule[i]] : std::string()) ||
                        nn.tag != (a_tag[i] >= 0 ? sstr[a_tag[i]] : std::string())) { bad("node names", c); ok = false; break; }
                    ++values;
                    if (nn.start != (size_t) a_start[i] || nn.end != (size_t) a_end[i]) { bad("node span", c); ok = false; break; }
                    ++values;
                    if ((int64_t) nn.invalid_utf8.size() != a_ic[i]) { bad("node invalid count", c + " node " + std::to_string(i)); ok = false; break; }
                    for (int64_t k = 0; k < a_ic[i]; ++k) {
                        ++values;
                        if (nn.invalid_utf8[k].pos != (size_t) aninv[(a_is[i] + k) * 2 + 0] ||
                            nn.invalid_utf8[k].len != (size_t) aninv[(a_is[i] + k) * 2 + 1]) {
                            bad("node invalid run", c + " node " + std::to_string(i));
                            ok = false;
                            break;
                        }
                    }
                }
            }
        }
    }
}

int main(void) {
    goose_init(0, 0);
    const std::vector<std::string> inputs = {
        "", "abcEND", "a\xff" "bEND", "\xff\xfe", "a\xc3", "\xc3" "a", "abcENDmore", "END", "xEN", "\xe4\xb8", "\xe4\xb8" "X",
    };
    const std::vector<int> starts = {0, 1, 2};
    const std::vector<common_peg_parse_flags> flags = {
        COMMON_PEG_PARSE_FLAG_NONE, COMMON_PEG_PARSE_FLAG_LENIENT,
    };

    { // tag over until
        CaseBuilder cb;
        cb.root = cb.b.tag("t", cb.b.until("END"));
        check(cb, "tag until", inputs, starts, flags);
    }
    { // rule under tag over until
        CaseBuilder cb;
        auto r = cb.b.rule("main", cb.b.until("END"));
        cb.root = cb.b.tag("outer", r);
        check(cb, "tag rule until", inputs, starts, flags);
    }
    { // sequence: literal then until
        CaseBuilder cb;
        auto seq = cb.b.sequence();
        seq += cb.b.literal("a");
        seq += cb.b.until("END");
        cb.root = cb.b.tag("t", seq);
        check(cb, "lit until", inputs, starts, flags);
    }
    { // until inside a rule inside a tag
        CaseBuilder cb;
        auto r = cb.b.rule("main", cb.b.until("END"));
        cb.root = cb.b.tag("t", r);
        check(cb, "tag rule until", inputs, starts, flags);
    }
    { // until then a failing literal: the failure drops the invalid runs
        CaseBuilder cb;
        auto seq = cb.b.sequence();
        seq += cb.b.until("END");
        seq += cb.b.literal("Z");
        cb.root = cb.b.tag("t", seq);
        check(cb, "until then fail", inputs, starts, flags);
    }
    { // the failing sequence itself is the root: no wrapper masks its invalid list
        CaseBuilder cb;
        auto seq = cb.b.sequence();
        seq += cb.b.until("END");
        seq += cb.b.literal("Z");
        cb.root = seq;
        check(cb, "root until then fail", inputs, starts, flags);
    }
    { // schema passthrough over a literal
        CaseBuilder cb;
        auto s = cb.b.schema(cb.b.literal("a"), "s", common_json::parse("{\"type\":\"string\"}"));
        cb.root = cb.b.tag("t", s);
        check(cb, "schema lit", inputs, starts, flags);
    }
    { // schema over until
        CaseBuilder cb;
        auto s = cb.b.schema(cb.b.until("END"), "s", common_json::parse("{\"type\":\"string\"}"));
        cb.root = cb.b.tag("t", s);
        check(cb, "schema until", inputs, starts, flags);
    }

    printf("FG485_PEGINV cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
