#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>

extern "C" int64_t fg366s_read_header(int64_t nl_ref, int64_t nl, int64_t cc, int64_t size,
                                      int64_t tr_ref, int64_t tr, int64_t rk_ref, int64_t rk,
                                      int64_t rv_ref, int64_t rv, const int32_t * kt, const int32_t * kr,
                                      const int32_t * vt, const int32_t * vr,
                                      uint64_t * k_mask, uint64_t * v_mask);
extern "C" int fg366s_enabled(void);

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

int main(void) {
    std::mt19937 rng(366011);
    check(fg366s_enabled() == 1, "enabled by default", 0);
    for (int trial = 0; trial < 5000; ++trial) {
        const int64_t nl = (int64_t)(rng() % 5);
        const int64_t nl_ref = (rng() % 6) ? nl : nl + 1;
        const int64_t size = 1 + (int64_t)(rng() % 10);
        const int64_t cc = (int64_t)(rng() % (size + 2));
        const int64_t tr_ref = (rng() % 6) ? 0 : 1, tr = (rng() % 6) ? 0 : 1;
        const int64_t rk_ref = (int64_t)(rng() % 4) * 64, rk = (int64_t)(rng() % 4) * 64;
        const int64_t rv_ref = (int64_t)(rng() % 4) * 64, rv = (int64_t)(rng() % 4) * 64;
        std::vector<int32_t> kt((size_t)(nl ? nl : 1)), kr(kt.size()), vt(kt.size()), vr(kt.size());
        for (int64_t i = 0; i < nl; ++i) {
            kt[(size_t)i] = (rng() % 8) ? 1 : 0; kr[(size_t)i] = (rng() % 8) ? 1 : 0;
            vt[(size_t)i] = (rng() % 8) ? 1 : 0; vr[(size_t)i] = (rng() % 8) ? 1 : 0;
        }
        int64_t code = 0;
        if (nl_ref != nl) code = 1;
        else if (cc > size) code = 2;
        else if (tr_ref != tr) code = 3;
        else if (rk_ref != rk) code = 4;
        else if (rv_ref != rv) code = 5;
        else for (int64_t il = 0; il < nl && !code; ++il) {
            if (!kt[(size_t)il]) code = 6;
            else if (!kr[(size_t)il]) code = 7;
            else if (!vt[(size_t)il]) code = 8;
            else if (!vr[(size_t)il]) code = 9;
        }
        uint64_t km = 0, vm = 0;
        const int64_t got = fg366s_read_header(nl_ref, nl, cc, size, tr_ref, tr, rk_ref, rk, rv_ref, rv,
                                               kt.data(), kr.data(), vt.data(), vr.data(), &km, &vm);
        check(got == code, "code", trial);
        if (code == 0) {
            const uint64_t want = cc > 0 ? (((uint64_t)1 << nl) - 1) : 0;
            check(km == want && vm == want, "masks", trial);
        }
    }
    std::printf("FG366S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
