// Differential gate for the Goose recurrent-state find_slot.
//
// Oracle: a transcription of llama_memory_recurrent::find_slot over a model that keeps the native
// std::set of sequence ids per cell.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <set>
#include <random>
#include <algorithm>

extern "C" uint8_t fg293dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg293k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg293k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg293k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg293k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg293k_fail(int64_t c) { std::fprintf(stderr, "FG293K validation code %lld\n", (long long)c); std::abort(); }

static const int N = 32;
static const int SQ = 8;
static const int SW = 1;

struct Cell {
    int pos = -1, src = -1, src0 = -1, tail = -1;
    std::set<int> seq;
    bool is_empty() const { return seq.empty(); }
    bool has(int s) const { return seq.count(s) != 0; }
};

struct State {
    std::vector<Cell> cells;
    uint32_t head = 0, used = 0, n = 0;
    void init() { cells.assign(N, Cell{}); head = 0; used = 0; n = 0; }
};

int g_swaps = 0;

struct Ubatch {
    std::vector<int> pos;
    std::vector<std::vector<int>> seq_id;
    int n_seq_tokens = 1, n_seqs = 1;
};

// Transcription of llama_memory_recurrent::find_slot.
static bool ref_find(State & S, const Ubatch & ub) {
    const uint32_t n_seq_tokens = (uint32_t)ub.n_seq_tokens;
    const uint32_t n_seqs = (uint32_t)ub.n_seqs;
    if (S.head > S.used + 2*n_seqs) S.head = 0;

    int32_t mn = N - 1, mx = 0;

    for (uint32_t s = 0; s < n_seqs; ++s) {
        const uint32_t i = s*n_seq_tokens;
        const uint32_t n_seq_id = (uint32_t)ub.seq_id[i].size();
        for (uint32_t j = 0; j < n_seq_id; ++j) {
            const int seq_id = ub.seq_id[i][j];
            if (seq_id < 0 || (uint32_t)seq_id >= (uint32_t)N) return false;
            if (j > 0) {
                Cell & seq = S.cells[seq_id];
                if (seq.tail >= 0) {
                    Cell & cell = S.cells[seq.tail];
                    cell.seq.erase(seq_id);
                    seq.tail = -1;
                    if (cell.seq.empty()) {
                        cell.pos = -1;
                        cell.src = -1;
                        S.used -= 1;
                    }
                }
            }
        }
    }

    uint32_t next_empty_cell = S.head;
    for (uint32_t i = 0; i < (uint32_t)N; ++i) {
        if (next_empty_cell >= (uint32_t)N) next_empty_cell -= N;
        if (S.cells[next_empty_cell].is_empty()) break;
        next_empty_cell += 1;
    }

    for (uint32_t s = 0; s < n_seqs; ++s) {
        const uint32_t i = s*n_seq_tokens;
        const int seq_id = ub.seq_id[i][0];
        Cell & seq_meta = S.cells[seq_id];
        bool has_cell = false;
        if (seq_meta.tail >= 0) {
            Cell & cell = S.cells[seq_meta.tail];
            if (cell.seq.size() == 1) has_cell = true;
        }
        if (!has_cell) {
            Cell & empty_cell = S.cells[next_empty_cell];
            if (seq_meta.tail >= 0) {
                Cell & orig_cell = S.cells[seq_meta.tail];
                empty_cell.pos = orig_cell.pos;
                empty_cell.src = orig_cell.src;
                orig_cell.seq.erase(seq_id);
                empty_cell.seq.insert(seq_id);
            }
            seq_meta.tail = (int)next_empty_cell;
            if (s + 1 < n_seqs) {
                for (uint32_t j = 0; j < (uint32_t)N; ++j) {
                    next_empty_cell += 1;
                    if (next_empty_cell >= (uint32_t)N) next_empty_cell -= N;
                    if (S.cells[next_empty_cell].is_empty()) break;
                }
            }
        }
        if (mn > seq_meta.tail) mn = seq_meta.tail;
        if (mx < seq_meta.tail) mx = seq_meta.tail;
    }

    for (uint32_t s = 0; s < n_seqs; ++s) {
        const uint32_t i = s*n_seq_tokens;
        const int dst_id = (int)s + mn;
        const int src_id = S.cells[ub.seq_id[i][0]].tail;
        if (dst_id != src_id) {
            ++g_swaps;
            Cell & dst_cell = S.cells[dst_id];
            Cell & src_cell = S.cells[src_id];
            std::swap(dst_cell.pos, src_cell.pos);
            std::swap(dst_cell.src, src_cell.src);
            std::swap(dst_cell.seq, src_cell.seq);
            for (uint32_t j = 0; j < (uint32_t)N; ++j) {
                int & tail = S.cells[j].tail;
                if (tail == src_id) tail = dst_id;
                else if (tail == dst_id) tail = src_id;
            }
        }
    }

    for (uint32_t s = 0; s < n_seqs; ++s) {
        const uint32_t i = s*n_seq_tokens;
        const int last_pos = ub.pos[i + n_seq_tokens - 1];
        const int cell_id = (int)s + mn;
        Cell & cell = S.cells[cell_id];
        cell.pos = last_pos;
        cell.seq.clear();
        for (int j = 0; j < (int)ub.seq_id[i].size(); ++j) {
            const int seq_id = ub.seq_id[i][j];
            cell.seq.insert(seq_id);
            S.cells[seq_id].tail = cell_id;
        }
    }

    {
        std::vector<int32_t> refcounts(N, 0);
        for (int i = 0; i < N; ++i) if (S.cells[i].src >= 0) refcounts[S.cells[i].src] += 1;
        int rs_z = -1;
        for (int i = mn; i <= mx; ++i) if (refcounts[i] == 0) { rs_z = i; break; }
        for (int i = mn; i <= mx; ++i) {
            if (S.cells[i].src < 0) { if (rs_z < 0) std::abort(); S.cells[i].src0 = rs_z; }
            else S.cells[i].src0 = S.cells[i].src;
            S.cells[i].src = i;
        }
    }

    S.head = mn;
    S.n = mx - mn + 1;
    S.used = 0;
    for (const auto & c : S.cells) if (!c.is_empty()) S.used += 1;
    return S.n >= n_seqs;
}

