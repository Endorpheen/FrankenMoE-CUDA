// Unicode differential probe: dumps a fixed-record binary stream for the whole code-point range plus a byte-sequence
// corpus. Linked twice -- against the untouched flint libllama and against the accepted runtime -- the two outputs are
// compared with cmp, so every value is checked exactly, not sampled.
#include "unicode.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <unordered_set>

static void put32(FILE * f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void put_flags(FILE * f, const unicode_cpt_flags & fl) {
    static_assert(sizeof(unicode_cpt_flags) <= 8, "flags struct too large for the record");
    unsigned char buf[8] = {0};
    memcpy(buf, &fl, sizeof(fl));
    fwrite(buf, 1, 8, f);
}

int main(int argc, char ** argv) {
    if (argc != 3) { fprintf(stderr, "usage: %s out.bin byte_corpus.hex\n", argv[0]); return 2; }
    FILE * f = fopen(argv[1], "wb");
    if (!f) { fprintf(stderr, "output open failed\n"); return 1; }

    // the byte-token domain: exactly the code points unicode_byte_to_utf8 maps, so cpt_to_byte never throws
    std::vector<char> is_byte(0x110000, 0);
    std::unordered_set<std::string> byte_utf8;
    for (int ch = 0; ch < 256; ++ch) {
        const std::string u8 = unicode_byte_to_utf8((unsigned char) ch);
        byte_utf8.insert(u8);
        size_t off = 0;
        const uint32_t cpt = (uint32_t) unicode_cpt_from_utf8(u8, off);
        if (cpt < is_byte.size()) is_byte[cpt] = 1;
    }

    // 1) the whole code-point range: encoding, byte mapping, flags, lowercase, han
    for (uint32_t c = 0; c <= 0x10FFFF; ++c) {
        const std::string u8 = unicode_cpt_to_utf8(c);
        uint32_t rec[8] = {0};
        rec[0] = c;
        rec[1] = (uint32_t) u8.size();
        for (size_t i = 0; i < u8.size() && i < 4; ++i) rec[2 + i] = (uint8_t) u8[i];
        rec[6] = is_byte[c] ? (uint32_t) unicode_cpt_to_byte(c) : 0xFFFFFFFFu;
        rec[7] = (uint32_t) unicode_tolower(c);
        fwrite(rec, 4, 8, f);
        put_flags(f, unicode_cpt_flags_from_cpt(c));
        put32(f, (uint32_t) unicode_cpt_is_han(c));
        put_flags(f, unicode_cpt_flags_from_utf8(u8));
        put32(f, (uint32_t) unicode_len_utf8((char) (u8.empty() ? 0 : (uint8_t) u8[0])));
    }

    // 2) a byte corpus: valid encodings, truncated, overlong, surrogates, continuation-only, five-byte forms
    std::ifstream in(argv[2]);
    std::string hex;
    uint32_t n_case = 0;
    while (std::getline(in, hex)) {
        std::string bytes;
        for (size_t i = 0; i + 1 < hex.size(); i += 2) {
            bytes.push_back((char) (uint8_t) strtol(hex.substr(i, 2).c_str(), nullptr, 16));
        }
        try {
            size_t pos = 0;
            const uint32_t cpt = bytes.empty() ? 0xFFFFFFFFu : (uint32_t) unicode_cpt_from_utf8(bytes, pos);
            put32(f, n_case++);
            put32(f, cpt);
            put32(f, (uint32_t) pos);
            put_flags(f, unicode_cpt_flags_from_utf8(bytes));
            put32(f, byte_utf8.count(bytes) ? (uint32_t) unicode_utf8_to_byte(bytes) : 0xFFFFFFFFu);
            const std::vector<uint32_t> cpts = bytes.empty() ? std::vector<uint32_t>{} : unicode_cpts_from_utf8(bytes);
            put32(f, (uint32_t) cpts.size());
            for (uint32_t c : cpts) put32(f, c);
            put32(f, 0xFFFFFFFFu);
        } catch (...) {
            put32(f, n_case++);
            put32(f, 0xFFFFFF01u);
            put32(f, 0xFFFFFF01u);
            unsigned char z[8] = {0};
            fwrite(z, 1, 8, f);
            put32(f, 0xFFFFFFFFu); put32(f, 0); put32(f, 0xFFFFFFFFu);
        }
    }

    // 3) normalisation over single code points and a sample of pairs
    for (uint32_t c = 0; c <= 0x10FFFF; c += 7) {
        const std::vector<uint32_t> one = { c };
        for (const auto & r : { unicode_cpts_normalize_nfc(one), unicode_cpts_normalize_nfd(one) }) {
            put32(f, c);
            put32(f, (uint32_t) r.size());
            for (uint32_t v : r) put32(f, v);
            put32(f, 0xFFFFFFFEu);
        }
    }
    for (uint32_t c = 0; c <= 0x10FFFF; c += 1021) {
        const std::vector<uint32_t> two = { c + 1 < 0x110000 ? c + 1 : 0x41, 0x0301u };
        for (const auto & r : { unicode_cpts_normalize_nfc(two), unicode_cpts_normalize_nfd(two) }) {
            put32(f, c);
            put32(f, (uint32_t) r.size());
            for (uint32_t v : r) put32(f, v);
            put32(f, 0xFFFFFFFEu);
        }
    }

    // 4) single-byte mapping both ways
    for (uint32_t b = 0; b < 256; ++b) {
        const std::string u8 = unicode_byte_to_utf8((unsigned char) b);
        put32(f, b);
        put32(f, (uint32_t) u8.size());
        for (unsigned char ch : u8) put32(f, ch);
        put32(f, 0xFFFFFFFDu);
    }
    fclose(f);
    return 0;
}
