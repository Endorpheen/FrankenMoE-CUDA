// Differential gate for the Goose hybrid pool layout.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <random>

extern "C" uint8_t fg310dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg310k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg310k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg310k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg310k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg310k_fail(int64_t c) { std::fprintf(stderr, "FG310K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 14);

static int64_t call(int64_t op, int64_t kpool, bool by_order, int64_t stale,
                    const std::vector<int64_t> & pos, const std::vector<int64_t> & cell,
                    int64_t prev_n, int64_t prev_pos_min, int64_t prev_j_next, int64_t rank_pos, int64_t rank_cell) {
    for (int i = 0; i < 16; ++i) g_packet[0][i] = 0;
    g_packet[0][0] = op;
    g_packet[0][1] = (int64_t)(uintptr_t)out.data();
    g_packet[0][2] = kpool;
    g_packet[0][3] = by_order ? 1 : 0;
    g_packet[0][4] = stale;
    g_packet[0][5] = (int64_t)(uintptr_t)pos.data();
    g_packet[0][6] = (int64_t)(uintptr_t)cell.data();
    g_packet[0][7] = (int64_t)pos.size();
    g_packet[0][8] = prev_n;
    g_packet[0][9] = prev_pos_min;
    g_packet[0][10] = 0;
    g_packet[0][11] = 0;
    g_packet[0][12] = prev_j_next;
    g_packet[0][13] = rank_pos;
    g_packet[0][14] = rank_cell;
    g_packet[3][0] = 0; g_packet[3][1] = 0;
    if (!fg310dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    return g_packet[3][1];
}

int main(void) {
    std::mt19937 rng(310011);

    for (int trial = 0; trial < 3000; ++trial) {
        const int64_t n_pool = (int64_t)(rng() % 600);
        const int64_t want = std::max<int64_t>(64, ((n_pool + 1 + 63) / 64) * 64);
        check(call(0, n_pool, false, 0, {}, {}, 0, 0, 0, 0, 0) == want, "kpool_pad", trial);
    }

    for (int trial = 0; trial < 3000; ++trial) {
        const int n = (int)(rng() % 12);
        std::vector<std::pair<int,int>> v;
        for (int i = 0; i < n; ++i) {
            const int pos = (int)(rng() % 5);
            v.push_back({pos, i});
        }
        std::sort(v.begin(), v.end());
        std::vector<int64_t> pos(n), cell(n);
        for (int i = 0; i < n; ++i) { pos[i] = v[i].first; cell[i] = v[i].second; }
        const int64_t rp = (int64_t)(rng() % 6);
        const int64_t rc = (int64_t)(rng() % 12);
        int64_t want = -1;
        auto it = std::lower_bound(v.begin(), v.end(), std::make_pair((int)rp, (int)rc));
        if (it != v.end() && it->first == rp && it->second == rc) want = it - v.begin();
        check(call(1, 1, false, 0, pos, cell, 0, 0, 0, rp, rc) == want, "kpool_rank", trial);
    }

    for (int trial = 0; trial < 4000; ++trial) {
        const int64_t kpool = 2 + (int64_t)(rng() % 4);
        const bool by_order = (rng() & 1) != 0;
        const bool stale = (rng() % 3) == 0;
        const int n = (int)(rng() % 24);
        const int base_pos = (int)(rng() % 5);
        std::vector<std::pair<int,int>> v;
        int pos = base_pos;
        for (int i = 0; i < n; ++i) {
            v.push_back({pos, i});
            pos += (rng() % 4 == 0) ? 2 : 1;   // occasional gap
        }
        std::vector<int64_t> p(n), c(n);
        for (int i = 0; i < n; ++i) { p[i] = v[i].first; c[i] = v[i].second; }

        // Whether the layout resumes or rebuilds.
        const bool clean = !stale;
        const int64_t prev_n = clean ? (int64_t)(rng() % (n + 1)) : 0;
        // Native can only resume from a non-empty previous layout, or from empty to empty.
        const bool clean_resume = clean && (prev_n > 0 || (n == 0 && prev_n == 0));
        const int64_t prev_pos_min = (n > 0) ? p[0] : 0;
        // A stale sequence may still carry an old scan point; a rebuild must drop it.
        const int64_t prev_j_next = clean_resume ? (int64_t)(rng() % (n + 1)) : (stale ? (int64_t)(rng() % (n + 1)) : 0);

        int64_t j = clean_resume ? prev_j_next : 0;
        const int64_t cells_n = n;
        std::vector<int64_t> want_pools;
        // The prefix pools a resumed sequence already carries.
        if (clean_resume) {
            int64_t jj = 0;
            if (by_order) {
                while (jj + kpool <= prev_j_next) { want_pools.push_back(jj); jj += kpool; }
            } else {
                const int64_t pos_min = p.empty() ? 0 : p[0];
                while (jj + kpool <= prev_j_next) {
                    const int64_t p0 = p[jj];
                    if ((p0 - pos_min) % kpool != 0) { ++jj; continue; }
                    bool ok = true;
                    for (int64_t k = 1; k < kpool; ++k) if (p[jj + k] != p0 + k) { ok = false; break; }
                    if (ok) { want_pools.push_back(jj); jj += kpool; } else ++jj;
                }
            }
        }
        const std::vector<int64_t> prefix_pools = want_pools;
        if (by_order) {
            for (; j + kpool <= cells_n; j += kpool) want_pools.push_back(j);
        } else {
            const int64_t pos_min = p.empty() ? 0 : p[0];
            while (j + kpool <= cells_n) {
                const int64_t p0 = p[j];
                if ((p0 - pos_min) % kpool != 0) { ++j; continue; }
                bool ok = true;
                for (int64_t k = 1; k < kpool; ++k) if (p[j + k] != p0 + k) { ok = false; break; }
                if (ok) { want_pools.push_back(j); j += kpool; }
                else ++j;
            }
        }

        static std::vector<int64_t> prev_pools_buf;
        prev_pools_buf = prefix_pools;   // what the module already carries
        for (int i = 0; i < 16; ++i) g_packet[0][i] = 0;
        g_packet[0][0] = 2;
        g_packet[0][1] = (int64_t)(uintptr_t)out.data();
        g_packet[0][2] = kpool;
        g_packet[0][3] = by_order ? 1 : 0;
        g_packet[0][4] = stale ? 1 : 0;
        g_packet[0][5] = (int64_t)(uintptr_t)p.data();
        g_packet[0][6] = (int64_t)(uintptr_t)c.data();
        g_packet[0][7] = n;
        g_packet[0][8] = prev_n;
        g_packet[0][9] = prev_pos_min;
        g_packet[0][10] = (int64_t)(uintptr_t)prev_pools_buf.data();
        g_packet[0][11] = (int64_t)prev_pools_buf.size();
        g_packet[0][12] = prev_j_next;
        g_packet[3][0] = 0; g_packet[3][1] = 0;
        if (!fg310dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
        const int64_t got = g_packet[3][1];
        if (got != (int64_t)want_pools.size() && failures < 3)
            std::fprintf(stderr, "DBG n=%d k=%lld ord=%d stale=%d prev_n=%lld pjn=%lld want=%zu got=%lld jwant=%lld jgot=%lld\n",
                n, (long long)kpool, (int)by_order, (int)stale, (long long)prev_n, (long long)prev_j_next,
                want_pools.size(), (long long)got, (long long)j, (long long)out[1]);
        if (got != (int64_t)want_pools.size() && failures < 3)
            std::fprintf(stderr, "   cells_n_out=%lld prev_pool_n=%lld pjn=%lld k=%lld pools_out:", (long long)out[2], (long long)out[3], (long long)out[4], (long long)out[5]);
        if (got != (int64_t)want_pools.size() && failures < 3) { for (int q = 0; q < (int)got; ++q) std::fprintf(stderr, " %lld", (long long)out[1200+q]); std::fprintf(stderr, "\n"); }
        check(got == (int64_t)want_pools.size(), "pool count", trial);
        check(out[0] == (int64_t)want_pools.size(), "pool count out", trial);
        check(out[1] == j, "scan point", trial);
        check(out[2] == cells_n, "cell count", trial);
        for (size_t i = 0; i < want_pools.size(); ++i) {
            check(out[1200 + (int64_t)i] == want_pools[i], "pool start", trial);
        }
    }

    std::printf("FG310K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
