#include <cstdint>
#include <cstdio>
extern "C" int64_t fg405s_decide(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t * out2);
extern "C" int fg405s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
int main(void) {
    check(fg405s_enabled() == 1, "enabled");
    int64_t o2;
    for (int r = 0; r <= 1; ++r) check(fg405s_decide(0, r, 0, 0, 0, 0, &o2) == r, "reserve");
    for (int64_t ctx = 0; ctx <= 40; ctx += 8)
        for (int64_t ub = 0; ub <= 40; ub += 8)
            check(fg405s_decide(1, ctx, ub, 0, 0, 0, &o2) == (ctx < ub ? ctx : ub), "width");
    for (int64_t q = -2; q <= 5; ++q) {
        const int64_t want = q <= 0 ? 0 : (q == 1 ? 1 : 2);
        check(fg405s_decide(2, q, 0, 0, 0, 0, &o2) == want, "stats");
    }
    std::printf("FG405S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
