// Composed gate using rules produced by the native parser, so the layout matches.
#include "../../../vendor/flint/src/llama-grammar.cpp"
#include <cstdio>
#include <cstring>
#include <vector>
extern "C" int64_t g_gr_call(const int64_t*, const int64_t*, int64_t,
                             const int64_t*, const int64_t*, int64_t,
                             const int64_t*, int64_t, const int64_t*, const int64_t*,
                             const int64_t*, const int64_t*, const int64_t*, int64_t,
                             int64_t*, int64_t*, int64_t*, int64_t*, int64_t*,
                             int64_t*, int64_t*, int64_t*, int64_t*,
                             int64_t, int64_t, int64_t);

static int fails = 0, cases = 0;

static void run(const char * gbnf, std::vector<std::vector<uint32_t>> cps) {
    llama_grammar_parser parser;
    if (!parser.parse(gbnf)) { printf("parse failed: %s\n", gbnf); fails++; return; }
    const llama_grammar_rules & rules = parser.rules;

    llama_grammar_stack st;
    st.push_back(rules[0].data());
    llama_grammar_stacks stacks;
    llama_grammar_advance_stack(rules, st, stacks);

    llama_grammar_candidates cands;
    std::vector<std::vector<uint32_t>> storage;
    for (size_t i = 0; i < cps.size(); ++i) {
        storage.push_back(cps[i]); storage.back().push_back(0);
        cands.push_back({(int) i, storage.back().data(), {}, 0});
    }
    auto rej = llama_grammar_reject_candidates(rules, stacks, cands);
    std::vector<int> native(cps.size(), 0);
    for (auto & r : rej) native[r.index] = 1;

    // flatten
    std::vector<int64_t> et, ev, ro(rules.size()), rl(rules.size());
    for (size_t r = 0; r < rules.size(); ++r) {
        ro[r] = (int64_t) et.size();
        rl[r] = (int64_t) rules[r].size();
        for (auto & e : rules[r]) { et.push_back((int64_t) e.type); ev.push_back((int64_t) e.value); }
    }
    const int64_t maxcp = 4, maxd = 8, maxs = 8, ncand = (int64_t) cps.size();
    std::vector<int64_t> cp(ncand*maxcp, 0), cpl(ncand, 0), pv(ncand,0), pn(ncand,0), cid(ncand,0);
    for (int64_t i = 0; i < ncand; ++i) {
        cpl[i] = (int64_t) cps[i].size();
        for (size_t j = 0; j < cps[i].size() && j < (size_t) maxcp; ++j) cp[i*maxcp+j] = (int64_t) cps[i][j];
    }
    int64_t seed[8] = {0}, wp[64] = {0}, wpd[8] = {0}, stk[64] = {0}, std_[8] = {0};
    int64_t sc[64] = {0}, tr[8] = {0}, tc[8] = {0}, rb[8] = {0}, out[8] = {0};
    g_gr_call(et.data(), ev.data(), (int64_t) et.size(), ro.data(), rl.data(), (int64_t) ro.size(),
              seed, 1, cp.data(), cpl.data(), pv.data(), pn.data(), cid.data(), ncand,
              wp, wpd, stk, std_, sc, tr, tc, rb, out, maxs, maxd, maxcp);
    cases++;
    for (int64_t i = 0; i < ncand; ++i) {
        if ((int) out[i] != native[i]) {
            fails++;
            printf("MISMATCH gbnf=[%s] cand %lld native=%d goose=%lld\n", gbnf, (long long) i,
                   native[i], (long long) out[i]);
        }
    }
}

int main() {
    run("root ::= \"a\" \"b\" \"c\"\n", {{'a'},{'b'},{'z'},{'a','b','c'},{'a','x'}});
    run("root ::= [a-c]\n", {{'a'},{'d'},{'c','c'}});
    run("root ::= r1 r2\nr1 ::= \"a\" | \"b\"\nr2 ::= \"c\"\n", {{'a'},{'b'},{'x'}});
    run("root ::= ( \"a\" | \"b\" ) \"c\"\n", {{'a'},{'b'},{'z'}});
    printf("grammar composed: cases=%d fails=%d\n", cases, fails);
    return fails ? 1 : 0;
}
