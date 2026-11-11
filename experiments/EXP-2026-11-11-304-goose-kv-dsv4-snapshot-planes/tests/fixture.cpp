// Differential gate for the Goose DSV4 restore/snapshot planes.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <algorithm>

extern "C" uint8_t fg304dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg304k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg304k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg304k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg304k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg304k_fail(int64_t c) { std::fprintf(stderr, "FG304K validation code %lld\n", (long long)c); std::abort(); }

struct Ubatch {
    std::vector<int> pos;
    std::vector<std::vector<int>> seq_id;
    std::vector<int> seq_id_unq;
};

static bool token_has_seq(const Ubatch & ub, int i, int seq_id) {
    for (int s : ub.seq_id[i]) if (s == seq_id) return true;
    return false;
}

struct Plan { std::vector<int> r_src, r_dst, s_src, s_dst; };

static Plan ref_planes(const Ubatch & ub, uint32_t state_size, uint32_t n_stream, uint32_t n_rs_seq,
                       const std::vector<uint32_t> & rs_idx) {
    Plan p;
    if (n_rs_seq == 0) return p;
    const int64_t state_rows = (int64_t)state_size*n_stream;
    for (uint32_t stream = 0; stream < n_stream; ++stream) {
        int seq_id = -1;
        if (n_stream == 1) {
            if (!ub.seq_id_unq.empty()) seq_id = ub.seq_id_unq[0];
        } else {
            for (int s : ub.seq_id_unq) if (s == (int)stream) seq_id = s;
        }
        const int64_t stream_off = (int64_t)stream*state_size;
        const uint32_t rollback = seq_id >= 0 && (size_t)seq_id < rs_idx.size() ? rs_idx[seq_id] : 0;
        const int64_t src_plane = rollback > 0 && rollback <= n_rs_seq ? (int64_t)rollback*state_rows : 0;
        for (uint32_t r = 0; r < state_size; ++r) {
            p.r_src.push_back((int)(src_plane + stream_off + r));
            p.r_dst.push_back((int)(stream_off + r));
        }

        std::vector<uint32_t> token_idxs;
        if (seq_id >= 0)
            for (uint32_t i = 0; i < ub.pos.size(); ++i)
                if (token_has_seq(ub, i, seq_id)) token_idxs.push_back(i);
        const uint32_t n_seq_tokens = (uint32_t)token_idxs.size();
        const int64_t scratch_off = state_rows*(1 + n_rs_seq);
        for (uint32_t d = 1; d <= n_rs_seq; ++d) {
            const int64_t dst_plane = (int64_t)d*state_rows;
            const uint32_t prefix = d <= n_seq_tokens ? n_seq_tokens - d : 0;
            for (uint32_t r = 0; r < state_size; ++r) {
                int32_t src = (int32_t)(stream_off + r);
                for (uint32_t j = 0; j < prefix; ++j) {
                    const uint32_t i_tok = token_idxs[j];
                    if (ub.pos[i_tok] >= 0 && (uint32_t)(ub.pos[i_tok]%state_size) == r)
                        src = (int32_t)(scratch_off + i_tok);
                }
                if (n_seq_tokens == 0) src = (int32_t)(dst_plane + stream_off + r);
                p.s_src.push_back(src);
                p.s_dst.push_back((int)(dst_plane + stream_off + r));
            }
        }
    }
    return p;
}

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

int main(void) {
    std::mt19937 rng(304011);
    static std::vector<int64_t> r_src(1 << 14), r_dst(1 << 14), s_src(1 << 16), s_dst(1 << 16);

    for (int trial = 0; trial < 3000; ++trial) {
        Ubatch ub;
        const uint32_t n_stream = 1 + (uint32_t)(rng() % 4);
        const uint32_t state_size = 1 + (uint32_t)(rng() % 8);
        const uint32_t n_rs_seq = (uint32_t)(rng() % 4);           // 0..3
        const uint32_t width = 1 + (uint32_t)(rng() % 3);
        const int n_tokens = 1 + (int)(rng() % 8);
        const int n_unq = (int)(rng() % 4);                         // may be zero
        const uint32_t n_seq_max = 8;

        std::vector<uint32_t> rs_idx(n_seq_max);
        for (auto & v : rs_idx) v = (uint32_t)(rng() % 5);          // 0..4, may exceed n_rs_seq

        ub.pos.resize(n_tokens);
        ub.seq_id.resize(n_tokens);
        ub.seq_id_unq.clear();
        for (int i = 0; i < n_unq; ++i) ub.seq_id_unq.push_back((int)(rng() % (n_stream + 2)) - 1);
        for (int i = 0; i < n_tokens; ++i) {
            ub.pos[i] = (rng() % 6 == 0) ? -1 : (int)(rng() % 40);
            const uint32_t k = 1 + (uint32_t)(rng() % width);
            ub.seq_id[i].clear();
            for (uint32_t j = 0; j < k; ++j) ub.seq_id[i].push_back((int)(rng() % (n_stream + 1)));
        }

        const Plan want = ref_planes(ub, state_size, n_stream, n_rs_seq, rs_idx);

        std::vector<int64_t> posv(ub.pos.begin(), ub.pos.end());
        std::vector<int64_t> nsid(n_tokens), sid(width * n_tokens, 0);
        std::vector<int64_t> unq(ub.seq_id_unq.begin(), ub.seq_id_unq.end());
        for (int i = 0; i < n_tokens; ++i) {
            nsid[i] = (int64_t)ub.seq_id[i].size();
            for (size_t j = 0; j < ub.seq_id[i].size(); ++j) sid[i*width + j] = ub.seq_id[i][j];
        }
        std::vector<int64_t> rsi(rs_idx.begin(), rs_idx.end());

        for (int i = 0; i < 16; ++i) g_packet[0][i] = 0;
        g_packet[0][1] = n_stream;
        g_packet[0][2] = state_size;
        g_packet[0][3] = n_rs_seq;
        g_packet[0][4] = (int64_t)(uintptr_t)rsi.data();
        g_packet[0][5] = n_unq;
        g_packet[0][6] = (int64_t)(uintptr_t)unq.data();
        g_packet[0][7] = n_tokens;
        g_packet[0][8] = (int64_t)(uintptr_t)posv.data();
        g_packet[0][9] = (int64_t)(uintptr_t)nsid.data();
        g_packet[0][10] = (int64_t)(uintptr_t)sid.data();
        g_packet[0][11] = width;
        g_packet[0][12] = (int64_t)rsi.size();
        g_packet[1][0] = (int64_t)(uintptr_t)r_src.data();
        g_packet[1][1] = (int64_t)(uintptr_t)r_dst.data();
        g_packet[1][2] = (int64_t)(uintptr_t)s_src.data();
        g_packet[1][3] = (int64_t)(uintptr_t)s_dst.data();
        g_packet[3][0] = 0;
        g_packet[3][1] = 0;
        if (!fg304dh_run((int64_t)(uintptr_t)g_packet)) std::abort();

        check((size_t)g_packet[3][1] == want.r_src.size(), "restore count", trial);
        check((size_t)(g_packet[3][1] * n_rs_seq) == want.s_src.size(), "snapshot count", trial);
        for (size_t i = 0; i < want.r_src.size(); ++i) {
            check(r_src[i] == want.r_src[i], "restore src", trial);
            check(r_dst[i] == want.r_dst[i], "restore dst", trial);
        }
        for (size_t i = 0; i < want.s_src.size(); ++i) {
            check(s_src[i] == want.s_src[i], "snapshot src", trial);
            check(s_dst[i] == want.s_dst[i], "snapshot dst", trial);
        }
    }
    std::printf("FG304K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
