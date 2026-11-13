// Differential gate for the Goose KV state read orchestration and clearing.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg344dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg344k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg344k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg344k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg344k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg344k_fail(int64_t c) { std::fprintf(stderr, "FG344K validation code %lld\n", (long long)c); std::exit(3); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 13), cnt(1);
struct Rec { int64_t k, a, b; };

static int64_t call_read(int64_t other, int64_t partial, int64_t swa_none, int64_t seq_id, int64_t n_stream,
                         int64_t mirror, int64_t mirror_size, int64_t n_stream_cur, std::vector<int64_t> & cc,
                         std::vector<int64_t> & mempty, std::vector<int64_t> & mok, std::vector<int64_t> & dthr,
                         std::vector<int64_t> & s2s, std::vector<Rec> & recs) {
    for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
    std::fill(out.begin(), out.end(), -99); cnt[0] = 0;
    g_packet[0][0] = 0; g_packet[0][1] = other; g_packet[0][2] = partial; g_packet[0][3] = swa_none;
    g_packet[0][4] = seq_id; g_packet[0][5] = n_stream; g_packet[0][6] = mirror; g_packet[0][7] = mirror_size;
    g_packet[0][8] = n_stream_cur;
    g_packet[0][9] = (int64_t)(uintptr_t)cc.data();
    g_packet[0][10] = (int64_t)(uintptr_t)mempty.data();
    g_packet[0][11] = (int64_t)(uintptr_t)mok.data();
    g_packet[0][12] = (int64_t)(uintptr_t)dthr.data();
    g_packet[0][13] = (int64_t)(uintptr_t)out.data();
    g_packet[1][0] = (int64_t)(uintptr_t)cnt.data();
    g_packet[1][1] = (int64_t)(uintptr_t)s2s.data();
    if (!fg344dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    recs.clear();
    for (int64_t i = 0; i < cnt[0]; ++i) recs.push_back({out[i*3], out[i*3+1], out[i*3+2]});
    return g_packet[2][1];
}

static int64_t call_clear(int64_t op, int64_t seq, int64_t strm, std::vector<int64_t> & idxs, int64_t nl,
                          std::vector<int64_t> & kr, std::vector<int64_t> & vp, std::vector<int64_t> & vr,
                          int64_t vtrans, int64_t vel, int64_t cells_size, int64_t head,
                          std::vector<int64_t> & shes, std::vector<int64_t> & scnt, std::vector<Rec> & recs) {
    for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
    std::fill(out.begin(), out.end(), -99); cnt[0] = 0;
    g_packet[0][0] = op; g_packet[0][4] = seq; g_packet[0][13] = (int64_t)(uintptr_t)out.data();
    g_packet[1][0] = (int64_t)(uintptr_t)cnt.data();
    g_packet[1][2] = (int64_t)(uintptr_t)idxs.data();
    g_packet[1][3] = (int64_t)idxs.size();
    g_packet[1][4] = nl;
    g_packet[1][5] = (int64_t)(uintptr_t)kr.data();
    g_packet[1][6] = (int64_t)(uintptr_t)vp.data();
    g_packet[1][7] = (int64_t)(uintptr_t)vr.data();
    g_packet[1][8] = vtrans; g_packet[1][9] = vel; g_packet[1][10] = cells_size; g_packet[1][11] = head;
    g_packet[1][12] = (int64_t)(uintptr_t)shes.data();
    g_packet[1][13] = (int64_t)(uintptr_t)scnt.data();
    if (!fg344dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    recs.clear();
    for (int64_t i = 0; i < cnt[0]; ++i) recs.push_back({out[i*3], out[i*3+1], out[i*3+2]});
    return g_packet[2][1];
}

int main(void) {
    std::mt19937 rng(344011);

    for (int trial = 0; trial < 4000; ++trial) {
        const int64_t n_stream = 1 + (int64_t)(rng() % 3);
        const int64_t other = (rng() % 6 == 0);
        const int64_t partial = rng() & 1;
        const int64_t swa_none = rng() & 1;
        const int64_t seq_id = (rng() % 4 == 0) ? -1 : (int64_t)(rng() % 3);
        const int64_t mirror = (rng() % 3 == 0);
        const int64_t mirror_size = (rng() % 3) ? n_stream : n_stream + 1;
        const int64_t n_stream_cur = (rng() % 5) ? n_stream : n_stream + 1;

        std::vector<int64_t> cc((size_t)n_stream), mempty((size_t)n_stream, 1), mok((size_t)n_stream, 1), dthr((size_t)n_stream, 0);
        for (int64_t s = 0; s < n_stream; ++s) {
            cc[(size_t)s] = (rng() % 3 == 0) ? 0 : 1 + (int64_t)(rng() % 4);
            mempty[(size_t)s] = (rng() % 3) ? 1 : 0;
            mok[(size_t)s] = (rng() % 5) ? 1 : 0;
            dthr[(size_t)s] = (rng() % 6 == 0) ? 1 : 0;
        }
        std::vector<int64_t> s2s = {0, 1, 2};

        // reference
        std::vector<Rec> want; int64_t wret = 0;
        if (other || (partial && swa_none)) {
            /* nothing */
        } else if (mirror && mirror_size != n_stream) { wret = -1; }
        else if (n_stream_cur != n_stream) { wret = -2; }
        else {
            if (seq_id == -1) want.push_back({0, 0, 0});
            bool bad = false;
            for (int64_t s = 0; s < n_stream && !bad; ++s) {
                if (cc[(size_t)s] == 0) {
                    if (mirror && !mempty[(size_t)s]) { wret = -3; bad = true; break; }
                    want.push_back({8, s, 0});
                    continue;
                }
                const int64_t strm = seq_id == -1 ? s : s2s[(size_t)seq_id];
                want.push_back({1, s, 0});
                bool ok = mok[(size_t)s] != 0;
                if (ok) want.push_back({2, strm, 0});
                want.push_back({3, strm, 0});
                if (dthr[(size_t)s]) ok = false;
                if (!ok) { want.push_back({4, strm, 0}); wret = -4; bad = true; }
            }
        }

        std::vector<Rec> got;
        check(call_read(other, partial, swa_none, seq_id, n_stream, mirror, mirror_size, n_stream_cur,
                        cc, mempty, mok, dthr, s2s, got) == wret, "read code", trial);
        check(got.size() == want.size(), "read count", trial);
        for (size_t i = 0; i < want.size() && i < got.size(); ++i)
            check(got[i].k == want[i].k && got[i].a == want[i].a && got[i].b == want[i].b, "read record", trial);
    }

    for (int trial = 0; trial < 3000; ++trial) {
        const int64_t nl = (int64_t)(rng() % 3);
        const int64_t n_idx = (int64_t)(rng() % 5);
        const int64_t seq_id = (rng() % 6 == 0) ? -1 : (int64_t)(rng() % 3);
        const int64_t head = (int64_t)(rng() % 6);
        const int64_t cells_size = 4 + (int64_t)(rng() % 5);
        const int64_t vtrans = rng() & 1;
        const int64_t vel = 1 + (int64_t)(rng() % 4);
        std::vector<int64_t> idxs((size_t)n_idx), kr((size_t)nl), vp((size_t)nl), vr((size_t)nl);
        int64_t cur = (int64_t)(rng() % 3);
        for (int64_t i = 0; i < n_idx; ++i) {
            idxs[(size_t)i] = cur;
            cur += (rng() % 4 == 0) ? 1 + (int64_t)(rng() % 2) : 1;   // mostly contiguous, sometimes not
        }
        for (int64_t il = 0; il < nl; ++il) {
            kr[(size_t)il] = 1 + (int64_t)(rng() % 32);
            vr[(size_t)il] = 1 + (int64_t)(rng() % 16);
            vp[(size_t)il] = (rng() % 4) ? 1 : 0;
        }
        bool contig = true;
        for (int64_t i = 1; i < n_idx; ++i) if (idxs[(size_t)i] != idxs[(size_t)(i-1)] + 1) contig = false;

        std::vector<Rec> want;
        if (seq_id == -1) { want.push_back({0, 0, 0}); }
        else {
            want.push_back({5, seq_id, 0});
            if (n_idx > 0) {
                for (int64_t il = 0; il < nl; ++il) {
                    if (contig) want.push_back({6, head*kr[(size_t)il], n_idx*kr[(size_t)il]});
                    else for (int64_t i = 0; i < n_idx; ++i) want.push_back({6, idxs[(size_t)i]*kr[(size_t)il], kr[(size_t)il]});
                }
                for (int64_t il = 0; il < nl; ++il) {
                    if (!vp[(size_t)il]) continue;
                    if (!vtrans) {
                        if (contig) want.push_back({6, head*vr[(size_t)il], n_idx*vr[(size_t)il]});
                        else for (int64_t i = 0; i < n_idx; ++i) want.push_back({6, idxs[(size_t)i]*vr[(size_t)il], vr[(size_t)il]});
                    } else {
                        if (contig) {
                            for (int64_t j = 0; j < vr[(size_t)il]; ++j)
                                want.push_back({6, (head + j*cells_size)*vel, n_idx*vel});
                        } else {
                            for (int64_t j = 0; j < vr[(size_t)il]; ++j)
                                for (int64_t i = 0; i < n_idx; ++i)
                                    want.push_back({6, (idxs[(size_t)i] + j*cells_size)*vel, vel});
                        }
                    }
                }
            }
        }

        std::vector<int64_t> e;
        std::vector<Rec> got;
        check(call_clear(1, seq_id, 0, idxs, nl, kr, vp, vr, vtrans, vel, cells_size, head, e, e, got) == 0,
              "clear code", trial);
        check(got.size() == want.size(), "clear count", trial);
        for (size_t i = 0; i < want.size() && i < got.size(); ++i)
            check(got[i].k == want[i].k && got[i].a == want[i].a && got[i].b == want[i].b, "clear record", trial);
    }

    for (int trial = 0; trial < 2000; ++trial) {
        const int64_t cells_size = (int64_t)(rng() % 8);
        const int64_t seq_id = (rng() % 5 == 0) ? -1 : (int64_t)(rng() % 3);
        std::vector<int64_t> has((size_t)cells_size), cnt2((size_t)cells_size);
        for (int64_t c = 0; c < cells_size; ++c) {
            has[(size_t)c] = rng() & 1;
            cnt2[(size_t)c] = 1 + (int64_t)(rng() % 2);
        }
        std::vector<Rec> want;
        int64_t wret = 0;
        if (seq_id == -1) want.push_back({0, 0, 0});
        else {
            for (int64_t c = 0; c < cells_size; ++c)
                if (has[(size_t)c] && cnt2[(size_t)c] == 1) { want.push_back({5, c, 0}); ++wret; }
        }
        std::vector<int64_t> e;
        std::vector<Rec> got;
        check(call_clear(2, seq_id, 0, e, 0, e, e, e, 0, 1, cells_size, 0, has, cnt2, got) == wret, "collect ret", trial);
        check(got.size() == want.size(), "collect count", trial);
        for (size_t i = 0; i < want.size() && i < got.size(); ++i)
            check(got[i].k == want[i].k && got[i].a == want[i].a, "collect record", trial);
    }

    std::printf("FG344K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
