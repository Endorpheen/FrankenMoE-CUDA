// Differential gate for P8-09 S5 (percent decoding and the query transport).
//
// Native arm:
//   - decode_path_component is transcribed statement by statement from
//     tools/server/server-http.cpp 488-506 (it is a static helper behind a
//     lambda and cannot be called);
//   - the query parse is the real vendored httplib::detail::parse_query_text,
//     compiled from vendor/cpp-httplib/httplib.cpp, so the split/trim/dedupe/
//     divide and the form decode are native code, not a reading of it;
//   - get_params (512-525) and build_query_string (531-541) are transcribed
//     over the real httplib::Params container;
//   - the encoder is the real httplib::encode_query_component.
//
// Port arm: the qry_ exports of source/query-transport.goose.
#include "qry.h"

#include <cpp-httplib/httplib.h>

#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

static int cases = 0;
static int values = 0;
static int fails = 0;

// Printable form of a byte string, NULs and high bytes included.
static std::string show(const std::string & s) {
    std::string out;

    for (unsigned char c : s) {
        if (c >= 32 && c < 127 && c != '\\') {
            out += (char) c;
        } else {
            char buf[8];
            snprintf(buf, sizeof buf, "\\x%02X", c);
            out += buf;
        }
    }
    return out;
}

static void eq_int(const std::string & what, const std::string & name, int64_t want, int64_t got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-14s %-46s want=%lld got=%lld\n", what.c_str(), name.c_str(),
               (long long) want, (long long) got);
    }
}

static void eq_str(const std::string & what, const std::string & name, const std::string & want,
                   const std::string & got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-14s %-46s want=[%s] got=[%s]\n", what.c_str(), name.c_str(),
               show(want).c_str(), show(got).c_str());
    }
}

static sl_u8 SL(const std::string & s) {
    return sl_u8{(uint8_t *) (void *) s.data(), (int64_t) s.size()};
}

// ---- the native arm -------------------------------------------------------

