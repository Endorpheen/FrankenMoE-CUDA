#include "../../../vendor/flint/src/llama-grammar.cpp"
#include <cstdio>
#include <vector>
typedef struct { void * data; int64_t len; } gg_slice;
extern "C" int64_t g_accept_call(gg_slice, gg_slice, gg_slice, gg_slice, gg_slice, gg_slice,
                                 int64_t, int64_t, int64_t, int64_t);
static int fails = 0, cases = 0;
static void run(const char * gbnf, uint32_t chr) {
    llama_grammar_parser parser;
    if (!parser.parse(gbnf)) { fails++; printf("parse failed\n"); return; }
    const llama_grammar_rules & rules = parser.rules;
    llama_grammar_stack st; st.push_back(rules[0].data());
    llama_grammar_stacks stacks; llama_grammar_advance_stack(rules, st, stacks);
    llama_grammar_stacks after;
    for (auto & s : stacks) {
        llama_grammar_stack ns(s.begin(), s.end());
        if (ns.empty()) continue;
        auto m = llama_grammar_match_char(ns.back(), chr);
        if (!m.first) continue;
        ns.pop_back();
        if (!llama_grammar_is_end_of_sequence(m.second)) ns.push_back(m.second);
        llama_grammar_advance_stack(rules, ns, after);
    }
    std::vector<int64_t> et, ev, ro(rules.size()), rl(rules.size());
    for (size_t r = 0; r < rules.size(); ++r) { ro[r]=(int64_t)et.size(); rl[r]=(int64_t)rules[r].size();
        for (auto &e : rules[r]) { et.push_back((int64_t)e.type); ev.push_back((int64_t)e.value); } }
    int64_t seed[8]={0}, std_[8]={0}, out[64]={0}, outd[8]={0};
    // Goose receives the same stacks the native reject would see (post-advance)
    int64_t stk[64]={0}, stdin[8]={0};
    for (size_t i=0;i<stacks.size() && i<8;++i){ stdin[i]=(int64_t)stacks[i].size();
        for (size_t j=0;j<stacks[i].size() && j<8;++j) stk[i*8+j]=(int64_t)(stacks[i][j]-rules[0].data()); }
    gg_slice a{(void*)et.data(), (int64_t)et.size()}, b{(void*)ev.data(), (int64_t)et.size()};
    gg_slice c{(void*)stk, 64}, d{(void*)stdin, 8}, e{(void*)out, 64}, f{(void*)outd, 8};
    int64_t n = g_accept_call(a, b, c, d, e, f, (int64_t)stacks.size(), chr, 8, 8);
    cases++;
    if ((size_t)n != after.size()) { fails++; printf("count %s chr=%u native=%zu goose=%lld\n", gbnf, chr, after.size(), (long long)n); return; }
    // compare the top element of each resulting stack (type and value)
    for (size_t i = 0; i < after.size(); ++i) {
        const llama_grammar_element * nt = after[i].empty() ? nullptr : after[i].back();
        int64_t d = outd[i];
        int64_t gt = d ? et[out[i*8 + d - 1]] : -1;
        int64_t gv = d ? ev[out[i*8 + d - 1]] : 0;
        int64_t nt_type = nt ? (int64_t) nt->type : -1;
        int64_t nt_val  = nt ? (int64_t) nt->value : 0;
        if (gt != nt_type || gv != nt_val) { fails++;
            printf("top %s chr=%u stack %zu native=(%lld,%lld) goose=(%lld,%lld)\n", gbnf, chr, i,
                   (long long) nt_type, (long long) nt_val, (long long) gt, (long long) gv); }
    }
}
int main() {
    run("root ::= \"a\" \"b\"\n", 'a');
    run("root ::= \"a\" \"b\"\n", 'z');
    run("root ::= [a-c] \"d\"\n", 'b');
    run("root ::= [^a-c] \"d\"\n", 'b');
    printf("accept: cases=%d fails=%d\n", cases, fails);
    return fails?1:0;
}
