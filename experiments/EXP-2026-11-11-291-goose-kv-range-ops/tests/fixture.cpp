// Differential gate for the Goose KV range operations.
//
// Oracle: a transcription of llama_kv_cache::seq_rm(seq_id, p0, p1) in both branches,
// seq_keep(seq_id) and the same-stream seq_cp, over a model that keeps the native std::set.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <bitset>
#include <set>
#include <random>
#include <algorithm>

extern "C" uint8_t fg291dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg291k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg291k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg291k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg291k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg291k_fail(int64_t c) { std::fprintf(stderr, "FG291K validation code %lld\n", (long long)c); std::abort(); }

static const int N = 32;
static const int SQ = 8;
static const int W = 8;
static const int NS = 2;

struct Cell {
    int pos = -1, shift = 0;
    bool used = false;
    std::bitset<64> seq{};
    int ex = 0, ey = 0, tok = 0;
};

struct Stream {
    std::vector<Cell> c;
    std::set<std::pair<int,uint32_t>> sp[SQ];
    uint32_t head = 0;
    void init() {
        c.assign(N, Cell{});
        for (int s = 0; s < SQ; ++s) sp[s].clear();
        head = 0;
    }
    bool has(int i, int s) const { return c[i].seq.test(s); }
    int count(int i) const { return (int)c[i].seq.count(); }
    void insert_sp(int i, int s) { sp[s].insert({c[i].pos, (uint32_t)i}); }
    bool pos_in(int i, int p0, int p1) const { return c[i].pos >= p0 && c[i].pos < p1; }

    void rm(int i) {
        for (int s = 0; s < SQ; ++s) if (has(i, s)) sp[s].erase({c[i].pos, (uint32_t)i});
        c[i] = Cell{};
    }
    void seq_rm_cell(int i, int s) {
        c[i].seq.reset(s);
        sp[s].erase({c[i].pos, (uint32_t)i});
        if (c[i].seq.none()) c[i] = Cell{};
    }
    int seq_keep_cell(int i, int s) {
        if (has(i, s)) {
            for (int t = 0; t < SQ; ++t) if (has(i, t)) sp[t].erase({c[i].pos, (uint32_t)i});
            c[i].seq.reset();
            c[i].seq.set(s);
            insert_sp(i, s);
            return 0;
        }
        if (c[i].seq.any()) {
            for (int t = 0; t < SQ; ++t) if (has(i, t)) sp[t].erase({c[i].pos, (uint32_t)i});
            c[i] = Cell{};
            return 1;
        }
        return 0;
    }
    void seq_add_cell(int i, int s) {
        c[i].seq.set(s);
        insert_sp(i, s);
    }
};

struct Model {
    Stream st[NS];
    void init() { for (int k = 0; k < NS; ++k) st[k].init(); }
};

struct Arena {
    std::vector<int64_t> v;
    static size_t per_slots() { return (size_t)(6*N + N*W + 2*N*N + SQ + 1); }
    void init() {
        v.assign((size_t)NS * per_slots(), -1);
        for (int k = 0; k < NS; ++k) {
            int64_t * a = base(k);
            for (int i = 0; i < N; ++i) {
                a[N + i] = 0;                 // shift
                a[2*N + i] = 0;               // used
            }
            for (int i = 0; i < N*W; ++i) a[3*N + i] = 0;
            for (int i = 0; i < N; ++i) { a[3*N + N*W + i] = 0; a[3*N + N*W + N + i] = 0; a[3*N + N*W + 2*N + i] = 0; }
            for (int s = 0; s < SQ; ++s) a[6*N + N*W + 2*N*N + s] = 0;
            a[6*N + N*W + 2*N*N + SQ] = 0;
        }
        heads.assign(NS, 0);
    }
    int64_t * base(int k) { return v.data() + (size_t)k * per_slots(); }
    int64_t &pos(int k, int i) { return base(k)[i]; }
    int64_t &shift(int k, int i) { return base(k)[N + i]; }
    int64_t &used(int k, int i) { return base(k)[2*N + i]; }
    int64_t &seqw(int k, int i, int w) { return base(k)[3*N + i*W + w]; }
    int64_t &ext(int k, int i, int f) { return base(k)[3*N + N*W + f*N + i]; }
    int64_t &spos(int k, int s, int j) { return base(k)[6*N + N*W + (s*N + j)]; }
    int64_t &sidx(int k, int s, int j) { return base(k)[6*N + N*W + N*N + (s*N + j)]; }
    int64_t &scnt(int k, int s) { return base(k)[6*N + N*W + 2*N*N + s]; }
    std::vector<int64_t> heads;
};

