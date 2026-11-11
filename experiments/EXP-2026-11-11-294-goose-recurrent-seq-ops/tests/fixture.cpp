// Differential gate for the Goose recurrent sequence operations.
//
// Oracle: a transcription of llama_memory_recurrent::{seq_cp, seq_keep, seq_add, seq_div} over a
// model that keeps the native std::set of sequence ids per cell.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <set>
#include <random>
#include <algorithm>

extern "C" uint8_t fg294dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg294k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg294k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg294k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg294k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg294k_fail(int64_t c) { std::fprintf(stderr, "FG294K validation code %lld\n", (long long)c); std::abort(); }

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
    uint32_t head = 0, used = 0;
    void init() { cells.assign(N, Cell{}); head = 0; used = 0; }
};

static void ref_cp(State & S, int src, int dst) {
    if (src == dst) return;
    if (!((uint32_t)dst < (uint32_t)N && (uint32_t)src < (uint32_t)N)) return;
    Cell & tail_dst = S.cells[dst];
    if (tail_dst.tail >= 0) {
        Cell & cell_dst = S.cells[tail_dst.tail];
        cell_dst.seq.erase(dst);
        tail_dst.tail = -1;
        if (cell_dst.seq.empty()) {
            cell_dst.pos = -1;
            cell_dst.src = -1;
            S.used -= 1;
        }
    }
    Cell & tail_src = S.cells[src];
    if (tail_src.tail >= 0) {
        Cell & cell_src = S.cells[tail_src.tail];
        cell_src.seq.insert(dst);
        tail_dst.tail = tail_src.tail;
    }
}

static void ref_keep(State & S, int seq_id) {
    uint32_t new_head = N;
    for (uint32_t i = 0; i < (uint32_t)N; ++i) {
        if ((int)i != seq_id) S.cells[i].tail = -1;
        if (!S.cells[i].has(seq_id)) {
            if (S.cells[i].pos >= 0) S.used--;
            S.cells[i].pos = -1;
            S.cells[i].src = -1;
            S.cells[i].seq.clear();
            if (new_head == (uint32_t)N) new_head = i;
        } else {
            S.cells[i].seq.clear();
            S.cells[i].seq.insert(seq_id);
        }
    }
    if (new_head != (uint32_t)N && new_head < S.head) S.head = new_head;
}

static void ref_shift(State & S, int seq_id, int p0, int p1, int shift, bool divide) {
    if (divide ? shift == 1 : shift == 0) return;
    if (p0 < 0) p0 = 0;
    if (p1 < 0) p1 = 2147483647;
    if (p0 == p1) return;
    if (0 <= seq_id && seq_id < (int64_t)N) {
        const int tail_id = S.cells[seq_id].tail;
        if (tail_id >= 0) {
            Cell & cell = S.cells[tail_id];
            if (cell.has(seq_id) && p0 <= cell.pos && cell.pos < p1) {
                if (divide) cell.pos /= shift;
                else        cell.pos += shift;
            }
        }
    }
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
};

static int checks, failures;
static void check(bool ok, const char * what, int trial, int step) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d step=%d\n", what, trial, step); }
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
    A.head() = S.head; A.used() = S.used;
}

static void run_goose(Arena & A, int op, int sid, int did, int p0, int p1, int sh) {
    g_packet[0][0] = op;
    g_packet[0][1] = (int64_t)(uintptr_t)A.v.data();
    g_packet[0][2] = N;
    g_packet[0][3] = SW;
    g_packet[1][0] = sid;
    g_packet[1][1] = did;
    g_packet[1][2] = p0;
    g_packet[1][3] = p1;
    g_packet[1][4] = sh;
    g_packet[3][0] = 0;
    if (!fg294dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
}

int main(void) {
    std::mt19937 rng(294011);
    for (int trial = 0; trial < 300; ++trial) {
        State S; S.init();
        Arena A; A.init();
        std::vector<int> tails(SQ, -1);
        for (int s = 0; s < SQ; ++s) {
            if (rng() % 100 < 40) continue;
            const int i = (int)(rng() % N);
            S.cells[i].seq.insert(s);
            if (S.cells[i].pos == -1) S.cells[i].pos = 100 + (int)(rng() % 50);
            tails[s] = i;
        }
        for (int s = 0; s < SQ; ++s) S.cells[s].tail = tails[s];
        for (const auto & c : S.cells) if (!c.is_empty()) S.used++;
        S.head = (uint32_t)(rng() % N);
        sync(A, S);

        for (int step = 0; step < 30; ++step) {
            const int op = (int)(rng() % 4);
            const int sid = (int)(rng() % SQ);
            const int did = (int)(rng() % SQ);
            int p0 = (int)(rng() % 200) - 20;
            int p1 = (int)(rng() % 200) - 20;
            if (rng() % 4 == 0) p1 = -1;
            int sh = 0;
            if (op == 2) sh = (int)(rng() % 21) - 10;
            if (op == 3) sh = 1 + (int)(rng() % 4);

            if (op == 0) ref_cp(S, sid, did);
            else if (op == 1) ref_keep(S, sid);
            else if (op == 2) ref_shift(S, sid, p0, p1, sh, false);
            else ref_shift(S, sid, p0, p1, sh, true);

            run_goose(A, op, sid, did, p0, p1, sh);

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
        }
    }
    std::printf("FG294K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
