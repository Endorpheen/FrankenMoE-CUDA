// Differential gate for the Goose recurrent state metadata (write + whole-cache read).
//
// Oracle: a transcription of state_write_meta and of the whole-cache branch of state_read_meta,
// writing into its own byte buffer so the two streams are compared byte for byte.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <set>
#include <random>
#include <algorithm>

extern "C" uint8_t fg296dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg296k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg296k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg296k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg296k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg296k_wr32(uint64_t base, int64_t off, int64_t v) { if (off < 0 || off + 4 > 1<<20) std::abort(); std::memcpy((void*)(uintptr_t)(base + (uint64_t)off), &v, 4); }
extern "C" int64_t fg296k_rd32(uint64_t base, int64_t off) { int32_t v; std::memcpy(&v, (const void*)(uintptr_t)(base + (uint64_t)off), 4); return v; }
extern "C" void fg296k_fail(int64_t c) { std::fprintf(stderr, "FG296K validation code %lld\n", (long long)c); std::abort(); }

static const int N = 32;
static const int SQ = 8;
static const int SW = 1;
static const int NSEQ = 12;

struct Cell {
    int pos = -1, src = -1, src0 = -1, tail = -1;
    std::set<int> seq;
};

struct State {
    std::vector<Cell> cells;
    uint32_t head = 0, used = 0;
    void init() { cells.assign(N, Cell{}); head = 0; used = 0; }
};

// state_write_meta
static void ref_write_meta(const State & S, const std::vector<std::pair<int,int>> & ranges, int seq_id,
                           std::vector<uint8_t> & out) {
    auto put32 = [&](int32_t v) {
        uint8_t b[4];
        std::memcpy(b, &v, 4);
        out.insert(out.end(), b, b + 4);
    };
    for (const auto & r : ranges) {
        for (int i = r.first; i < r.second; ++i) {
            const Cell & cell = S.cells[i];
            put32(cell.pos);
            const uint32_t n_seq_id = seq_id == -1 ? (uint32_t)cell.seq.size() : 0;
            put32((int32_t)n_seq_id);
            if (n_seq_id) for (int s : cell.seq) put32(s);
        }
    }
}

// whole-cache branch of state_read_meta
static bool ref_read_meta(State & S, const std::vector<uint8_t> & in, size_t & cursor, int n_seq_max, int cell_count) {
    auto get32 = [&](int32_t & v) {
        if (cursor + 4 > in.size()) std::abort();
        std::memcpy(&v, in.data() + cursor, 4);
        cursor += 4;
    };
    if (cell_count > N) return false;
    for (int i = 0; i < N; ++i) { S.cells[i] = Cell{}; }
    S.head = 0; S.used = 0;
    for (int i = 0; i < cell_count; ++i) {
        Cell & cell = S.cells[i];
        int32_t pos, ns;
        get32(pos);
        get32(ns);
        cell.pos = pos;
        for (int j = 0; j < ns; ++j) {
            int32_t sid;
            get32(sid);
            if (sid < 0 || sid >= n_seq_max) return false;
            cell.seq.insert(sid);
            int & tail = S.cells[sid].tail;
            if (tail != -1) return false;
            tail = i;
        }
    }
    S.head = 0;
    S.used = (uint32_t)cell_count;
    for (int i = 0; i < cell_count; ++i) S.cells[i].src = i;
    return true;
}

struct Arena {
    std::vector<int64_t> v;
    static size_t slots() { return (size_t)(4*N + N*SW + 4); }
    void init() { v.assign(slots(), 0); for (int i = 0; i < N; ++i) { v[i] = -1; v[N+i] = -1; v[2*N+i] = -1; v[3*N+i] = -1; } }
    int64_t &pos(int i) { return v[i]; }
    int64_t &src(int i) { return v[N + i]; }
    int64_t &tail(int i) { return v[3*N + i]; }
    int64_t &seqw(int i, int w) { return v[4*N + i*SW + w]; }
    int64_t &head() { return v[4*N + N*SW]; }
    int64_t &used() { return v[4*N + N*SW + 1]; }
    int64_t &cursor() { return v[4*N + N*SW + 3]; }
};

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static void sync(Arena & A, const State & S) {
    for (int i = 0; i < N; ++i) {
        A.pos(i) = S.cells[i].pos;
        A.src(i) = S.cells[i].src;
        A.tail(i) = S.cells[i].tail;
        uint64_t word = 0;
        for (int s : S.cells[i].seq) word |= (1ull << s);
        A.seqw(i, 0) = (int64_t)word;
    }
    A.head() = S.head; A.used() = S.used;
}

