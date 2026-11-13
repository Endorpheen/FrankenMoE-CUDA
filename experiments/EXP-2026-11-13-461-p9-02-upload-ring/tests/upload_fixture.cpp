// Differential fixture for the async-upload staging ring (EXP461).
//
// Right arm: transcriptions of the native expressions in llama-model-loader.cpp
// (alignment aggregation, ring rotation, the chunk loop and the eligibility
// chain, plus the staged-tensor ordering). Left arm: the exported Goose module.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <random>
#include <algorithm>

#include "upload-gen.h"

extern "C" {
void fg461_st_i64(uint64_t, uint64_t, int64_t);
}

void fg461_st_i64(uint64_t p, uint64_t o, int64_t v) {
    *(int64_t *) ((char *) (uintptr_t) p + o) = v;
}

static int cases = 0, checks = 0, fails = 0;

static void fail(const char * what, const std::string & detail) {
    ++fails;
    if (fails <= 10) printf("FAIL %s: %s\n", what, detail.c_str());
}

int main(void) {
    goose_init(0, 0);
    const int64_t DIRECT_IO_BUFFER = 64LL * 1024 * 1024;
    const int64_t n_buffers = 4;

    // 1. alignment aggregation: max over the files, floor 1
    for (int n = 0; n <= 4; ++n)
    for (int64_t a : (int64_t[]){1, 2, 512, 4096, 0}) {
        std::vector<int64_t> xs((size_t) n, a);
        if (n > 1) xs[(size_t) (n - 1)] = 4096;
        sl_i64 s; s.data = xs.data(); s.len = (int64_t) xs.size();
        int64_t want = 1;
        for (int64_t v : xs) want = std::max(want, v);
        ++cases; ++checks;
        if (align_max(s) != want) fail("align_max", std::to_string(n) + "/" + std::to_string(a));
    }

    // 2. four-buffer rotation, including a zero ring
    for (int64_t idx = 0; idx < 12; ++idx) {
        ++cases; ++checks;
        if (ring_next(idx, n_buffers) != (idx + 1) % n_buffers) fail("ring_next", std::to_string(idx));
        ++checks;
        if (ring_next(idx, 0) != 0) fail("ring_next-zero", std::to_string(idx));
    }

    // 3. chunk loop compared with the native expressions, byte accounting included
    std::mt19937 rng(461001);
    for (int round = 0; round < 200; ++round) {
        const int64_t alignment = (int64_t[]){1, 32, 512, 4096}[rng() % 4];
        // native sizes the staging buffer at >= 64 MiB or 1 MiB, always larger
        // than the device alignment; the loop relies on that
        const int64_t buffer_size = (int64_t[]){4096, 65536, DIRECT_IO_BUFFER / 4}[rng() % 3];
        const int64_t offset = (int64_t) (rng() % (uint64_t) (1 << 20));
        const int64_t n_size = (int64_t) (rng() % (uint64_t) (1 << 15)) + 1;

        const int64_t aligned = offset & ~(alignment - 1);
        const int64_t from = offset - aligned;
        const int64_t read_start = aligned;
        const int64_t read_end = (offset + n_size + alignment - 1) & ~(alignment - 1);
        const int64_t total = read_end - read_start;
        const int64_t tail = read_end - (offset + n_size);

        int64_t bytes_read = 0, data_read = 0, idx = 0;
        while (bytes_read < total) {
            int64_t out[3];
            chunk(total, buffer_size, bytes_read, from, tail, (uint64_t) (uintptr_t) out);
            // native expressions for the same step
            const int64_t read_size = std::min<int64_t>(buffer_size, total - bytes_read);
            int64_t ptr = 0, copy = read_size;
            if (bytes_read == 0) { ptr = from; copy -= from; }
            if (aligned + bytes_read + read_size > offset + n_size) {
                copy -= (read_end - (offset + n_size));
            }
            ++cases; ++checks;
            if (out[0] != read_size || out[1] != ptr || out[2] != copy) {
                const std::string d = "round=" + std::to_string(round) + " off=" + std::to_string(offset) +
                                      " n=" + std::to_string(n_size) + " a=" + std::to_string(alignment);
                fail("chunk", d);
            }
            ++checks;
            if (copy < 0) fail("chunk-negative", "copy below zero");
            data_read += copy;
            bytes_read += read_size;
            ++checks;
            if (ring_next(idx, n_buffers) != (idx + 1) % n_buffers) fail("ring-in-loop", "rotation");
            idx = ring_next(idx, n_buffers);
        }
        ++cases; ++checks;
        if (data_read != n_size) {
            const std::string d = "data_read=" + std::to_string(data_read) + " n_size=" + std::to_string(n_size);
            fail("chunk-total", d);
        }
    }

    // 4. eligibility chain: every flag flips the reason in the native order
    struct Case { int use_mmap, check, has_buf, has_dev, is_def, has_async, has_host, has_ev, has_buft, alloc, backend, want; };
    const Case cases_t[] = {
        {0,0,1,1,1,1,1,1,1,1,1, 0},
        {1,0,1,1,1,1,1,1,1,1,1, 1},
        {0,1,1,1,1,1,1,1,1,1,1, 1},
        {0,0,0,1,1,1,1,1,1,1,1, 2},
        {0,0,1,0,1,1,1,1,1,1,1, 3},
        {0,0,1,1,0,1,1,1,1,1,1, 4},
        {0,0,1,1,1,0,1,1,1,1,1, 5},
        {0,0,1,1,1,1,0,1,1,1,1, 5},
        {0,0,1,1,1,1,1,0,1,1,1, 5},
        {0,0,1,1,1,1,1,1,0,1,1, 6},
        {0,0,1,1,1,1,1,1,1,0,1, 7},
        {0,0,1,1,1,1,1,1,1,1,0, 7},
    };
    for (const auto & c : cases_t) {
        int64_t out[1] = {-1};
        const int64_t got = eligible(c.use_mmap, c.check, c.has_buf, c.has_dev, c.is_def, c.has_async,
                                     c.has_host, c.has_ev, c.has_buft, c.alloc, c.backend,
                                     (uint64_t) (uintptr_t) out);
        ++cases; ++checks;
        if (got != c.want || out[0] != c.want) {
            fail("eligible", "want=" + std::to_string(c.want) + " got=" + std::to_string(got));
        }
    }

    // 5. staged-tensor ordering: compare a stable sort with the native comparator
    for (int round = 0; round < 200; ++round) {
        struct T { int staged; int64_t bytes; int seq; };
        std::vector<T> v;
        for (int i = 0; i < 12; ++i) {
            const int staged = (rng() % 2) ? 1 : 0;
            const int64_t bytes = (int64_t) ((rng() % 8) * 1024 + 1);
            v.push_back({staged, bytes, i});
        }
        std::vector<T> want = v;
        std::stable_sort(want.begin(), want.end(), [](const T & a, const T & b) {
            const bool staged_a = a.staged != 0;
            const bool staged_b = b.staged != 0;
            if (staged_a != staged_b) return staged_a;
            return staged_a && a.bytes > b.bytes;
        });
        std::stable_sort(v.begin(), v.end(), [](const T & a, const T & b) {
            return before(a.staged, a.bytes, b.staged, b.bytes) != 0;
        });
        for (size_t i = 0; i < v.size(); ++i) {
            ++cases; ++checks;
            if (v[i].seq != want[i].seq || v[i].staged != want[i].staged) {
                fail("order", "stable sort differs at " + std::to_string(i));
                break;
            }
        }
    }

    printf("FG461_UPLOAD cases=%d values=%d failures=%d\n", cases, checks, fails);
    return fails ? 1 : 0;
}
