// Differential gate for the Goose cross-stream seq_cp.
//
// Oracle: a transcription of the stream-copy branch of llama_kv_cache::seq_cp over a model that
// keeps the native std::set per sequence.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <bitset>
#include <set>
#include <random>
#include <algorithm>

extern "C" uint8_t fg292dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg292k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg292k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg292k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg292k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg292k_fail(int64_t c) { std::fprintf(stderr, "FG292K validation code %lld\n", (long long)c); std::abort(); }

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
    void init() { c.assign(N, Cell{}); for (int s = 0; s < SQ; ++s) sp[s].clear(); head = 0; }
    bool has(int i, int s) const { return c[i].seq.test(s); }
    void insert_sp(int i, int s) { sp[s].insert({c[i].pos, (uint32_t)i}); }
    void reset() {
        for (int i = 0; i < N; ++i) c[i] = Cell{};
        for (int s = 0; s < SQ; ++s) sp[s].clear();
    }
    void pos_set(int i, int p) { c[i].pos = p; c[i].used = true; }
    void seq_add(int i, int s) { c[i].seq.set(s); insert_sp(i, s); }
    void pos_add(int i, int d) {
        for (int s = 0; s < SQ; ++s) if (has(i, s)) sp[s].erase({c[i].pos, (uint32_t)i});
        c[i].pos += d;
        c[i].shift += d;
        for (int s = 0; s < SQ; ++s) if (has(i, s)) sp[s].insert({c[i].pos, (uint32_t)i});
    }
};

// Transcription of the stream-copy branch, without the sc_info enqueue.
static bool ref_copy(Stream & s0s, Stream & s1s, int sid, int did, int p0, int p1, int size_,
                     int * qsrc, int * qdst) {
    bool is_full = true;
    if (p0 > 0 && p0 + 1 < size_) is_full = false;
    if (p1 > 0 && p1 + 1 < size_) is_full = false;
    if (!is_full) return false;
    *qsrc = 0; *qdst = 1;   // the pair the caller must queue
    s1s.reset();
    for (int i = 0; i < N; ++i) {
        if (!s0s.has(i, sid)) continue;
        int pos = s0s.c[i].pos;
        const int shift = s0s.c[i].shift;
        const int ex = s0s.c[i].ex, ey = s0s.c[i].ey, et = s0s.c[i].tok;
        if (shift != 0) { pos -= shift; if (pos < 0) std::abort(); }
        s1s.pos_set(i, pos);
        s1s.seq_add(i, did);
        if (shift != 0) s1s.pos_add(i, shift);
        s1s.c[i].ex = ex; s1s.c[i].ey = ey; s1s.c[i].tok = et;
    }
    s1s.head = s0s.head;
    return true;
}

