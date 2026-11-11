// Differential gate for the Goose KV state metadata.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg322dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg322k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg322k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg322k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg322k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg322k_fail(int64_t c) { std::fprintf(stderr, "FG322K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 16);

int main(void) {
    std::mt19937 rng(322011);

    for (int trial = 0; trial < 3000; ++trial) {
        const int64_t n_seq_max = 1 + (int64_t)(rng() % 5);
        const int64_t n_cells = 1 + (int64_t)(rng() % 30);
        const int64_t seq_id = (int64_t)(rng() % (n_seq_max + 1)) - 1;
        const int64_t has_ext = rng() & 1;
        const int64_t W = 1;

        std::vector<int64_t> cpos(n_cells), cseqw(n_cells*W, 0), cext(n_cells*3);
        for (int64_t i = 0; i < n_cells; ++i) {
            cpos[i] = (int64_t)(rng() % 100);
            for (int64_t s = 0; s < n_seq_max; ++s) if (rng() % 2) cseqw[i*W] |= (1ull << s);
            cext[i*3+0] = (int64_t)(rng() % 50);
            cext[i*3+1] = (int64_t)(rng() % 50);
            cext[i*3+2] = (int64_t)(rng() % 1000);
        }
        // a few contiguous ranges covering a random subset
        std::vector<int64_t> ranges;
        int64_t i = 0;
        while (i < n_cells) {
            const int64_t len = 1 + (int64_t)(rng() % 5);
            int64_t hi = std::min<int64_t>(n_cells, i + len);
            if (rng() % 3) { ranges.push_back(i); ranges.push_back(hi); }
            i = hi + (int64_t)(rng() % 3);
        }
        const int64_t n_ranges = (int64_t)ranges.size() / 2;

        // reference
        std::vector<int64_t> want;
        int64_t cells = 0;
        for (int64_t r = 0; r < n_ranges; ++r) {
            for (int64_t k = ranges[2*r]; k < ranges[2*r+1]; ++k) {
                std::vector<int64_t> ids;
                for (int64_t cur = 0; cur < n_seq_max; ++cur) {
                    const bool take = (cur == seq_id) || (seq_id == -1);
                    if (take && ((cseqw[k*W] >> cur) & 1)) ids.push_back(cur);
                }
                want.push_back(cpos[k]);
                want.push_back((int64_t)ids.size());
                if (has_ext) {
                    want.push_back(cext[k*3+0]);
                    want.push_back(cext[k*3+1]);
                    want.push_back(cext[k*3+2]);
                }
                for (int64_t s : ids) want.push_back(s);
                ++cells;
            }
        }

        for (int j = 0; j < 16; ++j) { g_packet[0][j] = 0; g_packet[1][j] = 0; }
        std::fill(out.begin(), out.end(), -99);
        g_packet[0][0] = n_seq_max; g_packet[0][1] = seq_id; g_packet[0][2] = has_ext;
        g_packet[0][3] = (int64_t)(uintptr_t)cpos.data();
        g_packet[0][4] = (int64_t)(uintptr_t)cseqw.data();
        g_packet[0][5] = W;
        g_packet[0][6] = (int64_t)(uintptr_t)cext.data();
        g_packet[0][7] = (int64_t)(uintptr_t)ranges.data();
        g_packet[0][8] = n_ranges;
        g_packet[1][0] = (int64_t)(uintptr_t)out.data();
        g_packet[3][0] = 0; g_packet[3][1] = 0;
        if (!fg322dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
        check(g_packet[3][1] == cells, "cell count", trial);
        check((int64_t)want.size() <= (int64_t)out.size(), "buffer fits", trial);
        static int dbgq = 0;
        for (size_t q = 0; q < want.size(); ++q) {
            if (out[q] != want[q] && dbgq < 4 && (++dbgq))
                std::fprintf(stderr, "DBG meta q=%zu goo=%lld want=%lld (seq_id=%lld has_ext=%lld)\n",
                    q, (long long)out[q], (long long)want[q], (long long)seq_id, (long long)has_ext);
            check(out[q] == want[q], "metadata stream", trial);
        }
    }

    std::printf("FG322K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
