// Differential fixture for the Goose shard naming and validation (EXP459).
//
// Right arm: llama_split_path transcribed with snprintf ("%s-%05d-of-%05d.gguf")
// plus the loader's split checks. Left arm: the Goose module.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
int64_t fg459_digits(int64_t v);
int64_t fg459_width(int64_t v);
int64_t fg459_split_path(const char * pre, int64_t pn, int64_t no, int64_t count, char * out, int64_t cap);
int64_t fg459_first(int64_t i);
int64_t fg459_count_ok(int64_t a, int64_t b);
int64_t fg459_index_ok(int64_t a, int64_t b);
int64_t fg459_tensor_ok(int64_t a, int64_t b);
int64_t fg459_magic(int64_t v);
int64_t fg459_version(int64_t v);
void    fg459_st_u8(uint64_t p, uint64_t o, uint32_t v);
void    fg459_st_i64(uint64_t p, uint64_t o, int64_t v);
}

void fg459_st_u8(uint64_t p, uint64_t o, uint32_t v) { ((uint8_t *) (uintptr_t) p)[o] = (uint8_t) v; }
void fg459_st_i64(uint64_t p, uint64_t o, int64_t v) { *(int64_t *) ((char *) (uintptr_t) p + o) = v; }

static int cases = 0, checks = 0, fails = 0;

static void fail(const char * what, const std::string & detail) {
    ++fails;
    if (fails <= 10) printf("FAIL %s: %s\n", what, detail.c_str());
}

// llama_split_path
static int32_t native_split_path(char * out, size_t maxlen, const char * prefix, int32_t no, int32_t count) {
    const int written = snprintf(out, maxlen, "%s-%05d-of-%05d.gguf", prefix, no + 1, count);
    if (written < 0 || (size_t) written >= maxlen) return 0;
    return written;
}

int main(void) {
    // 1. decimal width: %05d pads to five but never truncates
    const int64_t values[] = {0, 1, 9, 10, 99, 100, 9999, 10000, 99999, 100000, 123456, 1000000};
    for (int64_t v : values) {
        ++cases; ++checks;
        const int64_t want_w = (v >= 100000) ? (int64_t) std::to_string(v).size() : 5;
        if (fg459_width(v) != want_w) fail("width", std::to_string(v));
        ++checks;
        if (fg459_digits(v) != (int64_t) std::to_string(v).size()) fail("digits", std::to_string(v));
    }

    // 2. shard paths against snprintf, including the truncation case
    const char * prefixes[] = {"model", "a/b/c", "", "very-long-name-with-dots.v1", "x"};
    const int32_t counts[] = {1, 2, 3, 10, 65535, 99999, 100000};
    for (const char * pre : prefixes)
    for (int32_t count : counts) {
        std::vector<std::string> got, want;
        for (int32_t no = 0; no < count && no < 4; ++no) {
            char ref[256] = {0};
            int32_t rn = native_split_path(ref, sizeof ref, pre, no, count);
            char out[256];
            memset(out, 0x7f, sizeof out);
            int64_t gn = fg459_split_path(pre, (int64_t) strlen(pre), no, count, out, (int64_t) sizeof out);
            ++cases; ++checks;
            if (gn != rn) fail("split-path-len", std::string(pre) + "/" + std::to_string(no) + "/" + std::to_string(count));
            ++checks;
            if (rn > 0 && memcmp(out, ref, (size_t) rn + 1) != 0)
                fail("split-path-bytes", std::string(ref) + " vs " + std::string(out, (size_t) (gn > 0 ? gn : 0)));
        }

        // capacity exactly equal to the result must be rejected (snprintf needs room for NUL)
        char ref[256] = {0};
        const int32_t rn = native_split_path(ref, sizeof ref, pre, 0, count);
        for (int delta = -1; delta <= 0; ++delta) {
            const int64_t cap = (int64_t) rn + delta;
            if (cap <= 0) continue;
            char out[256];
            memset(out, 0x55, sizeof out);
            const int64_t gn = fg459_split_path(pre, (int64_t) strlen(pre), 0, count, out, cap);
            ++cases; ++checks;
            if (gn != 0) fail("split-path-capacity", std::string(pre) + " cap=" + std::to_string(cap));
        }
    }

    // 3. loader validations
    for (int idx = -1; idx <= 2; ++idx) {
        ++cases; ++checks;
        if (fg459_first(idx) != (idx == 0 ? 1 : 0)) fail("first-shard", std::to_string(idx));
    }
    for (int64_t a = 1; a <= 5; ++a)
    for (int64_t b = 1; b <= 5; ++b) {
        ++cases; ++checks;
        if (fg459_count_ok(a, b) != (a == b ? 1 : 0)) fail("count", std::to_string(a) + "/" + std::to_string(b));
        ++checks;
        if (fg459_index_ok(a, b) != (a == b ? 1 : 0)) fail("index", std::to_string(a) + "/" + std::to_string(b));
        ++checks;
        if (fg459_tensor_ok(a, b) != (a == b ? 1 : 0)) fail("tensors", std::to_string(a) + "/" + std::to_string(b));
    }

    // 4. endian: only the little-endian magic and GGUF v2/v3 are accepted
    const int64_t magics[] = {1179993927, 0, -1, 0x46554747, 0x47475546, 1196444487};
    for (int64_t m : magics) {
        ++cases; ++checks;
        if (fg459_magic(m) != (m == 1179993927 ? 1 : 0)) fail("magic", std::to_string(m));
    }
    for (int64_t v = -1; v <= 5; ++v) {
        ++cases; ++checks;
        if (fg459_version(v) != ((v == 2 || v == 3) ? 1 : 0)) fail("version", std::to_string(v));
    }

    printf("FG459_SHARDS cases=%d values=%d failures=%d\n", cases, checks, fails);
    return fails ? 1 : 0;
}
