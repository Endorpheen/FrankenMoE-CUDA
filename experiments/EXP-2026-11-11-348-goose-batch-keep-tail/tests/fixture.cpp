// Differential gate for the batch split tail policy.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg348dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg348k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg348k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg348k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg348k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg348k_fail(int64_t c) { std::fprintf(stderr, "FG348K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 14), cnt(1);
struct Rec { int64_t k, a, b; };

static int64_t call(int64_t n_seqs, std::vector<int64_t> & sizes, std::vector<int64_t> & cur,
                    std::vector<int64_t> & idxs, std::vector<int64_t> & off, std::vector<int64_t> & used,
                    int64_t n_used, int64_t n_ubatch, int64_t n_keep_tail, std::vector<Rec> & recs) {
    for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
    std::fill(out.begin(), out.end(), -99); cnt[0] = 0;
    g_packet[0][0] = n_seqs;
    g_packet[0][1] = (int64_t)(uintptr_t)sizes.data();
    g_packet[0][2] = (int64_t)(uintptr_t)cur.data();
    g_packet[0][3] = (int64_t)(uintptr_t)idxs.data();
    g_packet[0][4] = (int64_t)(uintptr_t)off.data();
    g_packet[0][5] = (int64_t)(uintptr_t)used.data();
    g_packet[0][6] = n_used; g_packet[0][7] = n_ubatch; g_packet[0][8] = n_keep_tail;
    g_packet[0][9] = (int64_t)used.size();
    g_packet[1][0] = (int64_t)(uintptr_t)out.data();
    g_packet[1][1] = (int64_t)(uintptr_t)cnt.data();
    if (!fg348dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    recs.clear();
    for (int64_t i = 0; i < cnt[0]; ++i) recs.push_back({out[i*3], out[i*3+1], out[i*3+2]});
    return g_packet[2][1];
}

int main(void) {
    std::mt19937 rng(348011);

    for (int trial = 0; trial < 5000; ++trial) {
        const int64_t n_seqs = 1 + (int64_t)(rng() % 4);
        const int64_t n_keep_tail = (rng() % 5 == 0) ? 0 : 1 + (int64_t)(rng() % 4);
        const int64_t n_ubatch = n_keep_tail + 1 + (int64_t)(rng() % 8);

        std::vector<int64_t> sizes((size_t)n_seqs), cur((size_t)n_seqs), off((size_t)n_seqs + 1, 0);
        std::vector<int64_t> idxs, used;
        int64_t next_idx = 0;
        for (int64_t s = 0; s < n_seqs; ++s) {
            sizes[(size_t)s] = 1 + (int64_t)(rng() % 6);
            cur[(size_t)s] = (int64_t)(rng() % (sizes[(size_t)s] + 1));
            // the list holds the whole set; cur says how much of it was already taken
            for (int64_t k = 0; k < sizes[(size_t)s]; ++k) idxs.push_back(next_idx++);
            off[(size_t)s + 1] = (int64_t)idxs.size();
        }
        used.assign((size_t)next_idx + 1, 0);
        int64_t n_used = 0;
        for (auto & x : used) { if (rng() & 1) { x = 1; ++n_used; } }

        // reference
        std::vector<Rec> want;
        std::vector<int64_t> rused = used, rcur = cur;
        int64_t rn_used = n_used;
        int64_t r_keep = -1;
        std::vector<int64_t> final_idxs;
        if (n_keep_tail > 0) {
            auto remaining = [&](int64_t s) { return sizes[(size_t)s] - rcur[(size_t)s]; };
            int64_t n_keep = 0;
            while (n_keep < n_seqs) {
                const int64_t rem = remaining(n_keep);
                if (rem != 0 && rem < n_keep_tail) break;
                ++n_keep;
            }
            if (n_keep == 0) {
                std::vector<int64_t> my;
                for (int64_t k = 0; k < rcur[0]; ++k) my.push_back(idxs[(size_t)(off[0] + k)]);
                if ((int64_t)my.size() + remaining(0) <= n_ubatch) {
                    while (remaining(0) > 0) {
                        const int64_t idx = idxs[(size_t)(off[0] + rcur[0])];
                        want.push_back({0, 0, idx});
                        rused[(size_t)idx] = 1; ++rn_used; ++rcur[0];
                        my.push_back(idx);
                    }
                } else {
                    while (remaining(0) < n_keep_tail) {
                        const int64_t last = my.back();
                        want.push_back({1, 0, last});
                        rused[(size_t)last] = 0; --rn_used; my.pop_back(); --rcur[0];
                    }
                }
                n_keep = 1;
                // the surviving first set is the (possibly modified) list
                off[0] = 0; (void)my;
            }
            // only what the set already took in this ubatch goes back to the pool
            for (int64_t s = n_keep; s < n_seqs; ++s)
                for (int64_t k = 0; k < rcur[(size_t)s]; ++k) {
                    const int64_t idx = idxs[(size_t)(off[(size_t)s] + k)];
                    want.push_back({2, s, idx});
                    rused[(size_t)idx] = 0; --rn_used;
                }
            for (int64_t s = 0; s < n_keep; ++s)
                for (int64_t k = 0; k < rcur[(size_t)s]; ++k) {
                    const int64_t idx = idxs[(size_t)(off[(size_t)s] + k)];
                    want.push_back({3, s, idx});
                    final_idxs.push_back(idx);
                }
            want.push_back({4, n_keep, 0});
            r_keep = n_keep;
        }

        std::vector<int64_t> g_used = used, g_cur = cur, g_off = off, g_idxs = idxs;
        std::vector<Rec> got;
        const int64_t ret = call(n_seqs, sizes, g_cur, g_idxs, g_off, g_used, n_used, n_ubatch, n_keep_tail, got);
        check(ret == r_keep, "keep count", trial);
        check(got.size() == want.size(), "record count", trial);
        for (size_t i = 0; i < want.size() && i < got.size(); ++i)
            check(got[i].k == want[i].k && got[i].a == want[i].a && got[i].b == want[i].b, "record", trial);
        for (size_t i = 0; i < g_used.size(); ++i) check(g_used[i] == rused[i], "used state", trial);
    }

    std::printf("FG348K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