static int checks, failures;
static void check(bool ok, const char * what, int trial, int step) {
    ++checks;
    if (!ok) { ++failures; if (failures < 12) std::fprintf(stderr, "FAIL %s trial=%d step=%d\n", what, trial, step); }
}

static void run_goose(int op, Arena & A, const std::vector<int> & s2s, int sid, int did, int p0, int p1) {
    g_packet[0][0] = op;
    g_packet[0][1] = (int64_t)(uintptr_t)A.v.data();
    g_packet[0][2] = (int64_t)((size_t)A.per_slots() * 8);
    g_packet[0][3] = (int64_t)(uintptr_t)A.heads.data();
    g_packet[0][4] = N;
    g_packet[0][5] = W;
    g_packet[0][6] = SQ;
    static std::vector<int64_t> s2s64(SQ);
    for (int s = 0; s < SQ; ++s) s2s64[s] = s2s[s];
    g_packet[0][7] = (int64_t)(uintptr_t)s2s64.data();
    g_packet[0][8] = NS;
    g_packet[1][0] = sid;
    g_packet[1][1] = did;
    g_packet[1][2] = p0;
    g_packet[1][3] = p1;
    g_packet[3][0] = 0;
    if (!fg291dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
}

int main(void) {
    std::mt19937 rng(291011);
    for (int trial = 0; trial < 120; ++trial) {
        Model M; M.init();
        Arena A; A.init();
        std::vector<int> s2s(SQ);
        for (int s = 0; s < SQ; ++s) s2s[s] = (int)(rng() % NS);

        // Random occupancy in both streams.
        for (int k = 0; k < NS; ++k) {
            for (int i = 0; i < N; ++i) {
                if (rng() % 100 < 40) continue;
                const int p = (int)(rng() % 60);
                const int ns = 1 + (int)(rng() % 2);
                uint64_t bits = 0;
                while (__builtin_popcountll(bits) < ns) bits |= (1ull << (rng() % SQ));
                M.st[k].c[i].pos = p;
                M.st[k].c[i].shift = (int)(rng() % 5);
                M.st[k].c[i].used = true;
                M.st[k].c[i].ex = (int)(rng() % 100);
                M.st[k].c[i].ey = (int)(rng() % 100);
                M.st[k].c[i].tok = (int)(rng() % 1000);
                A.pos(k, i) = p;
                A.shift(k, i) = M.st[k].c[i].shift;
                A.used(k, i) = 1;
                A.ext(k, i, 0) = M.st[k].c[i].ex;
                A.ext(k, i, 1) = M.st[k].c[i].ey;
                A.ext(k, i, 2) = M.st[k].c[i].tok;
                for (int s = 0; s < SQ; ++s) if (bits & (1ull << s)) {
                    M.st[k].c[i].seq.set(s);
                    M.st[k].sp[s].insert({p, (uint32_t)i});
                }
                for (int w = 0; w < W; ++w) {
                    uint64_t word = 0;
                    for (int b = 0; b < 64; ++b) if (b < SQ && M.st[k].c[i].seq.test(b)) word |= (1ull << b);
                    A.seqw(k, i, w) = (w == 0) ? (int64_t)word : 0;
                }
            }
            const int j0 = 0;
            for (int s = 0; s < SQ; ++s) {
                int j = j0;
                for (const auto & pr : M.st[k].sp[s]) {
                    A.spos(k, s, j) = pr.first;
                    A.sidx(k, s, j) = pr.second;
                    ++j;
                }
                A.scnt(k, s) = (int64_t)M.st[k].sp[s].size();
            }
            A.heads[k] = (int64_t)(rng() % N);
            M.st[k].head = (uint32_t)A.heads[k];
        }

        for (int step = 0; step < 40; ++step) {
            const int op = (int)(rng() % 4);
            int p0 = (int)(rng() % 60) - 10;
            int p1 = (int)(rng() % 60) - 10;
            if (rng() % 3 == 0) p1 = -1;
            if (rng() % 4 == 0) p0 = -1;

            if (op == 0) {
                const int sid = (int)(rng() % SQ);
                run_goose(0, A, s2s, sid, 0, p0, p1);
                int q0 = std::max(0, p0), q1 = (p1 < 0) ? 2147483647 : p1;
                Stream & S = M.st[s2s[sid]];
                uint32_t new_head = N;
                for (int i = 0; i < N; ++i) {
                    if (!S.pos_in(i, q0, q1)) continue;
                    if (S.has(i, sid)) {
                        const bool emptied = S.c[i].seq.count() == 1;
                        S.seq_rm_cell(i, sid);
                        if (emptied && new_head == (uint32_t)N) new_head = i;
                    }
                }
                if (new_head != (uint32_t)N && new_head < S.head) S.head = new_head;
            } else if (op == 1) {
                run_goose(1, A, s2s, 0, 0, p0, p1);
                int q0 = std::max(0, p0), q1 = (p1 < 0) ? 2147483647 : p1;
                for (int k = 0; k < NS; ++k) {
                    uint32_t new_head = N;
                    for (int i = 0; i < N; ++i) {
                        if (!M.st[k].pos_in(i, q0, q1)) continue;
                        M.st[k].rm(i);
                        if (new_head == (uint32_t)N) new_head = i;
                    }
                    if (new_head != (uint32_t)N && new_head < M.st[k].head) M.st[k].head = new_head;
                }
            } else if (op == 2) {
                const int sid = (int)(rng() % SQ);
                run_goose(2, A, s2s, sid, 0, p0, p1);
                Stream & S = M.st[s2s[sid]];
                uint32_t new_head = N;
                for (int i = 0; i < N; ++i)
                    if (S.seq_keep_cell(i, sid) == 1 && new_head == (uint32_t)N) new_head = i;
                if (new_head != (uint32_t)N && new_head < S.head) S.head = new_head;
            } else {
                const int a = (int)(rng() % SQ), b = (int)(rng() % SQ);
                if (s2s[a] != s2s[b]) continue;   // cross-stream needs the copy queue
                {
                    // Native seq_add asserts that the destination bit is absent; only valid
                    // states are comparable, so a conflicting draw skips the step.
                    const int q0 = std::max(0, p0), q1 = (p1 < 0) ? 2147483647 : p1;
                    bool conflict = false;
                    for (int i = 0; i < N && !conflict; ++i)
                        if (M.st[s2s[a]].pos_in(i, q0, q1) && M.st[s2s[a]].has(i, a) && M.st[s2s[a]].has(i, b))
                            conflict = true;
                    if (conflict) continue;
                }
                run_goose(3, A, s2s, a, b, p0, p1);
                if (a != b) {
                    Stream & S = M.st[s2s[a]];
                    const int q0 = std::max(0, p0), q1 = (p1 < 0) ? 2147483647 : p1;
                    for (int i = 0; i < N; ++i)
                        if (S.pos_in(i, q0, q1) && S.has(i, a)) S.seq_add_cell(i, b);
                }
            }

            // Full comparison.
            for (int k = 0; k < NS; ++k) {
                for (int i = 0; i < N; ++i) {
                    check(A.pos(k, i) == M.st[k].c[i].pos, "pos", trial, step);
                    check(A.shift(k, i) == M.st[k].c[i].shift, "shift", trial, step);
                    check((A.used(k, i) != 0) == M.st[k].c[i].used, "used", trial, step);
                    check(A.ext(k, i, 0) == M.st[k].c[i].ex, "extx", trial, step);
                    check(A.ext(k, i, 1) == M.st[k].c[i].ey, "exty", trial, step);
                    check(A.ext(k, i, 2) == M.st[k].c[i].tok, "exttok", trial, step);
                    for (int w = 0; w < W; ++w) {
                        uint64_t want = 0;
                        for (int b = 0; b < 64; ++b) if (b < SQ && M.st[k].c[i].seq.test(b)) want |= (1ull << b);
                        check((uint64_t)A.seqw(k, i, w) == (w == 0 ? want : 0), "seqw", trial, step);
                    }
                }
                for (int s = 0; s < SQ; ++s) {
                    if (failures < 3 && ((size_t)A.scnt(k, s) != M.st[k].sp[s].size())) {
                        std::fprintf(stderr, "DBG cnt trial=%d step=%d k=%d s=%d want=%zu got=%lld\n",
                            trial, step, k, s, M.st[k].sp[s].size(), (long long)A.scnt(k, s));
                    }
                    check((size_t)A.scnt(k, s) == M.st[k].sp[s].size(), "sp size", trial, step);
                    int j = 0;
                    for (const auto & pr : M.st[k].sp[s]) {
                        if (failures < 3 && j < (int)A.scnt(k, s) &&
                            (A.spos(k, s, j) != pr.first || A.sidx(k, s, j) != pr.second)) {
                            std::fprintf(stderr, "DBG pair trial=%d step=%d k=%d s=%d j=%d want=(%d,%u) got=(%lld,%lld)\n",
                                trial, step, k, s, j, pr.first, pr.second,
                                (long long)A.spos(k, s, j), (long long)A.sidx(k, s, j));
                        }
                        check(A.spos(k, s, j) == pr.first, "sp pos", trial, step);
                        check(A.sidx(k, s, j) == pr.second, "sp idx", trial, step);
                        ++j;
                    }
                }
                check((uint32_t)A.heads[k] == M.st[k].head, "head", trial, step);
            }
        }
    }
    std::printf("FG291K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
