// Differential gate for the Goose recurrent state restore.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg338dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg338k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg338k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg338k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg338k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg338k_fail(int64_t c) { std::fprintf(stderr, "FG338K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 14), cnt(1);
struct Rec { int64_t k, a, b; };

struct Case {
    int64_t op = 0, cc = 0, size = 0, dest = -1, nsm = 4, find_ok = 1, head = 0;
    std::vector<int64_t> pos, nseq, ids;
    std::vector<int64_t> tail;
    int64_t nl_ref = 0, nl = 0, s_trans = 0;
    std::vector<int64_t> rp, rr, pp, pr, sp, sr;
    int64_t rt = 1, rr_ok = 1, pr_ok = 1, st = 1, sr_ok = 1;
};

static int64_t run_case(Case & c, std::vector<Rec> & recs) {
    for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
    std::fill(out.begin(), out.end(), -99); cnt[0] = 0;
    g_packet[0][0] = c.op; g_packet[0][1] = c.cc; g_packet[0][2] = c.size; g_packet[0][3] = c.dest;
    g_packet[0][4] = c.nsm;
    g_packet[0][5] = (int64_t)(uintptr_t)c.pos.data();
    g_packet[0][6] = (int64_t)(uintptr_t)c.nseq.data();
    g_packet[0][7] = (int64_t)(uintptr_t)c.ids.data();
    g_packet[0][8] = c.find_ok; g_packet[0][9] = (int64_t)(uintptr_t)&c.head;
    g_packet[0][10] = (int64_t)(uintptr_t)c.tail.data();
    g_packet[0][11] = c.nl_ref; g_packet[0][12] = c.nl; g_packet[0][13] = c.s_trans;
    g_packet[0][14] = (int64_t)(uintptr_t)c.rp.data();
    g_packet[0][15] = (int64_t)(uintptr_t)c.rr.data();
    g_packet[1][0] = (int64_t)(uintptr_t)out.data();
    g_packet[1][1] = (int64_t)(uintptr_t)cnt.data();
    g_packet[1][2] = (int64_t)(uintptr_t)c.pp.data();
    g_packet[1][3] = (int64_t)(uintptr_t)c.pr.data();
    g_packet[1][4] = (int64_t)(uintptr_t)c.sp.data();
    g_packet[1][5] = (int64_t)(uintptr_t)c.sr.data();
    g_packet[1][6] = c.rt; g_packet[1][7] = c.rr_ok; g_packet[1][8] = c.pr_ok;
    g_packet[1][9] = c.st; g_packet[1][10] = c.sr_ok;
    if (!fg338dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    recs.clear();
    for (int64_t i = 0; i < cnt[0]; ++i) recs.push_back({out[i*3], out[i*3+1], out[i*3+2]});
    return g_packet[2][1];
}

int main(void) {
    std::mt19937 rng(338011);

    // metadata
    for (int trial = 0; trial < 4000; ++trial) {
        Case c;
        c.op = rng() & 1;
        c.size = (int64_t)(rng() % 12);
        c.cc = (int64_t)(rng() % (c.size + 2));
        c.nsm = 1 + (int64_t)(rng() % 5);
        c.find_ok = (rng() % 5) ? 1 : 0;
        c.head = (int64_t)(rng() % 6);
        c.pos.assign((size_t)c.cc, 0);
        c.nseq.assign((size_t)c.cc, 0);
        c.ids.assign((size_t)c.cc * 8, 0);
        c.tail.assign((size_t)c.nsm + 1, -1);
        for (int64_t i = 0; i < c.cc; ++i) {
            c.pos[(size_t)i] = (int64_t)(rng() % 40);
            // single-sequence records must carry zero ids; the whole-cache path draws 0..3
            c.nseq[(size_t)i] = c.op == 0 ? ((rng() % 6 == 0) ? 1 : 0) : (int64_t)(rng() % 4);
            for (int64_t j = 0; j < 8; ++j) c.ids[(size_t)(i*8+j)] = (int64_t)(rng() % (c.nsm + 2)) - 1;
        }
        for (int64_t q = 0; q < (int64_t)c.tail.size(); ++q) {
            if (rng() % 4 == 0) c.tail[(size_t)q] = (int64_t)q;   // pre-set tail: a duplicate
        }

        // reference
        std::vector<Rec> w; int64_t ret = 0;
        std::vector<int64_t> tail = c.tail;
        if (c.cc > c.size) { ret = -1; }
        else if (c.op == 0) {
            w.push_back({0, 0, 0});
            if (c.cc == 0) { /* done */ }
            else {
                bool bad = false;
                for (int64_t i = 0; i < c.cc; ++i) {
                    if (c.nseq[(size_t)i] != 0) { ret = -2; bad = true; break; }
                    w.push_back({1, i, c.nseq[(size_t)i]});
                }
                if (!bad) {
                    if (!c.find_ok) ret = -3;
                    else for (int64_t k = 0; k < c.cc; ++k) w.push_back({7, c.head + k, 0});
                }
            }
        } else {
            w.push_back({2, 0, 0});
            bool bad = false;
            for (int64_t i = 0; i < c.cc && !bad; ++i) {
                w.push_back({1, i, c.nseq[(size_t)i]});
                for (int64_t j = 0; j < c.nseq[(size_t)i]; ++j) {
                    const int64_t id = c.ids[(size_t)(i*8+j)];
                    if (id < 0 || id >= c.nsm) { ret = -4; bad = true; break; }
                    w.push_back({3, i, id});
                    if (tail[(size_t)id] != -1) { ret = -5; bad = true; break; }
                    tail[(size_t)id] = i;
                    w.push_back({4, i, i});
                }
            }
            if (!bad) {
                w.push_back({5, 0, 0});
                w.push_back({6, c.cc, 0});
                for (int64_t k = 0; k < c.cc; ++k) w.push_back({7, k, 0});
            }
        }

        std::vector<Rec> got;
        check(run_case(c, got) == ret, "meta code", trial);
        check(got.size() == w.size(), "meta count", trial);
        for (size_t i = 0; i < w.size() && i < got.size(); ++i)
            check(got[i].k == w[i].k && got[i].a == w[i].a && got[i].b == w[i].b, "meta record", trial);
        for (size_t q = 0; q < c.tail.size(); ++q) check(c.tail[q] == tail[q], "tail state", trial);
    }

    // data
    for (int trial = 0; trial < 3000; ++trial) {
        Case c;
        c.op = 2;
        c.size = (int64_t)(rng() % 12);
        c.cc = (int64_t)(rng() % (c.size + 2));
        c.head = (int64_t)(rng() % 6);
        c.nl = (int64_t)(rng() % 4);
        c.nl_ref = (rng() % 5) ? c.nl : c.nl + 1;
        c.s_trans = (rng() % 6) ? 0 : 1;
        c.rp.assign((size_t)c.nl, 0); c.rr.assign((size_t)c.nl, 0);
        c.pp.assign((size_t)c.nl, 0); c.pr.assign((size_t)c.nl, 0);
        c.sp.assign((size_t)c.nl, 0); c.sr.assign((size_t)c.nl, 0);
        for (int64_t il = 0; il < c.nl; ++il) {
            c.rp[(size_t)il] = rng() & 1;
            c.pp[(size_t)il] = rng() & 1;
            c.sp[(size_t)il] = rng() & 1;
            c.rr[(size_t)il] = 1 + (int64_t)(rng() % 64);
            c.pr[(size_t)il] = 1 + (int64_t)(rng() % 64);
            c.sr[(size_t)il] = 1 + (int64_t)(rng() % 64);
        }
        c.rt = (rng() % 6) ? 1 : 0; c.rr_ok = (rng() % 6) ? 1 : 0; c.pr_ok = (rng() % 6) ? 1 : 0;
        c.st = (rng() % 6) ? 1 : 0; c.sr_ok = (rng() % 6) ? 1 : 0;

        std::vector<Rec> w; int64_t ret = 0;
        bool bad = false;
        if (c.nl_ref != c.nl) { ret = -1; bad = true; }
        else if (c.cc > c.size) { ret = -2; bad = true; }
        else if (c.s_trans) { ret = -3; bad = true; }
        if (!bad) {
            for (int64_t il = 0; il < c.nl && !bad; ++il) {
                if (!c.rp[(size_t)il]) continue;
                if (!c.rt) { ret = -4; bad = true; break; }
                if (!c.rr_ok) { ret = -5; bad = true; break; }
                if (c.cc > 0) w.push_back({8, c.head * c.rr[(size_t)il], c.cc * c.rr[(size_t)il]});
                if (c.pp[(size_t)il]) {
                    if (!c.pr_ok) { ret = -6; bad = true; break; }
                    if (c.cc > 0) w.push_back({8, c.head * c.pr[(size_t)il], c.cc * c.pr[(size_t)il]});
                }
            }
            for (int64_t il = 0; il < c.nl && !bad; ++il) {
                if (!c.sp[(size_t)il]) continue;
                if (!c.st) { ret = -7; bad = true; break; }
                if (!c.sr_ok) { ret = -8; bad = true; break; }
                if (c.cc > 0) w.push_back({8, c.head * c.sr[(size_t)il], c.cc * c.sr[(size_t)il]});
            }
        }

        std::vector<Rec> got;
        check(run_case(c, got) == ret, "data code", trial);
        check(got.size() == w.size(), "data count", trial);
        for (size_t i = 0; i < w.size() && i < got.size(); ++i)
            check(got[i].k == w[i].k && got[i].a == w[i].a && got[i].b == w[i].b, "data record", trial);
    }

    std::printf("FG338K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
