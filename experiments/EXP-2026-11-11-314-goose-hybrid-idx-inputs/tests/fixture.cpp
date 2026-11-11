// Differential gate for the Goose hybrid-idx host input plan.
//
// Both arms read the SAME arena: one seqdesc table (stream, cell range, pool range) plus the cell and
// pool arrays. The transcription never uses its own copies of those fields, which is what removed the
// divergence the first attempt had.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <random>

extern "C" uint8_t fg314dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg314k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg314k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg314k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg314k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg314k_fail(int64_t c) { std::fprintf(stderr, "FG314K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static const int64_t NEG_INF = -16777216;

struct Arena {
    std::vector<int64_t> seqdesc, cpos, ccell, pools, is_new, seq_in_ub, pos, ub_seq, rankv;
    std::vector<int64_t> pcell, pidx, nidx, nrep, npos, mask, selmask, tail, sps, pend;
    int64_t kpool = 0, n_pool = 0, n_pool_real = 0, n_kv = 0, n_stream = 0, n_tokens = 0, n_seq = 0;
    int64_t n_sel = 0, n_top = 0, width = 4, gen = 0, dummy = 0;
    bool by_order = false, sel_present = false;

    int64_t ds(int s, int f) const { return seqdesc[(size_t)s*5 + f]; }
    int64_t d_strm(int s) const { return ds(s, 0); }
    int64_t d_coff(int s) const { return ds(s, 1); }
    int64_t d_ccnt(int s) const { return ds(s, 2); }
    int64_t d_poff(int s) const { return ds(s, 3); }
    int64_t d_pcnt(int s) const { return ds(s, 4); }
};

// The transcription: reads only the arena's seqdesc/cells/pools.
static void ref_tables(const Arena & A, std::vector<int64_t> & w_pcell, std::vector<int64_t> & w_pidx,
                       std::vector<int64_t> & w_nidx, std::vector<int64_t> & w_nrep,
                       std::vector<int64_t> & w_sps, std::vector<int64_t> & w_pend,
                       std::vector<int64_t> & w_npos, int64_t & i_new) {
    const int64_t sentinel = A.n_kv;
    w_pcell.assign(A.n_pool, 0);
    w_pidx.assign((size_t)A.n_pool*A.kpool, 0);
    w_nidx.assign((size_t)A.n_pool*A.kpool, 0);
    w_nrep.assign(A.n_pool, 0);
    w_npos.assign((size_t)4*std::max<int64_t>(1, A.n_pool_real), 0);
    w_sps.assign(A.n_seq, 0);
    w_pend.clear();
    i_new = 0;
    int64_t ip = 0;
    for (int s = 0; s < A.n_seq; ++s) {
        w_sps[s] = ip;
        const bool inert = A.n_stream > 1 && A.seq_in_ub[s] == 0;
        const int64_t strm = A.d_strm(s), base = A.d_coff(s), poff = A.d_poff(s), pcnt = A.d_pcnt(s);
        for (int64_t pi = 0; pi < pcnt; ++pi) {
            const int64_t j = A.pools[poff + pi];
            const int64_t rep = A.ccell[base + j + A.kpool - 1];
            w_pcell[ip] = strm*A.n_kv + rep;
            for (int64_t k = 0; k < A.kpool; ++k)
                w_pidx[ip*A.kpool + k] = inert ? sentinel : A.ccell[base + j + k];
            if (A.is_new[ip] == A.gen) {
                for (int64_t k = 0; k < A.kpool; ++k)
                    w_nidx[i_new*A.kpool + k] = strm*A.n_kv + A.ccell[base + j + k];
                w_nrep[i_new] = strm*A.n_kv + rep;
                w_npos[0*A.n_pool_real + i_new] = A.cpos[base + j];
                w_npos[1*A.n_pool_real + i_new] = 0;
                w_npos[2*A.n_pool_real + i_new] = 0;
                w_npos[3*A.n_pool_real + i_new] = A.cpos[base + j];
                ++i_new;
            }
            w_pend.push_back(A.cpos[base + j + A.kpool - 1]);
            ++ip;
        }
    }
    std::vector<int64_t> reps(w_pcell.begin(), w_pcell.begin() + w_pend.size());
    std::sort(reps.begin(), reps.end());
    int64_t padc = 0;
    for (int64_t i = A.n_pool_real; i < A.n_pool; ++i, ++padc) {
        while (std::binary_search(reps.begin(), reps.end(), padc)) ++padc;
        for (int64_t k = 0; k < A.kpool; ++k) w_nidx[i*A.kpool + k] = padc;
        w_nrep[i] = padc;
    }
    for (int64_t i2 = A.n_pool_real; i2 < A.n_pool; ++i2) {
        w_pcell[i2] = A.dummy;
        for (int64_t k = 0; k < A.kpool; ++k) w_pidx[i2*A.kpool + k] = sentinel;
    }
}

static void ref_masks(const Arena & A, std::vector<int64_t> & w_mask, std::vector<int64_t> & w_sel,
                      std::vector<int64_t> & w_tail) {
    const int64_t sentinel = A.n_kv;
    w_mask.assign((size_t)A.n_tokens*A.n_pool, 0);
    w_sel.assign((size_t)A.n_tokens*A.n_sel, 0);
    w_tail.assign((size_t)A.n_tokens*(A.kpool - 1), 0);
    for (int64_t i = 0; i < A.n_tokens; ++i) {
        const int s = (int)A.ub_seq[i*A.width];
        const int64_t p = A.pos[i];
        const int64_t base = A.d_coff(s);
        const int64_t ccnt = A.d_ccnt(s);
        const int64_t pcnt = A.d_pcnt(s);
        const int64_t p0 = A.sps[s];
        int64_t nv;
        if (A.by_order) {
            nv = std::min<int64_t>(pcnt, (A.rankv[i] + 1)/A.kpool);
        } else {
            nv = 0;
            for (int64_t q = 0; q < pcnt; ++q) if (A.pend[p0 + q] <= p) ++nv;
        }
        for (int64_t q = 0; q < A.n_pool; ++q) w_mask[i*A.n_pool + q] = NEG_INF;
        for (int64_t z = 0; z < nv; ++z) w_mask[i*A.n_pool + p0 + z] = 0;
        if (A.sel_present) {
            const int64_t nvc = std::min(nv, A.n_top);
            for (int64_t t = 0; t < A.n_top*A.kpool; ++t) w_sel[i*A.n_sel + t] = NEG_INF;
            for (int64_t z = 0; z < nvc*A.kpool; ++z) w_sel[i*A.n_sel + z] = 0;
        }
        int64_t n_tail;
        if (A.by_order) n_tail = (A.rankv[i] + 1) % A.kpool;
        else            n_tail = (p - (ccnt > 0 ? A.cpos[base] : 0) + 1) % A.kpool;
        for (int64_t k = 0; k < A.kpool - 1; ++k) {
            int64_t cell = sentinel;
            bool real = false;
            if (k < n_tail) {
                if (A.by_order) {
                    cell = A.ccell[base + A.rankv[i] - k];
                    real = true;
                } else {
                    const int64_t pt = p - k;
                    for (int64_t q = 0; q < ccnt; ++q)
                        if (A.cpos[base + q] == pt) { cell = A.ccell[base + q]; real = true; break; }
                }
            }
            w_tail[i*(A.kpool - 1) + k] = cell;
            if (A.sel_present && A.n_sel % A.kpool != 0)
                w_sel[i*A.n_sel + A.n_top*A.kpool + k] = real ? 0 : NEG_INF;
        }
    }
}

int main(void) {
    std::mt19937 rng(314011);

    for (int trial = 0; trial < 2000; ++trial) {
        Arena A;
        A.kpool = 2 + (int64_t)(rng() % 3);
        A.n_stream = 1 + (int64_t)(rng() % 3);
        A.n_kv = 8 + (int64_t)(rng() % 8);
        A.n_seq = 1 + (int64_t)(rng() % 4);
        A.by_order = (rng() & 1) != 0;
        A.gen = 1 + (int64_t)(rng() % 5);
        A.dummy = 5;
        A.seqdesc.assign((size_t)A.n_seq*5, 0);
        A.cpos.clear(); A.ccell.clear(); A.pools.clear();
        A.seq_in_ub.assign(A.n_seq, 0);
        int64_t n_pool_real = 0;
        for (int s = 0; s < A.n_seq; ++s) {
            const int nc = (int)(rng() % 12);
            std::vector<std::pair<int64_t,int64_t>> cv;
            int64_t p = (int64_t)(rng() % 3);
            for (int i = 0; i < nc; ++i) { cv.push_back({p, (int64_t)(rng() % 40)}); p += 1 + (int64_t)(rng() % 2); }
            std::sort(cv.begin(), cv.end());
            const int64_t coff = (int64_t)A.cpos.size();
            for (auto & pr : cv) { A.cpos.push_back(pr.first); A.ccell.push_back(pr.second); }
            std::vector<int64_t> pl;
            for (uint32_t j = 0; j + A.kpool <= (int64_t)cv.size(); j += A.kpool) pl.push_back(j);
            const int64_t poff = (int64_t)A.pools.size();
            for (int64_t j : pl) A.pools.push_back(j);
            A.seqdesc[s*5 + 0] = (int64_t)(rng() % A.n_stream);
            A.seqdesc[s*5 + 1] = coff;
            A.seqdesc[s*5 + 2] = (int64_t)cv.size();
            A.seqdesc[s*5 + 3] = poff;
            A.seqdesc[s*5 + 4] = (int64_t)pl.size();
            A.seq_in_ub[s] = (rng() % 3) ? 1 : 0;
            n_pool_real += (int64_t)pl.size();
        }
        A.n_pool_real = n_pool_real;
        A.n_pool = n_pool_real + 1 + (int64_t)(rng() % 4);
        A.is_new.assign(A.n_pool, 0);
        for (int64_t i = 0; i < n_pool_real; ++i) A.is_new[i] = (rng() % 2) ? A.gen : 0;

        A.n_tokens = 1 + (int64_t)(rng() % 4);
        A.pos.assign(A.n_tokens, 0); A.ub_seq.assign((size_t)A.n_tokens*4, 0); A.rankv.assign(A.n_tokens, 0);
        // In order mode a token's rank must exist, so tokens only pick sequences that have cells.
        bool any_cells = false;
        for (int s = 0; s < A.n_seq; ++s) if (A.d_ccnt(s) > 0) any_cells = true;
        if (!any_cells) continue;
        for (int64_t i = 0; i < A.n_tokens; ++i) {
            int s;
            do { s = (int)(rng() % A.n_seq); } while (A.d_ccnt(s) == 0);
            A.ub_seq[i*4] = s;
            const int cc = (int)A.d_ccnt(s);
            A.pos[i] = cc ? A.cpos[A.d_coff(s) + (size_t)(rng() % cc)] : 0;
            int64_t r = 0;
            for (int q = 0; q < cc; ++q) if (A.cpos[A.d_coff(s) + q] == A.pos[i]) { r = q; break; }
            A.rankv[i] = r;
        }
        A.n_sel = A.kpool * (1 + (int64_t)(rng() % 3));
        A.n_top = A.n_sel / A.kpool;
        A.sel_present = (rng() & 1) != 0;

        A.pcell.assign(A.n_pool, 0); A.pidx.assign((size_t)A.n_pool*A.kpool, 0);
        A.nidx.assign((size_t)A.n_pool*A.kpool, 0); A.nrep.assign(A.n_pool, 0);
        A.npos.assign((size_t)4*std::max<int64_t>(1, A.n_pool_real), 0);
        A.mask.assign((size_t)A.n_tokens*A.n_pool, 0); A.selmask.assign((size_t)A.n_tokens*A.n_sel, 0);
        A.tail.assign((size_t)A.n_tokens*(A.kpool - 1), 0); A.sps.assign(A.n_seq, 0); A.pend.assign(A.n_pool, 0);

        std::vector<int64_t> w_pcell, w_pidx, w_nidx, w_nrep, w_sps, w_pend, w_npos;
        int64_t w_inew = 0;
        ref_tables(A, w_pcell, w_pidx, w_nidx, w_nrep, w_sps, w_pend, w_npos, w_inew);

        for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
        g_packet[0][1] = A.kpool; g_packet[0][2] = A.n_pool; g_packet[0][3] = A.n_pool_real;
        g_packet[0][4] = A.n_kv; g_packet[0][5] = A.n_stream; g_packet[0][6] = A.n_tokens;
        g_packet[0][7] = A.n_seq; g_packet[0][8] = A.by_order ? 1 : 0;
        g_packet[0][9] = A.n_sel; g_packet[0][10] = A.n_top; g_packet[0][11] = A.sel_present ? 1 : 0;
        g_packet[1][0] = (int64_t)(uintptr_t)A.seqdesc.data();
        g_packet[1][1] = (int64_t)(uintptr_t)A.cpos.data();
        g_packet[1][2] = (int64_t)(uintptr_t)A.ccell.data();
        g_packet[1][3] = (int64_t)(uintptr_t)A.is_new.data();
        g_packet[1][4] = A.gen;
        g_packet[1][5] = (int64_t)(uintptr_t)A.seq_in_ub.data();
        g_packet[1][6] = (int64_t)(uintptr_t)A.pos.data();
        g_packet[1][7] = (int64_t)(uintptr_t)A.ub_seq.data();
        g_packet[1][8] = 4;
        g_packet[1][9] = (int64_t)(uintptr_t)A.rankv.data();
        g_packet[1][10] = A.dummy;
        g_packet[1][11] = (int64_t)(uintptr_t)A.pools.data();
        g_packet[2][0] = (int64_t)(uintptr_t)A.pcell.data();
        g_packet[2][1] = (int64_t)(uintptr_t)A.pidx.data();
        g_packet[2][2] = (int64_t)(uintptr_t)A.nidx.data();
        g_packet[2][3] = (int64_t)(uintptr_t)A.nrep.data();
        g_packet[2][4] = (int64_t)(uintptr_t)A.npos.data();
        g_packet[2][5] = (int64_t)(uintptr_t)A.mask.data();
        g_packet[2][6] = (int64_t)(uintptr_t)A.selmask.data();
        g_packet[2][7] = (int64_t)(uintptr_t)A.tail.data();
        g_packet[2][8] = (int64_t)(uintptr_t)A.sps.data();
        g_packet[2][9] = (int64_t)(uintptr_t)A.pend.data();
        g_packet[3][0] = 0; g_packet[3][1] = 0;
        if (!fg314dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
        check(g_packet[3][1] == w_inew, "fresh pool count", trial);
        for (int64_t ip = 0; ip < A.n_pool; ++ip) {
            check(A.pcell[ip] == w_pcell[ip], "pool cell", trial);
            for (int64_t k = 0; k < A.kpool; ++k)
                check(A.pidx[ip*A.kpool + k] == w_pidx[ip*A.kpool + k], "pool member", trial);
        }
        for (int64_t i = 0; i < A.n_pool; ++i) {
            for (int64_t k = 0; k < A.kpool; ++k)
                check(A.nidx[i*A.kpool + k] == w_nidx[i*A.kpool + k], "scatter member", trial);
            check(A.nrep[i] == w_nrep[i], "scatter rep", trial);
        }
        for (size_t q = 0; q < w_npos.size(); ++q) check(A.npos[q] == w_npos[q], "pool positions", trial);
        for (int s = 0; s < A.n_seq; ++s) check(A.sps[s] == w_sps[s], "sequence pool start", trial);
        check((int64_t)w_pend.size() == A.n_pool_real, "pool end size", trial);
        for (size_t i = 0; i < w_pend.size(); ++i) check(A.pend[i] == w_pend[i], "pool end", trial);

        std::vector<int64_t> w_mask, w_sel, w_tail;
        ref_masks(A, w_mask, w_sel, w_tail);
        std::fill(A.mask.begin(), A.mask.end(), -99);
        std::fill(A.selmask.begin(), A.selmask.end(), -99);
        std::fill(A.tail.begin(), A.tail.end(), -99);

        g_packet[0][0] = 1;
        g_packet[3][0] = 0; g_packet[3][1] = 0;
        if (!fg314dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
        check(g_packet[3][1] == A.n_tokens, "mask step count", trial);
        for (size_t i = 0; i < w_mask.size(); ++i) check(A.mask[i] == w_mask[i], "pool mask", trial);
        // The selection mask is only written when the graph asks for it.
        if (A.sel_present)
            for (size_t i = 0; i < w_sel.size(); ++i) check(A.selmask[i] == w_sel[i], "selection mask", trial);
        for (size_t i = 0; i < w_tail.size(); ++i) check(A.tail[i] == w_tail[i], "tail cell", trial);
    }

    std::printf("FG314K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
