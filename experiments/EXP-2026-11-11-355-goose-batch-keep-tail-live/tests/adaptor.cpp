// Gate: the bridge must reproduce the native keep-tail block, action for action, state for state.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>

extern "C" int64_t fg348s_keep_tail(int64_t n_seqs, const int32_t * sizes, int32_t * cur, const int32_t * idxs,
                                    const int32_t * off, int32_t * used, int64_t used_len, int32_t * n_used_inout,
                                    int64_t n_ubatch, int64_t n_keep_tail,
                                    int32_t * ext, int64_t * ext_n, int32_t * tr, int64_t * tr_n,
                                    int32_t * rel, int64_t * rel_n);
extern "C" int fg348s_enabled(void);

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

int main(void) {
    std::mt19937 rng(355011);
    check(fg348s_enabled() == 1, "enabled by default", 0);

    for (int trial = 0; trial < 4000; ++trial) {
        const int64_t n_seqs = 1 + (int64_t)(rng() % 4);
        const int64_t n_keep_tail = (rng() % 5 == 0) ? 0 : 1 + (int64_t)(rng() % 4);
        const int64_t n_ubatch = n_keep_tail + 1 + (int64_t)(rng() % 8);

        std::vector<int32_t> sizes((size_t)n_seqs), cur((size_t)n_seqs), off((size_t)n_seqs + 1, 0), idxs;
        std::vector<int32_t> used;
        int32_t next = 0;
        for (int64_t s = 0; s < n_seqs; ++s) {
            sizes[(size_t)s] = (int32_t)(1 + (int64_t)(rng() % 6));
            cur[(size_t)s] = (int32_t)(rng() % (sizes[(size_t)s] + 1));
            for (int64_t k = 0; k < sizes[(size_t)s]; ++k) idxs.push_back(next++);
            off[(size_t)s + 1] = (int32_t)idxs.size();
        }
        used.assign((size_t)next + 2, 0);
        int32_t n_used = 0;
        for (auto & x : used) { if (rng() & 1) { x = 1; ++n_used; } }

        // native block
        std::vector<int32_t> nused = used, ncur = cur;
        int32_t nn_used = n_used;
        std::vector<int32_t> n_ext, n_tr, n_rel;
        int64_t n_keep = -1;
        if (n_keep_tail > 0) {
            // per set: only the tokens already taken in this ubatch (idxs_per_seq in native)
            std::vector<int32_t> per[4];
            for (int64_t s = 0; s < n_seqs; ++s)
                for (int32_t k = 0; k < ncur[(size_t)s]; ++k) per[s].push_back(idxs[(size_t)(off[(size_t)s] + k)]);
            auto remaining = [&](int64_t s) { return sizes[(size_t)s] - ncur[(size_t)s]; };
            int64_t keep = 0;
            while (keep < n_seqs) {
                const int64_t rem = remaining(keep);
                if (rem != 0 && rem < n_keep_tail) break;
                ++keep;
            }
            if (keep == 0) {
                if ((int64_t)per[0].size() + remaining(0) <= n_ubatch) {
                    while (remaining(0) > 0) {
                        const int32_t idx = idxs[(size_t)(off[0] + ncur[0])];
                        n_ext.push_back(idx); nused[(size_t)idx] = 1; ++nn_used; ++ncur[0];
                        per[0].push_back(idx);
                    }
                } else {
                    while (remaining(0) < n_keep_tail) {
                        const int32_t last = per[0].back();
                        n_tr.push_back(last); nused[(size_t)last] = 0; --nn_used; per[0].pop_back(); --ncur[0];
                    }
                }
                keep = 1;
            }
            for (int64_t s = keep; s < n_seqs; ++s)
                for (int32_t idx : per[s]) { n_rel.push_back(idx); nused[(size_t)idx] = 0; --nn_used; }
            n_keep = keep;
        }

        // port
        std::vector<int32_t> pused = used, pcur = cur, p_ext(64, -1), p_tr(64, -1), p_rel(256, -1);
        int32_t pn_used = n_used;
        int64_t e_n = 0, t_n = 0, r_n = 0;
        const int64_t got = fg348s_keep_tail(n_seqs, sizes.data(), pcur.data(), idxs.data(), off.data(),
                                             pused.data(), (int64_t)pused.size(), &pn_used, n_ubatch,
                                             n_keep_tail, p_ext.data(), &e_n, p_tr.data(), &t_n,
                                             p_rel.data(), &r_n);
        static int dbgc = 0;
        if (got != n_keep && dbgc < 3 && (++dbgc))
            std::fprintf(stderr, "DBG trial=%d got=%lld want=%lld n_seqs=%lld tail=%lld ub=%lld sizes/cur:", trial,
                (long long)got, (long long)n_keep, (long long)n_seqs, (long long)n_keep_tail, (long long)n_ubatch);
        if (trial == 0) {
            std::fprintf(stderr, "A0 n_seqs=%lld tail=%lld ub=%lld sizes/cur:", (long long)n_seqs, (long long)n_keep_tail, (long long)n_ubatch);
            for (int64_t z = 0; z < n_seqs; ++z) std::fprintf(stderr, " %d/%d", sizes[(size_t)z], cur[(size_t)z]);
            std::fprintf(stderr, " off:"); for (int64_t z = 0; z <= n_seqs; ++z) std::fprintf(stderr, " %d", off[(size_t)z]);
            std::fprintf(stderr, "\nA0 port_rel:"); for (int64_t i = 0; i < r_n; ++i) std::fprintf(stderr, " %d", p_rel[(size_t)i]);
            std::fprintf(stderr, " native_rel:"); for (size_t i = 0; i < n_rel.size(); ++i) std::fprintf(stderr, " %d", n_rel[i]);
            std::fprintf(stderr, "\n");
        }
        check(got == n_keep, "n_keep", trial);
        check(e_n == (int64_t)n_ext.size(), "extend count", trial);
        for (size_t i = 0; i < n_ext.size() && i < (size_t)e_n; ++i) check(p_ext[i] == n_ext[i], "extend", trial);
        check(t_n == (int64_t)n_tr.size(), "truncate count", trial);
        for (size_t i = 0; i < n_tr.size() && i < (size_t)t_n; ++i) check(p_tr[i] == n_tr[i], "truncate", trial);
        check(r_n == (int64_t)n_rel.size(), "release count", trial);
        for (size_t i = 0; i < n_rel.size() && i < (size_t)r_n; ++i) check(p_rel[i] == n_rel[i], "release", trial);
        if (n_keep_tail > 0) {
            for (int64_t s = 0; s < n_seqs; ++s) check(pcur[(size_t)s] == ncur[(size_t)s], "cursor", trial);
            for (size_t i = 0; i < used.size(); ++i) check(pused[i] == nused[i], "used", trial);
            if (pn_used != nn_used && dbgc < 3 && (++dbgc))
                std::fprintf(stderr, "DBG used0=%d got=%d want=%d port[e=%lld t=%lld r=%lld] native[e=%zu t=%zu r=%zu]\n",
                    n_used, pn_used, nn_used, (long long)e_n, (long long)t_n, (long long)r_n,
                    n_ext.size(), n_tr.size(), n_rel.size());
            check(pn_used == nn_used, "n_used", trial);
        }
    }

    std::printf("FG348S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
