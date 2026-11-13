// Differential gate for the Goose KV slot-search module against a direct transcription of
// llama-kv-cache.cpp:1039-1107.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <random>
#include <vector>

extern "C" {
int64_t fg205f_head_start(int64_t, int64_t, int64_t);
int64_t fg205f_find_slots(uint64_t, int64_t, int64_t, int64_t, int64_t, uint64_t);
}

static int checks, failures;
static void ck(bool ok, const char * what, long long a, long long b) {
    ++checks;
    if (!ok) { if (failures < 20) printf("FAIL %s: goose=%lld ref=%lld\n", what, a, b); ++failures; }
}

// Native transcription. Returns the chosen indices, or empty for "no slot" (what native returns).
static std::vector<uint32_t> ref_find(const std::vector<uint8_t> & can_use, uint32_t size,
                                      uint32_t head0, uint32_t n_tokens, bool cont) {
    std::vector<uint32_t> idxs;
    if (n_tokens > size) return {};
    uint32_t head_cur = head0;
    uint32_t n_tested = 0;
    const uint32_t n_test = cont ? n_tokens : 1;

    while (true) {
        if (head_cur + n_test > size) {
            n_tested += size - head_cur;
            head_cur = 0;
            continue;
        }
        for (uint32_t i = 0; i < n_test; i++) {
            const auto idx = head_cur;
            head_cur++;
            n_tested++;
            if (can_use[idx]) {
                idxs.push_back(idx);
            } else {
                if (cont) break;
            }
        }
        if (idxs.size() == n_tokens) break;
        if (cont) idxs.clear();
        if (n_tested >= size) return {};
    }
    if (idxs.size() < n_tokens) return {};
    return idxs;
}

int main(void) {
    std::mt19937 rng(205011);

    // head_start
    for (int i = 0; i < 2000; ++i) {
        const int64_t h = (int64_t)(rng() % 400), u = (int64_t)(rng() % 400), n = (int64_t)(rng() % 40);
        const int64_t ref = (h > u + 2*(int64_t)n) ? 0 : h;
        ck(fg205f_head_start(h, u, n) == ref, "head_start", h, ref);
    }

    // find_slots over many occupancy patterns
    for (int t = 0; t < 6000; ++t) {
        const uint32_t size = 1 + (rng() % 24);
        const uint32_t head0 = rng() % size;
        const uint32_t n_tokens = 1 + (rng() % (size < 6 ? size : 6));
        const bool cont = (rng() & 1) != 0;
        std::vector<uint8_t> can(size);
        const unsigned density = 1 + (rng() % 100);
        for (auto & c : can) c = (rng() % 100) < density ? 1 : 0;

        auto ref = ref_find(can, size, head0, n_tokens, cont);
        std::vector<int64_t> out(n_tokens + 8, -1);
        const int64_t got = fg205f_find_slots((uint64_t)(uintptr_t)can.data(), size, head0,
                                              n_tokens, cont ? 1 : 0, (uint64_t)(uintptr_t)out.data());
        if (ref.empty()) {
            ck(got == -1, "no-slot", got, -1);
        } else {
            ck(got == (int64_t)ref.size(), "count", got, (int64_t)ref.size());
            for (size_t i = 0; i < ref.size() && i < (size_t)got; ++i)
                ck((uint32_t)out[i] == ref[i], "index", out[i], (int64_t)ref[i]);
        }
    }
    // the full-cache-free case must succeed immediately
    {
        std::vector<uint8_t> can(8, 1);
        std::vector<int64_t> out(8, -1);
        ck(fg205f_find_slots((uint64_t)(uintptr_t)can.data(), 8, 0, 4, 1, (uint64_t)(uintptr_t)out.data()) == 4, "all free cont", 0, 4);
    }
    // n_tokens > size must give no slot
    {
        std::vector<uint8_t> can(2, 1);
        std::vector<int64_t> out(8, -1);
        ck(fg205f_find_slots((uint64_t)(uintptr_t)can.data(), 2, 0, 5, 0, (uint64_t)(uintptr_t)out.data()) == -1, "too many", 0, -1);
    }

    printf("FG205F_FIXTURE checks=%d failures=%d\n", checks, failures);
    if (failures) { printf("EXP205_LOCAL_GATES_FAIL\n"); return 1; }
    printf("EXP205_LOCAL_GATES_PASS\n");
    return 0;
}
