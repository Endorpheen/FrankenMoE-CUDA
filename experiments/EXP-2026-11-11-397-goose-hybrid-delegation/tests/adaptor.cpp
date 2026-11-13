#include <cstdint>
#include <cstdio>
extern "C" int64_t fg397s_decide(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t * out2);
extern "C" int fg397s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
int main(void) {
    check(fg397s_enabled() == 1, "enabled");
    int64_t o2;
    for (int64_t step = -5; step <= 9; ++step)
        check(fg397s_decide(0, step, 0, 0, 0, 0, &o2) == (step == 0 ? 1 : 0), "order");
    for (int64_t f = -5; f <= 9; ++f) check(fg397s_decide(1, f, 0, 0, 0, 0, &o2) == (f != 0 ? 1 : 0), "clear");
    // a full delegation plan: step 0 attention, step 1 recurrent, exactly one each
    for (int step = 0; step < 2; ++step) {
        const int64_t is_attn = fg397s_decide(0, step, 0, 0, 0, 0, &o2);
        check((step == 0 && is_attn == 1) || (step == 1 && is_attn == 0), "plan");
    }
    std::printf("FG397S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
