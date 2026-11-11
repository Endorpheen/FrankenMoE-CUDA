// Differential gate for the Goose recurrent state clearing and context steps.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg339dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg339k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg339k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg339k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg339k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg339k_fail(int64_t c) { std::fprintf(stderr, "FG339K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 14), cnt(1);
struct Rec { int64_t k, a, b; };

static int64_t call(int64_t op, int64_t seq_id, int64_t cc, int64_t nl, std::vector<int64_t> & rp,
                    std::vector<int64_t> & rr, std::vector<int64_t> & sp, std::vector<int64_t> & sr,
                    std::vector<int64_t> & pp, std::vector<int64_t> & pr, int64_t head, int64_t n_ub,
                    int64_t i_next, int64_t status, std::vector<Rec> & recs) {
    for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
    std::fill(out.begin(), out.end(), -99); cnt[0] = 0;
    g_packet[0][0] = op; g_packet[0][1] = seq_id; g_packet[0][2] = cc; g_packet[0][3] = nl;
    g_packet[0][4] = (int64_t)(uintptr_t)rp.data(); g_packet[0][5] = (int64_t)(uintptr_t)rr.data();
    g_packet[0][6] = (int64_t)(uintptr_t)sp.data(); g_packet[0][7] = (int64_t)(uintptr_t)sr.data();
    g_packet[0][8] = (int64_t)(uintptr_t)pp.data(); g_packet[0][9] = (int64_t)(uintptr_t)pr.data();
    g_packet[0][10] = head; g_packet[0][11] = n_ub; g_packet[0][12] = i_next; g_packet[0][13] = status;
    g_packet[1][0] = (int64_t)(uintptr_t)out.data();
    g_packet[1][1] = (int64_t)(uintptr_t)cnt.data();
    if (!fg339dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    recs.clear();
    for (int64_t i = 0; i < cnt[0]; ++i) recs.push_back({out[i*3], out[i*3+1], out[i*3+2]});
    return g_packet[2][1];
}

int main(void) {
    std::mt19937 rng(339011);

    for (int trial = 0; trial < 5000; ++trial) {
        const int64_t nl = (int64_t)(rng() % 4);
        const int64_t head = (int64_t)(rng() % 8);
        const int64_t seq_id = (rng() % 5 == 0) ? -1 : (int64_t)(rng() % 4);
        const int64_t cc = (rng() % 4 == 0) ? 0 : (int64_t)(rng() % 7);
        std::vector<int64_t> rp(nl), rr(nl), sp(nl), sr(nl), pp(nl), pr(nl);
        for (int64_t il = 0; il < nl; ++il) {
            rp[(size_t)il] = rng() & 1; sp[(size_t)il] = rng() & 1; pp[(size_t)il] = rng() & 1;
            rr[(size_t)il] = 1 + (int64_t)(rng() % 64);
            sr[(size_t)il] = 1 + (int64_t)(rng() % 64);
            pr[(size_t)il] = 1 + (int64_t)(rng() % 64);
        }

        std::vector<Rec> w;
        if (seq_id == -1) {
            w.push_back({0, 0, 0});
        } else {
            w.push_back({1, seq_id, 0});
            if (cc != 0) {
                // clear order is recurrent, state, PLE -- not the read order
                for (int64_t il = 0; il < nl; ++il) {
                    if (rp[(size_t)il]) w.push_back({2, head*rr[(size_t)il], cc*rr[(size_t)il]});
                    if (sp[(size_t)il]) w.push_back({2, head*sr[(size_t)il], cc*sr[(size_t)il]});
                    if (pp[(size_t)il]) w.push_back({2, head*pr[(size_t)il], cc*pr[(size_t)il]});
                }
            }
        }
        std::vector<Rec> got;
        check(call(0, seq_id, cc, nl, rp, rr, sp, sr, pp, pr, head, 0, 0, 0, got) == 0, "clear status", trial);
        check(got.size() == w.size(), "clear count", trial);
        for (size_t i = 0; i < w.size() && i < got.size(); ++i)
            check(got[i].k == w[i].k && got[i].a == w[i].a && got[i].b == w[i].b, "clear record", trial);
    }

    for (int trial = 0; trial < 2000; ++trial) {
        const int64_t n_ub = 1 + (int64_t)(rng() % 5);
        const int64_t i_next = (int64_t)(rng() % n_ub);
        std::vector<int64_t> e; std::vector<Rec> got;
        const int64_t ni = i_next + 1;
        check(call(1, 0, 0, 0, e, e, e, e, e, e, 0, n_ub, i_next, 0, got) == (ni >= n_ub ? 0 : 1),
              "next status", trial);
        check(got.size() == (ni >= n_ub ? 2 : 1), "next records", trial);
        check(got[0].k == 3 && got[0].a == ni, "cursor", trial);
    }

    for (int trial = 0; trial < 1000; ++trial) {
        std::vector<int64_t> e; std::vector<Rec> got;
        check(call(2, 0, 0, 0, e, e, e, e, e, e, 0, 0, 0, 1, got) == 1, "update apply", trial);
        check(got.size() == 1 && got[0].k == 5, "no placement", trial);

        const int64_t n_ub = 1 + (int64_t)(rng() % 4);
        const int64_t i_next = (int64_t)(rng() % n_ub);
        std::vector<Rec> got2;
        check(call(2, 0, 0, 0, e, e, e, e, e, e, 0, n_ub, i_next, 0, got2) == 1, "batch apply", trial);
        check(got2.size() == 1 && got2[0].k == 6 && got2[0].a == i_next, "placement index", trial);
    }

    for (int trial = 0; trial < 500; ++trial) {
        const int64_t status = (int64_t)(rng() % 3);
        std::vector<int64_t> e; std::vector<Rec> got;
        check(call(3, 0, 0, 0, e, e, e, e, e, e, 0, 0, 0, status, got) == status, "status passthrough", trial);
    }

    std::printf("FG339K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
