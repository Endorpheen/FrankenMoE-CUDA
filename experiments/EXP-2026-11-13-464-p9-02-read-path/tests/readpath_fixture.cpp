// Differential fixture for the mmap/pread read decisions (EXP464).
//
// Right arm: transcriptions of llama_file::read_raw_unsafe, the errno fallback
// chain and the loader's source selection. Left arm: the exported Goose module.
// A scripted errno sequence drives the direct-read loop, so retry, fallback and
// fatal paths are all exercised.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <random>

#include "readpath-gen.h"

extern "C" {
void fg464_st_i64(uint64_t, uint64_t, int64_t);
}

void fg464_st_i64(uint64_t p, uint64_t o, int64_t v) {
    *(int64_t *) ((char *) (uintptr_t) p + o) = v;
}

static int cases = 0, checks = 0, fails = 0;

static void fail(const char * what, const std::string & detail) {
    ++fails;
    if (fails <= 10) printf("FAIL %s: %s\n", what, detail.c_str());
}

enum { ERR_OK = 0, ERR_EINTR = 1, ERR_EFAULT = 2, ERR_EINVAL = 3, ERR_OTHER = 4 };

// native: read_raw_unsafe with fd == -1 (buffered)
struct Buffered {
    int64_t size;
    int64_t off;

    // returns bytes delivered, -1 when the short-read rule fires
    int64_t read(void * dst, int64_t len) {
        if (len == 0) return 0;
        const int64_t curr = off;
        int64_t to_read = len;
        if (curr >= size) to_read = 0;
        else { const int64_t left = size - curr; if (left < to_read) to_read = left; }
        if (to_read > 0 && to_read < len) {
            // fread returns 1 item of to_read bytes; the code then checks ret != 1 only
            // when to_read > 0, so a short read of the truncated size is fine
            memcpy(dst, src.data() + curr, (size_t) to_read);
            off += to_read;
            return to_read;
        }
        if (to_read > 0) {
            memcpy(dst, src.data() + curr, (size_t) to_read);
            off += to_read;
        }
        return to_read;
    }
    std::vector<uint8_t> src;
};

// native: direct loop with a scripted errno sequence
// returns 0 ok, 1 fallback, 2 fatal, 3 zero-filled
static int native_direct(const std::vector<int> & errs, int64_t size, int64_t pos,
                         int64_t len, int64_t & delivered, std::string & why) {
    delivered = 0;
    size_t k = 0;
    while (delivered < len) {
        const int e = k < errs.size() ? errs[k++] : ERR_OK;
        if (e == ERR_EINTR) { why = "retry"; continue; }
        if (e == ERR_EFAULT || e == ERR_EINVAL) { why = "fallback"; return 1; }
        if (e == ERR_OTHER) { why = "read error"; return 2; }
        // a successful read returns min(remaining, size - pos - delivered)
        const int64_t left = size - (pos + delivered);
        if (left <= 0) {
            if (pos + delivered == size) { delivered = len; why = "zero-fill"; return 3; }
            why = "unexpectedly reached end of file";
            return 2;
        }
        const int64_t n = std::min(left, len - delivered);
        delivered += n;
    }
    why = "ok";
    return 0;
}

