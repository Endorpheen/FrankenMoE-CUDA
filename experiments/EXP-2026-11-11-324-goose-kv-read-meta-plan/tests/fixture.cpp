// Differential gate for the Goose KV metadata restore plan.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg324dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg324k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg324k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg324k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg324k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg324k_fail(int64_t c) { std::fprintf(stderr, "FG324K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 14), cnt(1);

struct Rec { int64_t kind, a, b; };

static int64_t run_case(int64_t dest, int64_t cell_count, int64_t size, int64_t has_ext, int64_t n_seq_max,
                        int64_t sin_present, int64_t sin_empty, int64_t sin_streams, int64_t sin_cells,
                        int64_t find_ok, std::vector<int64_t> & idxs, std::vector<int64_t> & rec_pos,
                        std::vector<int64_t> & rec_nseq, std::vector<int64_t> & rec_ids,
                        std::vector<Rec> & recs) {
    for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
    std::fill(out.begin(), out.end(), -99);
    cnt[0] = 0;
    g_packet[0][0] = dest; g_packet[0][1] = cell_count; g_packet[0][2] = size;
    g_packet[0][3] = has_ext; g_packet[0][4] = n_seq_max;
    g_packet[0][5] = sin_present; g_packet[0][6] = sin_empty; g_packet[0][7] = sin_streams;
    g_packet[0][8] = sin_cells; g_packet[0][9] = find_ok;
    g_packet[0][10] = (int64_t)(uintptr_t)idxs.data();
    g_packet[0][12] = (int64_t)(uintptr_t)rec_pos.data();
    g_packet[0][13] = (int64_t)(uintptr_t)rec_nseq.data();
    g_packet[0][14] = (int64_t)(uintptr_t)rec_ids.data();
    g_packet[0][15] = cell_count;
    g_packet[1][0] = (int64_t)(uintptr_t)out.data();
    g_packet[2][0] = (int64_t)(uintptr_t)cnt.data();
    g_packet[3][0] = 0; g_packet[3][1] = 0;
    if (!fg324dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    recs.clear();
    for (int64_t i = 0; i < cnt[0]; ++i) recs.push_back({out[i*3], out[i*3+1], out[i*3+2]});
    return g_packet[3][1];
}

int main(void) {
    std::mt19937 rng(324011);

    for (int trial = 0; trial < 4000; ++trial) {
        const int64_t dest = (rng() % 3) ? (int64_t)(rng() % 4) : -1;
        const int64_t size = (int64_t)(rng() % 20);
        const int64_t cell_count = (int64_t)(rng() % (size + 3));
        const int64_t n_seq_max = 1 + (int64_t)(rng() % 5);
        const int64_t find_ok = rng() & 1;
        const int64_t sin_present = rng() & 1;
        const int64_t sin_empty = sin_present && (rng() % 4 == 0);
        const int64_t sin_streams = 1 + (int64_t)(rng() % 2);
        const int64_t sin_cells = (rng() % 2) ? cell_count : cell_count + (int64_t)(rng() % 3);

        std::vector<int64_t> idxs(cell_count + 2), rec_pos(cell_count + 2), rec_nseq(cell_count + 2);
        std::vector<int64_t> rec_ids((cell_count + 2)*8, 0);
        for (int64_t i = 0; i < cell_count; ++i) {
            idxs[i] = (int64_t)(rng() % (size + 2));
            rec_pos[i] = (int64_t)(rng() % 50);
            if (dest != -1) rec_nseq[i] = (rng() % 5 == 0) ? 2 : 1;
            else            rec_nseq[i] = (int64_t)(rng() % (n_seq_max + 2));
            for (int64_t j = 0; j < 8; ++j) rec_ids[i*8 + j] = (int64_t)(rng() % (n_seq_max + 2));
        }
        const bool has_ext = (rng() & 1) != 0;

        // reference
        int64_t want = 0;
        std::vector<Rec> wrec;
        if (cell_count > size) want = -1;
        else if (dest != -1) {
            wrec.push_back({0, 0, 0});
            for (int64_t i = 0; i < cell_count; ++i) {
                if (rec_nseq[i] != 1) { want = -2; break; }
                wrec.push_back({1, rec_pos[i], rec_nseq[i]});
            }
            if (want == 0) {
                if (sin_present) {
                    bool bad = sin_empty || sin_streams != 1 || sin_cells != cell_count;
                    if (bad) want = -3;
                    else {
                        for (int64_t q = 0; q < cell_count; ++q) if (idxs[q] >= size) { want = -4; break; }
                    }
                    if (want == 0) wrec.push_back({2, 0, 0});
                } else {
                    if (!find_ok) want = -5;
                    else wrec.push_back({3, 0, 0});
                }
                if (want == 0) {
                    wrec.push_back({4, 0, 0});
                    if (has_ext)
                        for (int64_t e = 0; e < cell_count; ++e) wrec.push_back({5, idxs[e], 0});
                }
            }
        } else {
            if (sin_present) {
                bool bad = sin_empty || sin_streams != 1 || sin_cells != cell_count;
                if (bad) want = -3;
            }
            if (want == 0) {
                for (int64_t i = 0; i < cell_count && want == 0; ++i) {
                    for (int64_t j = 0; j < rec_nseq[i]; ++j) {
                        const int64_t id = rec_ids[i*8 + j];
                        if (id < 0 || id >= n_seq_max) { want = -6; break; }
                    }
                    if (want == 0) {
                        wrec.push_back({6, i, rec_pos[i]});
                        wrec.push_back({1, rec_pos[i], rec_nseq[i]});
                    }
                }
                if (want == 0) wrec.push_back({7, 0, 0});
            }
        }

        std::vector<Rec> got;
        const int64_t ret = run_case(dest, cell_count, size, has_ext ? 1 : 0, n_seq_max,
                                     sin_present, sin_empty ? 1 : 0, sin_streams, sin_cells,
                                     find_ok, idxs, rec_pos, rec_nseq, rec_ids, got);
        check(ret == want, "result code", trial);
        if (want == 0) {
            check(got.size() == wrec.size(), "record count", trial);
            for (size_t i = 0; i < wrec.size() && i < got.size(); ++i)
                check(got[i].kind == wrec[i].kind && got[i].a == wrec[i].a && got[i].b == wrec[i].b,
                      "record", trial);
        }
    }

    std::printf("FG324K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
