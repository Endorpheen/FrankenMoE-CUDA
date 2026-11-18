#include "../../../vendor/flint/src/llama-grammar.cpp"
#include <cstdio>
#include <vector>
extern "C" int64_t g_match_call(const int64_t*, const int64_t*, int64_t, int64_t);
static int fails = 0, cases = 0;
static void one(const char * name,
                std::vector<llama_grammar_element> rule,
                int64_t * et, int64_t * ev,
                uint32_t chr, int64_t pos) {
    llama_grammar_element * p = rule.data() + pos;
    auto r = llama_grammar_match_char(p, chr);
    const bool nm = r.first;
    const int64_t npos = r.second - rule.data();
    int64_t g = g_match_call(et, ev, pos, (int64_t) chr);
    const bool gm = g > 0;
    const int64_t gpos = g > 0 ? g - 1 : -g - 1;
    cases++;
    if (nm != gm || npos != gpos) {
        fails++;
        printf("MISMATCH %s chr=%u native(%d,%lld) goose(%d,%lld)\n", name, chr, (int) nm,
               (long long) npos, (int) gm, (long long) gpos);
    }
}
int main() {
    // class "a": elements at index 0
    std::vector<llama_grammar_element> r1 = {{LLAMA_GRETYPE_CHAR,'a'},{LLAMA_GRETYPE_END,0}};
    int64_t et1[8] = {3,0,0,0,0,0,0,0}; int64_t ev1[8] = {'a',0,0,0,0,0,0,0};
    one("exact", r1, et1, ev1, 'a', 0);
    one("exact", r1, et1, ev1, 'b', 0);
    // range [a-z]
    std::vector<llama_grammar_element> r2 = {{LLAMA_GRETYPE_CHAR,'a'},{LLAMA_GRETYPE_CHAR_RNG_UPPER,'z'},{LLAMA_GRETYPE_END,0}};
    int64_t et2[8] = {3,5,0,0,0,0,0,0}; int64_t ev2[8] = {'a','z',0,0,0,0,0,0};
    one("range", r2, et2, ev2, 'm', 0);
    one("range", r2, et2, ev2, 'A', 0);
    // negation [^a-z]
    std::vector<llama_grammar_element> r3 = {{LLAMA_GRETYPE_CHAR_NOT,'a'},{LLAMA_GRETYPE_CHAR_RNG_UPPER,'z'},{LLAMA_GRETYPE_END,0}};
    int64_t et3[8] = {4,5,0,0,0,0,0,0}; int64_t ev3[8] = {'a','z',0,0,0,0,0,0};
    one("not", r3, et3, ev3, 'm', 0);
    one("not", r3, et3, ev3, '5', 0);
    // any
    std::vector<llama_grammar_element> r4 = {{LLAMA_GRETYPE_CHAR_ANY,0},{LLAMA_GRETYPE_END,0}};
    int64_t et4[8] = {7,0,0,0,0,0,0,0}; int64_t ev4[8] = {0,0,0,0,0,0,0,0};
    one("any", r4, et4, ev4, 42, 0);
    // alternation inside a class: [ab]
    std::vector<llama_grammar_element> r5 = {{LLAMA_GRETYPE_CHAR,'a'},{LLAMA_GRETYPE_CHAR_ALT,'b'},{LLAMA_GRETYPE_END,0}};
    int64_t et5[8] = {3,6,0,0,0,0,0,0}; int64_t ev5[8] = {'a','b',0,0,0,0,0,0};
    one("alt", r5, et5, ev5, 'a', 0);
    one("alt", r5, et5, ev5, 'b', 0);
    one("alt", r5, et5, ev5, 'c', 0);
    // negated alternation [^ab]
    std::vector<llama_grammar_element> r6 = {{LLAMA_GRETYPE_CHAR_NOT,'a'},{LLAMA_GRETYPE_CHAR_ALT,'b'},{LLAMA_GRETYPE_END,0}};
    int64_t et6[8] = {4,6,0,0,0,0,0,0}; int64_t ev6[8] = {'a','b',0,0,0,0,0,0};
    one("alt-not", r6, et6, ev6, 'a', 0);
    one("alt-not", r6, et6, ev6, 'z', 0);
    printf("match_char: cases=%d fails=%d\n", cases, fails);
    return fails ? 1 : 0;
}
