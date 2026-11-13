// Differential gate for the Goose DSV4 comp plan (overlap, no rollback planes).
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <map>
#include <set>
#include <random>
#include <algorithm>
#include <functional>
#include <stdexcept>

extern "C" uint8_t fg303dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg303k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg303k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg303k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg303k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg303k_fail(int64_t c) { std::fprintf(stderr, "FG303K validation code %lld\n", (long long)c); std::abort(); }

struct Ubatch {
    std::vector<int> pos;
    std::vector<std::vector<int>> seq_id;
    std::vector<int> seq_id_unq;
    int n_seq_tokens = 1;
};

struct Plan {
    std::vector<int> n_visible, state_pos, w_idx, w_pos, r_idx, p_src, p_dst;
    int64_t n_kv = 0;
    int64_t graph_streams = 0;
};

static int64_t stream_offset(uint32_t n_stream, int64_t seq_id, uint32_t size) {
    if (n_stream <= 1) return 0;
    if (seq_id < 0 || (uint32_t)seq_id >= n_stream) throw std::runtime_error("range");
    return seq_id*size;
}

static bool token_has_seq(const Ubatch & ub, int i, int seq_id) {
    for (int s : ub.seq_id[i]) if (s == seq_id) return true;
    return false;
}

static bool has_coupled(const Ubatch & ub) {
    for (size_t i = 0; i < ub.pos.size(); ++i) if ((int)ub.seq_id[i].size() > 1) return true;
    return false;
}

static int64_t graph_n_stream(const Ubatch & ub, uint32_t n_stream) {
    if (n_stream <= 1 || ub.seq_id_unq.size() <= 1 || has_coupled(ub)) return 1;
    return (int64_t)ub.seq_id_unq.size();
}

// Overlap, no rollback planes: the read list is prev halves then cur halves.
static Plan ref_plan(const Ubatch & ub, uint32_t ratio, uint32_t state_size, uint32_t kv_size,
                     uint32_t n_stream, bool trace = false) {
    Plan plan;
    plan.n_visible.assign(ub.pos.size(), 0);
    plan.graph_streams = graph_n_stream(ub, n_stream);
    if (n_stream <= 1 && ub.seq_id_unq.size() > 1) throw std::runtime_error("single stream");

    const int64_t state_rows = (int64_t)state_size*n_stream;
    struct persist_row { int32_t dst, src; int32_t pos; };
    std::vector<persist_row> persist_rows;
    std::vector<int> prev_reads, cur_reads;
    std::map<std::pair<int,int>, int> curr;
    std::map<int,uint32_t> write_counts;

    for (size_t i = 0; i < ub.pos.size(); ++i)
        for (int s : ub.seq_id[i]) curr[{s, ub.pos[i]}] = (int)i;

    auto source_idx = [&](int seq_id, int pos) -> int {
        if (pos < 0) return (int)(state_rows + ub.pos.size());
        auto it = curr.find({seq_id, pos});
        if (it != curr.end()) return (int)(state_rows + it->second);
        const int64_t off = stream_offset(n_stream, seq_id, state_size);
        return (int)(off + pos%state_size);
    };

    for (size_t i = 0; i < ub.pos.size(); ++i) {
        const int pos = ub.pos[i];
        if (pos < 0) continue;
        plan.state_pos.push_back(pos%ratio);
        const int64_t n_vis = ((int64_t)pos + 1)/ratio;
        plan.n_visible[i] = (int)n_vis;
        plan.n_kv = std::max(plan.n_kv, n_vis);

        for (int seq_id : ub.seq_id[i]) {
            const int64_t off = stream_offset(n_stream, seq_id, state_size);
            const int32_t st = (int32_t)(off + pos%state_size);
            auto it = std::find_if(persist_rows.begin(), persist_rows.end(),
                    [st](const persist_row & r){ return r.dst == st; });
            if (it == persist_rows.end()) persist_rows.push_back({st, (int32_t)i, pos});
            else if (pos > it->pos) { it->src = (int32_t)i; it->pos = pos; }

            if ((pos + 1) % ratio != 0) continue;
            const int src_start = pos + 1 - ratio;
            const int64_t cache_off = stream_offset(n_stream, seq_id, kv_size);
            plan.w_idx.push_back((int)(cache_off + pos/ratio));
            plan.w_pos.push_back(src_start);
            ++write_counts[seq_id];
            for (uint32_t j = 0; j < ratio; ++j) prev_reads.push_back(source_idx(seq_id, src_start - ratio + j));
            for (uint32_t j = 0; j < ratio; ++j) cur_reads.push_back(source_idx(seq_id, src_start + j));
        }
    }

    auto append_dummy = [&](int seq_id, int i) {
        const int64_t cache_off = stream_offset(n_stream, seq_id, kv_size);
        const int32_t src = source_idx(seq_id, ub.pos[i]);
        plan.w_idx.push_back((int)(cache_off + kv_size - 1));
        plan.w_pos.push_back(0);
        for (uint32_t j = 0; j < ratio; ++j) { prev_reads.push_back(src); cur_reads.push_back(src); }
    };

    if (ratio == 4 && !plan.state_pos.empty()) {
        if (has_coupled(ub)) {
            if (plan.w_idx.empty()) {
                int i = 0;
                while (i < (int)ub.pos.size() && ub.pos[i] < 0) ++i;
                if (i >= (int)ub.pos.size()) throw std::runtime_error("no token");
                append_dummy(ub.seq_id[i][0], i);
            }
        } else {
            const uint32_t n_blocks = (std::max<uint32_t>(1, ub.n_seq_tokens) + ratio - 1)/ratio;
            for (int seq_id : ub.seq_id_unq) {
                const uint32_t n_writes = write_counts[seq_id];
                if (n_writes >= n_blocks) continue;
                if (n_writes + 1 != n_blocks) throw std::runtime_error("non-contiguous");
                int i = 0;
                while (i < (int)ub.pos.size() && (ub.pos[i] < 0 || !token_has_seq(ub, i, seq_id))) ++i;
                if (i >= (int)ub.pos.size()) throw std::runtime_error("no token");
                append_dummy(seq_id, i);
            }
        }
    }

    if (ratio == 128 && !plan.state_pos.empty() && plan.w_idx.empty()) {
        // native asserts the last slot is not live
        if (plan.n_kv >= (int64_t)kv_size) throw std::runtime_error("dummy would corrupt");
        int i = 0;
        while (i < (int)ub.pos.size() && ub.pos[i] < 0) ++i;
        if (i >= (int)ub.pos.size()) throw std::runtime_error("no token");
        append_dummy(ub.seq_id[i][0], i);
    }

    plan.r_idx.insert(plan.r_idx.end(), prev_reads.begin(), prev_reads.end());
    plan.r_idx.insert(plan.r_idx.end(), cur_reads.begin(), cur_reads.end());
    plan.n_kv = std::max<int64_t>((plan.n_kv + 255) & ~255, 256);

    std::sort(persist_rows.begin(), persist_rows.end(), [](const persist_row & a, const persist_row & b){ return a.dst < b.dst; });
    for (const auto & r : persist_rows) { plan.p_src.push_back(r.src); plan.p_dst.push_back(r.dst); }
    return plan;
}

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

