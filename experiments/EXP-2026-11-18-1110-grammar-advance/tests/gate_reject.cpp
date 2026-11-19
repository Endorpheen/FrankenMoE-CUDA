// Gate for g_reject (single stack, single-codepoint candidates) against the native
// reject_candidates compiled into the fixture.
#include "../../../vendor/flint/src/llama-grammar.cpp"
#include <cstdio>
#include <vector>
extern "C" int64_t g_reject_call(const int64_t*, const int64_t*, const int64_t*, const int64_t*,
                                 const int64_t*, int64_t, const int64_t*, const int64_t*,
                                 int64_t*, int64_t*, int64_t, const int64_t*, const int64_t*,
                                 const int64_t*, int64_t*, int64_t, int64_t);
static int fails = 0, cases = 0;

static void one(const char * name, llama_grammar_rules & rules, llama_grammar_stack stack,
                std::vector<std::vector<uint32_t>> cps,
                int64_t * et, int64_t * ev, int64_t * ro, int64_t * rl, int64_t * st) {
    // native
    llama_grammar_stacks stacks{stack};
    llama_grammar_candidates cands;
    std::vector<std::vector<uint32_t>> storage;
    storage.reserve(cps.size());
    for (size_t i = 0; i < cps.size(); ++i) {
        storage.push_back(cps[i]);
        storage.back().push_back(0);
        cands.push_back({ (int) i, storage.back().data(), {}, 0 });
    }
    auto rej = llama_grammar_reject_candidates(rules, stacks, cands);
    std::vector<int> native(cps.size(), 0);
    for (auto & r : rej) native[r.index] = 1;

    // goose
    const int64_t maxcp = 4;
    std::vector<int64_t> cp(cps.size() * maxcp, 0), cpl(cps.size()), cur(cps.size(), 0);
    std::vector<int64_t> rejv(cps.size(), 0), scratch(64, 0);
    for (size_t i = 0; i < cps.size(); ++i) {
        cpl[i] = (int64_t) cps[i].size();
        for (size_t j = 0; j < cps[i].size() && j < (size_t) maxcp; ++j) cp[i*maxcp + j] = (int64_t) cps[i][j];
    }
    std::vector<int64_t> pv(cps.size(), 0), pn(cps.size(), 0), cid(cps.size(), 0);
    for (size_t i = 0; i < cps.size(); ++i) cid[i] = (int64_t) cands[i].id;
    g_reject_call(et, ev, ro, rl, st, (int64_t) stack.size(), cp.data(), cpl.data(), cur.data(),
                  rejv.data(), (int64_t) cps.size(), pv.data(), pn.data(), cid.data(), scratch.data(), 8, maxcp);

    cases++;
    for (size_t i = 0; i < cps.size(); ++i) {
        if (native[i] != rejv[i]) {
            fails++;
            printf("MISMATCH %s cand %zu native=%d goose=%lld\n", name, i,
                   native[i], (long long) rejv[i]);
            return;
        }
    }
}
int main() {
    // rule 0: "abc"
    llama_grammar_rules r1(1);
    r1[0].push_back({LLAMA_GRETYPE_CHAR,'a'});
    r1[0].push_back({LLAMA_GRETYPE_CHAR,'b'});
    r1[0].push_back({LLAMA_GRETYPE_CHAR,'c'});
    r1[0].push_back({LLAMA_GRETYPE_END,0});
    int64_t et1[8] = {3,3,3,0,0,0,0,0};
    int64_t ev1[8] = {'a','b','c',0,0,0,0,0};
    int64_t ro1[1] = {0}; int64_t rl1[1] = {4}; int64_t st1[8] = {0,0,0,0,0,0,0,0};
    llama_grammar_stack s1; s1.push_back(r1[0].data());
    one("abc", r1, s1, {{'a'},{'b'},{'z'},{'a','b','c'},{'a','x'}}, et1, ev1, ro1, rl1, st1);

    // rule 0: [a-c]
    llama_grammar_rules r2(1);
    r2[0].push_back({LLAMA_GRETYPE_CHAR,'a'});
    r2[0].push_back({LLAMA_GRETYPE_CHAR_RNG_UPPER,'c'});
    r2[0].push_back({LLAMA_GRETYPE_END,0});
    int64_t et2[8] = {3,5,0,0,0,0,0,0};
    int64_t ev2[8] = {'a','c',0,0,0,0,0,0};
    int64_t ro2[1] = {0}; int64_t rl2[1] = {3}; int64_t st2[8] = {0,0,0,0,0,0,0,0};
    llama_grammar_stack s2; s2.push_back(r2[0].data());
    one("range", r2, s2, {{'a'},{'d'},{'Z'},{'c','c'}}, et2, ev2, ro2, rl2, st2);

    // partial UTF-8: no codepoints, incomplete byte sequence
    {
        llama_grammar_rules r3(1);
        r3[0].push_back({LLAMA_GRETYPE_CHAR,'a'});
        r3[0].push_back({LLAMA_GRETYPE_CHAR_RNG_UPPER,'z'});
        r3[0].push_back({LLAMA_GRETYPE_END,0});
        int64_t et3[8] = {3,5,0,0,0,0,0,0};
        int64_t ev3[8] = {'a','z',0,0,0,0,0,0};
        int64_t ro3[1] = {0}, rl3[1] = {3}, st3[8] = {0,0,0,0,0,0,0,0};
        llama_grammar_stack s3; s3.push_back(r3[0].data());
        llama_grammar_stacks stacks{s3};
        llama_grammar_candidates cands;
        static uint32_t z1[1] = {0}, z2[1] = {0};
        llama_partial_utf8 p1{}; p1.value = 1; p1.n_remain = 1;
        llama_partial_utf8 p2{}; p2.value = 2; p2.n_remain = 1;
        cands.push_back({0, z1, p1, 0});
        cands.push_back({1, z2, p2, 0});
        auto rej = llama_grammar_reject_candidates(r3, stacks, cands);
        int native[2] = {0,0};
        for (auto & r : rej) native[r.index] = 1;

        int64_t cp3[2] = {0,0}, cpl3[2] = {0,0}, cur3[2] = {0,0}, rej3[2] = {0,0};
        int64_t pv3[2] = {1,2}, pn3[2] = {1,1}, sc3[64] = {0};
        int64_t cid3[2] = {0,0};
        g_reject_call(et3, ev3, ro3, rl3, st3, 1, cp3, cpl3, cur3, rej3, 2, pv3, pn3, cid3, sc3, 8, 2);
        cases++;
        for (int i = 0; i < 2; ++i) if (native[i] != rej3[i]) { fails++; printf("MISMATCH partial cand %d native=%d goose=%lld\n", i, native[i], (long long) rej3[i]); }
    }
    // token rule: <5>
    {
        llama_grammar_rules r4(1);
        r4[0].push_back({LLAMA_GRETYPE_TOKEN, 5});
        r4[0].push_back({LLAMA_GRETYPE_END, 0});
        int64_t et4[8] = {8,0,0,0,0,0,0,0};
        int64_t ev4[8] = {5,0,0,0,0,0,0,0};
        int64_t ro4[1] = {0}, rl4[1] = {2}, st4[8] = {0,0,0,0,0,0,0,0};
        llama_grammar_stack s4; s4.push_back(r4[0].data());
        llama_grammar_stacks stacks{s4};
        llama_grammar_candidates cands;
        static uint32_t c1[2] = {'x',0}, c2[2] = {'x',0};
        cands.push_back({0, c1, {}, 5});
        cands.push_back({1, c2, {}, 6});
        auto rej = llama_grammar_reject_candidates(r4, stacks, cands);
        int native[2] = {0,0};
        for (auto & r : rej) native[r.index] = 1;

        int64_t cp4[4] = {'x',0,'x',0}, cpl4[2] = {1,1}, cur4[2] = {0,0}, rej4[2] = {0,0};
        int64_t pv4[2] = {0,0}, pn4[2] = {0,0}, cid4[2] = {5,6}, sc4[64] = {0};
        g_reject_call(et4, ev4, ro4, rl4, st4, 1, cp4, cpl4, cur4, rej4, 2, pv4, pn4, cid4, sc4, 8, 2);
        cases++;
        for (int i = 0; i < 2; ++i) if (native[i] != rej4[i]) { fails++; printf("MISMATCH token cand %d native=%d goose=%lld\n", i, native[i], (long long) rej4[i]); }
    }
    printf("reject: cases=%d fails=%d\n", cases, fails);
    return fails ? 1 : 0;
}
