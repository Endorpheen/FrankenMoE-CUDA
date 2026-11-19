#include "../../../vendor/flint/src/llama-grammar.cpp"
#include <cstdio>
#include <cstdint>
#include <vector>
extern "C" int64_t g_partial_call(const int64_t*, const int64_t*, int64_t, int64_t, int64_t);
static int fails = 0, cases = 0;
static void one(const char * name, std::vector<llama_grammar_element> rule,
                int64_t * et, int64_t * ev, uint32_t value, int n_remain) {
    llama_partial_utf8 p{}; p.value = value; p.n_remain = n_remain;
    bool native = llama_grammar_match_partial_char(rule.data(), p);
    bool goose = g_partial_call(et, ev, 0, (int64_t) value, (int64_t) n_remain) != 0;
    cases++;
    if (native != goose) { fails++; printf("MISMATCH %s v=%u nr=%d native=%d goose=%d\n",
        name, value, n_remain, (int) native, (int) goose); }
}
int main() {
    std::vector<llama_grammar_element> r1 = {{LLAMA_GRETYPE_CHAR,'a'},{LLAMA_GRETYPE_END,0}};
    int64_t et1[8]={3,0,0,0,0,0,0,0}, ev1[8]={'a',0,0,0,0,0,0,0};
    for (int nr = 0; nr <= 3; ++nr) for (uint32_t v = 0; v < 40; v += 7) one("exact", r1, et1, ev1, v, nr);
    std::vector<llama_grammar_element> r2 = {{LLAMA_GRETYPE_CHAR,'a'},{LLAMA_GRETYPE_CHAR_RNG_UPPER,'z'},{LLAMA_GRETYPE_END,0}};
    int64_t et2[8]={3,5,0,0,0,0,0,0}, ev2[8]={'a','z',0,0,0,0,0,0};
    for (int nr = 0; nr <= 3; ++nr) for (uint32_t v = 0; v < 60; v += 5) one("range", r2, et2, ev2, v, nr);
    std::vector<llama_grammar_element> r3 = {{LLAMA_GRETYPE_CHAR_NOT,'a'},{LLAMA_GRETYPE_CHAR_RNG_UPPER,'z'},{LLAMA_GRETYPE_END,0}};
    int64_t et3[8]={4,5,0,0,0,0,0,0}, ev3[8]={'a','z',0,0,0,0,0,0};
    for (int nr = 0; nr <= 3; ++nr) for (uint32_t v = 0; v < 60; v += 5) one("not", r3, et3, ev3, v, nr);
    std::vector<llama_grammar_element> r4 = {{LLAMA_GRETYPE_CHAR_ANY,0},{LLAMA_GRETYPE_END,0}};
    int64_t et4[8]={7,0,0,0,0,0,0,0}, ev4[8]={0,0,0,0,0,0,0,0};
    for (int nr = 0; nr <= 3; ++nr) one("any", r4, et4, ev4, 0, nr);
    std::vector<llama_grammar_element> r5 = {{LLAMA_GRETYPE_CHAR,'a'},{LLAMA_GRETYPE_CHAR_ALT,'b'},{LLAMA_GRETYPE_END,0}};
    int64_t et5[8]={3,6,0,0,0,0,0,0}, ev5[8]={'a','b',0,0,0,0,0,0};
    for (int nr = 0; nr <= 3; ++nr) for (uint32_t v = 0; v < 40; v += 7) one("alt", r5, et5, ev5, v, nr);
    printf("match_partial: cases=%d fails=%d\n", cases, fails);
    return fails ? 1 : 0;
}
