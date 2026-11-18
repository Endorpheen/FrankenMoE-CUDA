// Gate: the same rule table is advanced by the native automaton (compiled in) and
// by the Goose implementation; both report (count, depth, top type, top value).
#include "../../../vendor/flint/src/llama-grammar.cpp"
#include <cstdio>
#include <vector>
extern "C" int64_t g_advance_call(const int64_t*, const int64_t*, const int64_t*, const int64_t*, const int64_t*, int64_t, int64_t*, int64_t*, int64_t, int64_t);
struct S { void * data; int64_t len; };
int main() {
    llama_grammar_rules rules(3);
    rules[0].push_back({LLAMA_GRETYPE_RULE_REF, 1});
    rules[0].push_back({LLAMA_GRETYPE_END, 0});
    rules[1].push_back({LLAMA_GRETYPE_CHAR, 'a'});
    rules[1].push_back({LLAMA_GRETYPE_END, 0});
    rules[2].push_back({LLAMA_GRETYPE_CHAR, 'x'});
    rules[2].push_back({LLAMA_GRETYPE_END, 0});

    llama_grammar_stacks out;
    llama_grammar_stack st;
    st.push_back(rules[0].data());
    llama_grammar_advance_stack(rules, st, out);
    printf("native: n=%zu", out.size());
    for (auto & s : out) {
        const llama_grammar_element * top = s.empty() ? nullptr : s.back();
        printf(" d=%zu t=%d v=%u", s.size(), top ? (int) top->type : -1, top ? top->value : 0);
    }
    printf("\n");

    int64_t et[8] = {2, 0, 3, 0, 3, 0, 0, 0};
    int64_t ev[8] = {1, 0, 'a', 0, 'x', 0, 0, 0};
    int64_t ro[3] = {0, 2, 4};
    int64_t rl[3] = {2, 2, 2};
    int64_t seed[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int64_t pool[64] = {0};
    int64_t outd[8] = {0};
    int64_t n = g_advance_call(et, ev, ro, rl, seed, 1, pool, outd, 8, 8);
    printf("goose : n=%lld", (long long) n);
    for (int64_t i = 0; i < n; ++i) {
        int64_t d = outd[i];
        int64_t top = d ? pool[i*8 + d - 1] : -1;
        printf(" d=%lld t=%lld v=%lld", (long long) d, (long long)(top>=0?et[top]:-1), (long long)(top>=0?ev[top]:0));
    }
    printf("\n");
    return 0;
}