int main(void) {
    std::mt19937 rng(296011);
    static std::vector<uint8_t> buf(1 << 20);
    for (int trial = 0; trial < 2000; ++trial) {
        State S; S.init();
        Arena A; A.init();
        std::vector<int> tails(SQ, -1);
        for (int s = 0; s < SQ; ++s) {
            if (rng() % 100 < 35) continue;
            const int i = (int)(rng() % N);
            S.cells[i].seq.insert(s);
            if (S.cells[i].pos == -1) S.cells[i].pos = 30 + (int)(rng() % 60);
            tails[s] = i;
        }
        for (int s = 0; s < SQ; ++s) S.cells[s].tail = tails[s];
        for (const auto & c : S.cells) if (!c.seq.empty()) S.used++;
        S.head = (uint32_t)(rng() % N);
        sync(A, S);

        // --- write ---
        const int n_ranges = 1 + (int)(rng() % 3);
        std::vector<std::pair<int,int>> ranges;
        std::vector<int64_t> flat;
        for (int r = 0; r < n_ranges; ++r) {
            int lo = (int)(rng() % (N - 1));
            int hi = lo + 1 + (int)(rng() % (N - lo - 1));
            ranges.push_back({lo, hi});
            flat.push_back(lo); flat.push_back(hi);
        }
        const int seq_sel = (rng() % 3 == 0) ? (int)(rng() % SQ) : -1;
        std::vector<uint8_t> want;
        ref_write_meta(S, ranges, seq_sel, want);

        // The Goose arm starts with the same cursor (A.cursor() is 0 from init).
        g_packet[0][0] = 0;
        g_packet[0][1] = (int64_t)(uintptr_t)A.v.data();
        g_packet[0][2] = N;
        g_packet[0][3] = SW;
        g_packet[0][4] = NSEQ;
        g_packet[0][5] = (int64_t)(uintptr_t)buf.data();
        g_packet[0][6] = n_ranges;
        g_packet[0][7] = (int64_t)(uintptr_t)flat.data();
        g_packet[1][0] = seq_sel;
        g_packet[1][1] = 0;
        g_packet[3][0] = 0;
        if (!fg296dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
        const int64_t off = A.cursor();
        check((size_t)off == want.size(), "stream length", trial);
        check(std::memcmp(buf.data(), want.data(), std::min(want.size(), (size_t)off)) == 0, "stream bytes", trial);

        // --- read back the whole cache from the reference stream ---
        State R = S;
        Arena B; B.init();
        sync(B, S);
        B.cursor() = 0;
        // Preload the arena with junk so clear() has to work for its result to match.
        for (int i = 0; i < N; ++i) { B.pos(i) = 7; B.src(i) = 5; B.tail(i) = 3; B.seqw(i, 0) = 0x1ff; }

        // Probe the exact-capacity boundary regularly; the native check is strict.
        const int cell_count = (trial % 16 == 0) ? N : (int)(rng() % (N + 1));
        std::vector<uint8_t> stream;
        State W; W.init();
        // Build a valid stream for cell_count cells: distinct tails, ids in range.
        {
            std::vector<int> used_sids;
            for (int i = 0; i < cell_count; ++i) {
                const int pos = 10 + (int)(rng() % 90);
                std::vector<int> ids;
                const int ns = (int)(rng() % 3);
                for (int j = 0; j < ns; ++j) {
                    int sid = (int)(rng() % NSEQ);
                    if (std::find(ids.begin(), ids.end(), sid) == ids.end()) ids.push_back(sid);
                }
                // distinct tails: a sid may own only one cell
                std::vector<int> keep;
                for (int sid : ids)
                    if (std::find(used_sids.begin(), used_sids.end(), sid) == used_sids.end()) { keep.push_back(sid); used_sids.push_back(sid); }
                // Occasionally inject the two rejected cases so the failure paths are reached.
                if (trial % 16 != 0) {
                    if (rng() % 4 == 0 && !used_sids.empty()) keep.push_back(used_sids.front());
                    if (rng() % 8 == 0) keep.push_back(NSEQ + 1);
                }
                auto put32 = [&](int32_t x) { uint8_t b[4]; std::memcpy(b, &x, 4); stream.insert(stream.end(), b, b + 4); };
                put32(pos);
                put32((int32_t)keep.size());
                for (int sid : keep) put32(sid);
            }
        }
        std::memcpy(buf.data(), stream.data(), stream.size());
        size_t rcursor = 0;
        const bool want_ok = ref_read_meta(R, stream, rcursor, NSEQ, cell_count);

        g_packet[0][0] = 1;
        g_packet[0][1] = (int64_t)(uintptr_t)B.v.data();
        g_packet[1][0] = -1;
        g_packet[1][1] = cell_count;
        g_packet[3][0] = 0;
        g_packet[3][1] = 0;
        if (!fg296dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
        const bool got_ok = g_packet[3][1] != 0;
        check(got_ok == want_ok, "read result", trial);
        if (!want_ok) continue;
        for (int i = 0; i < N; ++i) {
            check(B.pos(i) == R.cells[i].pos, "pos", trial);
            check(B.src(i) == R.cells[i].src, "src", trial);
            check(B.tail(i) == R.cells[i].tail, "tail", trial);
            uint64_t word = 0;
            for (int s : R.cells[i].seq) word |= (1ull << s);
            check((uint64_t)B.seqw(i, 0) == word, "seq", trial);
        }
        check((uint32_t)B.head() == R.head, "head", trial);
        check((uint32_t)B.used() == R.used, "used", trial);
    }
    std::printf("FG296K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
