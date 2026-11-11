#include <cstdint>
#include <cstdio>
extern "C" int64_t fg404s_decide(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t * out2);
extern "C" int fg404s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
int main(void) {
    check(fg404s_enabled() == 1, "enabled");
    int64_t o2;
    for (int none = 0; none <= 1; ++none) check(fg404s_decide(0, none, 0, 0, 0, 0, &o2) == none, "fast path");
    for (int sup = 0; sup <= 1; ++sup)
        for (int idx = 0; idx <= 3; ++idx)
            check(fg404s_decide(1, sup, idx, 0, 0, 0, &o2) == (sup ? idx : -1), "candidate");
    for (int walked = 0; walked <= 3; ++walked)
        for (int chosen = -1; chosen <= 3; ++chosen)
            check(fg404s_decide(2, walked, chosen, 0, 0, 0, &o2) == (chosen >= 0 ? chosen : -1), "result");
    std::printf("FG404S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
