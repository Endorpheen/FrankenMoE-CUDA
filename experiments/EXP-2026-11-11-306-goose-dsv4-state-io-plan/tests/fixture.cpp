// Differential gate for the Goose DSV4 state tensor I/O plan.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg306dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg306k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg306k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg306k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg306k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg306k_fail(int64_t c) { std::fprintf(stderr, "FG306K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 12);

static int64_t call(int op, int64_t n_rows, int64_t tensor_rows, int64_t row_size, int64_t s0, int64_t ns,
                    const std::vector<int64_t> * ids, int64_t ne2, int64_t type, int64_t ne0,
                    int64_t rt, int64_t rn, int64_t rr, int64_t rs) {
    for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; }
    g_packet[0][0] = op;
    g_packet[0][1] = (int64_t)(uintptr_t)out.data();
    g_packet[0][2] = n_rows;
    g_packet[0][3] = tensor_rows;
    g_packet[0][4] = row_size;
    g_packet[0][5] = s0;
    g_packet[0][6] = ns;
    g_packet[0][7] = ids ? (int64_t)(uintptr_t)ids->data() : 0;
    g_packet[0][8] = ids ? (int64_t)ids->size() : 0;
    g_packet[0][9] = ne2;
    g_packet[0][10] = type;
    g_packet[0][11] = ne0;
    g_packet[0][15] = ids ? 1 : 0;
    g_packet[1][0] = rt; g_packet[1][1] = rn; g_packet[1][2] = rr; g_packet[1][3] = rs;
    g_packet[3][0] = 0;
    g_packet[3][1] = 0;
    if (!fg306dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    return g_packet[3][1];
}

int main(void) {
    std::mt19937 rng(306011);
    int64_t want[64]; int64_t wsize = 0;

    for (int trial = 0; trial < 4000; ++trial) {
        const int64_t n_rows = (int64_t)(rng() % 8);
        const int64_t tensor_rows = (int64_t)(rng() % 8);
        const int64_t row_size = (int64_t)(rng() % 4 == 0 ? 0 : 1 + rng() % 64);
        const int64_t s0 = (int64_t)(rng() % 8);
        const int64_t ns = (int64_t)(rng() % 5);
        const int64_t ne2 = (int64_t)(rng() % 8);
        const bool use_ids = (rng() % 2) != 0;
        std::vector<int64_t> ids;
        if (use_ids) {
            // Probe the map-size contract: a shorter or longer map must be rejected.
            int64_t nids = ns;
            const int kind = (int)(rng() % 4);
            if (kind == 0 && ns > 0) nids = ns - 1;
            if (kind == 1) nids = ns + 1;
            for (int64_t i = 0; i < nids; ++i) ids.push_back((int64_t)(rng() % 10));
        }

        // reference
        int64_t ret; wsize = 0;
        const int64_t stride = tensor_rows*row_size;
        const int64_t size = n_rows*row_size;
        if (n_rows > tensor_rows) ret = -1;
        else if (size == 0) ret = 0;
        else if (use_ids && (int64_t)ids.size() != ns) ret = -1;
        else {
            bool bad = false;
            for (int64_t s = 0; s < ns && !bad; ++s) {
                const int64_t stream = use_ids ? ids[s] : s0 + s;
                if (stream >= ne2) bad = true;
                else { want[2*wsize] = stream*stride; want[2*wsize+1] = size; ++wsize; }
            }
            ret = bad ? -1 : ns;
        }

        const int64_t got = call(0, n_rows, tensor_rows, row_size, s0, ns,
                                 use_ids ? &ids : nullptr, ne2, 0, 0, 0, 0, 0, 0);
        check(got == ret, "write result", trial);
        if (ret >= 0) {
            for (int64_t i = 0; i < wsize; ++i) {
                check(out[2*i] == want[2*i], "write offset", trial);
                check(out[2*i+1] == want[2*i+1], "write size", trial);
            }
        }
    }

    for (int trial = 0; trial < 4000; ++trial) {
        const int64_t n_rows = (int64_t)(rng() % 8);
        const int64_t tensor_rows = (int64_t)(rng() % 8);
        const int64_t row_size = (int64_t)(rng() % 4 == 0 ? 0 : 1 + rng() % 64);
        const int64_t s0 = (int64_t)(rng() % 8);
        const int64_t ns = (int64_t)(rng() % 5);
        const int64_t type = (int64_t)(rng() % 4);
        const int64_t ne0 = (int64_t)(rng() % 32);
        int64_t rt = type, rn = ne0, rr = n_rows, rs = row_size;
        const int which = (int)(rng() % 5);
        if (which == 0) rt = (type > 0) ? type - 1 : type + 1;
        if (which == 1) rn = ne0 + 1;
        if (which == 2) rr = n_rows + 1;
        if (which == 3) rs = row_size + 1;

        int64_t ret; wsize = 0;
        const int64_t stride = tensor_rows*row_size;
        const int64_t size = n_rows*row_size;
        if (rt != type || rn != ne0 || rr != n_rows || rs != row_size) ret = -1;
        else if (n_rows > tensor_rows) ret = -1;
        else if (size == 0) ret = 0;
        else {
            for (int64_t s = 0; s < ns; ++s) { want[2*wsize] = (s0 + s)*stride; want[2*wsize+1] = size; ++wsize; }
            ret = ns;
        }

        const int64_t got = call(1, n_rows, tensor_rows, row_size, s0, ns, nullptr, 0, type, ne0, rt, rn, rr, rs);
        check(got == ret, "read result", trial);
        if (ret >= 0) {
            for (int64_t i = 0; i < wsize; ++i) {
                check(out[2*i] == want[2*i], "read offset", trial);
                check(out[2*i+1] == want[2*i+1], "read size", trial);
            }
        }
    }

    std::printf("FG306K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
