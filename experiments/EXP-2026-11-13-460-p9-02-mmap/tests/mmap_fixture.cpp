// Differential fixture for the Goose mmap/direct-I/O/mlock decisions (EXP460).
//
// Right arm: transcriptions of the native expressions in llama-model-loader.cpp
// and llama-mmap.cpp. Left arm: the exported Goose module.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <random>

extern "C" {
int64_t load_flags(int64_t mode, int64_t mmap_supported, int64_t * out);
int64_t align_ok(int64_t data_offset, int64_t align);
int64_t chunk_plan(int64_t offset, int64_t size, int64_t align, int64_t * out);
int64_t staging_size(int64_t alignment, int64_t * out);
int64_t mlock_step(int64_t failed, int64_t size, int64_t target, int64_t granularity, int64_t * out);
int64_t align_range(int64_t first, int64_t last, int64_t page, int64_t * out);
void    goose_init(int argc, char ** argv);
void    fg460_st_i64(uint64_t p, uint64_t o, int64_t v);
}

void fg460_st_i64(uint64_t p, uint64_t o, int64_t v) {
    *(int64_t *) ((char *) (uintptr_t) p + o) = v;
}

static int cases = 0, checks = 0, fails = 0;

static void fail(const char * what, const char * detail) {
    ++fails;
    if (fails <= 10) printf("FAIL %s: %s\n", what, detail);
}

static int64_t ru(int64_t x, int64_t a) { return (x + a - 1) & ~(a - 1); }
static int64_t rd(int64_t x, int64_t a) { return x & ~(a - 1); }

