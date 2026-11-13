// Differential gate for the Goose DSV4 compressed-state operations.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg305dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg305k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg305k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg305k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg305k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg305k_fail(int64_t c) { std::fprintf(stderr, "FG305K validation code %lld\n", (long long)c); std::abort(); }

struct R { uint32_t s0 = 0, ns = 0; };

static bool src_range(uint32_t n_stream, int64_t seq_id, R & r) {
    if (seq_id >= 0 && n_stream > 1) {
        if ((uint32_t)seq_id >= n_stream) return false;
        r.s0 = (uint32_t)seq_id; r.ns = 1; return true;
    }
    r.s0 = 0;
    r.ns = seq_id >= 0 ? 1 : n_stream;
    return true;
}

static bool dst_range(uint32_t n_stream, int64_t seq_id, uint32_t ns, uint32_t & s0) {
    if (seq_id >= 0) {
        if (ns != 1) return false;
        if (n_stream > 1 && (uint32_t)seq_id >= n_stream) return false;
        s0 = n_stream > 1 ? (uint32_t)seq_id : 0;
        return true;
    }
    if (ns != n_stream) return false;
    s0 = 0;
    return true;
}

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 12);

static int64_t call(int op, int64_t a, int64_t b, int64_t c, int64_t d,
                    int64_t * arr, int64_t n, int64_t * arr2, int64_t n2) {
    for (int i = 0; i < 16; ++i) g_packet[0][i] = 0;
    g_packet[0][0] = op;
    g_packet[0][1] = (int64_t)(uintptr_t)out.data();
    g_packet[0][2] = a; g_packet[0][3] = b; g_packet[0][4] = c; g_packet[0][5] = d;
    g_packet[0][6] = (int64_t)(uintptr_t)arr;
    g_packet[0][7] = n;
    g_packet[0][8] = (int64_t)(uintptr_t)arr2;
    g_packet[0][9] = n2;
    g_packet[3][0] = 0;
    g_packet[3][1] = 0;
    if (!fg305dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    return g_packet[3][1];
}

int main(void) {
    std::mt19937 rng(305011);

    for (int trial = 0; trial < 2000; ++trial) {
        const uint32_t n_stream = 1 + (uint32_t)(rng() % 6);
        const int64_t seq_id = (int64_t)(rng() % (n_stream + 3)) - 1;
        R want;
        const bool ok = src_range(n_stream, seq_id, want);
        const int64_t got = call(0, n_stream, seq_id, 0, 0, nullptr, 0, nullptr, 0);
        check((got == -1) == !ok, "src range result", trial);
        if (ok) { check(out[0] == want.s0 && out[1] == want.ns, "src range values", trial); }
    }

    for (int trial = 0; trial < 2000; ++trial) {
        const uint32_t n_stream = 1 + (uint32_t)(rng() % 6);
        const int64_t seq_id = (int64_t)(rng() % (n_stream + 3)) - 1;
        const uint32_t ns = (uint32_t)(rng() % (n_stream + 2));
        uint32_t s0 = 0;
        const bool ok = dst_range(n_stream, seq_id, ns, s0);
        const int64_t got = call(1, n_stream, seq_id, ns, 0, nullptr, 0, nullptr, 0);
        check((got == -1) == !ok, "dst range result", trial);
        if (ok) check(out[0] == s0, "dst range value", trial);
    }

    for (int trial = 0; trial < 1000; ++trial) {
        const uint32_t n_stream = 1 + (uint32_t)(rng() % 6);
        const uint32_t n_rs_seq = (uint32_t)(rng() % 4);
        const int64_t seq_id = (int64_t)(rng() % (n_stream + 2)) - 1;
        const bool data = (rng() & 1) != 0;
        // Native asserts that a sequence id is inside the stream range before clearing its column.
        if (data && seq_id >= 0 && (uint32_t)seq_id >= n_stream) continue;
        const int64_t got = call(2, n_stream, seq_id, n_rs_seq, data ? 1 : 0, nullptr, 0, nullptr, 0);
        if (!data) { check(got == 1, "clear without data is a no-op", trial); continue; }
        if (seq_id >= 0) {
            check(got == (int64_t)n_rs_seq + 1, "clear column count", trial);
            for (int dd = 0; dd <= (int)n_rs_seq; ++dd)
                check(out[dd] == (int64_t)dd*n_stream + seq_id, "clear column id", trial);
        } else {
            check(got == (int64_t)n_stream*(1 + n_rs_seq), "clear all planes", trial);
        }
    }

    for (int trial = 0; trial < 1000; ++trial) {
        const uint32_t n_stream = 1 + (uint32_t)(rng() % 5);
        const int64_t src = (int64_t)(rng() % n_stream);
        const int64_t dst = (int64_t)(rng() % n_stream);
        const int64_t got = call(3, n_stream, src, dst, 0, nullptr, 0, nullptr, 0);
        check(got == (src == dst ? 0 : 1), "seq_cp decision", trial);
        if (src != dst) check(out[0] == src && out[1] == dst, "seq_cp queue pair", trial);
    }

    for (int trial = 0; trial < 500; ++trial) {
        const int n = 1 + (int)(rng() % 6);
        std::vector<int64_t> ssrc(n), sdst(n);
        for (int i = 0; i < n; ++i) { ssrc[i] = (int64_t)(rng() % 8); sdst[i] = (int64_t)(rng() % 8); }
        const int64_t got = call(4, 0, 0, 0, 0, ssrc.data(), n, sdst.data(), n);
        check(got == n, "apply count", trial);
        for (int i = 0; i < n; ++i) {
            check(out[2*i] == sdst[i], "apply dst order", trial);
            check(out[2*i+1] == ssrc[i], "apply src order", trial);
        }
    }

    for (int trial = 0; trial < 2000; ++trial) {
        const uint32_t n_stream = 1 + (uint32_t)(rng() % 5);
        const uint32_t n_rs_seq = 1 + (uint32_t)(rng() % 4);
        const int64_t seq_id = (int64_t)(rng() % (n_stream + 2)) - 1;
        const uint32_t s0 = (uint32_t)(rng() % n_stream);
        const uint32_t ns = 1 + (uint32_t)(rng() % 3);
        std::vector<int64_t> rs(8);
        for (auto & v : rs) v = (int64_t)(rng() % (n_rs_seq + 2));   // may exceed n_rs_seq

        // reference
        bool ok = true;
        std::vector<int64_t> want(ns, 0);
        for (uint32_t i = 0; i < ns && ok; ++i) {
            const uint32_t seq = seq_id >= 0 ? (uint32_t)seq_id : s0 + i;
            if (seq >= rs.size()) { ok = false; break; }
            if ((uint32_t)rs[seq] > n_rs_seq) { ok = false; break; }
            want[i] = (int64_t)rs[seq]*n_stream + s0 + i;
        }
        const int64_t got = call(5, n_stream, seq_id, n_rs_seq, s0, rs.data(), (int64_t)rs.size(), nullptr, ns);
        check((got == -1) == !ok, "write ids result", trial);
        if (ok) for (uint32_t i = 0; i < ns; ++i) check(out[i] == want[i], "write ids value", trial);
    }

    std::printf("FG305K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
