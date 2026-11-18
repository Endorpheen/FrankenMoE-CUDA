// Reference: llama_vocab::normalize transcribed, with the escaped space as three bytes.
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
extern "C" int64_t g_norm_call(const char*,int64_t,char*,int64_t,int64_t,int64_t,int64_t);
static std::string ref(const std::string & input, bool esc, bool pre, bool mrg, bool suf) {
    const std::string space = esc ? "\xE2\x96\x81" : " ";
    const bool shall_prepend_space = pre;
    const bool shall_append_space  = suf;
    const bool shall_merge_spaces  = mrg;
    std::string out; bool is_space_prepended = false; bool processing_non_ws = false;
    for (char c : input) {
        if (c != ' ') {
            if (!processing_non_ws) {
                processing_non_ws = true;
                if ((shall_prepend_space && !is_space_prepended) || shall_merge_spaces) {
                    out += space; is_space_prepended = true;
                }
            }
            out += c;
        } else {
            if (processing_non_ws) processing_non_ws = false;
            if (!shall_merge_spaces) out += space;
        }
    }
    if (shall_append_space) out += space;
    return out;
}
int main() {
    struct C { const char * name; const char * in; bool esc, pre, mrg, suf; };
    const C cs[] = {
      {"spm plain",            "hello world", true,  true,  false, false},
      {"leading space kept",   "  hi",        true,  true,  false, false},
      {"trailing space kept",  "hi  ",        true,  true,  false, false},
      {"no escape",            "a b",         false, true,  false, false},
      {"no prepend",           "a b",         true,  false, false, false},
      {"merge extra",          "  a   b  ",   true,  true,  true,  false},
      {"merge only spaces",    "   ",         true,  true,  true,  false},
      {"suffix",               "a b",         true,  false, false, true},
      {"empty",                "",            true,  true,  false, false},
      {"merge empty",          "",            true,  true,  true,  false},
      {"single char",          "x",           true,  true,  false, false},
    };
    int fails = 0, n = (int) (sizeof(cs)/sizeof(cs[0]));
    for (int i = 0; i < n; ++i) {
        char out[128]; memset(out, 0, sizeof(out));
        int64_t r = g_norm_call(cs[i].in, (int64_t) strlen(cs[i].in), out, cs[i].esc, cs[i].pre, cs[i].mrg, cs[i].suf);
        std::string e = ref(cs[i].in, cs[i].esc, cs[i].pre, cs[i].mrg, cs[i].suf);
        if (r != (int64_t) e.size() || std::string(out, (size_t) r) != e) {
            fails++;
            printf("MISMATCH %s\n  goose (%lld): ", cs[i].name, (long long) r);
            for (int64_t k = 0; k < r; ++k) printf("%02X ", (unsigned char) out[k]);
            printf("\n  ref   (%zu): ", e.size());
            for (unsigned char ch : e) printf("%02X ", ch);
            printf("\n");
        }
    }
    printf("normalize: cases=%d fails=%d\n", n, fails);
    return fails ? 1 : 0;
}
