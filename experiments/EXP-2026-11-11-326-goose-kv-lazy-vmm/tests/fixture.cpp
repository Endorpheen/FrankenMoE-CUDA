// Differential gate for the Goose lazy VMM KV policy.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg326dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg326k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg326k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg326k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg326k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg326k_fail(int64_t c) { std::fprintf(stderr, "FG326K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 12), cnt(1);

static int64_t call(int op, int64_t n_kv, int64_t vmm, int64_t offload, int64_t vtrans, int64_t avail,
                    int64_t n_layer, int64_t n_stream, std::vector<int64_t> * ranges,
                    std::vector<int64_t> * tensors) {
    for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
    std::fill(out.begin(), out.end(), -99);
    cnt[0] = 0;
    g_packet[0][0] = op;
    g_packet[0][1] = (int64_t)(uintptr_t)out.data();
    g_packet[0][2] = n_kv; g_packet[0][3] = vmm; g_packet[0][4] = offload; g_packet[0][5] = vtrans;
    g_packet[0][6] = avail; g_packet[0][7] = n_layer; g_packet[0][8] = n_stream;
    g_packet[0][9] = ranges ? (int64_t)(uintptr_t)ranges->data() : 0;
    g_packet[0][10] = ranges ? (int64_t)ranges->size()/2 : 0;
    g_packet[1][0] = tensors ? (int64_t)(uintptr_t)tensors->data() : 0;
    g_packet[2][0] = (int64_t)(uintptr_t)cnt.data();
    g_packet[3][0] = 0; g_packet[3][1] = 0;
    if (!fg326dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    return g_packet[3][1];
}

int main(void) {
    std::mt19937 rng(326011);

    for (int trial = 0; trial < 2000; ++trial) {
        const int64_t offload = rng() & 1, vtrans = rng() & 1, avail = rng() & 1;
        const int64_t want = (offload && !vtrans && avail) ? 1 : 0;
        call(0, 0, 0, offload, vtrans, avail, 0, 0, nullptr, nullptr);
        check(out[0] == want, "buffer choice", trial);
        check(out[1] == want, "vmm_lazy flag", trial);
    }

    for (int trial = 0; trial < 3000; ++trial) {
        const int64_t n_layer = (int64_t)(rng() % 4);
        const int64_t n_stream = (int64_t)(rng() % 3);
        const int64_t count = n_layer*n_stream*2;
        std::vector<int64_t> tensors((size_t)count*4, 0);
        for (int64_t i = 0; i < count; ++i) {
            tensors[i*4+0] = (rng() % 5) ? 1 : 0;              // present
            tensors[i*4+1] = (int64_t)(rng() % 4096);           // offset
            tensors[i*4+2] = (int64_t)(rng() % 512);            // row size
        }
        const int64_t n_kv = (int64_t)(rng() % 256);
        const int64_t vmm = rng() & 1;

        int64_t want_n = 0;
        std::vector<std::pair<int64_t,int64_t>> want;
        if (vmm && n_kv > 0) {
            for (int64_t i = 0; i < count; ++i) {
                if (!tensors[i*4+0]) continue;
                want.push_back({tensors[i*4+1], tensors[i*4+2]*n_kv});
            }
            want_n = (int64_t)want.size();
        }
        const int64_t got = call(1, n_kv, vmm, 0, 0, 0, n_layer, n_stream, nullptr, &tensors);
        check(got == want_n, "commit count", trial);
        for (int64_t i = 0; i < got && i < (int64_t)want.size(); ++i) {
            check(out[i*2] == want[i].first, "commit offset", trial);
            check(out[i*2+1] == want[i].second, "commit size", trial);
        }
    }

    for (int trial = 0; trial < 2000; ++trial) {
        const int64_t n_ranges = (int64_t)(rng() % 5);
        std::vector<int64_t> ranges;
        int64_t cur = 0;
        for (int64_t r = 0; r < n_ranges; ++r) {
            const int64_t lo = cur + (int64_t)(rng() % 4);
            const int64_t hi = lo + 1 + (int64_t)(rng() % 8);
            ranges.push_back(lo); ranges.push_back(hi);
            cur = hi;
        }
        int64_t want = 0;
        for (int64_t r = 0; r < n_ranges; ++r) want = std::max(want, ranges[r*2+1]);
        check(call(2, 0, 0, 0, 0, 0, 0, 0, &ranges, nullptr) == want, "highest cell", trial);
    }

    std::printf("FG326K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
