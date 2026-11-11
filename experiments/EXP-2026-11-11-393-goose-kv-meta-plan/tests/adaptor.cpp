#include <cstdint>
#include <cstdio>
extern "C" int64_t fg393s_decide(int64_t op, int64_t a, int64_t b, int64_t c);
extern "C" int fg393s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
int main(void) {
    check(fg393s_enabled() == 1, "enabled");
    for (int64_t scope = -1; scope <= 3; ++scope)
        for (int64_t cur = 0; cur <= 3; ++cur)
            for (int holds = 0; holds <= 1; ++holds) {
                const bool want = holds && (scope == -1 || cur == scope);
                check(fg393s_decide(0, cur, scope, holds) == (want ? 1 : 0), "include");
            }
    for (int ext = 0; ext <= 1; ++ext) check(fg393s_decide(1, ext, 0, 0) == ext, "ext");
    std::printf("FG393S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
