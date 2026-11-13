// Differential gate for the Goose KV slot search (find_slot) and the SWA predicate.
//
// Oracle: the SWA predicate is the verbatim native function from llama-hparams.h, and find_slot
// is a transcription of llama_kv_cache::find_slot that keeps the native std::set seq_pos.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <bitset>
#include <set>
#include <random>
#include <algorithm>
#include <cassert>

typedef int32_t llama_pos;

extern "C" uint8_t fg290dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg290k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg290k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg290k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg290k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg290k_fail(int64_t c) { std::fprintf(stderr, "FG290K validation code %lld\n", (long long)c); std::abort(); }

enum llama_swa_type {
    LLAMA_SWA_TYPE_NONE      = 0,
    LLAMA_SWA_TYPE_STANDARD  = 1,
    LLAMA_SWA_TYPE_CHUNKED   = 2,
    LLAMA_SWA_TYPE_SYMMETRIC = 3,
};
#include "swa-ref.cuh"

static const int N = 64;
static const int SQ = 8;
static const int W = 8;

struct Model {
    std::vector<int> pos;
    std::vector<char> used;
    std::vector<std::bitset<64>> seq;
    std::set<std::pair<int,uint32_t>> sp[SQ];

    void init() {
        pos.assign(N, -1); used.assign(N, 0); seq.assign(N, {});
        for (int s = 0; s < SQ; ++s) sp[s].clear();
    }
    bool empty(int i) const { return pos[i] == -1; }
    int count(int i) const { return (int)seq[i].count(); }
    int first(int i) const {
        for (int s = 0; s < SQ; ++s) if (seq[i].test(s)) return s;
        return -1;
    }
    int spmax(int s) const { return sp[s].empty() ? -1 : sp[s].rbegin()->first; }
    void occupy(int i, int p, uint64_t bits) {
        pos[i] = p;
        used[i] = 1;
        for (int s = 0; s < SQ; ++s) if (bits & (1ull << s)) { seq[i].set(s); sp[s].insert({p, (uint32_t)i}); }
    }
};

struct Slot { bool ok; uint32_t s0, s1; std::vector<uint32_t> strm; std::vector<std::vector<uint32_t>> idxs; };

// Transcription of llama_kv_cache::find_slot without the debug block.
static Slot ref_find(const Model & M, uint32_t head0, uint32_t n_swa, int swa_type,
                     const std::vector<uint32_t> & seq_id_unq, uint32_t n_tokens0,
                     uint32_t n_stream, const std::vector<uint32_t> & seq_to_stream, bool cont) {
    uint32_t n_tokens = n_tokens0;
    uint32_t n_seqs = 1;
    if (n_stream > 1) {
        n_seqs = (uint32_t)seq_id_unq.size();
        n_tokens = n_tokens / n_seqs;
    }
    Slot res{true, 512, 0, {}, {}};
    res.strm.resize(n_seqs);
    res.idxs.resize(n_seqs);
    for (uint32_t s = 0; s < n_seqs; ++s) {
        const auto seq_id = seq_id_unq[s];
        res.s0 = std::min<uint32_t>(res.s0, seq_to_stream[seq_id]);
        res.s1 = std::max<uint32_t>(res.s1, seq_to_stream[seq_id]);
        res.strm[s] = seq_to_stream[seq_id];
        uint32_t head_cur = head0;
        int used_cnt = 0;
        for (int i = 0; i < N; ++i) used_cnt += M.used[i] ? 1 : 0;
        if (head_cur > (uint32_t)used_cnt + 2*n_tokens) head_cur = 0;
        if (n_tokens > (uint32_t)N) return Slot{false, 0, 0, {}, {}};
        uint32_t n_tested = 0;
        const uint32_t n_test = cont ? n_tokens : 1;
        while (true) {
            if (head_cur + n_test > (uint32_t)N) {
                n_tested += (uint32_t)N - head_cur;
                head_cur = 0;
                continue;
            }
            for (uint32_t i = 0; i < n_test; i++) {
                const uint32_t idx = head_cur;
                head_cur++;
                n_tested++;
                bool can_use = M.empty((int)idx);
                if (!can_use && M.count((int)idx) == 1) {
                    const int pos_cell = M.pos[idx];
                    if (!can_use) {
                        const int seq_cell = M.first((int)idx);
                        if (is_masked_swa(n_swa, (llama_swa_type)swa_type, pos_cell, M.spmax(seq_cell) + 1)) can_use = true;
                    }
                }
                if (can_use) res.idxs[s].push_back(idx);
                else if (cont) break;
            }
            if (res.idxs[s].size() == n_tokens) break;
            if (cont) res.idxs[s].clear();
            if (n_tested >= (uint32_t)N) return Slot{false, 0, 0, {}, {}};
        }
        if (res.idxs[s].size() < n_tokens) return Slot{false, 0, 0, {}, {}};
    }
    return res;
}

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

