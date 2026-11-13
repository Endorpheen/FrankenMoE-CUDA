// Differential fixture for the PEG AST helpers (EXP497).
// Oracle: common_peg_ast_arena::find_by_tag / find_by_rule / visit and
// common_peg_ast_node::sanitized_text from common/peg-parser.cpp.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <algorithm>

#include "asthelp-gen.h"
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

struct Interner {
    std::vector<std::string> strs;
    std::map<std::string, int> idx;
    int sid(const std::string & s) {
        auto it = idx.find(s);
        if (it != idx.end()) return it->second;
        int i = (int) strs.size();
        strs.push_back(s);
        idx[s] = i;
        return i;
    }
};

struct CaseBuilder {
    common_peg_parser_builder b;
    common_peg_parser root;
    CaseBuilder() : b(), root(b.eps()) {}
};

static void run(CaseBuilder & cb, const std::string & label, const std::vector<std::string> & inputs) {
    auto arena = cb.b.build();
    const size_t root_id = cb.root.id();

    for (const auto & input : inputs) {
        common_peg_parse_context ctx(input, COMMON_PEG_PARSE_FLAG_NONE);
        const auto res = arena.parse(root_id, ctx, 0);
        ++cases;
        if (res.fail()) {
            ++values;
            // a rejected input still exercises the type name
            continue;
        }
        const auto & ast = ctx.ast;
        const size_t n = ast.size();

        Interner in;
        std::vector<uint8_t> strs;
        std::vector<int64_t> sdir;

        std::vector<int64_t> a_rule(n, -1), a_tag(n, -1), a_start(n, 0), a_end(n, 0),
            a_toff(n, 0), a_tlen(n, 0), a_cs(n, 0), a_cc(n, 0), a_is(n, 0), a_ic(n, 0);
        std::vector<int64_t> achild, inv;
        for (size_t i = 0; i < n; ++i) {
            const auto & node = ast.get(i);
            a_rule[i] = in.sid(node.rule);
            a_tag[i]  = in.sid(node.tag);
            a_start[i] = (int64_t) node.start;
            a_end[i] = (int64_t) node.end;
            if (node.start < input.size()) {
                a_toff[i] = (int64_t) node.start;
                a_tlen[i] = (int64_t) (node.end - node.start);
            }
            a_cs[i] = (int64_t) achild.size();
            for (auto c : node.children) achild.push_back((int64_t) c);
            a_cc[i] = (int64_t) achild.size() - a_cs[i];
            a_is[i] = (int64_t) inv.size() / 2;
            for (const auto & r : node.invalid_utf8) {
                inv.push_back((int64_t) r.pos);
                inv.push_back((int64_t) r.len);
            }
            a_ic[i] = (int64_t) inv.size() / 2 - a_is[i];
        }
        for (size_t i = 0; i < in.strs.size(); ++i) {
            sdir.push_back((int64_t) strs.size());
            sdir.push_back((int64_t) in.strs[i].size());
            for (unsigned char c : in.strs[i]) strs.push_back(c);
        }

        // unique rule and tag names present in the tree, plus a miss
        std::vector<std::string> tags, rules;
        for (size_t i = 0; i < n; ++i) {
            const auto & node = ast.get(i);
            if (std::find(tags.begin(), tags.end(), node.tag) == tags.end()) tags.push_back(node.tag);
            if (std::find(rules.begin(), rules.end(), node.rule) == rules.end()) rules.push_back(node.rule);
        }
        tags.push_back("no-such-tag");
        rules.push_back("no-such-rule");

        const std::vector<int> depths = {0, 1, 2, 3, 5};
        for (size_t parent = 0; parent < n; ++parent) {
            const auto & pnode = ast.get(parent);
            for (const auto & tag : tags) {
                for (int d : depths) {
                    const size_t want = ast.find_by_tag(pnode, tag, d);
                    const int64_t got = ast_find((int64_t) parent, 0,
                        sl_u8{ (uint8_t *) tag.data(), (int64_t) tag.size() }, (int64_t) d,
                        sli(a_tag), sli(a_rule), sli(a_cs), sli(a_cc), sli(achild), slv(strs), sli(sdir));
                    ++values;
                    if (got != (int64_t) want) {
                        bad("find_by_tag", label + " input=[" + input + "] parent " + std::to_string(parent) +
                            " tag=[" + tag + "] depth " + std::to_string(d) +
                            " goose=" + std::to_string(got) + " native=" + std::to_string(want));
                    }
                }
            }
            for (const auto & rule : rules) {
                for (int d : depths) {
                    const size_t want = ast.find_by_rule(pnode, rule, d);
                    const int64_t got = ast_find((int64_t) parent, 1,
                        sl_u8{ (uint8_t *) rule.data(), (int64_t) rule.size() }, (int64_t) d,
                        sli(a_tag), sli(a_rule), sli(a_cs), sli(a_cc), sli(achild), slv(strs), sli(sdir));
                    ++values;
                    if (got != (int64_t) want) {
                        bad("find_by_rule", label + " input=[" + input + "] parent " + std::to_string(parent) +
                            " rule=[" + rule + "] depth " + std::to_string(d) +
                            " goose=" + std::to_string(got) + " native=" + std::to_string(want));
                    }
                }
            }

            // visit from this node
            std::vector<size_t> want_order;
            ast.visit((common_peg_ast_id) parent, [&](const common_peg_ast_node & node) { want_order.push_back(node.id); });
            std::vector<int64_t> got_order(n + 1, -1), st(8, 0);
            const int64_t cnt = ast_visit((int64_t) parent, sli(a_cs), sli(a_cc), sli(achild), sli(got_order), sli(st));
            ++values;
            if (cnt != (int64_t) want_order.size()) {
                bad("visit count", label + " input=[" + input + "] parent " + std::to_string(parent) +
                    " goose=" + std::to_string(cnt) + " native=" + std::to_string(want_order.size()));
            } else {
                for (int64_t k = 0; k < cnt; ++k) {
                    ++values;
                    if (got_order[k] != (int64_t) want_order[(size_t) k]) {
                        bad("visit order", label + " input=[" + input + "] parent " + std::to_string(parent) +
                            " index " + std::to_string(k));
                        break;
                    }
                }
            }
        }

        // sanitized text for every node
        for (size_t i = 0; i < n; ++i) {
            const std::string want = ast.get(i).sanitized_text();
            std::vector<uint8_t> out(1 << 16, 0);
            const int64_t len = ast_sanitized((int64_t) i, sl_u8{ (uint8_t *) input.data(), (int64_t) input.size() },
                sli(a_toff), sli(a_tlen), sli(a_is), sli(a_ic), sli(inv), slv(out));
            const std::string got((const char *) out.data(), (size_t) len);
            ++values;
            if (got != want) {
                bad("sanitized_text", label + " node " + std::to_string(i) +
                    " is=" + std::to_string(a_is[i]) + " ic=" + std::to_string(a_ic[i]) +
                    " inv=" + std::to_string(inv.size()) + " off=" + std::to_string(a_toff[i]) +
                    " len=" + std::to_string(a_tlen[i]) +
                    " got=[" + got + "] want=[" + want + "]");
            }
        }
    }
}

