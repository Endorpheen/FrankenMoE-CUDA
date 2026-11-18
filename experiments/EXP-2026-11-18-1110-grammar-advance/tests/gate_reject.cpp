// Gate for g_reject (single stack, single-codepoint candidates) against the native
// reject_candidates compiled into the fixture.
#include "../../../vendor/flint/src/llama-grammar.cpp"
#include <cstdio>
#include <vector>
extern "C" int64_t g_reject_call(const int64_t*, const int64_t*, const int64_t*, const int64_t*,
                                 const int64_t*, int64_t, const int64_t*, int64_t*, int64_t,
                                 int64_t*, int64_t);
static int fails = 0, cases = 0;

static void one(const char * name, llama_grammar_rules & rules, llama_grammar_stack stack,
                std::vector<uint32_t> cps,
                int64_t * et, int64_t * ev, int64_t * ro, int64_t * rl, int64_t * st) {
    // native
    llama_grammar_stacks stacks{stack};
    llama_grammar_candidates cands;
    std::vector<std::vector<uint32_t>> storage;
    storage.reserve(cps.size());
    for (size_t i = 0; i < cps.size(); ++i) {
        storage.push_back({cps[i], 0});
        cands.push_back({ (int) i, storage.back().data(), {}, 0 });
    }
    auto rej = llama_grammar_reject_candidates(rules, stacks, cands);
    std::vector<int> native(cps.size(), 0);
    for (auto & r : rej) native[r.index] = 1;

    // goose
    std::vector<int64_t> cp(cps.size()), rejv(cps.size(), 0), scratch(64, 0);
    for (size_t i = 0; i < cps.size(); ++i) cp[i] = (int64_t) cps[i];
    g_reject_call(et, ev, ro, rl, st, (int64_t) stack.size(), cp.data(), rejv.data(),
                  (int64_t) cps.size(), scratch.data(), 8);

    cases++;
    for (size_t i = 0; i < cps.size(); ++i) {
        if (native[i] != rejv[i]) {
            fails++;
            printf("MISMATCH %s cand %zu cp=%u native=%d goose=%lld\n", name, i, cps[i],
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
    one("abc", r1, s1, {'a','b','z'}, et1, ev1, ro1, rl1, st1);

    // rule 0: [a-c]
    llama_grammar_rules r2(1);
    r2[0].push_back({LLAMA_GRETYPE_CHAR,'a'});
    r2[0].push_back({LLAMA_GRETYPE_CHAR_RNG_UPPER,'c'});
    r2[0].push_back({LLAMA_GRETYPE_END,0});
    int64_t et2[8] = {3,5,0,0,0,0,0,0};
    int64_t ev2[8] = {'a','c',0,0,0,0,0,0};
    int64_t ro2[1] = {0}; int64_t rl2[1] = {3}; int64_t st2[8] = {0,0,0,0,0,0,0,0};
    llama_grammar_stack s2; s2.push_back(r2[0].data());
    one("range", r2, s2, {'a','d','Z'}, et2, ev2, ro2, rl2, st2);

    printf("reject: cases=%d fails=%d\n", cases, fails);
    return fails ? 1 : 0;
}
