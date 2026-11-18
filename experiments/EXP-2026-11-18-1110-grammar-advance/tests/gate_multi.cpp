#include "../../../vendor/flint/src/llama-grammar.cpp"
#include <cstdio>
#include <vector>
extern "C" int64_t g_reject_multi_call(const int64_t*, const int64_t*, const int64_t*, const int64_t*,
                                       const int64_t*, const int64_t*, int64_t,
                                       const int64_t*, const int64_t*, const int64_t*, const int64_t*,
                                       int64_t*, int64_t*, int64_t*, int64_t*, int64_t*,
                                       int64_t, int64_t, int64_t);
int main() {
    // rule 0: "a" | "b" ; two stacks after advance (one per alternate)
    llama_grammar_rules rules(1);
    rules[0].push_back({LLAMA_GRETYPE_CHAR,'a'});
    rules[0].push_back({LLAMA_GRETYPE_ALT,0});
    rules[0].push_back({LLAMA_GRETYPE_CHAR,'b'});
    rules[0].push_back({LLAMA_GRETYPE_END,0});
    llama_grammar_stack s1, s2;
    s1.push_back(rules[0].data());
    s2.push_back(rules[0].data() + 2);
    llama_grammar_stacks stacks{s1, s2};

    std::vector<std::vector<uint32_t>> cand_cps = {{'a'},{'b'},{'z'}};
    llama_grammar_candidates cands;
    std::vector<std::vector<uint32_t>> storage;
    for (size_t i = 0; i < cand_cps.size(); ++i) {
        storage.push_back(cand_cps[i]); storage.back().push_back(0);
        cands.push_back({(int) i, storage.back().data(), {}, 0});
    }
    auto rej = llama_grammar_reject_candidates(rules, stacks, cands);
    int native[3] = {0,0,0};
    for (auto & r : rej) native[r.index] = 1;

    int64_t et[8] = {3,1,3,0,0,0,0,0};
    int64_t ev[8] = {'a',0,'b',0,0,0,0,0};
    int64_t ro[2] = {0,0}, rl[2] = {4,0};
    int64_t stk[16] = {0,0,0,0, 2,0,0,0, 0,0,0,0, 0,0,0,0};
    int64_t std[2] = {1,1};
    int64_t cp[9] = {'a',0,0,'b',0,0,'z',0,0};
    int64_t cpl[3] = {1,1,1};
    int64_t pv[3] = {0,0,0}, pn[3] = {0,0,0}, out[3] = {0,0,0};
    int64_t scratch[64] = {0}, tr[3] = {0}, tc[3] = {0}, rb[8] = {0};
    g_reject_multi_call(et, ev, ro, rl, stk, std, 2, cp, cpl, pv, pn, out, scratch, tr, tc, rb, 3, 4, 3);
    printf("native: %d %d %d\ngoose : %lld %lld %lld\n", native[0], native[1], native[2],
           (long long) out[0], (long long) out[1], (long long) out[2]);
    return (native[0]==out[0] && native[1]==out[1] && native[2]==out[2]) ? 0 : 1;
}
