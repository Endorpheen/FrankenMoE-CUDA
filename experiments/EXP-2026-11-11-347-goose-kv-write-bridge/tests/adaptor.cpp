// Gate for the adapter: the journal translated back into ranges must equal the native selection.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" int64_t fg345w_select(int64_t other, int64_t partial, int64_t swa_none, int64_t seq_id,
                                 int64_t n_stream, int64_t n_swa, int64_t swa_type, int64_t size,
                                 const int32_t * pos, const uint64_t * seqmask, int32_t * out_counts,
                                 int32_t * out_ranges, int64_t out_ranges_cap);
extern "C" int fg345w_enabled(void);

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static bool masked_native(int64_t n_swa, int64_t swa_type, int64_t p0, int64_t p1) {
    switch (swa_type) {
        case 0: break;
        case 1: if (p1 - p0 >= n_swa) return true; break;
        case 2: { const int64_t start = (p1 / n_swa) * n_swa; if (p0 < start) return true; } break;
        default: {
            const int64_t half = n_swa / 2;
            const int64_t diff = p1 - p0;
            if (diff < -half || diff > half) return true;
        } break;
    }
    return false;
}

int main(void) {
    std::mt19937 rng(347011);
    check(fg345w_enabled() == 1, "enabled by default", 0);

    for (int trial = 0; trial < 3000; ++trial) {
        const int64_t n_stream = 1 + (int64_t)(rng() % 3);
        const int64_t size = (int64_t)(rng() % 10);
        const int64_t other = rng() % 10 == 0, partial = rng() & 1, swa_none = rng() & 1;
        const int64_t seq_id = (rng() % 3 == 0) ? -1 : (int64_t)(rng() % 3);
        const int64_t swa_type = (int64_t)(rng() % 4);
        const int64_t n_swa = 1 + (int64_t)(rng() % 8);

        std::vector<int32_t> pos((size_t)(n_stream*size));
        std::vector<uint64_t> mask((size_t)(n_stream*size));
        for (int64_t i = 0; i < n_stream*size; ++i) {
            pos[(size_t)i] = (rng() % 4 == 0) ? -1 : (int32_t)(rng() % 40);
            uint64_t m = 0;
            for (int64_t q = 0; q < 3; ++q) if (rng() & 1) m |= (1ULL << q);
            mask[(size_t)i] = m;
        }

        std::vector<int32_t> want_counts((size_t)n_stream, -1);
        std::vector<int32_t> want_ranges;
        const bool refused = other || (partial && swa_none);
        if (!refused) {
            for (int64_t s = 0; s < n_stream; ++s) {
                const int64_t base = s*size;
                int64_t smax = -1;
                if (seq_id != -1)
                    for (int64_t q = 0; q < size; ++q)
                        if (mask[(size_t)(base+q)] & (1ULL << seq_id)) smax = std::max<int64_t>(smax, pos[(size_t)(base+q)]);
                int64_t count = 0, begin = -1;
                for (int64_t i = 0; i < size; ++i) {
                    const int64_t p = pos[(size_t)(base+i)];
                    bool add = p != -1 && (seq_id == -1 || (mask[(size_t)(base+i)] & (1ULL << seq_id)));
                    if (add && seq_id != -1 && masked_native(n_swa, swa_type, p, smax)) add = false;
                    if (add) { ++count; if (begin < 0) begin = i; }
                    else if (begin >= 0) { want_ranges.push_back((int32_t)begin); want_ranges.push_back((int32_t)i); begin = -1; }
                }
                if (begin >= 0) { want_ranges.push_back((int32_t)begin); want_ranges.push_back((int32_t)size); }
                want_counts[(size_t)s] = (int32_t)count;
            }
        }

        std::vector<int32_t> got_counts((size_t)n_stream, -1);
        std::vector<int32_t> got_ranges((size_t)(4*n_stream*size + 8), -1);
        const int64_t n = fg345w_select(other, partial, swa_none, seq_id, n_stream, n_swa, swa_type, size,
                                        pos.data(), mask.data(), got_counts.data(), got_ranges.data(),
                                        (int64_t)got_ranges.size()/2);
        if (refused) {
            check(n == -1, "refusal reported", trial);
            continue;
        }
        check(n == (int64_t)want_ranges.size()/2, "range count", trial);
        for (int64_t s = 0; s < n_stream; ++s) check(got_counts[(size_t)s] == want_counts[(size_t)s], "cell count", trial);
        for (int64_t i = 0; i < n*2 && i < (int64_t)want_ranges.size(); ++i)
            check(got_ranges[(size_t)i] == want_ranges[(size_t)i], "range", trial);
    }

    std::printf("FG345W_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
