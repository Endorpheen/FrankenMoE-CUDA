// Host shift + Goose advance versus the native accept flow.
#include "../../../vendor/flint/src/llama-grammar.cpp"
#include <cstdio>
#include <vector>
typedef struct { void * data; int64_t len; } gg_slice;
extern "C" int64_t g_advance_call(gg_slice et, gg_slice ev, gg_slice ro, gg_slice rl,
                                  gg_slice seed, int64_t seedd, gg_slice wp, gg_slice wpd,
                                  gg_slice out, gg_slice outd, int64_t base,
                                  int64_t maxs, int64_t maxd);
static int fails = 0, cases = 0;
static void run(const char * gbnf, uint32_t chr) {
    llama_grammar_parser parser;
    if (!parser.parse(gbnf)) { fails++; return; }
    const llama_grammar_rules & rules = parser.rules;
    llama_grammar_stack st; st.push_back(rules[0].data());
    llama_grammar_stacks stacks; llama_grammar_advance_stack(rules, st, stacks);

    // native: shift + advance per stack
    llama_grammar_stacks want;
    for (auto & s : stacks) {
        llama_grammar_stack ns(s.begin(), s.end());
        if (ns.empty()) continue;
        auto m = llama_grammar_match_char(ns.back(), chr);
        if (!m.first) continue;
        ns.pop_back();
        if (!llama_grammar_is_end_of_sequence(m.second)) ns.push_back(m.second);
        llama_grammar_advance_stack(rules, ns, want);
    }

    // split: host shift, then Goose advance on the shifted stack
    std::vector<int64_t> et, ev, ro(rules.size()), rl(rules.size());
    for (size_t r = 0; r < rules.size(); ++r) { ro[r]=(int64_t)et.size(); rl[r]=(int64_t)rules[r].size();
        for (auto &e : rules[r]) { et.push_back((int64_t)e.type); ev.push_back((int64_t)e.value); } }
    const int64_t maxd = 8, maxs = 8;
    std::vector<int64_t> got;
    std::vector<int64_t> gotd;
    for (auto & s : stacks) {
        if (s.empty()) continue;
        auto m = llama_grammar_match_char(s.back(), chr);
        if (!m.first) continue;
        std::vector<int64_t> shifted;
        for (size_t i = 0; i + 1 < s.size(); ++i) shifted.push_back((int64_t)(s[i] - rules[0].data()));
        if (!llama_grammar_is_end_of_sequence(m.second)) shifted.push_back((int64_t)(m.second - rules[0].data()));
        std::vector<int64_t> wp(maxs*maxd,0), wpd(maxs,0), out(maxs*maxd,0), outd(maxs,0);
        gg_slice a{et.data(),(int64_t)et.size()}, b{ev.data(),(int64_t)et.size()};
        gg_slice c{ro.data(),(int64_t)ro.size()}, d{rl.data(),(int64_t)ro.size()};
        gg_slice e{shifted.data(),(int64_t)shifted.size()};
        gg_slice f{wp.data(),maxs*maxd}, g{wpd.data(),maxs};
        gg_slice h{out.data(),maxs*maxd}, i{outd.data(),maxs};
        int64_t n = g_advance_call(a,b,c,d,e,(int64_t)shifted.size(),f,g,h,i,0,maxs,maxd);
        for (int64_t k = 0; k < n; ++k) { got.push_back(out[k*maxd + outd[k]-1]); gotd.push_back(outd[k]); }
    }
    cases++;
    if (got.size() != want.size()) { fails++; printf("count %s chr=%u native=%zu split=%zu\n", gbnf, chr, want.size(), got.size()); return; }
    for (size_t i = 0; i < want.size(); ++i) {
        int64_t nt = (int64_t)(want[i].back() - rules[0].data());
        if (got[i] != nt) { fails++; printf("top %s chr=%u stack %zu native=%lld split=%lld\n", gbnf, chr, i,
            (long long) nt, (long long) got[i]); }
    }
}
int main() {
    run("root ::= \"a\" \"b\"\n", 'a');
    run("root ::= \"a\" \"b\"\n", 'z');
    run("root ::= r1 \"c\"\nr1 ::= \"a\" | \"b\"\n", 'a');
    run("root ::= r1 \"c\"\nr1 ::= \"a\" | \"b\"\n", 'b');
    printf("split accept: cases=%d fails=%d\n", cases, fails);
    return fails?1:0;
}