struct Arena {
    std::vector<int64_t> v;
    static size_t slots() { return (size_t)(4*N + N*SW + 3); }
    void init() { v.assign(slots(), 0); for (int i = 0; i < N; ++i) { v[i] = -1; v[N+i] = -1; v[2*N+i] = -1; v[3*N+i] = -1; } }
    int64_t &pos(int i) { return v[i]; }
    int64_t &src(int i) { return v[N + i]; }
    int64_t &src0(int i) { return v[2*N + i]; }
    int64_t &tail(int i) { return v[3*N + i]; }
    int64_t &seqw(int i, int w) { return v[4*N + i*SW + w]; }
    int64_t &head() { return v[4*N + N*SW]; }
    int64_t &used() { return v[4*N + N*SW + 1]; }
    int64_t &nr() { return v[4*N + N*SW + 2]; }
};

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static void sync_arena(Arena & A, const State & S) {
    for (int i = 0; i < N; ++i) {
        A.pos(i) = S.cells[i].pos;
        A.src(i) = S.cells[i].src;
        A.src0(i) = S.cells[i].src0;
        A.tail(i) = S.cells[i].tail;
        uint64_t word = 0;
        for (int s : S.cells[i].seq) word |= (1ull << s);
        A.seqw(i, 0) = (int64_t)word;
    }
    A.head() = S.head; A.used() = S.used; A.nr() = S.n;
}

