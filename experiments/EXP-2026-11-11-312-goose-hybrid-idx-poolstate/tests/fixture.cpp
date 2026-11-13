// Differential gate for the Goose k-pool state marking.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <random>

extern "C" uint8_t fg312dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg312k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg312k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg312k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg312k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg312k_fail(int64_t c) { std::fprintf(stderr, "FG312K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static uint32_t kpool_pad(uint32_t n_pool) {
    return std::max<uint32_t>(64u, ((n_pool + 1 + 63u) / 64u) * 64u);
}

struct Seq {
    std::vector<int64_t> cpos, ccell;
    std::vector<uint32_t> pools;
    uint32_t strm = 0;
};

int main(void) {
    std::mt19937 rng(312011);
    static std::vector<int64_t> is_new(1 << 12), rep_gen(1 << 12), genbuf(1), nnewbuf(1), nngbuf(1);
    static std::vector<int64_t> pools_off, pools_cnt, cells_off, cells_cnt_all, cpos_all, ccell_all, strm_all, ub_seq, tok_cell, tok_nsid, posv, stale;

    for (int trial = 0; trial < 3000; ++trial) {
        const uint32_t kpool = 2 + (uint32_t)(rng() % 3);
        const uint32_t kv_size = 8 + (uint32_t)(rng() % 200);
        const uint32_t n_stream = 1 + (uint32_t)(rng() % 3);
        const uint32_t n_seq_max = 1 + (uint32_t)(rng() % 8);
        const bool by_order = (rng() & 1) != 0;
        const int n_seq = 1 + (int)(rng() % 3);
        const int64_t i_cur = (rng() % 3 == 0) ? 0 : 1;

        std::vector<Seq> seqs(n_seq);
        pools_off.assign(n_seq, 0); pools_cnt.assign(n_seq, 0);
        cells_off.assign(n_seq, 0);
        cells_cnt_all.assign(n_seq, 0);
        cpos_all.clear(); ccell_all.clear(); strm_all.assign(n_seq, 0);
        uint32_t n_pool_real = 0;
        for (int s = 0; s < n_seq; ++s) {
            const int ncells = (int)(rng() % 16);
            std::vector<std::pair<int64_t,int64_t>> cv;
            int p = (int)(rng() % 3);
            for (int i = 0; i < ncells; ++i) {
                cv.push_back({p, (int64_t)(rng() % 64)});
                p += (int)(rng() % 3);
            }
            // cells are kept sorted by (position, cell), which is the order every lookup assumes
            std::sort(cv.begin(), cv.end());
            for (auto & pr : cv) { seqs[s].cpos.push_back(pr.first); seqs[s].ccell.push_back(pr.second); }
            for (uint32_t j = 0; j + kpool <= seqs[s].cpos.size(); j += kpool) seqs[s].pools.push_back(j);
            seqs[s].strm = (uint32_t)(rng() % n_stream);
            pools_off[s] = (int64_t)pools_off.size() == 0 ? 0 : 0;
            pools_cnt[s] = (int64_t)seqs[s].pools.size();
            cells_off[s] = (int64_t)cpos_all.size();
            cells_cnt_all[s] = (int64_t)seqs[s].cpos.size();
            for (size_t i = 0; i < seqs[s].cpos.size(); ++i) { cpos_all.push_back(seqs[s].cpos[i]); ccell_all.push_back(seqs[s].ccell[i]); }
            strm_all[s] = seqs[s].strm;
            n_pool_real += (uint32_t)seqs[s].pools.size();
        }
        // pools are laid out consecutively
        int64_t acc = 0;
        for (int s = 0; s < n_seq; ++s) { pools_off[s] = acc; acc += pools_cnt[s]; }
        std::vector<int64_t> pools_all;
        for (int s = 0; s < n_seq; ++s) for (uint32_t j : seqs[s].pools) pools_all.push_back(j);

        // Large batches make the graph-size bound exceed the padding slot, which is what makes the
        // bound formula observable at all.
        const int n_tokens = (rng() % 2) ? (int)(rng() % 6) : (int)(rng() % 300);
        posv.assign(n_tokens, 0); ub_seq.assign((size_t)n_tokens * 4, 0);
        tok_cell.assign(n_tokens, 0); tok_nsid.assign(n_tokens, 1);
        for (int i = 0; i < n_tokens; ++i) { posv[i] = (int64_t)(rng() % 24); ub_seq[i*4] = (int64_t)(rng() % n_seq); tok_cell[i] = (int64_t)(rng() % 64); }
        stale.assign(n_seq, 2147483647);
        for (int s = 0; s < n_seq; ++s) if (rng() % 3 == 0) stale[s] = (int64_t)(rng() % 24);

        // reference
        genbuf[0] = (genbuf[0] + 1) & 0x7fffffff;
        int64_t gen = genbuf[0];
        std::fill(is_new.begin(), is_new.end(), 0);
        std::fill(rep_gen.begin(), rep_gen.end(), 0);
        int64_t n_new = 0;
        auto mark = [&](int s, int k) {
            const int64_t j = seqs[s].pools[k];
            const int64_t rep = (int64_t)seqs[s].strm*kv_size + seqs[s].ccell[j + kpool - 1];
            if (rep_gen[rep] != gen) { rep_gen[rep] = gen; is_new[pools_off[s] + k] = gen; ++n_new; }
        };
        for (int s = 0; s < n_seq; ++s) {
            const int64_t stale_from = (i_cur == 0) ? stale[s] : 2147483647;
            if (stale_from == 2147483647) continue;
            auto first = std::lower_bound(seqs[s].pools.begin(), seqs[s].pools.end(), stale_from,
                    [&](uint32_t j, int64_t p) { return seqs[s].cpos[j + kpool - 1] < p; });
            for (auto it = first; it != seqs[s].pools.end(); ++it) mark(s, (int)(it - seqs[s].pools.begin()));
        }
        for (int i = 0; i < n_tokens; ++i) {
            const int64_t p = posv[i];
            for (int t = 0; t < tok_nsid[i]; ++t) {
                const int s = (int)ub_seq[i*4 + t];
                auto & sq = seqs[s];
                if (by_order) {
                    int64_t r = -1;
                    for (size_t q = 0; q < sq.cpos.size(); ++q) if (sq.cpos[q] == p && sq.ccell[q] == tok_cell[i]) { r = (int64_t)q; break; }
                    if (r >= 0 && (size_t)(r / kpool) < sq.pools.size()) mark(s, (int)(r / kpool));
                    continue;
                }
                auto it = std::upper_bound(sq.pools.begin(), sq.pools.end(), p,
                        [&](int64_t pos, uint32_t j) { return pos < sq.cpos[j]; });
                if (it == sq.pools.begin()) continue;
                --it;
                if (p <= sq.cpos[*it + kpool - 1]) mark(s, (int)(it - sq.pools.begin()));
            }
        }
        uint32_t n_pool_max = kv_size / kpool * n_seq_max;
        uint32_t bound = (uint32_t)n_tokens / kpool + (uint32_t)n_seq;
        uint32_t cap = kpool_pad(n_pool_real) - 1;
        if (cap > n_pool_max) cap = n_pool_max;
        int64_t want_nng = std::max<int64_t>({n_new, 1, (int64_t)std::min({bound, cap, n_pool_max})});

        // goose arm: rebuild its own gen arrays from the same starting point
        static std::vector<int64_t> gen2(1), nnew2(1), nng2(1);
        static std::vector<int64_t> isnew2(1 << 12), rep2(1 << 12);
        static bool inited = false;
        if (!inited) { gen2[0] = genbuf[0] - 1; std::fill(isnew2.begin(), isnew2.end(), 0); std::fill(rep2.begin(), rep2.end(), 0); inited = true; }
        // keep both arms on the same generation: set the goose gen to the reference's previous value
        gen2[0] = gen - 1;

        for (int i = 0; i < 16; ++i) g_packet[0][i] = 0;
        g_packet[0][0] = kpool;
        g_packet[0][1] = kv_size;
        g_packet[0][2] = n_stream;
        g_packet[0][3] = n_seq_max;
        g_packet[0][4] = n_pool_real;
        g_packet[0][5] = by_order ? 1 : 0;
        g_packet[0][6] = n_tokens;
        g_packet[0][7] = n_seq;
        g_packet[0][8] = i_cur;
        g_packet[0][9] = (int64_t)(uintptr_t)posv.data();
        g_packet[0][10] = (int64_t)(uintptr_t)ub_seq.data();
        g_packet[0][11] = 4;
        g_packet[0][12] = (int64_t)(uintptr_t)stale.data();
        g_packet[0][13] = (int64_t)(uintptr_t)tok_cell.data();
        g_packet[0][14] = (int64_t)(uintptr_t)tok_nsid.data();
        g_packet[1][0] = (int64_t)(uintptr_t)pools_off.data();
        g_packet[1][1] = (int64_t)(uintptr_t)pools_cnt.data();
        g_packet[1][2] = (int64_t)(uintptr_t)cells_off.data();
        g_packet[1][3] = (int64_t)(uintptr_t)cpos_all.data();
        g_packet[1][4] = (int64_t)(uintptr_t)ccell_all.data();
        g_packet[1][5] = (int64_t)(uintptr_t)strm_all.data();
        g_packet[1][6] = (int64_t)(uintptr_t)isnew2.data();
        g_packet[1][7] = (int64_t)(uintptr_t)rep2.data();
        g_packet[1][8] = (int64_t)(uintptr_t)gen2.data();
        g_packet[1][9] = (int64_t)(uintptr_t)nnew2.data();
        g_packet[1][10] = (int64_t)(uintptr_t)nng2.data();
        g_packet[1][11] = n_seq;
        g_packet[1][12] = (int64_t)(uintptr_t)cells_cnt_all.data();
        g_packet[1][13] = (int64_t)(uintptr_t)pools_all.data();
        static std::vector<int64_t> probe(8);
        g_packet[2][0] = (int64_t)(uintptr_t)probe.data();
        g_packet[3][0] = 0; g_packet[3][1] = 0;
        if (!fg312dh_run((int64_t)(uintptr_t)g_packet)) std::abort();

        check(g_packet[3][1] == n_new, "n_new", trial);
        check(nnew2[0] == n_new, "n_new out", trial);
        check(nng2[0] == want_nng, "n_new_g", trial);
        // Marks are generation stamps, so compare membership in the current generation, not the raw
        // value: the arrays are not cleared between steps, only on a generation wrap.
        for (int i = 0; i < (int)n_pool_real; ++i) {
            check((isnew2[i] == gen) == (is_new[i] == gen), "is_new marks", trial);
        }
    }

    std::printf("FG312K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