struct Arena {
    std::vector<int64_t> v;
    static size_t per_slots() { return (size_t)(6*N + N*W + 2*N*N + SQ + 1); }
    void init() {
        v.assign((size_t)NS * per_slots(), -1);
        for (int k = 0; k < NS; ++k) {
            int64_t * a = base(k);
            for (int i = 0; i < N; ++i) { a[N + i] = 0; a[2*N + i] = 0; }
            for (int i = 0; i < N*W; ++i) a[3*N + i] = 0;
            for (int i = 0; i < 3*N; ++i) a[3*N + N*W + i] = 0;
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
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static void fill_arena_from_model(Arena & A, const Stream * S) {
    for (int k = 0; k < NS; ++k) {
        for (int i = 0; i < N; ++i) {
            A.pos(k, i) = S[k].c[i].pos;
            A.shift(k, i) = S[k].c[i].shift;
            A.used(k, i) = S[k].c[i].used ? 1 : 0;
            A.ext(k, i, 0) = S[k].c[i].ex;
            A.ext(k, i, 1) = S[k].c[i].ey;
            A.ext(k, i, 2) = S[k].c[i].tok;
            for (int w = 0; w < W; ++w) {
                uint64_t word = 0;
                for (int b = 0; b < 64; ++b) if (b < SQ && S[k].c[i].seq.test(b)) word |= (1ull << b);
                A.seqw(k, i, w) = (w == 0) ? (int64_t)word : 0;
            }
        }
        for (int s = 0; s < SQ; ++s) {
            int j = 0;
            for (const auto & pr : S[k].sp[s]) { A.spos(k, s, j) = pr.first; A.sidx(k, s, j) = pr.second; ++j; }
            A.scnt(k, s) = (int64_t)S[k].sp[s].size();
        }
    }
}

int main(void) {
    std::mt19937 rng(292011);
    for (int trial = 0; trial < 150; ++trial) {
        Stream S[NS];
        for (int k = 0; k < NS; ++k) S[k].init();
        for (int k = 0; k < NS; ++k) {
            for (int i = 0; i < N; ++i) {
                if (rng() % 100 < 40) continue;
                const int p = (int)(rng() % 60) + 8;     // keep room for the shift
                const int ns = 1 + (int)(rng() % 2);
                uint64_t bits = 0;
                while (__builtin_popcountll(bits) < ns) bits |= (1ull << (rng() % SQ));
                S[k].c[i].pos = p;
                S[k].c[i].shift = (int)(rng() % 4);
                S[k].c[i].used = true;
                S[k].c[i].ex = (int)(rng() % 100);
                S[k].c[i].ey = (int)(rng() % 100);
                S[k].c[i].tok = (int)(rng() % 1000);
                for (int s = 0; s < SQ; ++s) if (bits & (1ull << s)) {
                    S[k].c[i].seq.set(s);
                    S[k].sp[s].insert({p, (uint32_t)i});
                }
            }
            S[k].head = (uint32_t)(rng() % N);
        }
        Arena A; A.init();
        fill_arena_from_model(A, S);
        for (int k = 0; k < NS; ++k) A.heads[k] = S[k].head;

        const int sid = (int)(rng() % SQ), did = (int)(rng() % SQ);
        if (sid == did) continue;
        std::vector<int> s2s(SQ);
        for (int s = 0; s < SQ; ++s) s2s[s] = (s == sid) ? 0 : (s == did) ? 1 : (int)(rng() % NS);
        const int opts[5] = { -1, 0, N - 1, N, 2*N };
        const int p0 = opts[rng() % 5], p1 = opts[rng() % 5];

        int qsrc = -1, qdst = -1;
        const bool want = ref_copy(S[0], S[1], sid, did, p0, p1, N, &qsrc, &qdst);

        static std::vector<int64_t> s2s64(SQ);
        for (int s = 0; s < SQ; ++s) s2s64[s] = s2s[s];
        int64_t qs = -1, qd = -1;
        g_packet[0][0] = 0;
        g_packet[0][1] = (int64_t)(uintptr_t)A.v.data();
        g_packet[0][2] = (int64_t)((size_t)A.per_slots() * 8);
        g_packet[0][3] = (int64_t)(uintptr_t)A.heads.data();
        g_packet[0][4] = N;
        g_packet[0][5] = W;
        g_packet[0][6] = SQ;
        g_packet[0][7] = (int64_t)(uintptr_t)s2s64.data();
        g_packet[0][8] = NS;
        g_packet[0][9] = N;
        g_packet[1][0] = sid;
        g_packet[1][1] = did;
        g_packet[1][2] = p0;
        g_packet[1][3] = p1;
        g_packet[1][4] = (int64_t)(uintptr_t)&qs;
        g_packet[1][5] = (int64_t)(uintptr_t)&qd;
        g_packet[3][0] = 0;
        g_packet[3][1] = 0;
        if (!fg292dh_run((int64_t)(uintptr_t)g_packet)) std::abort();

        const bool got = g_packet[3][1] != 0;
        check(got == want, "result", trial);
        if (!want) continue;
        check((int)qs == qsrc && (int)qd == qdst, "queue pair", trial);
        for (int k = 0; k < NS; ++k) {
            for (int i = 0; i < N; ++i) {
                check(A.pos(k, i) == S[k].c[i].pos, "pos", trial);
                check(A.shift(k, i) == S[k].c[i].shift, "shift", trial);
                check((A.used(k, i) != 0) == S[k].c[i].used, "used", trial);
                check(A.ext(k, i, 0) == S[k].c[i].ex, "extx", trial);
                check(A.ext(k, i, 1) == S[k].c[i].ey, "exty", trial);
                check(A.ext(k, i, 2) == S[k].c[i].tok, "exttok", trial);
                for (int w = 0; w < W; ++w) {
                    uint64_t word = 0;
                    for (int b = 0; b < 64; ++b) if (b < SQ && S[k].c[i].seq.test(b)) word |= (1ull << b);
                    check((uint64_t)A.seqw(k, i, w) == (w == 0 ? word : 0), "seqw", trial);
                }
            }
            for (int s = 0; s < SQ; ++s) {
                check((size_t)A.scnt(k, s) == S[k].sp[s].size(), "sp size", trial);
                int j = 0;
                for (const auto & pr : S[k].sp[s]) {
                    check(A.spos(k, s, j) == pr.first, "sp pos", trial);
                    check(A.sidx(k, s, j) == pr.second, "sp idx", trial);
                    ++j;
                }
            }
            check((uint32_t)A.heads[k] == S[k].head, "head", trial);
        }
    }
    std::printf("FG292K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
