// Differential gate for the Goose hybrid memory policy.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <map>
#include <algorithm>

extern "C" uint8_t fg335dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg335k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg335k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg335k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg335k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg335k_fail(int64_t c) { std::fprintf(stderr, "FG335K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 12), cnt(1);
struct Rec { int64_t k, a, b; };

static int64_t call(int64_t op, int64_t embd, int64_t n_stream, int64_t n_rs, std::vector<int64_t> & ubtok,
                    int64_t n_ub, int64_t n_used, int64_t n_tok, int64_t recr_ok, int64_t heads,
                    int64_t recr_ok2, int64_t amin, int64_t amax, int64_t rmin, int64_t rmax,
                    std::vector<int64_t> * ak, std::vector<int64_t> * as, std::vector<int64_t> * rk,
                    std::vector<int64_t> * rs, std::vector<Rec> & recs) {
    for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
    std::fill(out.begin(), out.end(), -99); cnt[0] = 0;
    g_packet[0][0] = op; g_packet[0][1] = (int64_t)(uintptr_t)out.data();
    g_packet[0][2] = embd; g_packet[0][3] = n_stream; g_packet[0][4] = n_rs;
    g_packet[0][5] = (int64_t)(uintptr_t)ubtok.data(); g_packet[0][6] = n_ub;
    g_packet[0][7] = n_used; g_packet[0][8] = n_tok; g_packet[0][9] = recr_ok; g_packet[0][10] = heads;
    g_packet[0][11] = recr_ok2; g_packet[0][12] = amin; g_packet[0][13] = amax;
    g_packet[0][14] = rmin; g_packet[0][15] = rmax;
    g_packet[1][0] = (int64_t)(uintptr_t)cnt.data();
    if (ak) {
        g_packet[1][1] = (int64_t)(uintptr_t)ak->data();
        g_packet[1][2] = (int64_t)(uintptr_t)as->data();
        g_packet[1][3] = (int64_t)(uintptr_t)rk->data();
        g_packet[1][4] = (int64_t)(uintptr_t)rs->data();
        g_packet[1][5] = (int64_t)ak->size();
    }
    if (!fg335dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    recs.clear();
    for (int64_t i = 0; i < cnt[0]; ++i) recs.push_back({out[i*3], out[i*3+1], out[i*3+2]});
    return g_packet[2][1];
}

int main(void) {
    std::mt19937 rng(335011);
    int dbgc = 0;

    // init_batch
    for (int trial = 0; trial < 3000; ++trial) {
        const int64_t embd = rng() & 1, n_stream = 1 + (int64_t)(rng() % 3), n_rs = (int64_t)(rng() % 3);
        const int64_t n_ub = (int64_t)(rng() % 5);
        std::vector<int64_t> ubtok(n_ub + 1, 0);
        for (int64_t i = 0; i < n_ub; ++i) ubtok[i] = (rng() % 4 == 0) ? 0 : 1 + (int64_t)(rng() % 8);
        const int64_t n_tok = (int64_t)(rng() % 30);
        const int64_t n_used = (rng() % 3) ? n_tok : (int64_t)(rng() % (n_tok + 1));
        const int64_t recr_ok = (rng() % 5) ? 1 : 0;
        const int64_t heads = (rng() % 5) ? 1 + (int64_t)(rng() % 4) : 0;

        std::vector<Rec> want; int64_t wret = 0;
        const int64_t mode = embd ? 0 : 1;
        const int64_t flag = embd ? 0 : (n_stream == 1 ? 0 : 1);
        const int64_t window = (!embd && n_rs > 0) ? n_rs + 1 : 0;
        want.push_back({0, mode, flag});
        if (mode == 1) want.push_back({1, window, 0});
        for (int64_t i = 0; i < n_ub; ++i) { if (!ubtok[i]) break; want.push_back({2, ubtok[i], 0}); }
        if (n_used < n_tok) wret = 1;
        else {
            want.push_back({3, recr_ok, 0});
            if (!recr_ok) wret = 1;
            else {
                want.push_back({4, heads, 0});
                if (!heads) wret = 1;
            }
        }
        std::vector<Rec> got;
        check(call(0, embd, n_stream, n_rs, ubtok, n_ub, n_used, n_tok, recr_ok, heads, 0, 0, 0, 0, 0,
                   nullptr, nullptr, nullptr, nullptr, got) == wret, "init status", trial);
        check(got.size() == want.size(), "init count", trial);
        for (size_t i = 0; i < want.size() && i < got.size(); ++i)
            check(got[i].k == want[i].k && got[i].a == want[i].a && got[i].b == want[i].b, "init record", trial);
    }

    // seq_rm ordering and refusal
    for (int trial = 0; trial < 2000; ++trial) {
        const int64_t recr_ok = rng() & 1;
        std::vector<Rec> want, got; std::vector<int64_t> e;
        want.push_back({5, 0, 0});
        if (recr_ok) want.push_back({6, 0, 0});
        check(call(1, 0, 1, 0, e, 0, 0, 0, 0, 0, recr_ok, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr, got)
              == (recr_ok ? 1 : 0), "rm status", trial);
        check(got.size() == want.size(), "rm count", trial);
        for (size_t i = 0; i < want.size() && i < got.size(); ++i)
            check(got[i].k == want[i].k, "rm order", trial);
    }

    // mutation pair order
    for (int trial = 0; trial < 500; ++trial) {
        std::vector<int64_t> e; std::vector<Rec> got;
        call(2, 0, 1, 0, e, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr, got);
        check(got.size() == 2 && got[0].k == 6 && got[1].k == 5, "mutation order", trial);
    }

    // state_read transaction
    for (int trial = 0; trial < 1500; ++trial) {
        const int64_t recr_ok = rng() % 3 ? 1 : 0;
        std::vector<int64_t> e, ak = {7}; std::vector<Rec> got;
        const int64_t ret = call(3, 0, 1, 0, e, 0, 0, 0, 0, 0, recr_ok, 0, 0, 0, 0, &ak, &ak, &ak, &ak, got);
        if (recr_ok) {
            check(ret == 0 && got.size() == 2 && got[0].k == 6 && got[1].k == 5, "read ok", trial);
        } else {
            check(ret == -1, "read throws", trial);
            check(got.size() == 3 && got[2].k == 7 && got[2].a == 7, "attention cleared", trial);
        }
    }

    // extrema
    for (int trial = 0; trial < 2000; ++trial) {
        const int64_t amin = (int64_t)(rng() % 20) - 1, amax = (int64_t)(rng() % 20) - 1;
        const int64_t rmin = (int64_t)(rng() % 20) - 1, rmax = (int64_t)(rng() % 20) - 1;
        std::vector<int64_t> e; std::vector<Rec> got;
        const int64_t lo = std::max(amin, rmin), hi = std::min(amax, rmax);
        check(call(4, 0, 1, 0, e, 0, 0, 0, 0, 0, 1, amin, amax, rmin, rmax, nullptr, nullptr, nullptr, nullptr, got) == lo,
              "min is the larger", trial);
        check(got.size() == 1 && got[0].a == lo && got[0].b == hi, "max is the smaller", trial);
    }

    // breakdown merge
    for (int trial = 0; trial < 1500; ++trial) {
        const int64_t nb = 1 + (int64_t)(rng() % 4);
        std::vector<int64_t> ak(nb), as(nb), rk(nb), rs(nb);
        std::map<int64_t,int64_t> want;
        {
            // both sides are maps, so keys are unique within each side; the two sides may overlap
            std::vector<int64_t> pool;
            for (int64_t i = 0; i < nb; ++i) { ak[i] = i; as[i] = (int64_t)(rng() % 100); }
            // some recurrent keys are absent on the attention side and must be inserted
            for (int64_t i = 0; i < nb; ++i) { rk[i] = (rng() % 3) ? i : 100 + i; rs[i] = (int64_t)(rng() % 100); }
            (void)pool;
        }
        // the merge adds every recurrent row whose key matches, per attention row
        std::vector<Rec> got;
        std::vector<int64_t> empty_vec;
        call(5, 0, 1, 0, empty_vec, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, &ak, &as, &rk, &rs, got);
        {
            std::vector<int64_t> keys;
            for (int64_t i = 0; i < nb; ++i) keys.push_back(ak[(size_t)i]);
            for (int64_t i = 0; i < nb; ++i)
                if (std::find(keys.begin(), keys.end(), rk[(size_t)i]) == keys.end()) keys.push_back(rk[(size_t)i]);
            check(got.size() == (int64_t)keys.size(), "breakdown count", trial);
            for (size_t i = 0; i < keys.size() && i < got.size(); ++i) {
                int64_t total = 0;
                for (int64_t j = 0; j < nb; ++j) {
                    if (ak[(size_t)j] == keys[i]) total += as[(size_t)j];
                    if (rk[(size_t)j] == keys[i]) total += rs[(size_t)j];
                }
                check(got[i].a == keys[i] && got[i].b == total, "breakdown sum", trial);
            }
        }
    }

    std::printf("FG335K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
