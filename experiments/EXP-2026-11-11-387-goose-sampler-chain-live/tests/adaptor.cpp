#include <cstdint>
#include <cstdio>
extern "C" int64_t fg387s_decide(int64_t op, int64_t a, int64_t b);
extern "C" int fg387s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
int main(void) {
    check(fg387s_enabled() == 1, "enabled");
    for (int chain = 0; chain <= 1; ++chain)
        for (int smpl = 0; smpl <= 1; ++smpl)
            check(fg387s_decide(0, chain, smpl) == (chain && smpl ? 1 : 0), "backend skip");
    for (int has = 0; has <= 1; ++has) check(fg387s_decide(1, has, 0) == (has ? 0 : 1), "missing apply");
    for (int n = 0; n <= 20; ++n) check(fg387s_decide(2, 0, n) == n, "accept count");
    for (int c = -2; c <= 20; ++c) check(fg387s_decide(3, c, 0) == c + 1, "counter");
    std::printf("FG387S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