// decode_path_component, server-http.cpp 488-506.
static std::string decode_path_native(const std::string & in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); i++) {
        if (in[i] == '%' && i + 2 < in.size()) {
            auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int hi = hex(in[i + 1]);
            int lo = hex(in[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(char((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.push_back(in[i]);
    }
    return out;
}

// get_params, server-http.cpp 512-525.
static std::map<std::string, std::string> get_params_native(const httplib::Params & query,
                                                            const httplib::Params & path) {
    std::map<std::string, std::string> params;

    for (const auto & [key, value] : query) {
        params[key] = value;
    }
    for (const auto & [key, value] : path) {
        params[key] = decode_path_native(value);
    }
    return params;
}

// build_query_string, server-http.cpp 531-541.
static std::string build_query_native(const httplib::Params & params) {
    std::string qs;

    for (const auto & [key, value] : params) {
        if (!qs.empty()) {
            qs += '&';
        }
        qs += httplib::encode_query_component(key) + "=" + httplib::encode_query_component(value);
    }
    return qs;
}

// ---- the port arm ---------------------------------------------------------

// One packed pair table, the layout qry_parse fills.
struct table {
    std::vector<uint8_t> kb = std::vector<uint8_t>(8192);
    std::vector<uint8_t> vb = std::vector<uint8_t>(8192);
    std::vector<int64_t> ko = std::vector<int64_t>(257, 0);
    std::vector<int64_t> vo = std::vector<int64_t>(257, 0);
    std::vector<int64_t> tok = std::vector<int64_t>(512, 0);
};

// Pairs the port parsed, rendered exactly like the native container would.
static std::vector<std::pair<std::string, std::string>> port_pairs(const table & t, int64_t np) {
    std::vector<std::pair<std::string, std::string>> out;

    for (int64_t i = 0; i < np; i++) {
        out.emplace_back(std::string((const char *) t.kb.data() + t.ko[i], (size_t) (t.ko[i + 1] - t.ko[i])),
                         std::string((const char *) t.vb.data() + t.vo[i], (size_t) (t.vo[i + 1] - t.vo[i])));
    }
    return out;
}

static std::string dump_pairs(const std::vector<std::pair<std::string, std::string>> & pairs) {
    std::string s;

    for (const auto & [k, v] : pairs) {
        if (!s.empty()) {
            s += " | ";
        }
        s += "[" + show(k) + "]=[" + show(v) + "]";
    }
    return s;
}

int main(int argc, char ** argv) {
    goose_init(argc, argv);

    // ---- 1. percent decoding of a path component --------------------------
    const std::vector<std::string> path_in = {
        "conv%3A%3Amodel",
        "%41%42C",
        "abc",
        "",
        "%",
        "%4",
        "a%2",
        "100%",
        "%zz",
        "%4z",
        "%z4",
        "a%00b",
        "%C3%A9",
        "%c3%a9",
        "%2525",
        "a+b",
        "%2B",
        "%20",
        "trailing%",
        "%GG%41",
        "%ff",
        "%FFFF",
        "%%41",
        "%41%42%43%44%45",
    };

    for (const auto & s : path_in) {
        cases++;
        uint8_t buf[256];
        const int64_t n = qry_decode_path(SL(s), sl_u8{buf, (int64_t) sizeof buf});
        eq_str("path decode", "in=[" + show(s) + "]", decode_path_native(s),
               n < 0 ? "<error>" : std::string((const char *) buf, (size_t) n));
    }

    // ---- 2. form decoding of a query component ----------------------------
    const std::vector<std::string> query_in = {
        "a+b", "a%2Bb", "+", "%2B", " ", "%20", "a b", "%", "%4", "%zz",
        "%C3%A9", "a%00b", "name=value", "&", "=", "%26", "%3D", "",
    };

    for (const auto & s : query_in) {
        cases++;
        uint8_t buf[256];
        const int64_t n = qry_decode_query(SL(s), sl_u8{buf, (int64_t) sizeof buf});
        eq_str("query decode", "in=[" + show(s) + "]", httplib::decode_query_component(s),
               n < 0 ? "<error>" : std::string((const char *) buf, (size_t) n));
    }

    // ---- 3. the encoder ---------------------------------------------------
    const std::vector<std::string> enc_in = {
        "abcXYZ019", "-._~", "!", "$", "'", "(", ")", "*", ",", ";", ":", "@",
        "/", "?", " ", "+", "&", "=", "%", "#", "[", "]", "\"", "<", ">",
        "\xC3\xA9", "\xFF", "\x01", "a b+c&d=e/f?g", "",
    };

    for (const auto & s : enc_in) {
        cases++;
        uint8_t buf[256];
        const int64_t n = qry_encode_component(SL(s), sl_u8{buf, (int64_t) sizeof buf});
        eq_str("encode", "in=[" + show(s) + "]", httplib::encode_query_component(s),
               n < 0 ? "<error>" : std::string((const char *) buf, (size_t) n));
    }

    // ---- 4. parse one query text -----------------------------------------
    const std::vector<std::string> queries = {
        "a=1&b=2",
        "b=2&a=1",
        "",
        "a",
        "a=",
        "=",
        "=v",
        "a=1&",
        "&a=1",
        "&&",
        "&",
        "a=1&&b=2",
        " a = 1 ",
        "\ta=1\t&\tb=2\t",
        "a%20b=c+d",
        "a=1&a=2",
        "a=1&a=1",
        "a=1&%61=%31",
        "a=1&a=%31",
        "a=%41%42&b=%43",
        "x=%zz&y=%4",
        "a=1&b=2&a=3",
        "k+=v+",
        "%FF=1",
        "a=%00b",
        "a=1&amp=2",
        "a%2Bb=c",
        "=1&=1",
        "=1&a=2&=1",
        "a=1&b=2&c=3&d=4&e=5&f=6&g=7&h=8",
        "flag&other=1",
        "a==b",
        "a=b=c",
        " a & b = c ",
        "a=%E2%9C%93",
        "a+b=c+d&e+f=g+h",
        "%%=1",
        "a=1&&&b=2&&",
    };

    for (const auto & q : queries) {
        cases++;
        httplib::Params want_q;
        httplib::detail::parse_query_text(q, want_q);

        table t;
        int64_t st[1] = {0};
        const int64_t rc = qry_parse(SL(q), sl_u8{t.kb.data(), (int64_t) t.kb.size()},
                                     sl_i64{t.ko.data(), (int64_t) t.ko.size()},
                                     sl_u8{t.vb.data(), (int64_t) t.vb.size()},
                                     sl_i64{t.vo.data(), (int64_t) t.vo.size()},
                                     sl_i64{t.tok.data(), (int64_t) t.tok.size()}, 256,
                                     sl_i64{st, 1});
        eq_int("parse rc", "q=[" + show(q) + "]", 0, rc);

        std::vector<std::pair<std::string, std::string>> want;

        for (const auto & [k, v] : want_q) {
            want.emplace_back(k, v);
        }
        eq_int("parse count", "q=[" + show(q) + "]", (int64_t) want.size(), st[0]);
        eq_str("parse pairs", "q=[" + show(q) + "]", dump_pairs(want), dump_pairs(port_pairs(t, st[0])));

        uint8_t buf[8192];
        const int64_t nb = qry_build(st[0], sl_u8{t.kb.data(), (int64_t) t.kb.size()},
                                     sl_i64{t.ko.data(), (int64_t) t.ko.size()},
                                     sl_u8{t.vb.data(), (int64_t) t.vb.size()},
                                     sl_i64{t.vo.data(), (int64_t) t.vo.size()},
                                     sl_u8{buf, (int64_t) sizeof buf});
        eq_str("build", "q=[" + show(q) + "]", build_query_native(want_q),
               nb < 0 ? "<error>" : std::string((const char *) buf, (size_t) nb));
    }

    // ---- 5. append a second text, the way a form body follows the URL ------
    {
        const std::vector<std::pair<std::string, std::string>> two = {
            {"a=1&b=2", "c=3&a=4"},
            {"a=1", "a=1"},
            {"", "x=%41"},
            {"x=1", ""},
            {"a=1&a=1", "a=1"},
        };

        for (const auto & [first, second] : two) {
            cases++;
            httplib::Params want_q;
            httplib::detail::parse_query_text(first, want_q);
            httplib::detail::parse_query_text(second, want_q);

            table t;
            int64_t st[1] = {0};
            int64_t rc = qry_parse(SL(first), sl_u8{t.kb.data(), (int64_t) t.kb.size()},
                                   sl_i64{t.ko.data(), (int64_t) t.ko.size()},
                                   sl_u8{t.vb.data(), (int64_t) t.vb.size()},
                                   sl_i64{t.vo.data(), (int64_t) t.vo.size()},
                                   sl_i64{t.tok.data(), (int64_t) t.tok.size()}, 256,
                                   sl_i64{st, 1});
            rc |= qry_parse(SL(second), sl_u8{t.kb.data(), (int64_t) t.kb.size()},
                            sl_i64{t.ko.data(), (int64_t) t.ko.size()},
                            sl_u8{t.vb.data(), (int64_t) t.vb.size()},
                            sl_i64{t.vo.data(), (int64_t) t.vo.size()},
                            sl_i64{t.tok.data(), (int64_t) t.tok.size()}, 256,
                            sl_i64{st, 1});
            eq_int("append rc", first + " + " + second, 0, rc);

            std::vector<std::pair<std::string, std::string>> want;

            for (const auto & [k, v] : want_q) {
                want.emplace_back(k, v);
            }
            eq_int("append count", first + " + " + second, (int64_t) want.size(), st[0]);
            eq_str("append pairs", first + " + " + second, dump_pairs(want),
                   dump_pairs(port_pairs(t, st[0])));
        }
    }

    // ---- 6. get_params: query then path, sorted map ------------------------
    struct params_case {
        std::string                          query;
        std::vector<std::pair<std::string, std::string>> path;
    };
    const std::vector<params_case> params_cases = {
        {"a=1&b=2", {}},
        {"a=1&b=2", {{"id", "conv%3A%3Amodel"}}},
        {"a=1&b=2", {{"a", "path"}}},
        {"a=1&a=2", {{"a", "p"}}},
        {"b=2&a=1", {{"c", "x+y"}, {"a", "z"}}},
        {"", {{"id", "conv%3A%3Amodel"}}},
        {"x=%41&y=%42", {{"x", "%41%42"}}},
        {"a=1", {{"a", "100%"}, {"a", "second"}}},
        {"a=1", {{"p", "x%2By+z"}, {"q", "a%20b"}}},
        {"%FF=1&%C3%A9=2", {{"\xC3\xA9", "3"}}},
        {"a=1&b=2&c=3", {{"b", "b1"}, {"d", "d1"}, {"a", "a1"}}},
        {"z=26&y=25&x=24", {{"m", "13"}}},
        {"a=%00", {{"b", "%00"}}},
        {"a=1", {{"", "empty-key"}}},
        {"=1&=2", {{"=", "="}}},
    };

    for (const auto & pc : params_cases) {
        cases++;
        httplib::Params want_q;
        httplib::detail::parse_query_text(pc.query, want_q);
        httplib::Params want_p;

        for (const auto & [k, v] : pc.path) {
            want_p.emplace(k, v);
        }
        const auto want = get_params_native(want_q, want_p);

        // the query side through qry_parse
        table qt;
        int64_t st[1] = {0};
        qry_parse(SL(pc.query), sl_u8{qt.kb.data(), (int64_t) qt.kb.size()},
                  sl_i64{qt.ko.data(), (int64_t) qt.ko.size()},
                  sl_u8{qt.vb.data(), (int64_t) qt.vb.size()},
                  sl_i64{qt.vo.data(), (int64_t) qt.vo.size()},
                  sl_i64{qt.tok.data(), (int64_t) qt.tok.size()}, 256, sl_i64{st, 1});

        // the path side packed by the driver, values raw
        std::vector<uint8_t> pkb, pvb;
        std::vector<int64_t> pko{0}, pvo{0};
        std::string pkb_s, pvb_s;

        for (const auto & [k, v] : pc.path) {
            pkb_s += k;
            pvb_s += v;
            pko.push_back((int64_t) pkb_s.size());
            pvo.push_back((int64_t) pvb_s.size());
        }

        std::vector<uint8_t> okb(8192), ovb(8192);
        std::vector<int64_t> oko(512, 0), ovo(512, 0);
        int64_t onp[1] = {0};
        const int64_t rc = qry_params(
            st[0], sl_u8{qt.kb.data(), (int64_t) qt.kb.size()},
            sl_i64{qt.ko.data(), (int64_t) qt.ko.size()},
            sl_u8{qt.vb.data(), (int64_t) qt.vb.size()},
            sl_i64{qt.vo.data(), (int64_t) qt.vo.size()},
            (int64_t) pc.path.size(), SL(pkb_s), sl_i64{pko.data(), (int64_t) pko.size()},
            SL(pvb_s), sl_i64{pvo.data(), (int64_t) pvo.size()},
            sl_u8{okb.data(), (int64_t) okb.size()}, sl_i64{oko.data(), (int64_t) oko.size()},
            sl_u8{ovb.data(), (int64_t) ovb.size()}, sl_i64{ovo.data(), (int64_t) ovo.size()},
            sl_i64{onp, 1});
        eq_int("params rc", "q=[" + show(pc.query) + "]", 0, rc);

        std::string want_dump;
        for (const auto & [k, v] : want) {
            if (!want_dump.empty()) want_dump += " | ";
            want_dump += "[" + show(k) + "]=[" + show(v) + "]";
        }
        std::string got_dump;
        for (int64_t i = 0; i < onp[0]; i++) {
            if (!got_dump.empty()) got_dump += " | ";
            got_dump += "[" + show(std::string((const char *) okb.data() + oko[i], (size_t) (oko[i + 1] - oko[i]))) + "]=";
            got_dump += "[" + show(std::string((const char *) ovb.data() + ovo[i], (size_t) (ovo[i + 1] - ovo[i]))) + "]";
        }
        eq_int("params count", "q=[" + show(pc.query) + "]", (int64_t) want.size(), onp[0]);
        eq_str("params map", "q=[" + show(pc.query) + "]", want_dump, got_dump);
    }

    // ---- 7. capacity contract --------------------------------------------
    {
        cases++;
        table t;
        int64_t st[1] = {0};
        // three pairs into a two-pair table
        const int64_t rc = qry_parse(SL("a=1&b=2&c=3"),
                                     sl_u8{t.kb.data(), (int64_t) t.kb.size()},
                                     sl_i64{t.ko.data(), 3},
                                     sl_u8{t.vb.data(), (int64_t) t.vb.size()},
                                     sl_i64{t.vo.data(), 3},
                                     sl_i64{t.tok.data(), (int64_t) t.tok.size()}, 2,
                                     sl_i64{st, 1});
        eq_int("capacity", "three pairs, two slots", -1, rc);
    }
    {
        cases++;
        table t;
        int64_t st[1] = {0};
        const int64_t rc = qry_parse(SL("a=1"), sl_u8{t.kb.data(), 0},
                                     sl_i64{t.ko.data(), (int64_t) t.ko.size()},
                                     sl_u8{t.vb.data(), (int64_t) t.vb.size()},
                                     sl_i64{t.vo.data(), (int64_t) t.vo.size()},
                                     sl_i64{t.tok.data(), (int64_t) t.tok.size()}, 256,
                                     sl_i64{st, 1});
        eq_int("capacity", "key blob too small", -1, rc);
    }
    {
        cases++;
        uint8_t buf[2];
        const int64_t rc = qry_encode_component(SL("abc"), sl_u8{buf, 2});
        eq_int("capacity", "encoder buffer too small", -1, rc);
    }

    printf("FQRY cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