struct Arena {
    std::vector<int64_t> pos, used, seqw;
    std::vector<int64_t> sid_unq, nsid, sid_tab, s2s;
    std::vector<int64_t> idxs, counts;
    void init() {
        pos.assign(N, -1); used.assign(N, 0); seqw.assign(N*W, 0);
        sid_unq.assign(8, 0); nsid.assign(8, 1);
        sid_tab.assign(8 * 512, 0);
        s2s.assign(SQ, 0);
        idxs.assign(8*8, 0); counts.assign(8, 0);
    }
};

int main(void) {
    std::mt19937 rng(290011);
    int swa_checks = 0, swa_fail = 0;

    // Exhaustive SWA predicate comparison.
    for (int type = 0; type <= 3; ++type) {
        for (uint32_t n_swa = 1; n_swa <= 17; ++n_swa) {
            for (int p0 = 0; p0 <= 40; ++p0) {
                for (int p1 = 0; p1 <= 40; p1 += 3) {
                    const bool want = is_masked_swa(n_swa, (llama_swa_type)type, p0, p1);
                    int64_t * P = &g_packet[0][0];
                    P[0] = 0; P[1] = 0; P[2] = 0; P[3] = 0; P[4] = 0; P[5] = 0; P[6] = 0;
                    P[7] = n_swa; P[8] = type;
                    int64_t buf[2] = { p0, p1 };
                    P[1] = (int64_t)(uintptr_t)buf;
                    g_packet[3][0] = 0; g_packet[3][1] = 0;
                    if (!fg290dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
                    ++swa_checks;
                    if ((g_packet[3][1] != 0) != want) {
                        ++swa_fail;
                        if (swa_fail < 6) std::fprintf(stderr, "SWA FAIL type=%d n=%u p0=%d p1=%d\n", type, n_swa, p0, p1);
                    }
                }
            }
        }
    }
    std::printf("FG290K_SWA checks=%d failures=%d\n", swa_checks, swa_fail);

    // find_slot comparison over random cache states.
    for (int trial = 0; trial < 300; ++trial) {
        Model M; M.init();
        Arena A; A.init();
        // Fill cells with random occupancy: empty, one sequence, or two sequences.
        for (int i = 0; i < N; ++i) {
            const int r = (int)(rng() % 100);
            if (r < 35) continue;
            const int p = (int)(rng() % 120);
            uint64_t bits = 0;
            const int ns = (r < 80) ? 1 : 2;
            while (__builtin_popcountll(bits) < ns) bits |= (1ull << (rng() % SQ));
            M.occupy(i, p, bits);
            A.pos[i] = p; A.used[i] = 1;
            for (int w = 0; w < W; ++w) {
                uint64_t word = 0;
                for (int b = 0; b < 64; ++b)
                    if (b < SQ && M.seq[i].test(b)) word |= (1ull << b);
                A.seqw[i*W + w] = (w == 0) ? (int64_t)word : 0;
            }
        }
        const uint32_t head = (uint32_t)(rng() % N);
        const uint32_t n_swa = 1 + (uint32_t)(rng() % 16);
        const int swa_type = (int)(rng() % 4);
        const uint32_t n_tokens = 1 + (uint32_t)(rng() % 8);
        const uint32_t n_seqs_unq = 1 + (uint32_t)(rng() % 2);
        const uint32_t n_stream = (rng() % 2) ? 2 : 1;
        std::vector<uint32_t> seq_id_unq(n_seqs_unq), seq_to_stream(SQ, 0);
        for (int s = 0; s < SQ; ++s) seq_to_stream[s] = (uint32_t)(rng() % n_stream);
        for (uint32_t s = 0; s < n_seqs_unq; ++s) {
            seq_id_unq[s] = (uint32_t)(rng() % SQ);
            A.s2s[seq_id_unq[s]] = seq_to_stream[seq_id_unq[s]];
            A.sid_unq[s] = seq_id_unq[s];
        }
        const bool cont = (rng() & 1) != 0;
        // Native asserts n_tokens % n_seqs_unq == 0 when streaming; skip those draws.
        const bool streaming = n_stream > 1 && (n_tokens % n_seqs_unq == 0);
        if (n_stream > 1 && !streaming) continue;
        const uint32_t use_stream = streaming ? n_stream : 1;
        std::vector<uint32_t> st(SQ, 0);
        for (int s = 0; s < SQ; ++s) st[s] = streaming ? seq_to_stream[s] : 0;
        for (int s = 0; s < SQ; ++s) A.s2s[s] = st[s];
        if (streaming) {
            for (uint32_t s = 0; s < n_seqs_unq; ++s)
                A.sid_tab[(size_t)(s * (n_tokens / n_seqs_unq)) * 512] = seq_id_unq[s];
        }

        const Slot want = ref_find(M, head, n_swa, swa_type, seq_id_unq, n_tokens, use_stream, st, cont);

        int64_t * P = &g_packet[0][0];
        P[0] = 1; P[1] = (int64_t)(uintptr_t)A.pos.data(); P[2] = (int64_t)(uintptr_t)A.used.data();
        P[3] = (int64_t)(uintptr_t)A.seqw.data(); P[4] = N; P[5] = W; P[6] = head;
        P[7] = n_swa; P[8] = swa_type; P[9] = n_tokens; P[10] = n_seqs_unq;
        P[11] = (int64_t)(uintptr_t)A.sid_unq.data(); P[12] = (int64_t)(uintptr_t)A.nsid.data();
        P[13] = (int64_t)(uintptr_t)A.sid_tab.data(); P[14] = (int64_t)(uintptr_t)A.s2s.data();
        P[15] = use_stream;
        g_packet[1][0] = cont ? 1 : 0;
        g_packet[1][1] = (int64_t)(uintptr_t)A.idxs.data();
        g_packet[1][2] = (int64_t)(uintptr_t)A.counts.data();
        g_packet[3][0] = 0; g_packet[3][1] = 0; g_packet[3][2] = 0; g_packet[3][3] = 0;
        if (!fg290dh_run((int64_t)(uintptr_t)g_packet)) std::abort();

        const bool got_ok = g_packet[3][1] != 0;
        check(got_ok == want.ok, "ok", trial);
        if (want.ok) {
            check((uint32_t)g_packet[3][2] == want.s0, "s0", trial);
            check((uint32_t)g_packet[3][3] == want.s1, "s1", trial);
            for (uint32_t s = 0; s < want.strm.size(); ++s)
                check((uint32_t)g_packet[3][4 + s] == want.strm[s], "strm", trial);
            const uint32_t nt_local = streaming ? n_tokens / n_seqs_unq : n_tokens;
            for (size_t s = 0; s < want.idxs.size(); ++s) {
                check((size_t)A.counts[s] == want.idxs[s].size(), "idxs count", trial);
                for (size_t j = 0; j < want.idxs[s].size() && j < (size_t)A.counts[s]; ++j) {
                    const bool eq = (uint32_t)A.idxs[s * nt_local + j] == want.idxs[s][j];
                    if (!eq && failures < 4)
                        std::fprintf(stderr, "DBG trial=%d head=%u cont=%d swa=%u type=%d nt=%u nseq=%u s=%zu j=%zu want=%u got=%d\n",
                            trial, head, (int)cont, n_swa, swa_type, n_tokens, n_seqs_unq, s, j, want.idxs[s][j], (int)A.idxs[s * nt_local + j]);
                    check(eq, "idxs value", trial);
                }
            }
        }
    }
    std::printf("FG290K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return (failures || swa_fail) ? 1 : 0;
}
