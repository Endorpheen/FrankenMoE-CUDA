// Differential fixture for the Goose bounded-reader decisions (EXP458).
//
// Right arm: a transcription of gguf_reader over an in-memory buffer (callback
// semantics: it returns min(chunk, end - pos)). Left arm: the Goose decisions
// driving the same state machine. Both arms run the same operation sequence and
// their (success, data_offset, nbytes_remain, bytes) results must match.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <random>

extern "C" {
int64_t fg458_fits(int64_t size, int64_t remain);
int64_t fg458_arr(int64_t n, int64_t elem, int64_t remain, int64_t * out);
int64_t fg458_str(int64_t size, int64_t remain, int64_t * out);
int64_t fg458_seek(int64_t abs_off, int64_t off, int64_t remain, int64_t * out);
int64_t fg458_raw(int64_t size, int64_t max_chunk, int64_t off, int64_t remain, int64_t end, int64_t * out);
int64_t fg458_fremain(int64_t end, int64_t cur, int64_t * out);
void    fg458_st_i64(uint64_t p, uint64_t o, int64_t v);
}

void fg458_st_i64(uint64_t p, uint64_t o, int64_t v) {
    *(int64_t *) ((char *) (uintptr_t) p + o) = v;
}

static int cases = 0, checks = 0, fails = 0;

static void fail(const char * what, const char * detail) {
    ++fails;
    if (fails <= 10) printf("FAIL %s: %s\n", what, detail);
}

// ---- native reader transcription ------------------------------------------
struct Native {
    const uint8_t * data;
    int64_t end;
    int64_t off;
    int64_t remain;
    int64_t max_chunk;

    int64_t read_raw(void * dst, int64_t size) {
        if (size == 0) return 0;
        int64_t total = 0;
        bool eof = false;
        while (total < size) {
            int64_t chunk = max_chunk < size - total ? max_chunk : size - total;
            if (off + total < off) break;                    // overflow guard
            const int64_t pos = off + total;
            const int64_t avail = end - pos;
            const int64_t nread = avail >= chunk ? chunk : (avail > 0 ? avail : 0);
            if (dst) memcpy((uint8_t *) dst + total, data + pos, (size_t) nread);
            total += nread;
            if (nread != chunk) { eof = true; break; }
        }
        off += total;
        remain -= total;
        if (eof) remain = 0;
        return total;
    }

    bool read(void * dst, int64_t size) {
        if (size > remain) return false;
        return read_raw(dst, size) == size;
    }

    bool read_string(std::string & dst, bool * ok) {
        uint64_t size = 0;
        if (!read(&size, 8)) { *ok = false; return false; }
        if ((int64_t) size > 1024LL * 1024 * 1024) { *ok = false; return false; }
        if ((int64_t) size > remain) { *ok = false; return false; }
        dst.resize((size_t) size);
        *ok = read_raw(dst.data(), (int64_t) size) == (int64_t) size;
        return *ok;
    }

    bool seek(int64_t abs_off) {
        // native seek takes uint64_t, so a negative target wraps and is rejected
        if (abs_off < 0) return false;
        const int64_t end_off = off + remain;
        if (abs_off > end_off) return false;
        off = abs_off;
        remain = end_off - abs_off;
        return true;
    }
};

// ---- goose arm -------------------------------------------------------------
struct Goose {
    int64_t end;
    int64_t off;
    int64_t remain;
    int64_t max_chunk;

    int64_t read_raw(int64_t size) {
        int64_t out[4] = {-1, -1, -1, -1};
        int64_t total = fg458_raw(size, max_chunk, off, remain, end, out);
        off = out[1];
        remain = out[2];
        return total;
    }

    bool read(int64_t size) {
        if (!fg458_fits(size, remain)) return false;
        return read_raw(size) == size;
    }

    bool seek(int64_t abs_off) {
        int64_t out[2] = {-1, -1};
        if (!fg458_seek(abs_off, off, remain, out)) return false;
        off = abs_off;
        remain = out[1];
        return true;
    }
};

// string read: 8-byte length, limit checks, then the payload
static bool native_string(Native & n, std::string & out) {
    uint64_t size = 0;
    if (!n.read(&size, 8)) return false;
    // native compares as uint64; a length above INT64_MAX must not be cast down
    if (size > 1024ULL * 1024 * 1024) return false;
    if (size > (uint64_t) n.remain) return false;
    out.resize((size_t) size);
    return n.read_raw(out.data(), (int64_t) size) == (int64_t) size;
}

static bool goose_string(Goose & g, const uint8_t * file, int64_t & len) {
    const int64_t start = g.off;
    if (!g.read(8)) return false;
    uint64_t size = 0;
    memcpy(&size, file + start, 8);
    int64_t st[1] = {-1};
    fg458_str((int64_t) size, g.remain, st);
    if (st[0] != 0) return false;
    len = (int64_t) size;
    return g.read_raw((int64_t) size) == (int64_t) size;
}

