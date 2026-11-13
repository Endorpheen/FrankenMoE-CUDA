// Differential gate for the Goose KV position-maintenance module.
//
// The oracle is a transcription of llama_kv_cells: std::bitset for the sequence words,
// std::set<std::pair<pos,uint32_t>> for seq_pos. After every operation the arena the Goose
// module updates is compared with that model slot by slot, and every observer (min, max,
// tok_le) is compared directly.
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" uint8_t fg289dh_run(int64_t packet);

static int64_t g_packet[2][16];

extern "C" int64_t fg289k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg289k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg289k_fail(int64_t c) { std::fprintf(stderr, "FG289K validation code %lld\n", (long long)c); std::abort(); }

extern "C" int64_t fg289h_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg289h_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" void fg289h_fail(int64_t c) { std::fprintf(stderr, "FG289K validation code %lld\n", (long long)c); std::abort(); }

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <bitset>
#include <set>
#include <random>
#include <utility>

static const int N = 32;      // cells
static const int SQ = 4;      // sequences
static const int W = 8;       // words per cell, matching the module

struct Model {
    std::vector<int> pos, shift, ext;
    std::vector<char> used;
    std::vector<std::bitset<64>> seq;
    std::set<std::pair<int,uint32_t>> sp[SQ];

    void init() {
        pos.assign(N, -1); shift.assign(N, 0); ext.assign(N, 0);
        used.assign(N, 0); seq.assign(N, {});
        for (int s = 0; s < SQ; ++s) sp[s].clear();
    }
    bool bit(int i, int s) const { return seq[i].test(s); }
    void pos_set(int i, int p) {
        pos[i] = p; used[i] = 1;
    }
    void seq_add(int i, int s) {
        seq[i].set(s);
        sp[s].insert({pos[i], (uint32_t)i});
    }
    void sp_rm(int i) {
        for (int s = 0; s < SQ; ++s)
            if (bit(i, s)) {
                const size_t n = sp[s].erase({pos[i], (uint32_t)i});
                if (n != 1) { std::fprintf(stderr, "transcription: erase miss\n"); std::abort(); }
            }
    }
    void sp_add(int i) {
        for (int s = 0; s < SQ; ++s)
            if (bit(i, s)) sp[s].insert({pos[i], (uint32_t)i});
    }
    int pos_add(int i, int d) {
        sp_rm(i);
        pos[i] += d;
        shift[i] += d;
        if (pos[i] < 0) {
            seq[i].reset();
            pos[i] = -1;
            shift[i] = 0;
            used[i] = 0;
            return 1;
        }
        sp_add(i);
        return 0;
    }
    void pos_div(int i, int d) {
        const int old = pos[i];
        sp_rm(i);
        pos[i] /= d;
        shift[i] += old - pos[i];
        sp_add(i);
    }
    int mn(int s) const { return sp[s].empty() ? -1 : sp[s].begin()->first; }
    int mx(int s) const { return sp[s].empty() ? -1 : sp[s].rbegin()->first; }
    int tok_le(int s, int p) const {
        auto it = sp[s].upper_bound({p, 0xffffffffu});
        if (it == sp[s].begin()) return -1;
        return ext[(--it)->second];
    }
};

struct Arena {
    std::vector<int64_t> v;
    void init() {
        v.assign((size_t)(4*N + N*W + 2*N*N + SQ + 2), -1);
        // regions that are not -1 by default
        for (int i = 0; i < N; ++i) {
            v[(size_t)(N + i)] = 0;              // shift
            v[(size_t)(2*N + i)] = 0;            // used
            v[(size_t)(3*N + N*W + i)] = 0;      // ext
        }
        for (int i = 0; i < N*W; ++i) v[(size_t)(3*N + i)] = 0;
        for (int s = 0; s < SQ; ++s) v[(size_t)(4*N + N*W + 2*N*N + s)] = 0;  // scnt
        v[(size_t)(4*N + N*W + 2*N*N + SQ)] = 0;                              // has_shift
    }
    int64_t &pos(int i) { return v[(size_t)i]; }
    int64_t &shift(int i) { return v[(size_t)(N + i)]; }
    int64_t &used(int i) { return v[(size_t)(2*N + i)]; }
    int64_t &seqw(int i, int w) { return v[(size_t)(3*N + i*W + w)]; }
    int64_t &ext(int i) { return v[(size_t)(3*N + N*W + i)]; }
    int64_t &spos(int s, int j) { return v[(size_t)(4*N + N*W + (s*N + j))]; }
    int64_t &sidx(int s, int j) { return v[(size_t)(4*N + N*W + N*N + (s*N + j))]; }
    int64_t &scnt(int s) { return v[(size_t)(4*N + N*W + 2*N*N + s)]; }
    int64_t &hsh() { return v[(size_t)(4*N + N*W + 2*N*N + SQ)]; }
};

