// Differential gate for the Goose KV copy-in plan.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg319dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg319k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg319k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg319k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg319k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg319k_fail(int64_t c) { std::fprintf(stderr, "FG319K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(256), cnt(1);

struct Rec { int64_t kind, a, b; };

static std::vector<Rec> run_goose(int op, int64_t head, int64_t nhead, int64_t ntok, int64_t nb2,
                                  int64_t ts, int64_t n_stream, int64_t kv, int64_t v_ne0,
                                  int64_t v_trans, int64_t v_nelem, int64_t vcur_nelem, int64_t gqa_max) {
    for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; }
    std::fill(out.begin(), out.end(), -99);
    cnt[0] = 0;
    g_packet[0][0] = op;
    g_packet[0][1] = (int64_t)(uintptr_t)out.data();
    g_packet[0][2] = head; g_packet[0][3] = nhead; g_packet[0][4] = ntok; g_packet[0][5] = nb2;
    g_packet[0][6] = ts; g_packet[0][7] = n_stream; g_packet[0][8] = kv; g_packet[0][9] = v_ne0;
    g_packet[0][10] = v_trans; g_packet[0][11] = v_nelem; g_packet[0][12] = vcur_nelem;
    g_packet[0][13] = gqa_max;
    g_packet[1][0] = (int64_t)(uintptr_t)cnt.data();
    g_packet[3][0] = 0; g_packet[3][1] = 0;
    if (!fg319dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    std::vector<Rec> res;
    for (int64_t i = 0; i < cnt[0]; ++i) res.push_back({out[i*3], out[i*3+1], out[i*3+2]});
    return res;
}

int main(void) {
    std::mt19937 rng(319011);

    for (int trial = 0; trial < 2000; ++trial) {
        const int64_t head = 1 + (int64_t)(rng() % 64);
        const int64_t nhead = 1 + (int64_t)(rng() % 8);
        const int64_t ntok = (int64_t)(rng() % 8);
        const int64_t ts = 2 * (1 + (int64_t)(rng() % 2));
        const int64_t n_stream = 1 + (int64_t)(rng() % 3);
        const int64_t kv = 1 + (int64_t)(rng() % 256);
        const int64_t gqa = head*nhead;

        std::vector<Rec> want;
        want.push_back({0, gqa, ntok});
        if (n_stream > 1) want.push_back({1, gqa, kv*n_stream});
        want.push_back({4, 0, 0});
        const auto got = run_goose(0, head, nhead, ntok, 0, ts, n_stream, kv, 0, 0, 0, 0, 0);
        check(got.size() == want.size(), "k op count", trial);
        for (size_t i = 0; i < want.size() && i < got.size(); ++i) {
            check(got[i].kind == want[i].kind && got[i].a == want[i].a && got[i].b == want[i].b, "k op", trial);
        }
    }

    for (int trial = 0; trial < 4000; ++trial) {
        const int64_t head = 1 + (int64_t)(rng() % 64);
        const int64_t nhead = 1 + (int64_t)(rng() % 8);
        const int64_t ntok = (int64_t)(rng() % 8);
        const int64_t ts = 2 * (1 + (int64_t)(rng() % 2));
        const int64_t n_stream = 1 + (int64_t)(rng() % 3);
        const int64_t kv = 1 + (int64_t)(rng() % 256);
        const int64_t gqa = head*nhead;
        const int64_t v_ne0 = gqa + (int64_t)(rng() % 5);
        const int64_t v_trans = rng() & 1;
        const int64_t v_nelem = 1 + (int64_t)(rng() % 5000);
        const int64_t vcur_nelem = 1 + (int64_t)(rng() % 5000);
        const int64_t nb2 = (rng() & 1) ? ts*gqa : ts*gqa + ts;
        const int64_t gqa_max = gqa + (int64_t)(rng() % 4);

        std::vector<Rec> want;
        if (!v_trans) {
            want.push_back({0, gqa, ntok});
            if (n_stream > 1) want.push_back({1, gqa, kv*n_stream});
            want.push_back({4, 0, 0});
        } else {
            want.push_back({nb2 == ts*gqa ? 1 : 2, gqa, ntok});
            if (gqa < v_ne0) want.push_back({3, v_ne0 - gqa, 0});
            want.push_back({1, 1, v_nelem});
            want.push_back({1, 1, vcur_nelem});
            want.push_back({4, 0, 0});
        }
        const auto got = run_goose(1, head, nhead, ntok, nb2, ts, n_stream, kv, v_ne0, v_trans,
                                   v_nelem, vcur_nelem, gqa_max);
        check(got.size() == want.size(), "v op count", trial);
        for (size_t i = 0; i < want.size() && i < got.size(); ++i) {
            check(got[i].kind == want[i].kind && got[i].a == want[i].a && got[i].b == want[i].b, "v op", trial);
        }

        const int64_t want_idx = !v_trans ? ntok : ntok*gqa_max;
        run_goose(2, head, nhead, ntok, nb2, ts, n_stream, kv, v_ne0, v_trans,
                  v_nelem, vcur_nelem, gqa_max);
        check(g_packet[3][1] == want_idx, "v idx shape", trial);
    }

    std::printf("FG319K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
