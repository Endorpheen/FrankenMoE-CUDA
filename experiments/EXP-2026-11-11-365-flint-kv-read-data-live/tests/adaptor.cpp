// Gate: rejection codes and read masks must match the native header logic.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>

extern "C" int64_t fg365s_read_data(int64_t nl_ref, int64_t nl, int64_t cc, int64_t size, int64_t s_trans,
                                    const int32_t * rp, const int32_t * pp, const int32_t * sp,
                                    int64_t rt, int64_t rr, int64_t pr, int64_t st, int64_t sr,
                                    uint64_t * k_mask, uint64_t * p_mask, uint64_t * v_mask);
extern "C" int fg365s_enabled(void);

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

int main(void) {
    std::mt19937 rng(365011);
    check(fg365s_enabled() == 1, "enabled by default", 0);

    for (int trial = 0; trial < 5000; ++trial) {
        const int64_t nl = (int64_t)(rng() % 5);
        const int64_t nl_ref = (rng() % 6) ? nl : nl + 1;
        const int64_t size = 1 + (int64_t)(rng() % 10);
        const int64_t cc = (int64_t)(rng() % (size + 2));
        const int64_t s_trans = (rng() % 8) ? 0 : 1;
        const int64_t rt = (rng() % 8) ? 1 : 0, rr = (rng() % 8) ? 1 : 0;
        const int64_t pr = (rng() % 8) ? 1 : 0, st = (rng() % 8) ? 1 : 0, sr = (rng() % 8) ? 1 : 0;
        std::vector<int32_t> rp((size_t)(nl ? nl : 1)), pp((size_t)(nl ? nl : 1)), sp((size_t)(nl ? nl : 1));
        for (int64_t i = 0; i < nl; ++i) {
            rp[(size_t)i] = rng() & 1; pp[(size_t)i] = rng() & 1; sp[(size_t)i] = rng() & 1;
        }

        int64_t code = 0; uint64_t km = 0, pm = 0, vm = 0;
        if (nl_ref != nl) code = 1;
        else if (cc > size) code = 2;
        else if (s_trans) code = 3;
        else {
            for (int64_t il = 0; il < nl && !code; ++il) {
                if (!rp[(size_t)il]) continue;
                if (!rt) { code = 4; break; }
                if (!rr) { code = 5; break; }
                if (cc > 0) km |= (1ULL << il);
                if (pp[(size_t)il]) {
                    if (!pr) { code = 6; break; }
                    if (cc > 0) pm |= (1ULL << il);
                }
            }
            for (int64_t il = 0; il < nl && !code; ++il) {
                if (!sp[(size_t)il]) continue;
                if (!st) { code = 7; break; }
                if (!sr) { code = 8; break; }
                if (cc > 0) vm |= (1ULL << il);
            }
        }

        uint64_t gk = 0, gp = 0, gv = 0;
        const int64_t got = fg365s_read_data(nl_ref, nl, cc, size, s_trans, rp.data(), pp.data(), sp.data(),
                                             rt, rr, pr, st, sr, &gk, &gp, &gv);
        check(got == code, "code", trial);
        if (code == 0) {
            check(gk == km, "keys mask", trial);
            check(gp == pm, "ple mask", trial);
            check(gv == vm, "values mask", trial);
        }
    }

    std::printf("FG365S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
