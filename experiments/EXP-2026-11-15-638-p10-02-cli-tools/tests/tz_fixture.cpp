// Differential gate for P10-02 S2 (llama-tokenize CLI decisions).
//
// Native arm: a transcription of tools/tokenize/tokenize.cpp lines 106-215,
// because the tool's main cannot be linked without a vocabulary. The
// transcription keeps the native printf formats ("%d", "%6d -> '...'",
// "Total number of tokens: %zu") and the same branch order; the gate feeds it
// the same synthetic token ids and pieces as the Goose port.
//
// Port arm: the tz_ exports of source/tokenize-cli.goose. Nothing here loads a
// model or tokenizes: the token list is the input.
#include "tz.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int cases = 0;
static int values = 0;
static int fails = 0;

static sl_u8 SL(const std::string & s) {
    return sl_u8{(uint8_t *) (void *) s.data(), (int64_t) s.size()};
}

static void fail(const char * name, const std::string & want, const std::string & got) {
    fails++;
    printf("FAIL %-30s want=[%s] got=[%s]\n", name, want.c_str(), got.c_str());
}

static void eq_str(const char * name, const std::string & want, const std::string & got) {
    values++;
    if (want != got) {
        fail(name, want, got);
    }
}

static void eq_int(const char * name, int64_t want, int64_t got) {
    values++;
    if (want != got) {
        fail(name, std::to_string(want), std::to_string(got));
    }
}

// ── native transcription (tokenize.cpp 187-215) ────────────────────────────
static std::string nat_tz_report(const std::vector<int> & tokens, const std::vector<std::string> & pieces,
                                 const std::vector<int> & invalid, bool ids, bool count) {
    std::string out;
    char        buf[64];
    if (ids) {
        out += "[";
    }
    for (size_t i = 0; i < tokens.size(); i++) {
        if (ids) {
            if (i > 0) {
                out += ", ";
            }
            snprintf(buf, sizeof buf, "%d", tokens[i]);
            out += buf;
        } else {
            snprintf(buf, sizeof buf, "%6d -> '", tokens[i]);
            out += buf;
            out += pieces[i];
            if (invalid[i]) {
                out += "' (utf-8 decode failure)\n";
            } else {
                out += "'\n";
            }
        }
    }
    if (ids) {
        out += "]\n";
    }
    if (count) {
        snprintf(buf, sizeof buf, "Total number of tokens: %zu\n", tokens.size());
        out += buf;
    }
    return out;
}

static std::string port_tz_report(const std::vector<int> & tokens, const std::vector<std::string> & pieces,
                                  const std::vector<int> & invalid, int ids, int count) {
    std::vector<int64_t> tok(tokens.begin(), tokens.end());
    std::vector<int64_t> off, len, inv(invalid.begin(), invalid.end());
    std::string          blob;
    for (const auto & p : pieces) {
        off.push_back((int64_t) blob.size());
        len.push_back((int64_t) p.size());
        blob += p;
    }
    if (tok.empty()) {
        off.push_back(0);
        len.push_back(0);
        inv.push_back(0);
    }
    char  b[4096];
    memset(b, 0x5a, sizeof b);
    int64_t n = tz_report(sl_i64{tok.data(), (int64_t) tok.size()},
                          sl_u8{(uint8_t *) (void *) blob.data(), (int64_t) blob.size()},
                          sl_i64{off.data(), (int64_t) off.size()},
                          sl_i64{len.data(), (int64_t) len.size()},
                          sl_i64{inv.data(), (int64_t) inv.size()}, ids, count,
                          sl_u8{(uint8_t *) b, (int64_t) sizeof b});
    if (n < 0) {
        return "!";
    }
    return std::string(b, (size_t) n);
}

