// Differential gate for the Goose hybrid indexed operations.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg341dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg341k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg341k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg341k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg341k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg341k_fail(int64_t c) { std::fprintf(stderr, "FG341K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 12), cnt(1);
struct Rec { int64_t k, a, b; };

static int64_t call(int64_t op, int64_t seq, int64_t p0, int64_t p1, int64_t shift, int64_t d, int64_t recr_ok,
                    int64_t has_idx, int64_t seq_min, std::vector<int64_t> & stale, int64_t sinfos_empty,
                    std::vector<Rec> & recs) {
    for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
    std::fill(out.begin(), out.end(), -99); cnt[0] = 0;
    g_packet[0][0] = op; g_packet[0][1] = (int64_t)(uintptr_t)out.data();
    g_packet[0][2] = seq; g_packet[0][3] = p0; g_packet[0][4] = p1; g_packet[0][5] = shift; g_packet[0][6] = d;
    g_packet[0][7] = recr_ok; g_packet[0][8] = has_idx; g_packet[0][9] = seq_min;
    g_packet[0][10] = (int64_t)(uintptr_t)stale.data();
    g_packet[0][11] = sinfos_empty; g_packet[0][12] = 4;
    g_packet[1][0] = (int64_t)(uintptr_t)cnt.data();
    if (!fg341dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    recs.clear();
    for (int64_t i = 0; i < cnt[0]; ++i) recs.push_back({out[i*3], out[i*3+1], out[i*3+2]});
    return g_packet[2][1];
}

static int64_t want_stale(int64_t cur, int64_t p0) {
    const int64_t v = std::max<int64_t>(p0, 0);
    return std::min(cur, v);
}

int main(void) {
    std::mt19937 rng(341011);

    for (int trial = 0; trial < 5000; ++trial) {
        const int64_t op = (int64_t)(rng() % 8);
        int64_t seq = (int64_t)(rng() % 4);
        if (rng() % 6 == 0) seq = -1;
        int64_t p0 = (int64_t)(rng() % 20) - 5;
        int64_t p1 = (rng() % 4 == 0) ? -1 : (int64_t)(rng() % 20);
        const int64_t shift = (int64_t)(rng() % 11) - 5;
        const int64_t d = 1 + (int64_t)(rng() % 4);
        const int64_t recr_ok = (rng() % 4) ? 1 : 0;
        const int64_t has_idx = rng() & 1;
        const int64_t seq_min = (int64_t)(rng() % 12);
        const int64_t sinfos_empty = (rng() % 4) ? 1 : 0;
        // force the boundary case: an edit at or below the sequence's first position must zero the mark
        if (rng() % 4 == 0) { p0 = seq_min - (int64_t)(rng() % 2); }
        if (op == 1) { p0 = 0; p1 = -1; }   // whole-sequence copy precondition

        // staleness model: one slot per sequence; a negative id updates every slot.
        // The stored value keeps the minimum, so starting values are drawn below the edits too.
        std::vector<int64_t> model(4);
        // a third of the slots start high so an edit actually lowers them, the rest start low so a
        // smaller-than-current edit must not raise them either
        for (auto & x : model) x = (rng() % 2 == 0) ? 50 : (int64_t)(rng() % 9);
        // the port must see the state from before this operation; the model is mutated below
        const std::vector<int64_t> model_before = model;
        std::vector<Rec> want;
        int64_t wret = 0;
        auto mark = [&](int64_t id, int64_t at) {
            const int64_t v = std::max<int64_t>(at, 0);
            if (id < 0) {
                for (auto & x : model) x = std::min(x, v);
                want.push_back({1, -1, v});
            } else {
                model[(size_t)id] = std::min(model[(size_t)id], v);
                want.push_back({1, id, model[(size_t)id]});
            }
        };
        switch (op) {
            case 0:
                want.push_back({0, 0, 0});
                if (recr_ok) {
                    if (has_idx) {
                        const int64_t st = (seq < 0 || p0 <= seq_min) ? 0 : p0;
                        want.push_back({0, 1, 0});
                        mark(seq, st);
                    }
                    want.push_back({0, 2, 0});
                    wret = 1;
                }
                break;
            case 1:
                want.push_back({0, 1, 0}); want.push_back({0, 0, 0});
                if (has_idx) { want.push_back({0, 1, 0}); mark(seq, 0); }
                break;
            case 2:
                want.push_back({0, 2, 0}); want.push_back({0, 0, 0});
                if (has_idx) { want.push_back({0, 1, 0}); mark(-1, 0); }
                break;
            case 3:
                want.push_back({0, 2, 0}); want.push_back({0, 0, 0});
                if (has_idx) {
                    const int64_t src = shift < 0 ? p0 + shift : p0;
                    const int64_t st = (seq < 0 || src <= seq_min) ? 0 : src;
                    want.push_back({0, 1, 0});
                    mark(seq, st);
                }
                break;
            case 4:
                want.push_back({0, 2, 0}); want.push_back({0, 0, 0});
                if (has_idx) { want.push_back({0, 1, 0}); mark(seq, 0); }
                break;
            case 5:
                want.push_back({0, 2, 0}); want.push_back({0, 0, 0});
                if (has_idx) want.push_back({0, 1, 0});
                break;
            case 6:
                want.push_back({0, 2, 0});
                if (!recr_ok) {
                    want.push_back({3, 2, 0}); want.push_back({3, 0, 0});
                    if (has_idx) { want.push_back({3, 1, 0}); mark(seq, 0); }
                    wret = -1;
                } else {
                    want.push_back({0, 0, 0});
                    if (has_idx) {
                        want.push_back({3, 1, 0});
                        if (!sinfos_empty) { want.push_back({2, 1, 0}); mark(seq, 0); }
                    }
                }
                break;
            default:
                if (seq < 0) { want.push_back({4, 0, 0}); }
                else {
                    want.push_back({3, 2, 0}); want.push_back({3, 0, 0});
                    if (has_idx) { want.push_back({3, 1, 0}); mark(seq, 0); }
                }
                break;
        }

        std::vector<int64_t> pre = model_before;
        std::vector<Rec> got;
        const int64_t ret = call(op, seq, p0, p1, shift, d, recr_ok, has_idx, seq_min, pre, sinfos_empty, got);

        check(ret == wret, "return value", trial);
        check(got.size() == want.size(), "record count", trial);
        for (size_t i = 0; i < want.size() && i < got.size(); ++i)
            check(got[i].k == want[i].k && got[i].a == want[i].a && got[i].b == want[i].b, "record", trial);
        for (size_t q = 0; q < pre.size(); ++q)
            check(pre[q] == model[q], "stale state", trial);
    }

    std::printf("FG341K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