int main(void) {
    goose_init(0, 0);
    const int64_t DIRECT_IO_BUFFER = 64LL * 1024 * 1024;

    // 1. load-mode mapping, including the unsupported-mmap fallback
    const int64_t modes[] = {-1, 0, 1, 2, 3, 4, 5, -2};
    for (int64_t mode : modes)
    for (int sup = 0; sup < 2; ++sup) {
        int64_t out[3] = {-1, -1, -1};
        load_flags(mode, sup, out);
        int64_t want_mmap = (mode == -1 || mode == 1 || mode == 3) ? 1 : 0;
        if (!sup) want_mmap = 0;
        const int64_t want_dio = mode == 4 ? 1 : 0;
        const int64_t want_mlock = (mode == 2 || mode == 3) ? 1 : 0;
        ++cases; ++checks;
        if (out[0] != want_mmap || out[1] != want_dio || out[2] != want_mlock) {
            const std::string d = "mode=" + std::to_string(mode) + " sup=" + std::to_string(sup);
            fail("load_flags", d.c_str());
        }
    }

    // 2. mmap data-offset alignment gate
    for (int64_t align : (int64_t[]){1, 2, 32, 64, 4096})
    for (int64_t off : (int64_t[]){0, 1, 31, 32, 63, 64, 4095, 4096, 10990944}) {
        const int64_t want = (off % align) == 0 ? 1 : 0;
        ++cases; ++checks;
        if (align_ok(off, align) != want) {
            const std::string d = "off=" + std::to_string(off) + " align=" + std::to_string(align);
            fail("align_ok", d.c_str());
        }
    }

    // 3. direct-I/O chunk plan against the native expressions
    for (int64_t align : (int64_t[]){1, 32, 512, 4096})
    for (int64_t size : (int64_t[]){0, 1, 31, 32, 4095, 4096, 100000, DIRECT_IO_BUFFER + 7, 2 * DIRECT_IO_BUFFER}) {
        for (int64_t off = 0; off < 3 * align; off += (align > 64 ? align / 4 : 1)) {
            int64_t out[5];
            chunk_plan(off, size, align, out);
            const int64_t a_off = rd(off, align);
            const int64_t from = off - a_off;
            const int64_t bytes = ru(from + size, align);
            const int64_t buf = bytes < DIRECT_IO_BUFFER ? bytes : DIRECT_IO_BUFFER;
            ++cases; ++checks;
            if (out[0] != a_off || out[1] != from || out[2] != bytes || out[3] != buf || out[4] != from) {
                const std::string d = "off=" + std::to_string(off) + " size=" + std::to_string(size) +
                                      " align=" + std::to_string(align);
                fail("chunk_plan", d.c_str());
            }
        }
    }

    // 3b. the copy loop delivers exactly `size` bytes from the requested offset
    {
        std::mt19937 rng(460001);
        std::vector<uint8_t> file((size_t) (2 * DIRECT_IO_BUFFER + 4096));
        for (size_t i = 0; i < file.size(); ++i) file[i] = (uint8_t) (i * 131 + 7);
        for (int round = 0; round < 200; ++round) {
            const int64_t align = (int64_t[]){1, 32, 512, 4096}[rng() % 4];
            const int64_t off = (int64_t) (rng() % (uint64_t) (file.size() / 2));
            const int64_t size = (int64_t) (rng() % (uint64_t) (file.size() / 2 - (size_t) off));
            int64_t plan[5];
            chunk_plan(off, size, align, plan);
            // simulate the native staging loop over the aligned window
            std::vector<uint8_t> dest((size_t) size, 0);
            const int64_t skip0 = plan[4];
            int64_t copied = 0;
            for (int64_t done = 0; done < plan[2]; ) {
                const int64_t n = plan[3] < plan[2] - done ? plan[3] : plan[2] - done;
                const int64_t skip = copied == 0 ? skip0 : 0;
                const int64_t avail = n - skip;
                const int64_t count = avail < size - copied ? avail : size - copied;
                if (count > 0) {
                    const int64_t src = plan[0] + done + skip;
                    memcpy(dest.data() + copied, file.data() + src, (size_t) count);
                    copied += count;
                }
                done += n;
            }
            ++cases; ++checks;
            if (copied != size) fail("copy-total", "staged bytes differ from the request");
            ++checks;
            if (size > 0 && memcmp(dest.data(), file.data() + off, (size_t) size) != 0)
                fail("copy-bytes", "staged bytes differ from the file window");
        }
    }

    // 4. staging buffer size
    for (int64_t align : (int64_t[]){1, 2, 32, 512, 4096}) {
        int64_t out[1] = {-1};
        staging_size(align, out);
        const int64_t want = align != 1 ? DIRECT_IO_BUFFER + 2 * align : 1024 * 1024;
        ++cases; ++checks;
        if (out[0] != want) fail("staging_size", std::to_string(align).c_str());
    }

    // 5. mlock growth: page align, lock only the tail, and stop after a failure
    const int64_t page = 4096;
    for (int64_t size : (int64_t[]){0, 1, 4095, 4096, 4097, 100000})
    for (int64_t target : (int64_t[]){0, 1, 4095, 4096, 5000, 200000})
    for (int failed = 0; failed < 2; ++failed) {
        int64_t out[3];
        mlock_step(failed, size, target, page, out);
        int64_t want_size = size, want_lock = 0;
        if (!failed) {
            const int64_t aligned = ru(target, page);
            if (aligned > size) { want_lock = aligned - size; want_size = aligned; }
        }
        ++cases; ++checks;
        if (out[0] != want_size || out[1] != want_lock || out[2] != failed) {
            const std::string d = "size=" + std::to_string(size) + " target=" + std::to_string(target) +
                                  " failed=" + std::to_string(failed);
            fail("mlock_step", d.c_str());
        }
    }

    // 6. unmap range alignment, including the empty-range clamp
    for (int64_t page_size : (int64_t[]){4096, 16384})
    for (int64_t first = 0; first <= 3 * 4096; first += 1024)
    for (int64_t last = 0; last <= 3 * 4096; last += 1024) {
        int64_t out[2];
        align_range(first, last, page_size, out);
        const int64_t off = first & (page_size - 1);
        int64_t f = first + (off == 0 ? 0 : page_size - off);
        int64_t l = last & ~(page_size - 1);
        if (l <= f) l = f;
        ++cases; ++checks;
        if (out[0] != f || out[1] != l) {
            const std::string d = "first=" + std::to_string(first) + " last=" + std::to_string(last);
            fail("align_range", d.c_str());
        }
    }

    printf("FG460_MMAP cases=%d values=%d failures=%d\n", cases, checks, fails);
    return fails ? 1 : 0;
}
