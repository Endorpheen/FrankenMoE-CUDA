// Differential gate for the Goose KV state ranges.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg321dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg321k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg321k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg321k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg321k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg321k_fail(int64_t c) { std::fprintf(stderr, "FG321K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static bool masked_swa(int64_t n_swa, int64_t t, int64_t p0, int64_t p1) {
    if (t == 0) return false;
    if (t == 1) return p1 - p0 >= n_swa;
    if (t == 2) return p0 < (p1 / n_swa) * n_swa;
    if (t == 3) { const int64_t h = n_swa / 2; const int64_t d = p1 - p0; return d < -h || d > h; }
    return false;
}

static std::vector<int64_t> outc(16), outr(1024), outt(1);

static int64_t call(int64_t n_stream, int64_t n_cells, int64_t seq_id, int64_t n_swa, int64_t swa_type,
                    int64_t partial, std::vector<int64_t> & cpos, std::vector<int64_t> & cseqw, int64_t W,
                    std::vector<int64_t> & spmax) {
    for (int i = 0; i < 16; ++i) g_packet[0][i] = 0;
    std::fill(outc.begin(), outc.end(), -99);
    std::fill(outr.begin(), outr.end(), -99);
    outt[0] = -99;
    g_packet[0][0] = n_stream; g_packet[0][1] = n_cells; g_packet[0][2] = seq_id;
    g_packet[0][3] = n_swa; g_packet[0][4] = swa_type; g_packet[0][5] = partial;
    g_packet[0][6] = (int64_t)(uintptr_t)cpos.data();
    g_packet[0][7] = (int64_t)(uintptr_t)cseqw.data();
    g_packet[0][8] = W;
    g_packet[0][9] = (int64_t)(uintptr_t)spmax.data();
    g_packet[1][0] = (int64_t)(uintptr_t)outc.data();
    g_packet[1][1] = (int64_t)(uintptr_t)outr.data();
    g_packet[1][2] = (int64_t)(uintptr_t)outt.data();
    g_packet[3][0] = 0; g_packet[3][1] = 0;
    if (!fg321dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    return g_packet[3][1];
}

int main(void) {
    std::mt19937 rng(321011);

    for (int trial = 0; trial < 3000; ++trial) {
        const int64_t n_stream = (int64_t)(rng() % 4);
        const int64_t n_cells = (int64_t)(rng() % 40);
        const int64_t n_seq_max = 4;
        const int64_t seq_id = (int64_t)(rng() % (n_seq_max + 1)) - 1;
        const int64_t n_swa = 1 + (int64_t)(rng() % 20);
        const int64_t swa_type = (int64_t)(rng() % 4);
        const int64_t partial = rng() % 4 == 0;
        const int64_t W = 1;

        std::vector<int64_t> cpos((size_t)(n_stream*n_cells), -1), cseqw((size_t)(n_stream*n_cells*W), 0);
        for (size_t c = 0; c < cpos.size(); ++c) {
            if (rng() % 4 == 0) continue;
            cpos[c] = (int64_t)(rng() % 60);
            for (int64_t s = 0; s < n_seq_max; ++s) if (rng() % 2) cseqw[c*W] |= (1ull << s);
            if (cseqw[c*W] == 0) cseqw[c*W] = 1ull << (rng() % n_seq_max);
        }
        std::vector<int64_t> spmax(n_stream);
        for (auto & v : spmax) v = (int64_t)(rng() % 60);

        if (partial && swa_type == 0) {
            check(call(n_stream, n_cells, seq_id, n_swa, swa_type, partial, cpos, cseqw, W, spmax) == -1,
                  "partial-only drops a non-SWA section", trial);
            continue;
        }

        int64_t total = 0;
        std::vector<int64_t> wcnt(n_stream, 0), wr;
        for (int64_t s = 0; s < n_stream; ++s) {
            const int64_t base = s*n_cells;
            int64_t cnt = 0, begin = n_cells;
            for (int64_t i = 0; i < n_cells; ++i) {
                bool add = cpos[base + i] != -1;
                if (add && seq_id != -1) add = ((cseqw[(base + i)*W] >> seq_id) & 1) != 0;
                if (add && seq_id != -1 && masked_swa(n_swa, swa_type, cpos[base + i], spmax[s])) add = false;
                if (add) {
                    ++cnt;
                    if (begin == n_cells) begin = i;
                } else if (begin != n_cells) {
                    wr.push_back(begin); wr.push_back(i);
                    begin = n_cells;
                }
            }
            if (begin != n_cells) { wr.push_back(begin); wr.push_back(n_cells); }
            wcnt[s] = cnt;
            total += cnt;
        }

        const int64_t got = call(n_stream, n_cells, seq_id, n_swa, swa_type, partial, cpos, cseqw, W, spmax);
        check(got == total, "total", trial);
        check(outt[0] == total, "total out", trial);
        for (int64_t s = 0; s < n_stream; ++s) check(outc[s] == wcnt[s], "stream count", trial);
        for (size_t i = 0; i < wr.size(); ++i) check(outr[i] == wr[i], "range boundary", trial);
        // counts must equal the sum of the range lengths across streams
        int64_t sum = 0;
        for (size_t i = 0; i + 1 < wr.size(); i += 2) sum += wr[i+1] - wr[i];
        check(sum == total, "ranges sum to the count", trial);
    }

    std::printf("FG321K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
