// Differential gate for P8-09 S3 (the embedded asset ETag).
//
// Native arm 1: fnv_hash from embed.cpp, included with its main renamed.
// Native arm 2: the generated ui.cpp for the synthetic bundle; every asset's
// etag field is the literal the real generator wrote, i.e. the exact bytes the
// server copies into the ETag header.
// Port arm: the et_ exports of source/asset-etag.goose.
//
// The check is the whole identity in one line: et_text(et_hash(bytes)) must
// equal the generated literal, for every asset and for synthetic buffers that
// stress the width of the hex field (leading zeros, the top nibble, the
// empty-input offset basis).
#include "etag.h"

#include "ui.h"

#define main embed_tool_main
#include "embed.cpp"
#undef main

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int cases = 0;
static int values = 0;
static int fails = 0;

static void eq_str(const char * what, const std::string & name, const std::string & want,
                   const std::string & got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-18s %-34s want=%s got=%s\n", what, name.c_str(), want.c_str(), got.c_str());
    }
}

static void eq_u64(const char * what, const std::string & name, uint64_t want, int64_t got_signed) {
    values++;
    const uint64_t got = (uint64_t) got_signed;
    if (want != got) {
        fails++;
        printf("FAIL %-18s %-34s want=%016llx got=%016llx\n", what, name.c_str(),
               (unsigned long long) want, (unsigned long long) got);
    }
}

static sl_u8 SL(const std::string & s) {
    return sl_u8{(uint8_t *) (void *) s.data(), (int64_t) s.size()};
}

static sl_u8 SLB(const std::vector<uint8_t> & b) {
    return sl_u8{(uint8_t *) (void *) b.data(), (int64_t) b.size()};
}

// The native literal, formatted the way embed.cpp formats it.
static std::string native_etag(uint64_t h) {
    char tmp[64];
    snprintf(tmp, sizeof tmp, "\"0x%016llx\"", (unsigned long long) h);
    return std::string(tmp);
}

static int64_t go_hash(const std::vector<uint8_t> & b) {
    int64_t out[1] = {0};
    sl_i64 o{out, 1};
    et_hash(SLB(b), o);
    return out[0];
}

static std::string go_text(int64_t h) {
    uint8_t buf[64];
    sl_u8 out{buf, (int64_t) sizeof buf};
    const int64_t n = et_text(h, out);
    if (n < 0) {
        return "<bad text>";
    }
    return std::string((const char *) buf, (size_t) n);
}

int main() {
    goose_init(0, nullptr);

    // every asset of the real generated table
    for (const auto & a : llama_ui_get_assets()) {
        cases++;
        const std::vector<uint8_t> bytes(a.data, a.data + a.size);
        const uint64_t want = fnv_hash(bytes.data(), bytes.size());
        const int64_t got = go_hash(bytes);
        eq_u64("hash vs embed", a.name, want, got);
        eq_str("etag vs ui.cpp", a.name, a.etag, go_text(got));
        eq_str("etag vs format", a.name, native_etag(want), go_text(got));
    }

    // synthetic buffers: the hex field width and the offset basis
    std::vector<std::vector<uint8_t>> blobs;
    for (size_t n : {1u, 2u, 3u, 7u, 8u, 9u, 15u, 16u, 17u, 63u, 64u, 65u, 255u, 1024u}) {
        std::vector<uint8_t> b(n);
        for (size_t i = 0; i < n; i++) {
            b[i] = (uint8_t) ((i * 37 + n * 11) & 0xFF);
        }
        blobs.push_back(b);
    }
    blobs.push_back(std::vector<uint8_t>{});                 // the offset basis
    blobs.push_back(std::vector<uint8_t>{0x00, 0x00, 0x00});  // zero bytes only
    blobs.push_back(std::vector<uint8_t>{0xFF, 0xFF, 0xFF});  // all bits set

    int bi = 0;
    for (const auto & b : blobs) {
        cases++;
        const std::string name = "blob-" + std::to_string(bi++);
        const uint64_t want = fnv_hash(b.data(), b.size());
        const int64_t got = go_hash(b);
        eq_u64("hash blob", name, want, got);
        eq_str("etag blob", name, native_etag(want), go_text(got));
    }

    // the offset basis itself, so a wrong constant cannot hide behind a file
    cases++;
    {
        int64_t out[1] = {0};
        sl_i64 o{out, 1};
        et_hash(SL(""), o);
        eq_u64("empty basis", "empty", 0xcbf29ce484222325ULL, out[0]);
        eq_str("empty text", "empty", "\"0xcbf29ce484222325\"", go_text(out[0]));
    }

    printf("FGWEB_ETAG cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
