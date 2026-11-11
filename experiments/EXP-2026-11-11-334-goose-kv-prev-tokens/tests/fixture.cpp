// Differential gate for the Goose previous-token lookup.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <map>
#include <tuple>

extern "C" uint8_t fg334dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg334k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg334k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg334k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg334k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg334k_fail(int64_t c) { std::fprintf(stderr, "FG334K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 16), cnt(1);

// the per-sequence index is a std::map<(pos, cell), cell>; at equal positions the highest cell wins
struct SeqIdx {
    std::map<std::pair<int64_t,int64_t>, int64_t> m;
    void add(int64_t pos, int64_t cell, int64_t tok) { (void)tok; m[{pos, cell}] = cell; }
    int64_t tok_le(int64_t p, const std::vector<int64_t> & ctok) const {
        auto it = m.upper_bound({p, INT64_MAX});
        if (it == m.begin()) return -1;
        --it;
        return ctok[(size_t)it->second];
    }
};

int main(void) {
    std::mt19937 rng(334011);

    for (int trial = 0; trial < 4000; ++trial) {
        const int64_t n_tokens = 1 + (int64_t)(rng() % 6);
        const int64_t n = (int64_t)(rng() % 4);
        const int64_t tok_present = rng() & 1;
        const int64_t is_mixed = rng() & 1;
        const int64_t n_cells = 1 + (int64_t)(rng() % 10);
        const bool by_order = !tok_present || is_mixed;

        std::vector<int64_t> ub_pos(n_tokens), ub_seq0(n_tokens);
        for (int64_t i = 0; i < n_tokens; ++i) {
            // repeated positions are common: that is where "last cell at a position" matters
            ub_pos[i] = (int64_t)(rng() % 6);
            ub_seq0[i] = (int64_t)(rng() % 3);
        }
        std::vector<int64_t> cpos(n_cells), ctok(n_cells), cmask(n_cells);
        for (int64_t c = 0; c < n_cells; ++c) {
            cpos[c] = (rng() % 5 == 0) ? -1 : (int64_t)(rng() % 8);
            ctok[c] = 1000 + (int64_t)(rng() % 100);
            int64_t m = 0;
            for (int64_t q = 0; q < 3; ++q) if (rng() & 1) m |= (1LL << q);
            cmask[c] = cpos[c] == -1 ? 0 : m;
        }

        // reference
        std::vector<int64_t> res((size_t)(n_tokens*n), -1);
        if (n > 0) {
            std::map<int64_t, SeqIdx> idx;
            for (int64_t c = 0; c < n_cells; ++c)
                if (cpos[c] >= 0)
                    for (int64_t q = 0; q < 3; ++q)
                        if (cmask[c] & (1LL << q)) idx[q].add(cpos[c], c, ctok[c]);

            std::map<int64_t, std::vector<int64_t>> v;
            std::vector<int64_t> ord(n_tokens, 0);
            if (by_order) {
                for (int64_t i = 0; i < n_tokens; ++i) {
                    ord[i] = (int64_t)v[ub_seq0[i]].size();
                    v[ub_seq0[i]].push_back(i);
                }
            }
            for (int64_t i = 0; i < n_tokens; ++i) {
                const int64_t seq = ub_seq0[i];
                for (int64_t j = 0; j < n; ++j) {
                    const int64_t d = n - j;
                    int64_t p;
                    if (by_order) {
                        const auto & vv = v[seq];
                        const int64_t k = ord[i] - d;
                        p = k >= 0 ? ub_pos[(size_t)vv[(size_t)k]] : ub_pos[(size_t)vv[0]] + k;
                    } else {
                        p = ub_pos[i] - d;
                    }
                    if (p < 0) continue;
                    auto it = idx.find(seq);
                    if (it == idx.end()) { continue; }
                    res[(size_t)(i*n + j)] = it->second.tok_le(p, ctok);
                }
            }
        }

        for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; g_packet[3][i] = 0; }
        std::fill(out.begin(), out.end(), -99); cnt[0] = 0;
        g_packet[0][0] = n_tokens; g_packet[0][1] = n;
        g_packet[0][2] = tok_present; g_packet[0][3] = is_mixed;
        g_packet[0][4] = (int64_t)(uintptr_t)ub_pos.data();
        g_packet[0][5] = (int64_t)(uintptr_t)ub_seq0.data();
        g_packet[0][6] = n_cells;
        g_packet[0][7] = (int64_t)(uintptr_t)cpos.data();
        g_packet[0][8] = (int64_t)(uintptr_t)ctok.data();
        g_packet[0][9] = (int64_t)(uintptr_t)cmask.data();
        g_packet[1][0] = (int64_t)(uintptr_t)out.data();
        g_packet[2][0] = (int64_t)(uintptr_t)cnt.data();
        if (!fg334dh_run((int64_t)(uintptr_t)g_packet)) std::abort();

        check(cnt[0] == n_tokens*n, "record count", trial);
        for (int64_t k = 0; k < cnt[0] && k < n_tokens*n; ++k) {
            check(out[k*3+1] == k, "flat index", trial);
            check(out[k*3+2] == res[(size_t)k], "token", trial);
        }
    }

    std::printf("FG334K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
