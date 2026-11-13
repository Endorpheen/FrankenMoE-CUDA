// Differential gate for the Goose recurrent accessors.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <map>

extern "C" uint8_t fg340dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg340k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg340k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg340k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg340k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg340k_fail(int64_t c) { std::fprintf(stderr, "FG340K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 12), cnt(1);
struct Rec { int64_t k, a, b; };

static int64_t call(int64_t op, int64_t n_cells, std::vector<int64_t> & cpos, std::vector<int64_t> & cmask,
                    int64_t seq, int64_t nsm, int64_t nrs, std::vector<int64_t> & rs, int64_t idx,
                    std::vector<int64_t> & keys, std::vector<int64_t> & sizes, int64_t nbuf,
                    std::vector<int64_t> & rp, std::vector<int64_t> & sp, std::vector<int64_t> & pp,
                    std::vector<int64_t> & rnb, std::vector<int64_t> & snb, std::vector<int64_t> & pnb,
                    std::vector<Rec> & recs) {
    for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
    std::fill(out.begin(), out.end(), -99); cnt[0] = 0;
    g_packet[0][0] = op; g_packet[0][1] = (int64_t)(uintptr_t)out.data();
    g_packet[0][2] = n_cells;
    g_packet[0][3] = (int64_t)(uintptr_t)cpos.data(); g_packet[0][4] = (int64_t)(uintptr_t)cmask.data();
    g_packet[0][5] = seq; g_packet[0][6] = nsm; g_packet[0][7] = nrs;
    g_packet[0][8] = (int64_t)(uintptr_t)rs.data(); g_packet[0][9] = idx;
    g_packet[0][10] = (int64_t)(uintptr_t)keys.data(); g_packet[0][11] = (int64_t)(uintptr_t)sizes.data();
    g_packet[0][12] = nbuf;
    g_packet[0][13] = (int64_t)(uintptr_t)rnb.data(); g_packet[0][14] = (int64_t)(uintptr_t)snb.data();
    g_packet[0][15] = (int64_t)(uintptr_t)pnb.data();
    g_packet[1][0] = (int64_t)(uintptr_t)cnt.data();
    g_packet[1][1] = (int64_t)(uintptr_t)rp.data();
    g_packet[1][2] = (int64_t)(uintptr_t)sp.data();
    g_packet[1][3] = (int64_t)(uintptr_t)pp.data();
    if (!fg340dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    recs.clear();
    for (int64_t i = 0; i < cnt[0]; ++i) recs.push_back({out[i*3], out[i*3+1], out[i*3+2]});
    return g_packet[2][1];
}

int main(void) {
    std::mt19937 rng(340011);

    for (int trial = 0; trial < 3000; ++trial) {
        const int64_t n_cells = (int64_t)(rng() % 8);
        const int64_t nsm = 1 + (int64_t)(rng() % 4);
        std::vector<int64_t> cpos((size_t)n_cells), cmask((size_t)n_cells);
        for (int64_t i = 0; i < n_cells; ++i) {
            cpos[(size_t)i] = (int64_t)(rng() % 20);
            int64_t m = 0;
            for (int64_t q = 0; q < nsm; ++q) if (rng() & 1) m |= (1LL << q);
            cmask[(size_t)i] = m;
        }
        std::vector<int64_t> e, rs((size_t)nsm, 0);
        for (int64_t q = 0; q < nsm; ++q) rs[(size_t)q] = (int64_t)(rng() % 5);
        for (int64_t seq = 0; seq < nsm; ++seq) {
            int64_t mn = INT64_MAX, mx = -1;
            for (int64_t i = 0; i < n_cells; ++i)
                if (cmask[(size_t)i] & (1LL << seq)) { mn = std::min(mn, cpos[(size_t)i]); mx = std::max(mx, cpos[(size_t)i]); }
            if (mn == INT64_MAX) mn = -1;
            std::vector<Rec> got;
            check(call(0, n_cells, cpos, cmask, seq, nsm, 0, rs, 0, e, e, 0, e, e, e, e, e, e, got) == mn, "pos_min", trial);
            check(got.size() == 1 && got[0].b == mn, "pos_min record", trial);
            std::vector<Rec> got2;
            check(call(1, n_cells, cpos, cmask, seq, nsm, 0, rs, 0, e, e, 0, e, e, e, e, e, e, got2) == mx, "pos_max", trial);
            check(got2.size() == 1 && got2[0].b == mx, "pos_max record", trial);
        }

        // set_rs_idx
        const int64_t seq = (rng() % 5 == 0) ? -1 : (int64_t)(rng() % nsm);
        const int64_t nrs = (int64_t)(rng() % 4);
        const int64_t idx = (int64_t)(rng() % (nrs + 1));   // inclusive bound
        std::vector<int64_t> rs2((size_t)nsm, 7);
        std::vector<Rec> got3;
        const int64_t ret = call(2, 0, e, e, seq, nsm, nrs, rs2, idx, e, e, 0, e, e, e, e, e, e, got3);
        if (seq < 0) {
            check(ret == 0 && got3.size() == (size_t)nsm, "rs_idx zeroed", trial);
            for (int64_t q = 0; q < nsm; ++q) check(rs2[(size_t)q] == 0, "rs_idx value", trial);
        } else {
            check(ret == idx, "rs_idx ret", trial);
            check(rs2[(size_t)seq] == idx, "rs_idx written", trial);
            for (int64_t q = 0; q < nsm; ++q) if (q != seq) check(rs2[(size_t)q] == 7, "rs_idx untouched", trial);
        }
    }

    for (int trial = 0; trial < 2000; ++trial) {
        const int64_t nbuf = (int64_t)(rng() % 6);
        std::vector<int64_t> keys((size_t)nbuf), sizes((size_t)nbuf);
        std::map<int64_t,int64_t> want;
        for (int64_t i = 0; i < nbuf; ++i) {
            keys[(size_t)i] = (int64_t)(rng() % 3);
            // zero-sized buffers happen and are what separates "empty list" from "zero total"
            sizes[(size_t)i] = (rng() % 4) ? 0 : (int64_t)(rng() % 500);
            want[keys[(size_t)i]] += sizes[(size_t)i];
        }
        std::vector<int64_t> cpos(1), cmask(1), rs(1), e, got;
        std::vector<Rec> gotr;
        call(3, 0, cpos, cmask, 0, 1, 0, rs, 0, keys, sizes, nbuf, e, e, e, e, e, e, gotr);
        check(gotr.size() == (int64_t)want.size(), "breakdown keys", trial);
        for (auto & r : gotr) check(want.count(r.a) && want[r.a] == r.b, "breakdown sum", trial);

        // is_empty
        std::vector<Rec> got4;
        const int64_t empty = call(6, 0, cpos, cmask, 0, 1, 0, rs, 0, keys, sizes, nbuf, e, e, e, e, e, e, got4);
        check(empty == (nbuf == 0 ? 1 : 0), "is_empty", trial);

        // can_shift
        std::vector<Rec> got5;
        check(call(5, 0, cpos, cmask, 0, 1, 0, rs, 0, e, e, 0, e, e, e, e, e, e, got5) == 1, "can_shift", trial);
    }

    for (int trial = 0; trial < 2000; ++trial) {
        const int64_t nl = (int64_t)(rng() % 6);
        std::vector<int64_t> rp((size_t)nl), sp((size_t)nl), pp((size_t)nl);
        std::vector<int64_t> rnb((size_t)nl), snb((size_t)nl), pnb((size_t)nl);
        int64_t wr = 0, ws = 0, wp = 0;
        for (int64_t i = 0; i < nl; ++i) {
            rp[(size_t)i] = rng() & 1; sp[(size_t)i] = rng() & 1; pp[(size_t)i] = rng() & 1;
            rnb[(size_t)i] = (int64_t)(rng() % 1000);
            snb[(size_t)i] = (int64_t)(rng() % 1000);
            pnb[(size_t)i] = (int64_t)(rng() % 1000);
            if (rp[(size_t)i]) wr += rnb[(size_t)i];
            if (sp[(size_t)i]) ws += snb[(size_t)i];
            if (pp[(size_t)i]) wp += pnb[(size_t)i];
        }
        std::vector<int64_t> e, rs(1);
        std::vector<Rec> got;
        const int64_t ret = call(4, nl, e, e, 0, 1, 0, rs, 0, e, e, 0, rp, sp, pp, rnb, snb, pnb, got);
        check(ret == wr + ws + wp, "sizes total", trial);
        check(got.size() == 3 && got[0].a == wr && got[1].a == ws && got[2].a == wp, "sizes per family", trial);
    }

    std::printf("FG340K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