static bool run_goose(Arena & A, const Ubatch & ub) {
    static std::vector<int64_t> upos, usid, unsid;
    upos.assign(ub.pos.begin(), ub.pos.end());
    usid.clear();
    unsid.clear();
    int width = 1;
    for (const auto & v : ub.seq_id) width = std::max(width, (int)v.size());
    for (const auto & v : ub.seq_id) {
        for (int j = 0; j < width; ++j) usid.push_back(j < (int)v.size() ? v[j] : 0);
        unsid.push_back((int)v.size());
    }
    g_packet[0][0] = (int64_t)(uintptr_t)A.v.data();
    g_packet[0][1] = N;
    g_packet[0][2] = SW;
    g_packet[0][3] = (int64_t)(uintptr_t)upos.data();
    g_packet[0][4] = (int64_t)(uintptr_t)usid.data();
    g_packet[0][5] = (int64_t)(uintptr_t)unsid.data();
    g_packet[0][6] = width;
    g_packet[0][7] = ub.n_seq_tokens;
    g_packet[0][8] = ub.n_seqs;
    g_packet[3][0] = 0;
    g_packet[3][1] = 0;
    if (!fg293dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    return g_packet[3][1] != 0;
}

int main(void) {
    std::mt19937 rng(293011);
    for (int trial = 0; trial < 800; ++trial) {
        State S0; S0.init();
        Arena A0; A0.init();
        // A valid recurrent state: each sequence owns at most one cell, and a cell may carry
        // several sequences. Cells that no sequence owns are empty.
        std::vector<int> tails(SQ, -1);
        for (int s = 0; s < SQ; ++s) {
            if (rng() % 100 < 40) continue;
            const int i = (int)(rng() % N);
            S0.cells[i].seq.insert(s);
            if (S0.cells[i].pos == -1) S0.cells[i].pos = 100 + (int)(rng() % 50);
            tails[s] = i;
        }
        for (int s = 0; s < SQ; ++s) S0.cells[s].tail = tails[s];
        for (const auto & c : S0.cells) if (!c.is_empty()) S0.used++;
        S0.head = (uint32_t)(rng() % N);
        sync_arena(A0, S0);

        Ubatch ub;
        ub.n_seq_tokens = 1 + (int)(rng() % 4);
        ub.n_seqs = 1 + (int)(rng() % 4);
        const int ntok = ub.n_seq_tokens * ub.n_seqs;
        ub.pos.resize(ntok);
        ub.seq_id.resize(ntok);
        const int base_pos = (int)(rng() % 50);
        // Native requires the first seq_id of each sequence group to be distinct.
        std::vector<int> pool(SQ);
        for (int s = 0; s < SQ; ++s) pool[s] = s;
        std::shuffle(pool.begin(), pool.end(), rng);
        for (int s = 0; s < ub.n_seqs; ++s) {
            const int i0 = s*ub.n_seq_tokens;
            ub.seq_id[i0].push_back(pool[s]);
            if (rng() % 3 == 0) {
                const int extra = (int)(rng() % SQ);
                if (extra != pool[s]) ub.seq_id[i0].push_back(extra);
            }
            for (int t = 0; t < ub.n_seq_tokens; ++t)
                ub.pos[s*ub.n_seq_tokens + t] = base_pos + t;
            for (int t = 1; t < ub.n_seq_tokens; ++t)
                ub.seq_id[i0 + t].push_back(ub.seq_id[i0].front());
        }

        if (trial < 3) {
            std::fprintf(stderr, "DBG trial=%d n_seqs=%d nst=%d tails:", trial, ub.n_seqs, ub.n_seq_tokens);
            for (int q = 0; q < SQ; ++q) std::fprintf(stderr, " %d", S0.cells[q].tail);
            std::fprintf(stderr, " head=%u used=%u\n", S0.head, S0.used);
        }
        State S = S0;
        const bool want = ref_find(S, ub);
        Arena A = A0;
        const bool got = run_goose(A, ub);

        if (g_packet[3][1] == 2) std::fprintf(stderr, "DBG stale tail trial=%d t=%lld sid=%lld\n", trial, (long long)g_packet[3][2], (long long)g_packet[3][3]);
        check(got == want, "result", trial);
        if (!want) continue;
        for (int i = 0; i < N; ++i) {
            check(A.pos(i) == S.cells[i].pos, "pos", trial);
            check(A.src(i) == S.cells[i].src, "src", trial);
            check(A.src0(i) == S.cells[i].src0, "src0", trial);
            check(A.tail(i) == S.cells[i].tail, "tail", trial);
            uint64_t word = 0;
            for (int s : S.cells[i].seq) word |= (1ull << s);
            check((uint64_t)A.seqw(i, 0) == word, "seq", trial);
        }
        check((uint32_t)A.head() == S.head, "head", trial);
        check((uint32_t)A.used() == S.used, "used", trial);
        check((uint32_t)A.nr() == S.n, "n", trial);
    }
    std::printf("FG293K_FIXTURE checks=%d failures=%d swaps=%d\n", checks, failures, g_swaps);
    return failures ? 1 : 0;
}
