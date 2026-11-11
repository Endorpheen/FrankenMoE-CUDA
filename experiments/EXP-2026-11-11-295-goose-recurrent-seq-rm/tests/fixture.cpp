// Differential gate for the Goose recurrent seq_rm / set_rs_idx / clear / state_clear.
//
// Oracle: a transcription of llama_memory_recurrent over a model with the native std::set and a
// real rs_idx vector.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <set>
#include <random>
#include <algorithm>

extern "C" uint8_t fg295dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg295k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg295k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg295k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg295k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg295k_fail(int64_t c) { std::fprintf(stderr, "FG295K validation code %lld\n", (long long)c); std::abort(); }

static const int N = 32;
static const int SQ = 8;
static const int SW = 1;
static const int NSEQ = 12;      // n_seq_max, larger than the cache to exercise the bound
static const int NRS = 8;        // n_rs_seq

struct Cell {
    int pos = -1, src = -1, src0 = -1, tail = -1;
    std::set<int> seq;
    bool is_empty() const { return seq.empty(); }
    bool has(int s) const { return seq.count(s) != 0; }
};

struct State {
    std::vector<Cell> cells;
    std::vector<uint32_t> rs_idx;
    uint32_t head = 0, used = 0;
    bool mem_empty = false;
    void init() {
        cells.assign(N, Cell{});
        rs_idx.assign(NSEQ, 0);
        head = 0; used = 0; mem_empty = false;
    }
};

static void ref_set_rs(State & S, int seq_id, int idx) {
    if (seq_id < 0) { std::fill(S.rs_idx.begin(), S.rs_idx.end(), 0); return; }
    if ((uint32_t)seq_id >= (uint32_t)NSEQ) std::abort();
    if (idx > NRS) std::abort();
    S.rs_idx[seq_id] = (uint32_t)idx;
}

static bool ref_seq_rm(State & S, int seq_id, int p0in, int p1in) {
    uint32_t new_head = N;
    int p0 = p0in, p1 = p1in;
    if (p0 < 0) p0 = 0;
    if (p1 < 0) p1 = 2147483647;
    if ((uint32_t)seq_id >= (uint32_t)NSEQ) return false;
    const bool rm_all = p0 == 0 && p1 == 2147483647;
    if (rm_all) ref_set_rs(S, seq_id, 0);
    if (seq_id >= (int64_t)N) return false;
    if (seq_id >= 0) {
        int32_t & tail_id = S.cells[seq_id].tail;
        if (tail_id >= 0) {
            Cell & cell = S.cells[tail_id];
            if (0 < p0 && p0 <= cell.pos && p1 > cell.pos) {
                if (cell.seq.size() > 1) return false;
                if (S.mem_empty) { cell.pos = p0 - 1; return true; }
                const int rollback = cell.pos - (p0 - 1);
                const bool pending = S.rs_idx[seq_id] != 0;
                if (!pending && rollback >= 1 && rollback <= NRS) {
                    ref_set_rs(S, seq_id, rollback);
                    cell.pos = p0 - 1;
                    return true;
                }
                return false;
            }
            if (p0 <= cell.pos && cell.pos < p1) tail_id = -1;
        }
    } else {
        if (p0 != p1 && (p0 != 0 || p1 != 2147483647)) return false;
    }

    for (uint32_t i = 0; i < (uint32_t)N; ++i) {
        if (S.cells[i].pos >= p0 && S.cells[i].pos < p1) {
            if (seq_id < 0) {
                S.cells[i].seq.clear();
            } else if (S.cells[i].has(seq_id)) {
                S.cells[i].seq.erase(seq_id);
            } else {
                continue;
            }
            if (S.cells[i].is_empty()) {
                if (S.cells[i].pos >= 0) S.used--;
                S.cells[i].pos = -1;
                S.cells[i].src = -1;
                if (new_head == (uint32_t)N) new_head = i;
            }
        }
    }
    if (new_head != (uint32_t)N && new_head < S.head) S.head = new_head;
    return true;
}