int main(void) {
    const int64_t file_size = 4096;
    std::vector<uint8_t> file((size_t) file_size);
    for (int64_t i = 0; i < file_size; ++i) file[(size_t) i] = (uint8_t) (i * 37 + 11);

    // 1. scalar fit table
    for (int64_t size : (int64_t[]){0, 1, 7, 8, 4096, 4097})
    for (int64_t remain : (int64_t[]){0, 1, 7, 8, 4095, 4096, 4097}) {
        ++cases; ++checks;
        if (fg458_fits(size, remain) != (size <= remain ? 1 : 0)) fail("fits", "table");
    }

    // 2. array and string limits
    const int64_t elems[] = {1, 2, 8, 1024, 1073741824LL, 1073741825LL, 4611686018427387904LL};
    for (int64_t n : elems)
    for (int64_t el : (int64_t[]){1, 2, 4, 8, 1LL << 40, 1LL << 60})
    for (int64_t rem : (int64_t[]){0, 8, 1024, 1LL << 40}) {
        int64_t out[1] = {-1};
        int64_t st = fg458_arr(n, el, rem, out);
        const int64_t want = (n < 0 || n > 1073741824LL) ? 1
                           : (el <= 0 || n > (INT64_MAX / el)) ? 2
                           : (n * el > rem ? 3 : 0);
        ++cases; ++checks;
        if (st != want || out[0] != want) {
            char d[160];
            snprintf(d, sizeof d, "n=%lld el=%lld rem=%lld goose=%lld want=%lld",
                     (long long) n, (long long) el, (long long) rem, (long long) st, (long long) want);
            fail("array_check", d);
        }
    }
    for (int64_t size : (int64_t[]){0, 1, 1023, 1073741824LL, 1073741825LL})
    for (int64_t rem : (int64_t[]){0, 1, 1023, 1LL << 40}) {
        int64_t out[1] = {-1};
        int64_t st = fg458_str(size, rem, out);
        const int64_t want = (size > 1073741824LL) ? 1 : (size > rem ? 2 : 0);
        ++cases; ++checks;
        if (st != want) fail("string_check", "table");
    }

    // 2b. seek boundaries, including the exact end of the readable window and
    // a zero-length window, which the random targets almost never hit
    for (int64_t off : (int64_t[]){0, 50, 100})
    for (int64_t remain : (int64_t[]){0, 1, 100})
    for (int64_t delta : (int64_t[]){-1, 0, 1}) {
        const int64_t end = off + remain;
        const int64_t target = end + delta;
        Native n{file.data(), file_size, off, remain, 64};
        Goose g{file_size, off, remain, 64};
        const bool na = n.seek(target);
        const bool ga = g.seek(target);
        ++cases; ++checks;
        if (na != ga) fail("seek-boundary-result", "mismatch");
        ++checks;
        if (na && (n.off != g.off || n.remain != g.remain)) fail("seek-boundary-state", "mismatch");
    }

    // 3. reader state machine over random operations
    std::mt19937 rng(458001);
    for (int round = 0; round < 200; ++round) {
        Native n{file.data(), file_size, (int64_t) (rng() % 64), 0, 0};
        Goose  g{file_size, n.off, 0, 0};
        const int64_t tail = file_size - n.off;
        n.remain = tail + (round % 2 ? (int64_t) (rng() % 100) : 0);
        g.remain = n.remain;
        const int64_t max_chunk = (int64_t) (rng() % 512) + 1;
        n.max_chunk = g.max_chunk = max_chunk;

        for (int step = 0; step < 40; ++step) {
            const int op = (int) (rng() % 100);
            if (op < 45) {
                const int64_t size = (int64_t) (rng() % (uint64_t) (max_chunk * 3 + 5));
                std::vector<uint8_t> a((size_t) size + 1);
                const bool na = n.read(a.data(), size);
                const bool ga = g.read(size);
                ++cases; ++checks;
                if (na != ga) fail("read-result", "mismatch");
                ++checks;
                if (n.remain != g.remain) fail("read-remain", "mismatch");
                ++checks;
                if (n.off != g.off) fail("read-offset", "mismatch");
            } else if (op < 62) {
                std::string ns;
                int64_t glen = -1;
                const bool na = native_string(n, ns);
                const bool ga = goose_string(g, file.data(), glen);
                ++cases; ++checks;
                if (na != ga) fail("string-result", "mismatch");
                ++checks;
                if (na && (int64_t) ns.size() != glen) fail("string-len", "mismatch");
                ++checks;
                if (n.remain != g.remain || n.off != g.off) fail("string-state", "mismatch");
            } else if (op < 80) {
                const int64_t target = (int64_t) (rng() % (uint64_t) (file_size + 64));
                const bool na = n.seek(target);
                const bool ga = g.seek(target);
                ++cases; ++checks;
                if (na != ga) fail("seek-result", "mismatch");
                ++checks;
                if (na && (n.off != g.off || n.remain != g.remain)) fail("seek-state", "mismatch");
            } else {
                // short read past EOF must clamp the remaining count to zero
                const int64_t huge = n.remain + (int64_t) (rng() % 64) + 1;
                std::vector<uint8_t> t((size_t) huge + 1);
                n.read(t.data(), huge);
                g.read(huge);
                ++cases; ++checks;
                if (n.remain != g.remain || n.off != g.off) fail("eof-state", "mismatch");
            }
        }
    }

    // 4. file_remain helper
    for (int64_t end : (int64_t[]){0, 1, 4096})
    for (int64_t cur : (int64_t[]){-1, 0, 1, 4095, 4096, 5000}) {
        int64_t out[1] = {-1};
        int64_t got = fg458_fremain(end, cur, out);
        const int64_t want = (cur >= 0 && end >= cur) ? end - cur : 0;
        ++cases; ++checks;
        if (got != want) fail("file_remain", "table");
    }

    printf("FG458_FILE_IO cases=%d values=%d failures=%d\n", cases, checks, fails);
    return fails ? 1 : 0;
}
