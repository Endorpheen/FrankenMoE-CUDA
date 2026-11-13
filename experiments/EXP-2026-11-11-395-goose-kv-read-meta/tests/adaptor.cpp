#include <cstdint>
#include <cstdio>
extern "C" int64_t fg395s_decide(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t * out2);
extern "C" int fg395s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
int main(void) {
    check(fg395s_enabled() == 1, "enabled");
    int64_t o2;
    for (int64_t ns = 0; ns <= 3; ++ns) check(fg395s_decide(0, ns, 0, 0, 0, 0, &o2) == (ns != 1 ? 1 : 0), "reject");
    for (int e = 0; e <= 1; ++e) check(fg395s_decide(1, e, 0, 0, 0, 0, &o2) == e, "ext");
    for (int empty = 0; empty <= 1; ++empty)
        for (int64_t streams = 0; streams <= 2; ++streams)
            for (int64_t cells = 0; cells <= 3; ++cells)
                for (int64_t want = 0; want <= 3; ++want) {
                    const bool ok = !empty && streams == 1 && cells == want;
                    check(fg395s_decide(2, empty, streams, cells, want, 0, &o2) == (ok ? 1 : 0), "mirror");
                }
    std::printf("FG395S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
