// Differential gate for the Goose hybrid-indexed staleness rules.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <random>

extern "C" uint8_t fg309dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg309k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg309k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg309k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg309k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg309k_fail(int64_t c) { std::fprintf(stderr, "FG309K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 10);

static int64_t call(int op, int64_t seq_id, int64_t p0, int64_t p1, int64_t seq_min,
                    int64_t n_seq, std::vector<int64_t> & stale, int64_t shift, bool has_idx) {
    for (int i = 0; i < 16; ++i) g_packet[0][i] = 0;
    g_packet[0][0] = op;
    g_packet[0][1] = (int64_t)(uintptr_t)out.data();
    g_packet[0][2] = seq_id;
    g_packet[0][3] = p0;
    g_packet[0][4] = p1;
    g_packet[0][5] = seq_min;
    g_packet[0][6] = n_seq;
    g_packet[0][7] = (int64_t)(uintptr_t)stale.data();
    g_packet[0][8] = shift;
    g_packet[0][9] = has_idx ? 1 : 0;
    g_packet[3][0] = 0; g_packet[3][1] = 0;
    if (!fg309dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    return g_packet[3][1];
}

int main(void) {
    std::mt19937 rng(309011);

    // stale_set
    for (int trial = 0; trial < 2000; ++trial) {
        const int n_seq = 1 + (int)(rng() % 8);
        std::vector<int64_t> stale(n_seq);
        for (auto & v : stale) v = (int64_t)(rng() % 20);
        const int64_t seq_id = (int64_t)(rng() % (n_seq + 1)) - 1;
        const int64_t p0 = (int64_t)(rng() % 24) - 4;

        std::vector<int64_t> want = stale;
        const int64_t q0 = std::max<int64_t>(p0, 0);
        if (seq_id < 0) { for (auto & v : want) v = std::min(v, q0); }
        else want[seq_id] = std::min(want[seq_id], q0);

        std::vector<int64_t> work = stale;
        call(0, seq_id, p0, 0, 0, n_seq, work, 0, true);
        for (int i = 0; i < n_seq; ++i) check(work[i] == want[i], "stale_set", trial);
    }

    // stale_pos
    for (int trial = 0; trial < 1000; ++trial) {
        const int64_t seq_id = (int64_t)(rng() % 4) - 1;
        const int64_t p0 = (int64_t)(rng() % 40) - 5;
        const int64_t seq_min = (int64_t)(rng() % 40) - 5;
        int64_t want;
        if (seq_id < 0 || p0 <= seq_min) want = 0;
        else want = p0;
        std::vector<int64_t> stale(4, 0);
        check(call(1, seq_id, p0, 0, seq_min, 4, stale, 0, true) == want, "stale_pos", trial);
    }

    // seq_rm: recurrent refusal stops the chain
    for (int trial = 0; trial < 500; ++trial) {
        const int n_seq = 1 + (int)(rng() % 6);
        std::vector<int64_t> stale(n_seq, (int64_t)(rng() % 20));
        const int64_t seq_id = (int64_t)(rng() % n_seq);
        const int64_t p0 = (int64_t)(rng() % 30);
        const int64_t seq_min = (int64_t)(rng() % 30);
        const bool recr_ok = (rng() & 1) != 0;
        std::vector<int64_t> want = stale;
        if (recr_ok) {
            const int64_t st = p0 <= seq_min ? 0 : p0;
            want[seq_id] = std::min(want[seq_id], st);
        }
        std::vector<int64_t> work = stale;
        const int64_t got = call(2, seq_id, p0, 0, seq_min, n_seq, work, recr_ok ? 1 : 0, true);
        check(got == (recr_ok ? 1 : 0), "seq_rm result", trial);
        for (int i = 0; i < n_seq; ++i) check(work[i] == want[i], "seq_rm stale", trial);
    }

    // seq_add with a negative shift
    for (int trial = 0; trial < 500; ++trial) {
        const int n_seq = 1 + (int)(rng() % 6);
        std::vector<int64_t> stale(n_seq, (int64_t)(rng() % 20));
        const int64_t seq_id = (int64_t)(rng() % n_seq);
        const int64_t p0 = (int64_t)(rng() % 30);
        const int64_t seq_min = (int64_t)(rng() % 30);
        const int64_t shift = (int64_t)(rng() % 11) - 5;
        const int64_t base = shift < 0 ? p0 + shift : p0;
        const int64_t st = base <= seq_min ? 0 : base;
        std::vector<int64_t> want = stale;
        want[seq_id] = std::min(want[seq_id], st);
        std::vector<int64_t> work = stale;
        call(3, seq_id, p0, 0, seq_min, n_seq, work, shift, true);
        for (int i = 0; i < n_seq; ++i) check(work[i] == want[i], "seq_add stale", trial);
    }

    // seq_cp / seq_keep / seq_div marks
    for (int trial = 0; trial < 500; ++trial) {
        const int n_seq = 1 + (int)(rng() % 6);
        std::vector<int64_t> stale(n_seq, (int64_t)(rng() % 20));
        const int64_t seq_id = (int64_t)(rng() % n_seq);
        std::vector<int64_t> work = stale;
        call(4, 0, seq_id, 0, 0, n_seq, work, 0, true);
        std::vector<int64_t> want = stale; want[seq_id] = 0;
        for (int i = 0; i < n_seq; ++i) check(work[i] == want[i], "seq_cp marks destination", trial);

        work = stale;
        call(4, 1, seq_id, 0, 0, n_seq, work, 0, true);
        for (int i = 0; i < n_seq; ++i) check(work[i] == 0, "seq_keep marks everything", trial);

        work = stale;
        call(4, 2, seq_id, 0, 0, n_seq, work, 0, true);
        want = stale; want[seq_id] = 0;
        for (int i = 0; i < n_seq; ++i) check(work[i] == want[i], "seq_div marks the sequence", trial);
    }

    std::printf("FG309K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