static int checks, failures;
static void check(bool ok, const char * what, int trial, int step) {
    ++checks;
    if (!ok) {
        ++failures;
        if (failures < 12) std::fprintf(stderr, "FAIL %s trial=%d step=%d\n", what, trial, step);
    }
}

static int64_t call(int op, Arena & A, int i, int d, int s, int p) {
    g_packet[0][0] = op;
    g_packet[0][1] = (int64_t)(uintptr_t)A.v.data();
    g_packet[0][2] = N;
    g_packet[0][3] = SQ;
    g_packet[0][4] = i;
    g_packet[0][5] = d;
    g_packet[0][6] = s;
    g_packet[0][7] = p;
    g_packet[1][0] = 0;
    g_packet[1][1] = 0;
    if (!fg289dh_run((int64_t)(uintptr_t)g_packet)) { std::fprintf(stderr, "run failed\n"); std::abort(); }
    return g_packet[1][0];
}

int main(void) {
    std::mt19937 rng(289011);
    for (int trial = 0; trial < 40; ++trial) {
        Model M; M.init();
        Arena A; A.init();
        for (int step = 0; step < 60; ++step) {
            const int op = (int)(rng() % 4);
            if (op == 0) {
                std::vector<int> cand;
                for (int i = 0; i < N; ++i) if (M.pos[i] == -1) cand.push_back(i);
                if (cand.empty()) continue;
                const int i = cand[rng() % cand.size()];
                const int p = (int)(rng() % 200) - 5;
                M.pos_set(i, p);
                call(6, A, i, p, 0, 0);
                const int ex = (int)(rng() % 1000) + 1;
                M.ext[i] = ex;
                call(5, A, i, ex, 0, 0);
            } else if (op == 1) {
                std::vector<std::pair<int,int>> cand;
                for (int i = 0; i < N; ++i)
                    if (M.pos[i] != -1)
                        for (int s = 0; s < SQ; ++s)
                            if (!M.bit(i, s)) cand.push_back({i, s});
                if (cand.empty()) continue;
                auto [i, s] = cand[rng() % cand.size()];
                M.seq_add(i, s);
                call(7, A, i, 0, s, 0);
            } else if (op == 2) {
                std::vector<int> cand;
                for (int i = 0; i < N; ++i) if (M.pos[i] != -1) cand.push_back(i);
                if (cand.empty()) continue;
                const int i = cand[rng() % cand.size()];
                const int d = (int)(rng() % 11) - 5;
                const int want = M.pos_add(i, d);
                const int got = (int)call(0, A, i, d, 0, 0);
                check(want == got, "pos_add return", trial, step);
            } else {
                std::vector<int> cand;
                for (int i = 0; i < N; ++i) if (M.pos[i] != -1) cand.push_back(i);
                if (cand.empty()) continue;
                const int i = cand[rng() % cand.size()];
                const int d = 1 + (int)(rng() % 4);
                M.pos_div(i, d);
                call(1, A, i, d, 0, 0);
            }

            // Compare the full state after every step.
            for (int i = 0; i < N; ++i) {
                check(A.pos(i) == M.pos[i], "pos", trial, step);
                check(A.shift(i) == M.shift[i], "shift", trial, step);
                check(A.used(i) == (M.used[i] ? 1 : 0), "used", trial, step);
                for (int w = 0; w < W; ++w) {
                    uint64_t want = 0;
                    for (int b = 0; b < 64; ++b)
                        if (b < SQ && M.seq[i].test(b)) want |= (1ull << b);
                    check((uint64_t)A.seqw(i, w) == (w == 0 ? want : 0), "seqw", trial, step);
                }
            }
            for (int s = 0; s < SQ; ++s) {
                check((size_t)A.scnt(s) == M.sp[s].size(), "seq_pos size", trial, step);
                int j = 0;
                for (const auto & pr : M.sp[s]) {
                    check(A.spos(s, j) == pr.first, "seq_pos pos", trial, step);
                    check(A.sidx(s, j) == pr.second, "seq_pos idx", trial, step);
                    ++j;
                }
                check((int)call(2, A, 0, 0, s, 0) == M.mn(s), "seq_pos_min", trial, step);
                check((int)call(3, A, 0, 0, s, 0) == M.mx(s), "seq_pos_max", trial, step);
                for (int p = -25; p <= 205; p += 23)
                    check((int)call(4, A, 0, 0, s, p) == M.tok_le(s, p), "seq_pos_tok_le", trial, step);
            }
        }
        // ext is written through the module; token lookups above only ever see 0 there.
    }
    std::printf("FG289K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
