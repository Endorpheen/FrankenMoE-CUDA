#include "../../../vendor/flint/src/llama-grammar.cpp"
#include <cstdio>
#include <vector>
extern "C" int64_t g_gr_call(const int64_t*, const int64_t*, const int64_t*, const int64_t*,
                             const int64_t*, int64_t, const int64_t*, const int64_t*,
                             const int64_t*, const int64_t*, const int64_t*, int64_t,
                             int64_t*, int64_t*, int64_t*, int64_t*, int64_t*,
                             int64_t*, int64_t*, int64_t*, int64_t*,
                             int64_t, int64_t, int64_t);
int main() {
    // rule 0: ref(1) then ref(2); rule 1: "a" | "b"; rule 2: "c"
    llama_grammar_rules rules(3);
    rules[0].push_back({LLAMA_GRETYPE_RULE_REF, 1});
    rules[0].push_back({LLAMA_GRETYPE_RULE_REF, 2});
    rules[0].push_back({LLAMA_GRETYPE_END, 0});
    rules[1].push_back({LLAMA_GRETYPE_CHAR,'a'});
    rules[1].push_back({LLAMA_GRETYPE_ALT,0});
    rules[1].push_back({LLAMA_GRETYPE_CHAR,'b'});
    rules[1].push_back({LLAMA_GRETYPE_END,0});
    rules[2].push_back({LLAMA_GRETYPE_CHAR,'c'});
    rules[2].push_back({LLAMA_GRETYPE_END,0});

    llama_grammar_stack st; st.push_back(rules[0].data());
    llama_grammar_stacks stacks{st};
    std::vector<std::vector<uint32_t>> cc = {{'a'},{'b'},{'x'}};
    llama_grammar_candidates cands;
    std::vector<std::vector<uint32_t>> storage;
    for (size_t i=0;i<cc.size();++i){ storage.push_back(cc[i]); storage.back().push_back(0);
        cands.push_back({(int)i, storage.back().data(), {}, 0}); }
    auto rej = llama_grammar_reject_candidates(rules, stacks, cands);
    int native[3]={0,0,0}; for (auto&r:rej) native[r.index]=1;

    int64_t et[16]; int64_t ev[16];
    auto put=[&](int i, int t, int v){ et[i]=t; ev[i]=v; };
    put(0,2,1); put(1,2,2); put(2,0,0);
    put(3,3,'a'); put(4,1,0); put(5,3,'b'); put(6,0,0);
    put(7,3,'c'); put(8,0,0);
    for (int i=9;i<16;++i) put(i,0,0);
    int64_t ro[3]={0,3,7}, rl[3]={3,4,2};
    int64_t seed[4]={0,0,0,0};
    int64_t cp[6]={'a',0,'b',0,'x',0}, cpl[3]={1,1,1}, pv[3]={0,0,0}, pn[3]={0,0,0}, cid[3]={0,0,0};
    int64_t wp[16]={0}, wpd[4]={0}, stk[16]={0}, std_[4]={0}, sc[64]={0};
    int64_t tr[3]={0}, tc[3]={0}, rb[4]={0}, out[3]={0};
    int64_t n = g_gr_call(et, ev, ro, rl, seed, 1, cp, cpl, pv, pn, cid, 3,
                          wp, wpd, stk, std_, sc, tr, tc, rb, out, 4, 4, 2);
    printf("native: %d %d %d\ngoose : %lld %lld %lld (n=%lld)\n", native[0], native[1], native[2],
           (long long) out[0], (long long) out[1], (long long) out[2], (long long) n);
    return (native[0]==out[0] && native[1]==out[1] && native[2]==out[2]) ? 0 : 1;
}
