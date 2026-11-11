// Differential gate for the Goose KV data restore plan.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg325dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg325k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg325k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg325k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg325k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg325k_fail(int64_t c) { std::fprintf(stderr, "FG325K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 16), cnt(1);

struct Rec { int64_t off, size, h; };

static int64_t run_case(int64_t vtr_ref, int64_t nl_ref, int64_t nrk_ref, int64_t nrv_ref,
                        int64_t cell_count, int64_t cells_size, int64_t vtr, int64_t nl,
                        int64_t nrk, int64_t nrv, std::vector<int64_t> & idxs,
                        int64_t k_row, int64_t v_row, int64_t v_el, std::vector<Rec> & recs) {
    for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
    std::fill(out.begin(), out.end(), -99);
    cnt[0] = 0;
    g_packet[0][0] = vtr_ref; g_packet[0][1] = nl_ref; g_packet[0][2] = nrk_ref; g_packet[0][3] = nrv_ref;
    g_packet[0][4] = cell_count; g_packet[0][5] = cells_size;
    g_packet[0][6] = vtr; g_packet[0][7] = nl; g_packet[0][8] = nrk; g_packet[0][9] = nrv;
    g_packet[0][10] = (int64_t)(uintptr_t)idxs.data();
    g_packet[0][11] = k_row; g_packet[0][12] = v_row;
    g_packet[0][13] = v_el;
    g_packet[1][0] = (int64_t)(uintptr_t)out.data();
    g_packet[2][0] = (int64_t)(uintptr_t)cnt.data();
    g_packet[3][0] = 0; g_packet[3][1] = 0;
    if (!fg325dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    recs.clear();
    for (int64_t i = 0; i < cnt[0]; ++i) recs.push_back({out[i*3], out[i*3+1], out[i*3+2]});
    return g_packet[3][1];
}

static void runs_of(const std::vector<int64_t> & idxs, int64_t n, std::vector<std::pair<int64_t,int64_t>> & runs) {
    int64_t i0 = 0;
    while (i0 < n) {
        int64_t i1 = i0 + 1;
        while (i1 < n && idxs[i1] == idxs[i1-1] + 1) ++i1;
        runs.push_back({idxs[i0], idxs[i1-1] + 1});
        i0 = i1;
    }
}

int main(void) {
    std::mt19937 rng(325011);

    for (int trial = 0; trial < 3000; ++trial) {
        const int64_t nl = (int64_t)(rng() % 4);
        const int64_t cells_size = 1 + (int64_t)(rng() % 40);
        const int64_t cell_count = (int64_t)(rng() % (cells_size + 2));
        const int64_t vtr = rng() & 1;
        const int64_t nrk = (int64_t)(rng() % 4) * 64;
        const int64_t nrv = (int64_t)(rng() % 4) * 64;
        int64_t nl_ref = nl, vtr_ref = vtr, nrk_ref = nrk, nrv_ref = nrv;
        switch (rng() % 6) {
            case 0: nl_ref = nl + 1; break;
            case 1: vtr_ref = 1 - vtr; break;
            case 2: nrk_ref = nrk + 64; break;
            case 3: nrv_ref = nrv + 64; break;
            default: break;
        }
        std::vector<int64_t> idxs(cell_count + 1);
        int64_t cur = 0;
        for (int64_t i = 0; i < cell_count; ++i) {
            cur += (rng() % 3 == 0) ? 1 + (int64_t)(rng() % 3) : 1;   // occasional gaps
            idxs[i] = cur;
            if (idxs[i] >= cells_size) idxs[i] = cells_size - 1;
        }
        const int64_t k_row = (int64_t)(rng() % 128);
        const int64_t v_row = 1 + (int64_t)(rng() % 32);
        const int64_t v_el = 1 + (int64_t)(rng() % 8);

        int64_t want;
        std::vector<Rec> wrec;
        if (nl_ref != nl) want = -1;
        else if (cell_count > cells_size) want = -2;
        else if (vtr_ref != vtr) want = -3;
        else if (nrk_ref != nrk) want = -4;
        else if (nrv_ref != nrv) want = -5;
        else {
            want = 0;
            std::vector<std::pair<int64_t,int64_t>> runs;
            runs_of(idxs, cell_count, runs);
            // the layer loop is outermost in native code
            for (int64_t il = 0; il < nl; ++il)
                for (auto & r : runs)
                    wrec.push_back({r.first*k_row, (r.second - r.first)*k_row, 0});
            if (!vtr) {
                for (int64_t il = 0; il < nl; ++il)
                    for (auto & r : runs)
                        wrec.push_back({r.first*v_row, (r.second - r.first)*v_row, 0});
            } else {
                for (int64_t il = 0; il < nl; ++il)
                    for (int64_t j = 0; j < v_row; ++j)
                        for (auto & r : runs)
                            wrec.push_back({(r.first + j*cells_size)*v_el, (r.second - r.first)*v_el, j});
            }
        }

        std::vector<Rec> got;
        const int64_t ret = run_case(vtr_ref, nl_ref, nrk_ref, nrv_ref, cell_count, cells_size, vtr, nl,
                                     nrk, nrv, idxs, k_row, v_row, v_el, got);
        check(ret == want, "result code", trial);
        if (want != 0) continue;
        check(got.size() == wrec.size(), "record count", trial);
        static int dbgr = 0;
        for (size_t i = 0; i < wrec.size() && i < got.size(); ++i) {
            const bool eq = got[i].off == wrec[i].off && got[i].size == wrec[i].size && got[i].h == wrec[i].h;
            if (!eq && dbgr < 4 && (++dbgr))
                std::fprintf(stderr, "DBG rec i=%zu got=[%lld %lld %lld] want=[%lld %lld %lld] vtr=%lld nl=%lld cc=%lld krow=%lld idxs:",
                    i, (long long)got[i].off, (long long)got[i].size, (long long)got[i].h,
                    (long long)wrec[i].off, (long long)wrec[i].size, (long long)wrec[i].h,
                    (long long)vtr, (long long)nl, (long long)cell_count, (long long)k_row);
                if (dbgr <= 1) { for (int64_t z = 0; z < cell_count; ++z) std::fprintf(stderr, " %lld", (long long)idxs[z]); std::fprintf(stderr, "\n"); }
            check(eq, "record", trial);
        }
    }

    std::printf("FG325K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