static void ref_clear(State & S) {
    for (int i = 0; i < N; ++i) {
        S.cells[i].pos = -1;
        S.cells[i].seq.clear();
        S.cells[i].src = -1;
        S.cells[i].tail = -1;
    }
    S.head = 0;
    S.used = 0;
    std::fill(S.rs_idx.begin(), S.rs_idx.end(), 0);
}

static int ref_pos_min(const State & S, int seq_id) {
    int r = 2147483647;
    for (int i = 0; i < N; ++i) if (S.cells[i].has(seq_id)) r = std::min(r, S.cells[i].pos);
    return r;
}
static int ref_pos_max(const State & S, int seq_id) {
    int r = -1;
    for (int i = 0; i < N; ++i) if (S.cells[i].has(seq_id)) r = std::max(r, S.cells[i].pos);
    return r;
}

struct Arena {
    std::vector<int64_t> v;
    static size_t slots() { return (size_t)(4*N + N*SW + 3 + NSEQ); }
    void init() {
        v.assign(slots(), 0);
        for (int i = 0; i < N; ++i) { v[i] = -1; v[N+i] = -1; v[2*N+i] = -1; v[3*N+i] = -1; }
    }
    int64_t &pos(int i) { return v[i]; }
    int64_t &src(int i) { return v[N + i]; }
    int64_t &src0(int i) { return v[2*N + i]; }
    int64_t &tail(int i) { return v[3*N + i]; }
    int64_t &seqw(int i, int w) { return v[4*N + i*SW + w]; }
    int64_t &head() { return v[4*N + N*SW]; }
    int64_t &used() { return v[4*N + N*SW + 1]; }
    int64_t &rs(int i) { return v[4*N + N*SW + 3 + i]; }
};

static int checks, failures;
static void check(bool ok, const char * what, int trial, int step) {
    ++checks;
    if (!ok) { ++failures; if (failures < 12) std::fprintf(stderr, "FAIL %s trial=%d step=%d\n", what, trial, step); }
}

static void sync(Arena & A, const State & S) {
    for (int i = 0; i < N; ++i) {
        A.pos(i) = S.cells[i].pos;
        A.src(i) = S.cells[i].src;
        A.src0(i) = S.cells[i].src0;
        A.tail(i) = S.cells[i].tail;
        uint64_t word = 0;
        for (int s : S.cells[i].seq) word |= (1ull << s);
        A.seqw(i, 0) = (int64_t)word;
    }
    A.head() = S.head;
    A.used() = S.used;
    for (int i = 0; i < NSEQ; ++i) A.rs(i) = S.rs_idx[i];
}

