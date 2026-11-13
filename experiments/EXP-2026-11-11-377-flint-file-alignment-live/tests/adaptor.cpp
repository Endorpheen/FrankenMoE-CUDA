#include <cstdint>
#include <cstdio>
#include <vector>
extern "C" int64_t fg377s_plan(int64_t offset, int64_t size, int64_t alignment, int64_t cap, int64_t * out);
extern "C" int fg377s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
static void ref(int64_t offset, int64_t size, int64_t alignment, int64_t cap, int64_t out[4]) {
    const int64_t aligned_offset = offset & ~(alignment - 1);
    const int64_t off_from = offset - aligned_offset;
    const int64_t bytes = (off_from + size + alignment - 1) & ~(alignment - 1);
    out[0] = aligned_offset; out[1] = bytes; out[2] = off_from;
    out[3] = bytes < cap ? bytes : cap;
}
int main(void) {
    check(fg377s_enabled() == 1, "enabled");
    const int64_t caps[3] = {16, 4096, 1 << 20};
    for (int alg = 0; alg <= 5; ++alg) {
        const int64_t alignment = (int64_t) 1 << alg;
        for (int64_t offset = 0; offset <= 40; offset += 7)
            for (int64_t size = 0; size <= 40; size += 9)
                for (int64_t cap : caps) {
                    int64_t got[4] = {-1, -1, -1, -1}, want[4];
                    fg377s_plan(offset, size, alignment, cap, got);
                    ref(offset, size, alignment, cap, want);
                    for (int i = 0; i < 4; ++i) check(got[i] == want[i], "field");
                }
    }
    std::printf("FG377S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