int main() {
    goose_init(0, nullptr);
    char b[2048];

    // ── prompt source, escapes and add_bos (106-115, 131/175, 180-182) ─────
    struct { int stdin_, file, prompt, want; } src[] = {
        {1, 1, 1, 0}, {1, 0, 0, 0}, {0, 1, 1, 1}, {0, 1, 0, 1}, {0, 0, 1, 2}, {0, 0, 0, -1},
    };
    for (auto & s : src) {
        cases++;
        eq_int("tz_source", s.want, tz_source(s.stdin_, s.file, s.prompt));
    }
    for (int source = -1; source <= 3; source++) {
        for (int esc = 0; esc < 2; esc++) {
            cases++;
            int64_t want = (esc != 0 && (source == 0 || source == 1)) ? 1 : 0;
            eq_int("tz_escape_needed", want, tz_escape_needed(source, esc));
        }
    }
    for (int wants = 0; wants < 2; wants++) {
        for (int no = 0; no < 2; no++) {
            cases++;
            eq_int("tz_add_bos", (wants != 0 && no == 0) ? 1 : 0, tz_add_bos(wants, no));
        }
    }

    // ── output loop (187-215) ─────────────────────────────────────────────
    struct R {
        std::vector<int>         tokens;
        std::vector<std::string> pieces;
        std::vector<int>         invalid;
        bool                     ids;
        bool                     count;
        const char *             name;
    };
    std::vector<R> rs = {
        {{}, {}, {}, true, false, "ids_empty"},
        {{}, {}, {}, false, true, "pieces_empty_count"},
        {{}, {}, {}, true, true, "ids_empty_count"},
        {{1, 2, 3}, {"a", "b", "c"}, {0, 0, 0}, true, false, "ids_three"},
        {{1, 2, 3}, {"a", "b", "c"}, {0, 0, 0}, true, true, "ids_three_count"},
        {{-1, 0, 32000}, {"<s>", "", "</s>"}, {0, 0, 0}, true, false, "ids_signed"},
        {{-1, 0, 32000}, {"<s>", "", "</s>"}, {0, 0, 0}, false, false, "pieces_signed"},
        {{1, 2}, {"Hello", " world"}, {0, 0}, false, false, "pieces_words"},
        {{1, 2}, {"Hello", " world"}, {0, 0}, false, true, "pieces_words_count"},
        {{7}, {"\n"}, {0}, false, false, "piece_newline"},
        {{7}, {"a'b"}, {0}, false, false, "piece_quote"},
        {{7}, {"\xc3\xa9"}, {0}, false, false, "piece_utf8"},
        {{7}, {"<20>"}, {1}, false, false, "piece_invalid"},
        {{1234567}, {"x"}, {0}, false, false, "piece_wide_id"},
        {{-12}, {"x"}, {0}, false, false, "piece_negative"},
        {{999999}, {"x"}, {0}, false, true, "piece_count_six"},
    };
    for (auto & r : rs) {
        cases++;
        std::string want = nat_tz_report(r.tokens, r.pieces, r.invalid, r.ids, r.count);
        std::string got  = port_tz_report(r.tokens, r.pieces, r.invalid, r.ids ? 1 : 0, r.count ? 1 : 0);
        eq_str(r.name, want, got);
    }

    // ── single-piece and single-list helpers ──────────────────────────────
    {
        std::vector<int> t = {1, 2, 3};
        std::vector<int64_t> ti(t.begin(), t.end());
        memset(b, 0, sizeof b);
        int64_t n = tz_fmt_ids(sl_i64{ti.data(), (int64_t) ti.size()}, sl_u8{(uint8_t *) b, sizeof b});
        cases++;
        eq_str("tz_fmt_ids", "[1, 2, 3]\n", std::string(b, (size_t) n));

        std::vector<int64_t> te;
        memset(b, 0, sizeof b);
        n = tz_fmt_ids(sl_i64{nullptr, 0}, sl_u8{(uint8_t *) b, sizeof b});
        cases++;
        eq_str("tz_fmt_ids_empty", "[]\n", std::string(b, (size_t) n));

        memset(b, 0, sizeof b);
        n = tz_fmt_piece(42, SL("hi"), 0, sl_u8{(uint8_t *) b, sizeof b});
        cases++;
        eq_str("tz_fmt_piece", "    42 -> 'hi'\n", std::string(b, (size_t) n));

        memset(b, 0, sizeof b);
        n = tz_fmt_piece(-3, SL(""), 1, sl_u8{(uint8_t *) b, sizeof b});
        cases++;
        eq_str("tz_fmt_piece_invalid", "    -3 -> '' (utf-8 decode failure)\n", std::string(b, (size_t) n));
    }

    printf("FG637_TZ cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