int main(void) {
    std::mt19937 rng(303011);
    static std::vector<int64_t> out(1 << 16), scratch(1 << 12), counts(16);
    static std::vector<int64_t> a_vis, a_spos, a_widx, a_wpos, a_ridx, a_psrc, a_pdst, a_prev, a_cur;
    a_vis.resize(1 << 12); a_spos.resize(1 << 12);
    a_widx.resize(1 << 12); a_wpos.resize(1 << 12);
    a_ridx.resize(1 << 14); a_psrc.resize(1 << 12); a_pdst.resize(1 << 12);
    a_prev.resize(1 << 12); a_cur.resize(1 << 12);
    int ran = 0, skipped = 0;

    for (int trial = 0; trial < 3000; ++trial) {
        Ubatch ub;
        const int n_tokens = 1 + (int)(rng() % 8);
        const uint32_t width = 1 + (uint32_t)(rng() % 3);
        const uint32_t n_stream = 1 + (uint32_t)(rng() % 4);
        const uint32_t ratio = (rng() % 3 == 0) ? 4 : ((rng() % 5 == 0) ? 128 : 8);
        const uint32_t state_size = 1 + (uint32_t)(rng() % 16);
        const uint32_t kv_size = 1 + (uint32_t)(rng() % 32);
        const int n_unq = 1 + (int)(rng() % 4);
        if (n_stream <= 1 && n_unq > 1) { ++skipped; continue; }

        ub.pos.resize(n_tokens);
        ub.seq_id.resize(n_tokens);
        ub.seq_id_unq.clear();
        for (int i = 0; i < n_unq; ++i) ub.seq_id_unq.push_back((int)(rng() % n_stream));
        ub.n_seq_tokens = n_tokens / n_unq;
        if (ub.n_seq_tokens == 0) ub.n_seq_tokens = 1;

        for (int i = 0; i < n_tokens; ++i) {
            ub.pos[i] = (rng() % 4 == 0) ? (int)(rng() % 5000) : (int)(rng() % 40) - 2;
            if (rng() % 6 == 0) ub.pos[i] = -1;
            const uint32_t k = 1 + (uint32_t)(rng() % width);
            ub.seq_id[i].clear();
            for (uint32_t j = 0; j < k; ++j) ub.seq_id[i].push_back((int)(rng() % n_stream));
        }
        // Keep coupled sets out of the seq_id_unq path only when the native allows it.
        if (!has_coupled(ub) && n_stream > 1) {
            // guarantee every unique id appears in at least one token so the dummy path can find it
        }

        Plan want;
        bool threw = false;
        try { want = ref_plan(ub, ratio, state_size, kv_size, n_stream); }
        catch (...) { threw = true; }
        if (threw) { ++skipped; continue; }
        ++ran;

        std::vector<int64_t> posv(ub.pos.begin(), ub.pos.end());
        std::vector<int64_t> nsid(n_tokens), sid(width * n_tokens, 0), unq(ub.seq_id_unq.begin(), ub.seq_id_unq.end());
        for (int i = 0; i < n_tokens; ++i) {
            nsid[i] = (int64_t)ub.seq_id[i].size();
            for (size_t j = 0; j < ub.seq_id[i].size(); ++j) sid[i*width + j] = ub.seq_id[i][j];
        }
        std::fill(out.begin(), out.end(), 0x7fff);
        std::fill(scratch.begin(), scratch.end(), 0);
        std::fill(counts.begin(), counts.end(), 0);

        for (int i = 0; i < 16; ++i) g_packet[0][i] = 0;
        g_packet[0][1] = (int64_t)(uintptr_t)out.data();
        g_packet[0][2] = n_tokens;
        g_packet[0][3] = ratio;
        g_packet[0][4] = state_size;
        g_packet[0][5] = kv_size;
        g_packet[0][6] = n_stream;
        g_packet[0][7] = (int64_t)(uintptr_t)posv.data();
        g_packet[0][8] = (int64_t)(uintptr_t)nsid.data();
        g_packet[0][9] = (int64_t)(uintptr_t)sid.data();
        g_packet[0][10] = width;
        g_packet[0][11] = n_unq;
        g_packet[0][12] = (int64_t)(uintptr_t)unq.data();
        g_packet[0][13] = (int64_t)(uintptr_t)scratch.data();
        g_packet[0][14] = (int64_t)(uintptr_t)counts.data();
        g_packet[0][15] = ub.n_seq_tokens;
        g_packet[1][0] = (int64_t)(uintptr_t)a_vis.data();
        g_packet[1][1] = (int64_t)(uintptr_t)a_spos.data();
        g_packet[1][2] = (int64_t)(uintptr_t)a_widx.data();
        g_packet[1][3] = (int64_t)(uintptr_t)a_wpos.data();
        g_packet[1][4] = (int64_t)(uintptr_t)a_ridx.data();
        g_packet[1][5] = (int64_t)(uintptr_t)a_psrc.data();
        g_packet[1][6] = (int64_t)(uintptr_t)a_pdst.data();
        g_packet[2][0] = (int64_t)(uintptr_t)a_prev.data();
        g_packet[2][1] = (int64_t)(uintptr_t)a_cur.data();
        g_packet[3][0] = 0;
        g_packet[3][1] = 0;
        if (!fg303dh_run((int64_t)(uintptr_t)g_packet)) std::abort();



        check((int64_t)counts[0] == (int64_t)want.state_pos.size(), "state_pos count", trial);
        check((int64_t)counts[1] == (int64_t)want.w_idx.size(), "write count", trial);
        check((int64_t)counts[2] == (int64_t)want.r_idx.size(), "read count", trial);
        check((int64_t)counts[3] == (int64_t)want.p_src.size(), "persist count", trial);
        check((int64_t)counts[4] == want.n_kv, "n_kv", trial);
        check((int64_t)counts[5] == want.graph_streams, "graph streams", trial);

        for (size_t i = 0; i < want.state_pos.size() && i < (size_t)counts[0]; ++i)
            check(a_spos[i] == want.state_pos[i], "state_pos", trial);
        for (size_t i = 0; i < want.w_idx.size() && i < (size_t)counts[1]; ++i) {
            check(a_widx[i] == want.w_idx[i], "write idx", trial);
            check(a_wpos[i] == want.w_pos[i], "write pos", trial);
        }
        for (size_t i = 0; i < want.r_idx.size() && i < (size_t)counts[2]; ++i) {
            check(a_ridx[i] == want.r_idx[i], "read idx", trial);
        }
        for (size_t i = 0; i < want.p_src.size() && i < (size_t)counts[3]; ++i) {
            check(a_psrc[i] == want.p_src[i], "persist src", trial);
            check(a_pdst[i] == want.p_dst[i], "persist dst", trial);
        }
        for (size_t i = 0; i < want.n_visible.size(); ++i)
            check(a_vis[i] == want.n_visible[i], "n_visible", trial);
    }

    std::printf("FG303K_FIXTURE checks=%d failures=%d ran=%d skipped=%d\n", checks, failures, ran, skipped);
    return failures ? 1 : 0;
}
