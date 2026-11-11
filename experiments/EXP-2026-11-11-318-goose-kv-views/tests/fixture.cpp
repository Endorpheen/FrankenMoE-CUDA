// Differential gate for the Goose KV view shapes and n_kv padding.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <random>

extern "C" uint8_t fg318dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg318k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg318k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg318k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg318k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg318k_fail(int64_t c) { std::fprintf(stderr, "FG318K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(64);

static int64_t call(int op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t n,
                    std::vector<int64_t> * arr, std::vector<int64_t> * arr2) {
    for (int i = 0; i < 16; ++i) g_packet[0][i] = 0;
    std::fill(out.begin(), out.end(), -99);
    g_packet[0][0] = op;
    g_packet[0][1] = (int64_t)(uintptr_t)out.data();
    g_packet[0][2] = a; g_packet[0][3] = b; g_packet[0][4] = c; g_packet[0][5] = d;
    g_packet[0][6] = n;
    g_packet[0][7] = arr ? (int64_t)(uintptr_t)arr->data() : 0;
    g_packet[0][8] = arr2 ? (int64_t)(uintptr_t)arr2->data() : 0;
    g_packet[3][0] = 0; g_packet[3][1] = 0;
    if (!fg318dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    return g_packet[3][1];
}

static int64_t padto(int64_t x, int64_t align) {
    if (align <= 1) return x;
    return ((x + align - 1) / align) * align;
}

int main(void) {
    std::mt19937 rng(318011);

    for (int trial = 0; trial < 2000; ++trial) {
        const int64_t ts = 2 * (1 + (int64_t)(rng() % 2));   // f16/bf16 or f32
        const int64_t head_k = 1 + (int64_t)(rng() % 64);
        const int64_t n_head_kv = 1 + (int64_t)(rng() % 8);
        const int64_t n_kv = 1 + (int64_t)(rng() % 256);
        const int64_t ns = 1 + (int64_t)(rng() % 4);
        const int64_t kv_size = 1 + (int64_t)(rng() % 512);
        const int64_t s0 = (int64_t)(rng() % ns);
        std::vector<int64_t> arr = { ts, s0 };
        call(0, head_k, n_head_kv, n_kv, ns, kv_size, &arr, nullptr);
        check(out[0] == head_k, "k ne0", trial);
        check(out[1] == n_head_kv, "k ne1", trial);
        check(out[2] == n_kv, "k ne2", trial);
        check(out[3] == ns, "k ne3", trial);
        check(out[4] == ts*head_k, "k nb1", trial);
        check(out[5] == ts*head_k*n_head_kv, "k nb2", trial);
        check(out[6] == ts*head_k*n_head_kv*kv_size, "k nb3", trial);
        check(out[7] == ts*head_k*n_head_kv*kv_size*s0, "k offset", trial);

        const int64_t head_v = 1 + (int64_t)(rng() % 64);
        const int64_t gqa = head_v*n_head_kv + (int64_t)(rng() % 4);
        const int64_t vtrans = rng() & 1;
        std::vector<int64_t> arrv = { ts, s0, vtrans, gqa };
        call(1, head_v, n_head_kv, n_kv, ns, kv_size, &arrv, nullptr);
        if (!vtrans) {
            check(out[0] == head_v && out[1] == n_head_kv && out[2] == n_kv && out[3] == ns, "v shape", trial);
            check(out[4] == ts*head_v, "v nb1", trial);
            check(out[5] == ts*gqa, "v nb2", trial);
            check(out[6] == ts*gqa*kv_size, "v nb3", trial);
            check(out[7] == ts*gqa*kv_size*s0, "v offset", trial);
        } else {
            check(out[0] == n_kv && out[1] == n_head_kv && out[2] == head_v && out[3] == ns, "vT shape", trial);
            check(out[4] == ts*n_kv*head_v, "vT nb1", trial);
            check(out[5] == ts*n_kv, "vT nb2", trial);
            check(out[6] == ts*n_kv*gqa, "vT nb3", trial);
            check(out[7] == ts*n_kv*gqa*s0, "vT offset", trial);
        }
    }

    for (int trial = 0; trial < 3000; ++trial) {
        const int64_t n_pad = (int64_t)(rng() % 512);
        const int64_t n_streams = 1 + (int64_t)(rng() % 4);
        std::vector<int64_t> sizes(n_streams), used(n_streams);
        for (int i = 0; i < n_streams; ++i) {
            sizes[i] = 1 + (int64_t)(rng() % 1024);
            used[i] = (int64_t)(rng() % 1024);
        }
        int64_t want = 0;
        const int64_t n_pad_cur = std::max<int64_t>(n_pad, 256);
        for (int i = 0; i < n_streams; ++i) {
            const int64_t v = std::max(n_pad_cur, padto(used[i], n_pad_cur));
            want = std::max(want, std::min(sizes[i], v));
        }
        check(call(2, n_pad, 0, 0, 0, n_streams, &sizes, &used) == want, "n_kv", trial);
    }

    std::printf("FG318K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