static int64_t run_goose(Arena & A, int op, int sid, int p0, int p1, int ch, int cc, bool mem_empty) {
    g_packet[0][0] = op;
    g_packet[0][1] = (int64_t)(uintptr_t)A.v.data();
    g_packet[0][2] = N;
    g_packet[0][3] = SW;
    g_packet[0][4] = NSEQ;
    g_packet[0][5] = NRS;
    g_packet[0][6] = mem_empty ? 1 : 0;
    g_packet[1][0] = sid;
    g_packet[1][1] = p0;
    g_packet[1][2] = p1;
    g_packet[1][3] = ch;
    g_packet[1][4] = cc;
    g_packet[3][0] = 0;
    g_packet[3][1] = 0;
    if (!fg295dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    return g_packet[3][1];
}

int main(void) {
    std::mt19937 rng(295011);
    for (int trial = 0; trial < 300; ++trial) {
        State S; S.init();
        Arena A; A.init();
        std::vector<int> tails(SQ, -1);
        for (int s = 0; s < SQ; ++s) {
            if (rng() % 100 < 35) continue;
            const int i = (int)(rng() % N);
            S.cells[i].seq.insert(s);
            if (S.cells[i].pos == -1) S.cells[i].pos = 50 + (int)(rng() % 40);
            tails[s] = i;
        }
        for (int s = 0; s < SQ; ++s) S.cells[s].tail = tails[s];
        for (const auto & c : S.cells) if (!c.is_empty()) S.used++;
        S.head = (uint32_t)(rng() % N);
        for (int i = 0; i < NSEQ; ++i) S.rs_idx[i] = (rng() % 2) ? 0 : (uint32_t)(1 + rng() % NRS);
        S.mem_empty = (rng() % 5) == 0;
        sync(A, S);

        for (int step = 0; step < 25; ++step) {
            const int op = (int)(rng() % 6);
            const int sid = (int)(rng() % (NSEQ + 2)) - 1;   // includes -1 and out-of-range
            int p0 = (int)(rng() % 200) - 20;
            int p1 = (int)(rng() % 200) - 20;
            if (rng() % 3 == 0) p1 = -1;
            if (rng() % 3 == 0) p0 = -1;
            // Half the removals aim at the sequence's own cell so the rollback branch is reached.
            if (op == 0 && sid >= 0 && sid < N && S.cells[sid].tail >= 0 && rng() % 2) {
                const int base = S.cells[S.cells[sid].tail].pos;
                p0 = base - (int)(rng() % 4);
                p1 = base + 1 + (int)(rng() % 4);
                // Probe the exact snapshot bound: rollback == n_rs_seq.
                if (rng() % 4 == 0) { p0 = base - NRS + 1; p1 = base + 3; }
            }
            int64_t want = 1;
            switch (op) {
                case 0: want = ref_seq_rm(S, sid, p0, p1) ? 1 : 0; break;
                case 1: ref_clear(S); want = 1; break;
                case 2: {
                    // set_rs_idx: only valid ids and indexes reach the native call
                    const int rsid = (sid >= NSEQ) ? (int)(rng() % NSEQ) : sid;
                    const int idx = p0 < 0 ? 0 : (p0 % (NRS + 1));
                    ref_set_rs(S, rsid, idx);
                    run_goose(A, 2, rsid, idx, 0, 0, 0, S.mem_empty);
                    for (int i = 0; i < N; ++i) check(A.pos(i) == S.cells[i].pos, "pos", trial, step);
                    for (int i = 0; i < NSEQ; ++i) check(A.rs(i) == (int64_t)S.rs_idx[i], "rs", trial, step);
                    continue;
                }
                case 3: want = ref_pos_min(S, sid >= NSEQ ? NSEQ - 1 : sid); break;
                case 4: want = ref_pos_max(S, sid >= NSEQ ? NSEQ - 1 : sid); break;
                default: {
                    // state_clear
                    (void)0;
                    if (sid == -1) ref_clear(S);
                    else if (sid < NSEQ) ref_seq_rm(S, sid, -1, -1);
                    want = 1;
                    break;
                }
            }
            const int use_sid = (op == 3 || op == 4) && sid >= NSEQ ? NSEQ - 1 : sid;
            const int64_t got = run_goose(A, op, use_sid, p0, p1, 0, op == 5 ? 0 : (int)(rng() % 8), S.mem_empty);
            check(got == want, "result", trial, step);

            for (int i = 0; i < N; ++i) {
                check(A.pos(i) == S.cells[i].pos, "pos", trial, step);
                check(A.src(i) == S.cells[i].src, "src", trial, step);
                check(A.tail(i) == S.cells[i].tail, "tail", trial, step);
                uint64_t word = 0;
                for (int s : S.cells[i].seq) word |= (1ull << s);
                check((uint64_t)A.seqw(i, 0) == word, "seq", trial, step);
            }
            check((uint32_t)A.head() == S.head, "head", trial, step);
            check((uint32_t)A.used() == S.used, "used", trial, step);
            for (int i = 0; i < NSEQ; ++i) check(A.rs(i) == (int64_t)S.rs_idx[i], "rs", trial, step);
        }
    }
    std::printf("FG295K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
