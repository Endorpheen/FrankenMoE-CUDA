// Differential gate for the Goose k-pool layout pass.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <algorithm>
#include <utility>

extern "C" uint8_t fg342dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg342k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg342k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg342k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg342k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg342k_fail(int64_t c) { std::fprintf(stderr, "FG342K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 15), cnt(1);
struct Rec { int64_t k, a, b; };

int main(void) {
    std::mt19937 rng(342011);
    const int64_t POS_CLEAN = INT64_MAX;

    for (int trial = 0; trial < 4000; ++trial) {
        const int64_t n_seq = 1 + (int64_t)(rng() % 4);
        const int64_t kpool = 1 + (int64_t)(rng() % 4);
        const int64_t n_stream_kv = 1 + (int64_t)(rng() % 3);
        const int64_t unified = rng() & 1;
        const int64_t by_order = rng() & 1;

        std::vector<int64_t> stale((size_t)n_seq), cn((size_t)n_seq), pn((size_t)n_seq);
        std::vector<int64_t> cpos, ccell, ppos, pcell, pj((size_t)n_seq), ppmin((size_t)n_seq);
        std::vector<std::vector<std::pair<int64_t,int64_t>>> cells((size_t)n_seq), prev((size_t)n_seq);

        for (int64_t s = 0; s < n_seq; ++s) {
            const int64_t n = (int64_t)(rng() % 9);
            std::vector<std::pair<int64_t,int64_t>> v;
            int64_t p = 0;
            for (int64_t i = 0; i < n; ++i) {
                // repeated positions happen (M-RoPE images): they make the (pos, cell) tie-break real
                if (rng() % 4 != 0) p += (rng() % 3 == 0) ? 1 + (int64_t)(rng() % 2) : 1;
                v.push_back({p, (int64_t)(rng() % 4)});
            }
            std::sort(v.begin(), v.end());
            cells[(size_t)s] = v;
            cn[(size_t)s] = n;
            // previous layout: a prefix of the current list (the normal case), a prefix with a changed
            // first position, or an unrelated list (the layout must then rebuild on the count alone)
            std::vector<std::pair<int64_t,int64_t>> pv;
            const int64_t pnc = (rng() % 3 == 0) ? 0 : (int64_t)(rng() % (n + 2));
            for (int64_t i = 0; i < std::min<int64_t>(pnc, n); ++i) pv.push_back(v[(size_t)i]);
            if (rng() % 3 == 1 && !pv.empty()) pv[0].first += 1 + (int64_t)(rng() % 2);
            if (rng() % 3 == 2 && !pv.empty()) {
                for (auto & c : pv) c.first += 5;
            }
            // a layout that starts right but holds cells the sequence no longer has: the resume then
            // covers fewer cells than the sequence and the count is the only thing that notices
            if (rng() % 4 == 0 && !v.empty()) {
                pv.clear();
                pv.push_back(v[0]);
                int64_t pp = v[0].first;
                for (int64_t i = 0; i < 2; ++i) { pp += 2; pv.push_back({pp + 50, (int64_t)(rng() % 4)}); }
            }
            prev[(size_t)s] = pv;
            pn[(size_t)s] = (int64_t)pv.size();
            stale[(size_t)s] = (rng() % 3 == 0) ? (int64_t)(rng() % 5) : POS_CLEAN;
            pj[(size_t)s] = (int64_t)(rng() % 4);
            ppmin[(size_t)s] = pv.empty() ? 0 : pv[0].first;
        }
        std::vector<int64_t> s2s((size_t)n_seq);
        for (int64_t s = 0; s < n_seq; ++s) s2s[(size_t)s] = (int64_t)(rng() % 3);

        // flatten
        for (int64_t s = 0; s < n_seq; ++s) {
            for (auto & c : cells[(size_t)s]) { cpos.push_back(c.first); ccell.push_back(c.second); }
            for (auto & c : prev[(size_t)s]) { ppos.push_back(c.first); pcell.push_back(c.second); }
        }

        // reference
        std::vector<Rec> want; int64_t wpool = 0;
        for (int64_t s = 0; s < n_seq; ++s) {
            if (!unified && s >= n_stream_kv) { want.push_back({6, s, 0}); continue; }
            const auto & cu = cells[(size_t)s];
            const auto & pv = prev[(size_t)s];
            bool rebuild = stale[(size_t)s] != POS_CLEAN || pv.empty() || cu.empty();
            if (!pv.empty() && !cu.empty() && pv[0].first != cu[0].first) rebuild = true;
            int64_t n_final = 0, pos_min;
            if (rebuild) {
                n_final = (int64_t)cu.size();
                pos_min = cu.empty() ? 0 : cu[0].first;
            } else {
                n_final = (int64_t)pv.size();
                pos_min = ppmin[(size_t)s];
                const auto last = pv.back();
                for (auto & c : cu) if (c.first > last.first || (c.first == last.first && c.second > last.second)) ++n_final;
                if (n_final != (int64_t)cu.size()) rebuild = true;
            }
            // a rebuild after a resume attempt re-copies the cell list, so the resume length must not survive
            if (rebuild) { n_final = (int64_t)cu.size(); pos_min = cu.empty() ? 0 : cu[0].first; }
            int64_t j = rebuild ? 0 : pj[(size_t)s];
            int64_t np = 0;
            if (by_order) {
                while (j + kpool <= n_final) { want.push_back({2, s, j}); ++np; j += kpool; }
            } else {
                while (j + kpool <= n_final) {
                    const int64_t pp = cu[(size_t)j].first;
                    if ((pp - pos_min) % kpool != 0) { ++j; continue; }
                    bool ok = true;
                    for (int64_t k = 1; k < kpool; ++k) if (cu[(size_t)(j+k)].first != pp + k) { ok = false; break; }
                    if (ok) { want.push_back({2, s, j}); ++np; j += kpool; } else ++j;
                }
            }
            want.push_back({0, s, rebuild ? 1 : 0});
            want.push_back({1, s, n_final});
            want.push_back({3, s, j});
            want.push_back({4, s, pos_min});
            want.push_back({5, s, s2s[(size_t)s]});
            wpool += np;
        }

        for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
        std::fill(out.begin(), out.end(), -99); cnt[0] = 0;
        g_packet[0][0] = kpool; g_packet[0][1] = n_stream_kv; g_packet[0][2] = unified;
        g_packet[0][3] = (int64_t)(uintptr_t)stale.data();
        g_packet[0][4] = (int64_t)(uintptr_t)cn.data();
        g_packet[0][5] = (int64_t)(uintptr_t)cpos.data();
        g_packet[0][6] = (int64_t)(uintptr_t)ccell.data();
        g_packet[0][7] = by_order;
        g_packet[0][8] = (int64_t)(uintptr_t)pn.data();
        g_packet[0][9] = (int64_t)(uintptr_t)ppos.data();
        g_packet[0][10] = (int64_t)(uintptr_t)pcell.data();
        g_packet[0][11] = (int64_t)(uintptr_t)pj.data();
        g_packet[0][12] = (int64_t)(uintptr_t)ppmin.data();
        g_packet[0][13] = POS_CLEAN;
        g_packet[1][0] = (int64_t)(uintptr_t)out.data();
        g_packet[1][1] = (int64_t)(uintptr_t)cnt.data();
        g_packet[1][2] = (int64_t)(uintptr_t)s2s.data();
        g_packet[1][3] = n_seq;
        if (!fg342dh_run((int64_t)(uintptr_t)g_packet)) std::abort();

        check(g_packet[2][1] == wpool, "pool count", trial);
        check(cnt[0] == (int64_t)want.size(), "record count", trial);
        for (size_t i = 0; i < want.size() && i < (size_t)cnt[0]; ++i)
            check(out[i*3] == want[i].k && out[i*3+1] == want[i].a && out[i*3+2] == want[i].b, "record", trial);
    }

    std::printf("FG342K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
