// Differential gate for the Goose recurrent state write and read.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <utility>

extern "C" uint8_t fg346dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg346k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg346k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg346k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg346k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg346k_fail(int64_t c) { std::fprintf(stderr, "FG346K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 13), cnt(1);
struct Rec { int64_t k, a, b; };

static int64_t call(int64_t op, int64_t seq, int64_t size, int64_t n_cells, std::vector<int64_t> & pos,
                    std::vector<int64_t> & mask, std::vector<int64_t> & src, int64_t n_rs,
                    std::vector<int64_t> & rs, int64_t on_dev, int64_t meta_ok, int64_t data_throw,
                    int64_t head, int64_t read_count, std::vector<Rec> & recs) {
    for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
    std::fill(out.begin(), out.end(), -99); cnt[0] = 0;
    g_packet[0][0] = op; g_packet[0][1] = seq; g_packet[0][2] = size; g_packet[0][3] = n_cells;
    g_packet[0][4] = (int64_t)(uintptr_t)pos.data();
    g_packet[0][5] = (int64_t)(uintptr_t)mask.data();
    g_packet[0][6] = (int64_t)(uintptr_t)src.data();
    g_packet[0][7] = n_rs; g_packet[0][8] = (int64_t)(uintptr_t)rs.data();
    g_packet[0][9] = on_dev; g_packet[0][10] = meta_ok; g_packet[0][11] = data_throw;
    g_packet[0][12] = head; g_packet[0][13] = read_count;
    g_packet[1][0] = (int64_t)(uintptr_t)out.data();
    g_packet[1][1] = (int64_t)(uintptr_t)cnt.data();
    if (!fg346dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    recs.clear();
    for (int64_t i = 0; i < cnt[0]; ++i) recs.push_back({out[i*3], out[i*3+1], out[i*3+2]});
    return g_packet[2][1];
}

int main(void) {
    std::mt19937 rng(346011);

    for (int trial = 0; trial < 5000; ++trial) {
        const int64_t size = 1 + (int64_t)(rng() % 6);
        const int64_t n_cells = (int64_t)(rng() % 10);
        const int64_t seq = (rng() % 4 == 0) ? -1 : (int64_t)(rng() % 3);
        const int64_t n_rs = (rng() % 3 == 0) ? 0 : 1 + (int64_t)(rng() % 3);
        const int64_t on_dev = (rng() % 5 == 0);

        std::vector<int64_t> pos((size_t)n_cells), mask((size_t)n_cells), src((size_t)n_cells), rs(4);
        for (int64_t q = 0; q < 4; ++q) rs[(size_t)q] = (int64_t)(rng() % 3);
        for (int64_t i = 0; i < n_cells; ++i) {
            pos[(size_t)i] = (rng() % 4 == 0) ? -1 : (int64_t)(rng() % 30);
            int64_t m = 0;
            for (int64_t q = 0; q < 3; ++q) if (rng() & 1) m |= (1LL << q);
            mask[(size_t)i] = m;
            src[(size_t)i] = (rng() % 3 == 0) ? -1 : (int64_t)(rng() % 8);
        }

        // reference
        std::vector<Rec> want; int64_t wret = 0;
        std::vector<std::pair<int64_t,int64_t>> lr, dr;
        int64_t count = 0, lbegin = -1, d_open = false, db = 0, de = 0;
        bool abort_rs = false;
        for (int64_t i = 0; i < n_cells; ++i) {
            const int64_t p = pos[(size_t)i], m = mask[(size_t)i];
            bool sel = seq == -1 ? (p != -1) : ((m & (1LL << seq)) != 0);
            if (!sel) {
                if (lbegin >= 0) { lr.push_back({lbegin, i}); lbegin = -1; }
                continue;
            }
            ++count;
            int64_t rsc = 0;
            if (n_rs != 0) {
                if (seq != -1) rsc = rs[(size_t)seq];
                else {
                    bool first = true;
                    for (int64_t q = 0; q < 64; ++q) {
                        if (!(m & (1LL << q))) continue;
                        const int64_t v = rs[(size_t)(q < 4 ? q : 0)];
                        if (first) { rsc = v; first = false; }
                        else if (rsc != v) { abort_rs = true; }
                    }
                    if (abort_rs) break;
                }
            }
            const int64_t row = src[(size_t)i] >= 0 ? src[(size_t)i] : i;
            const int64_t cid = rsc*size + row;
            if (!d_open) { db = cid; de = cid + 1; d_open = true; }
            else if (de == cid) { de = cid + 1; }
            else { dr.push_back({db, de}); db = cid; de = cid + 1; }
            if (lbegin < 0) lbegin = i;
        }
        if (abort_rs) {
            wret = -1;
        } else {
            if (lbegin >= 0) lr.push_back({lbegin, n_cells});
            if (d_open) dr.push_back({db, de});
            int64_t n_ranges = (int64_t)lr.size();
            if (on_dev && n_ranges > 1) { want.push_back({5, n_ranges, 0}); wret = -2; }
            else {
                for (auto & r : lr) want.push_back({1, r.first, r.second});
                for (auto & r : dr) want.push_back({2, r.first, r.second});
                want.push_back({0, count, 0});
                want.push_back({3, 0, 0});
                want.push_back({4, 0, 0});
            }
        }

        std::vector<Rec> got;
        check(call(0, seq, size, n_cells, pos, mask, src, n_rs, rs, on_dev, 0, 0, 0, 0, got) == wret,
              "write status", trial);
        check(got.size() == want.size(), "write count", trial);
        for (size_t i = 0; i < want.size() && i < got.size(); ++i)
            check(got[i].k == want[i].k && got[i].a == want[i].a && got[i].b == want[i].b, "write record", trial);
    }

    for (int trial = 0; trial < 2000; ++trial) {
        const int64_t seq = (rng() % 4 == 0) ? -1 : (int64_t)(rng() % 3);
        const int64_t meta_ok = (rng() % 5) ? 1 : 0;
        const int64_t data_throw = (rng() % 6 == 0) ? 1 : 0;
        const int64_t n_rs = (rng() % 2) ? 0 : 1 + (int64_t)(rng() % 3);
        const int64_t head = (int64_t)(rng() % 10);
        const int64_t read_count = (int64_t)(rng() % 8);
        std::vector<int64_t> e, rs((size_t)4, 1);
        std::vector<Rec> want;
        bool bad = !meta_ok || data_throw;
        if (meta_ok) want.push_back({3, 0, 0});
        want.push_back({4, 0, 0});
        if (bad) {
            want.push_back({6, head, meta_ok ? read_count : 0});
        } else if (n_rs != 0) {
            want.push_back({7, seq, 0});
        }
        std::vector<Rec> got;
        const int64_t ret = call(1, seq, 1, 0, e, e, e, n_rs, rs, 0, meta_ok, data_throw, head, read_count, got);
        check(ret == (bad ? -3 : 0), "read status", trial);
        check(got.size() == want.size(), "read count", trial);
        for (size_t i = 0; i < want.size() && i < got.size(); ++i)
            check(got[i].k == want[i].k && got[i].a == want[i].a && got[i].b == want[i].b, "read record", trial);
    }

    std::printf("FG346K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