int main(void) {
    goose_init(0, 0);

    // 1. buffered plan: truncation at EOF, no error when starting past it
    for (int64_t size : (int64_t[]){0, 1, 100, 4096})
    for (int64_t off : (int64_t[]){0, 1, 50, 99, 100, 101, 4095, 4096, 5000})
    for (int64_t len : (int64_t[]){0, 1, 50, 4096}) {
        int64_t out[2] = {-1, -1};
        const int64_t to_read = buffered_plan(size, off, len, (uint64_t) (uintptr_t) out);
        int64_t want = len;
        if (off >= size) want = 0;
        else { const int64_t left = size - off; if (left < want) want = left; }
        ++cases; ++checks;
        if (to_read != want || out[0] != want) {
            fail("buffered_plan", std::to_string(size) + "/" + std::to_string(off) + "/" + std::to_string(len));
        }
        ++checks;
        if (out[1] != (want > 0 ? 1 : 0)) fail("buffered-short-rule", "flag");
    }

    // 2. errno actions
    for (int e = 0; e <= 5; ++e) {
        int64_t out[1] = {-1};
        const int64_t got = err_action(e, (uint64_t) (uintptr_t) out);
        const int64_t want = e == ERR_EINTR ? 0 : (e == ERR_EFAULT || e == ERR_EINVAL) ? 1 : 2;
        ++cases; ++checks;
        if (got != want) fail("err_action", std::to_string(e));
    }

    // 3. EOF zero-fill only at exactly the file size
    for (int64_t size : (int64_t[]){0, 1, 4096})
    for (int64_t pos : (int64_t[]){0, 1, 4095, 4096, 4097}) {
        int64_t out[1] = {-1};
        const int64_t got = eof_policy(size, pos, (uint64_t) (uintptr_t) out);
        ++cases; ++checks;
        if (got != (pos == size ? 1 : 0)) fail("eof_policy", std::to_string(size) + "/" + std::to_string(pos));
    }

    // 4. alignment selection
    for (int fd = 0; fd < 2; ++fd)
    for (int64_t blk : (int64_t[]){0, 512, 4096, 65536}) {
        int64_t out[1] = {-1};
        const int64_t got = read_align(fd, blk, (uint64_t) (uintptr_t) out);
        int64_t want = 1;
        if (fd && blk > 0) want = blk;
        ++cases; ++checks;
        if (got != want) fail("read_align", std::to_string(fd) + "/" + std::to_string(blk));
    }

    // 5. direct-read loop against the native transcription, with scripted errnos
    std::mt19937 rng(464001);
    for (int round = 0; round < 300; ++round) {
        const int64_t size = (int64_t) (rng() % 4096);
        const int64_t pos = (int64_t) (rng() % (uint64_t) (size + 8));
        const int64_t len = (int64_t) (rng() % 4096);
        std::vector<int> errs;
        const int n = 1 + (int) (rng() % 6);
        for (int i = 0; i < n; ++i) {
            const int r = (int) (rng() % 100);
            errs.push_back(r < 80 ? ERR_OK : r < 88 ? ERR_EINTR : r < 94 ? ERR_EFAULT
                             : r < 97 ? ERR_EINVAL : ERR_OTHER);
        }
        int64_t delivered_native = 0;
        std::string why;
        const int native_rc = native_direct(errs, size, pos, len, delivered_native, why);

        // Goose arm: same script, decisions from the module
        int64_t delivered = 0;
        int rc = 0;
        size_t k = 0;
        while (delivered < len) {
            const int e = k < errs.size() ? errs[k++] : ERR_OK;
            int64_t act[1] = {-1};
            err_action(e, (uint64_t) (uintptr_t) act);
            if (act[0] == 0) { rc = 0; continue; }
            if (act[0] == 1) { rc = 1; break; }
            if (e != ERR_OK) { rc = 2; break; }
            const int64_t left = size - (pos + delivered);
            if (left <= 0) {
                int64_t eo[1] = {-1};
                eof_policy(size, pos + delivered, (uint64_t) (uintptr_t) eo);
                if (eo[0]) { delivered = len; rc = 3; break; }
                rc = 2; break;
            }
            const int64_t nn = std::min(left, len - delivered);
            delivered += nn;
        }
        ++cases; ++checks;
        if (rc != native_rc) fail("direct-rc", why + " vs goose " + std::to_string(rc));
        ++checks;
        if (delivered != delivered_native) fail("direct-bytes", "delivery mismatch");
    }

    // 6. source selection and mapping offset
    for (int mm = 0; mm < 2; ++mm)
    for (int hmap = 0; hmap < 2; ++hmap) {
        int64_t out[1] = {-1};
        const int64_t kind = src_kind(mm, hmap, (uint64_t) (uintptr_t) out);
        const int64_t want = (mm && hmap) ? 1 : 0;
        ++cases; ++checks;
        if (kind != want) fail("src_kind", std::to_string(mm) + "/" + std::to_string(hmap));
        for (int64_t woffs : (int64_t[]){0, 1000, 4096})
        for (int64_t inner : (int64_t[]){0, 7}) {
            int64_t mo[1] = {-1};
            map_offset(kind, woffs, inner, (uint64_t) (uintptr_t) mo);
            const int64_t want_off = kind ? woffs + inner : 0;
            ++cases; ++checks;
            if (mo[0] != want_off) fail("map_offset", std::to_string(kind) + "/" + std::to_string(woffs));
        }
    }

    printf("FG464_READPATH cases=%d values=%d failures=%d\n", cases, checks, fails);
    return fails ? 1 : 0;
}
