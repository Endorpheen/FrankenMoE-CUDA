// The allowed first-codepoint set must agree with match_char on the class at the top.
#include "../../../vendor/flint/src/llama-grammar.cpp"
#include <cstdio>
#include <cstdint>
typedef struct { void * data; int64_t len; } gg_slice;
extern "C" int64_t g_first_call(gg_slice et, gg_slice ev, gg_slice st, gg_slice std_,
                                int64_t nstacks, gg_slice lo, gg_slice hi, int64_t maxr);
static int fails = 0, cases = 0;
static void run(const char * name, std::vector<llama_grammar_element> rule,
                int64_t * et, int64_t * ev) {
    llama_grammar_stack stk; stk.push_back(rule.data());
    llama_grammar_stacks adv; llama_grammar_advance_stack({rule}, stk, adv);
    if (adv.empty()) return;
    int64_t st[8]={0}, std_[1]={(int64_t)adv[0].size()}, lo[8]={0}, hi[8]={0};
    for (size_t i=0;i<adv[0].size();++i) st[i] = (int64_t)(adv[0][i]-rule.data());
    gg_slice a{et,8}, b{ev,8}, c{st,8}, d{std_,1}, e{lo,8}, f{hi,8};
    int64_t n = g_first_call(a,b,c,d,1,e,f,8);
    if (n < 0) { printf("SKIP %s (not expressible)\n", name); return; }
    // probe codepoints around the class and compare with match_char
    for (int64_t cp = 0; cp < 300; ++cp) {
        bool in = false;
        for (int64_t r = 0; r < n; ++r) if (cp >= lo[r] && cp <= hi[r]) in = true;
        const bool native = llama_grammar_match_char(adv[0].back(), (uint32_t)cp).first;
        cases++;
        if (in != native) { fails++; if (fails<5) printf("MISMATCH %s cp=%lld set=%d native=%d\n", name, (long long)cp, (int)in, (int)native); }
    }
}
int main() {
    { std::vector<llama_grammar_element> r = {{LLAMA_GRETYPE_CHAR,'a'},{LLAMA_GRETYPE_END,0}};
      int64_t et[8]={3,0,0,0,0,0,0,0}, ev[8]={'a',0,0,0,0,0,0,0}; run("exact", r, et, ev); }
    { std::vector<llama_grammar_element> r = {{LLAMA_GRETYPE_CHAR,'a'},{LLAMA_GRETYPE_CHAR_RNG_UPPER,'z'},{LLAMA_GRETYPE_END,0}};
      int64_t et[8]={3,5,0,0,0,0,0,0}, ev[8]={'a','z',0,0,0,0,0,0}; run("range", r, et, ev); }
    { std::vector<llama_grammar_element> r = {{LLAMA_GRETYPE_CHAR,'a'},{LLAMA_GRETYPE_CHAR_ALT,'b'},{LLAMA_GRETYPE_END,0}};
      int64_t et[8]={3,6,0,0,0,0,0,0}, ev[8]={'a','b',0,0,0,0,0,0}; run("alt", r, et, ev); }
    { std::vector<llama_grammar_element> r = {{LLAMA_GRETYPE_CHAR_ANY,0},{LLAMA_GRETYPE_END,0}};
      int64_t et[8]={7,0,0,0,0,0,0,0}, ev[8]={0,0,0,0,0,0,0,0}; run("any", r, et, ev); }
    printf("first_set: cases=%d fails=%d\n", cases, fails);
    return fails?1:0;
}