int main(void) {
    goose_init(0, 0);

    // the result-type names
    {
        std::vector<uint8_t> out(64, 0);
        for (int t = 0; t < 5; ++t) {
            const int64_t n = result_type_name(t, slv(out));
            const std::string got((const char *) out.data(), (size_t) n);
            const std::string want = common_peg_parse_result_type_name((common_peg_parse_result_type) t);
            ++cases; ++values;
            if (got != want) bad("result_type_name", std::to_string(t) + " got=[" + got + "] want=[" + want + "]");
        }
    }

    const std::vector<std::string> inputs = {
        "", "abc", "aENDb", "\xff", "a\xff" "b", "\xc3", "\xc3" "a", "caf\xc3\xa9",
        "\xe4\xb8" "x", "END",
    };

    { // a tag over a literal
        CaseBuilder cb;
        cb.root = cb.b.tag("t", cb.b.literal("abc"));
        run(cb, "tag literal", inputs);
    }
    { // tag over a rule over until, which records invalid runs
        CaseBuilder cb;
        auto r = cb.b.rule("main", cb.b.until("END"));
        cb.root = cb.b.tag("outer", r);
        run(cb, "tag rule until", inputs);
    }
    { // nested tags three deep
        CaseBuilder cb;
        cb.root = cb.b.tag("l1", cb.b.tag("l2", cb.b.tag("l3", cb.b.until("END"))));
        run(cb, "nested tags", inputs);
    }
    { // a repetition of tags
        CaseBuilder cb;
        auto t = cb.b.tag("chunk", cb.b.literal("a"));
        auto rep = cb.b.repeat(t, 0, -1);
        auto r = cb.b.rule("list", rep);
        cb.root = cb.b.tag("all", r);
        run(cb, "repetition of tags", inputs);
    }
    { // sequence of two tags inside a rule
        CaseBuilder cb;
        auto seq = cb.b.sequence();
        seq += cb.b.tag("x", cb.b.literal("a"));
        seq += cb.b.tag("y", cb.b.until("END"));
        auto r = cb.b.rule("pair", seq);
        cb.root = cb.b.tag("both", r);
        run(cb, "two tags in a rule", inputs);
    }

    printf("FG497_ASTHELP cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
