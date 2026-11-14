// Gate for P9-06 S4: the serialized PEG arena load and build_parse_params.
//
// The JSON fixtures are written in the native serialization shape
// (upstream/llama.cpp/common/peg-parser.cpp: serialized by
// serialize_parser_variant, read by from_json); the expected flat layout is the
// one the accepted peg_dump reads (goose/json/peg-dump.goose).
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <cstdlib>

#include "g.h"

extern "C" double gs_json_strtod(sl_u8 s) {
    std::string tmp((const char *) s.data, (size_t) s.len);
    return std::strtod(tmp.c_str(), nullptr);
}

static int cases = 0, checks = 0, fails = 0;

static void expect(const char * what, bool ok, const std::string & detail = "") {
    ++checks;
    if (!ok) {
        ++fails;
        printf("  FAIL %s%s\n", what, detail.empty() ? "" : (" : " + detail).c_str());
    }
}

static sl_u8 S(const std::string & s) {
    return sl_u8{(uint8_t *) s.data(), (int64_t) s.size()};
}

struct Arena {
    std::vector<int64_t> kind, cfirst, ccount, children, sidx, dstart, dcount, delims, mn, mx, sdir;
    std::vector<uint8_t> strs;
    std::vector<int64_t> out = std::vector<int64_t>(16, 0);
    std::vector<uint8_t> err = std::vector<uint8_t>(512, 0);

    Arena()
        : kind(128, 0), cfirst(128, 0), ccount(128, 0), children(512, 0), sidx(128, 0),
          dstart(128, 0), dcount(128, 0), delims(128, 0), mn(128, 0), mx(128, 0),
          sdir(512, 0), strs(8192, 0) {}

    int64_t load(const std::string & json) {
        sl_u8 j{S(json)};
        sl_i64 k{kind.data(), (int64_t) kind.size()};
        sl_i64 cf{cfirst.data(), (int64_t) cfirst.size()};
        sl_i64 cc{ccount.data(), (int64_t) ccount.size()};
        sl_i64 ch{children.data(), (int64_t) children.size()};
        sl_i64 sx{sidx.data(), (int64_t) sidx.size()};
        sl_i64 ds{dstart.data(), (int64_t) dstart.size()};
        sl_i64 dc{dcount.data(), (int64_t) dcount.size()};
        sl_i64 dl{delims.data(), (int64_t) delims.size()};
        sl_i64 mn_s{mn.data(), (int64_t) mn.size()};
        sl_i64 mx_s{mx.data(), (int64_t) mx.size()};
        sl_u8 st{strs.data(), (int64_t) strs.size()};
        sl_i64 sd{sdir.data(), (int64_t) sdir.size()};
        sl_i64 o{out.data(), (int64_t) out.size()};
        sl_u8 e{err.data(), (int64_t) err.size()};
        return arena_load(j, k, cf, cc, ch, sx, ds, dc, dl, mn_s, mx_s, st, sd, o, e);
    }

    std::string text() const { return std::string((const char *) err.data(), (size_t) out[3]); }

    std::string str(int64_t id) const {
        if (sidx[id] < 0) return "<none>";
        const int64_t o = sdir[2 * sidx[id]];
        const int64_t l = sdir[2 * sidx[id] + 1];
        return std::string((const char *) strs.data() + o, (size_t) l);
    }
};

// start + content(rest()) + end, as the content-only builder produces it.
static const char * JSON_CONTENT =
    "{\"parsers\":["
    "{\"type\":\"start\"},"
    "{\"type\":\"until\",\"delimiters\":[]},"
    "{\"type\":\"tag\",\"child\":1,\"tag\":\"content\"},"
    "{\"type\":\"end\"},"
    "{\"type\":\"sequence\",\"children\":[0,2,3]}"
    "],\"rules\":{},\"root\":4}";

// A tool-style arena: start + content(until "<tool_call>") + zero_or_more(tool) + end,
// tool = "<tool_call>" name(chars) args(until "</tool_call>") "</tool_call>".
static const char * JSON_TOOL =
    "{\"parsers\":["
    "{\"type\":\"start\"},"                                             // 0
    "{\"type\":\"literal\",\"literal\":\"<tool_call>\"},"               // 1
    "{\"type\":\"chars\",\"pattern\":\"[a-zA-Z_]\",\"ranges\":[{\"start\":65,\"end\":90}],"
    " \"negated\":false,\"min_count\":1,\"max_count\":64},"              // 2
    "{\"type\":\"tag\",\"child\":2,\"tag\":\"tool-name\"},"             // 3
    "{\"type\":\"until\",\"delimiters\":[\"</tool_call>\"]},"           // 4
    "{\"type\":\"literal\",\"literal\":\"</tool_call>\"},"              // 5
    "{\"type\":\"tag\",\"child\":5,\"tag\":\"tool-close\"},"            // 6
    "{\"type\":\"tag\",\"child\":1,\"tag\":\"tool-open\"},"             // 7
    "{\"type\":\"sequence\",\"children\":[7,3,4,6]},"                    // 8
    "{\"type\":\"tag\",\"child\":8,\"tag\":\"tool\"},"                  // 9
    "{\"type\":\"until\",\"delimiters\":[\"<tool_call>\"]},"            // 10
    "{\"type\":\"tag\",\"child\":10,\"tag\":\"content\"},"              // 11
    "{\"type\":\"repetition\",\"child\":9,\"min_count\":0,\"max_count\":-1}," // 12
    "{\"type\":\"end\"},"                                               // 13
    "{\"type\":\"sequence\",\"children\":[0,11,12,13]}"                  // 14
    "],\"rules\":{},\"root\":14}";

int main() {
    goose_init(0, 0);

    // ── 1: content-only arena (from_json: parsers/rules/root).
    {
        ++cases;
        printf("CASE %d content-only arena\n", cases);
        Arena a;
        int64_t s = a.load(JSON_CONTENT);
        expect("status", s == 0, std::to_string(s));
        expect("root", a.out[0] == 4, std::to_string(a.out[0]));
        expect("count", a.out[1] == 5, std::to_string(a.out[1]));
        expect("start kind", a.kind[0] == 1);
        expect("until kind", a.kind[1] == 13);
        expect("tag kind", a.kind[2] == 17);
        expect("end kind", a.kind[3] == 2);
        expect("seq kind", a.kind[4] == 4);
        expect("tag name", a.str(2) == "content", a.str(2));
        expect("seq children", a.ccount[4] == 3 && a.children[a.cfirst[4] + 0] == 0 &&
                                   a.children[a.cfirst[4] + 1] == 2 &&
                                   a.children[a.cfirst[4] + 2] == 3);
        expect("tag child", a.ccount[2] == 1 && a.children[a.cfirst[2]] == 1);
        expect("rest has no delimiters", a.dcount[1] == 0);
    }

    // ── 2: tool-style arena: tags, chars, until, repetition, sequence.
    {
        ++cases;
        printf("CASE %d tool-style arena\n", cases);
        Arena a;
        int64_t s = a.load(JSON_TOOL);
        expect("status", s == 0, std::to_string(s));
        expect("root", a.out[0] == 14);
        expect("count", a.out[1] == 15);
        expect("rep kind", a.kind[12] == 6);
        expect("rep child", a.children[a.cfirst[12]] == 9);
        expect("rep unbounded", a.mn[12] == 0 && a.mx[12] == -1,
               std::to_string(a.mn[12]) + "," + std::to_string(a.mx[12]));
        expect("chars kind", a.kind[2] == 11);
        expect("chars pattern", a.str(2) == "[a-zA-Z_]", a.str(2));
        expect("chars counts", a.mn[2] == 1 && a.mx[2] == 64);
        expect("tool-name tag", a.str(3) == "tool-name");
        expect("content tag", a.str(11) == "content");
        const int64_t d10 = a.delims[a.dstart[10]];
        expect("until text", std::string((const char *) a.strs.data() + a.sdir[2 * d10],
                                         (size_t) a.sdir[2 * d10 + 1]) == "<tool_call>");
        {
            const int64_t d = a.delims[a.dstart[10]];
            expect("until delims", a.dcount[10] == 1 &&
                   std::string((const char *) a.strs.data() + a.sdir[2 * d],
                               (size_t) a.sdir[2 * d + 1]) == "<tool_call>");
        }
        expect("tool body", a.ccount[8] == 4 && a.children[a.cfirst[8] + 3] == 6);
        expect("root children", a.ccount[14] == 4 && a.children[a.cfirst[14] + 2] == 12);
    }

    // ── 3: repetition bounds and an Until with two delimiters.
    {
        ++cases;
        printf("CASE %d bounds and multiple delimiters\n", cases);
        Arena a;
        int64_t s = a.load("{\"parsers\":["
                           "{\"type\":\"repetition\",\"child\":1,\"min_count\":2,\"max_count\":5},"
                           "{\"type\":\"any\"},"
                           "{\"type\":\"until\",\"delimiters\":[\"</a>\",\"</b>\"]}"
                           "],\"rules\":{},\"root\":0}");
        expect("status", s == 0, std::to_string(s));
        expect("rep bounds", a.mn[0] == 2 && a.mx[0] == 5);
        expect("any kind", a.kind[1] == 9);
        expect("two delims", a.dcount[2] == 2);
        {
            const int64_t d1 = a.delims[a.dstart[2]];
            expect("first delim", std::string((const char *) a.strs.data() + a.sdir[2 * d1],
                                              (size_t) a.sdir[2 * d1 + 1]) == "</a>");
        }
        const int64_t d2 = a.delims[a.dstart[2] + 1];
        const int64_t o = a.sdir[2 * d2];
        const int64_t l = a.sdir[2 * d2 + 1];
        expect("second delim", std::string((const char *) a.strs.data() + o, (size_t) l) == "</b>");
    }

    // ── 4: a literal with escapes and UTF-8 survives the JSON round trip.
    {
        ++cases;
        printf("CASE %d literal escapes\n", cases);
        Arena a;
        int64_t s = a.load("{\"parsers\":[{\"type\":\"literal\",\"literal\":\"a\\\"b\\n\\u2192\"}],"
                           "\"rules\":{},\"root\":0}");
        expect("status", s == 0, std::to_string(s));
        expect("literal bytes", a.str(0) == "a\"b\n\xe2\x86\x92", a.str(0));
    }

    // ── 5: unknown parser type (from_json: "Unknown parser type: <t>").
    {
        ++cases;
        printf("CASE %d unknown type\n", cases);
        Arena a;
        int64_t s = a.load("{\"parsers\":[{\"type\":\"banana\"}],\"rules\":{},\"root\":0}");
        expect("status", s == -3, std::to_string(s));
        expect("message", a.text() == "Unknown parser type: banana", a.text());
    }

    // ── 6: the input is not JSON at all.
    {
        ++cases;
        printf("CASE %d not json\n", cases);
        Arena a;
        int64_t s = a.load("not json");
        expect("status", s != 0, std::to_string(s));
        expect("has message", !a.text().empty(), a.text());
    }

    // ── 7: missing members (from_json: three explicit errors).
    {
        ++cases;
        printf("CASE %d missing members\n", cases);
        Arena a;
        expect("no parsers", a.load("{\"rules\":{},\"root\":0}") == -2 &&
                                 a.text() == "JSON missing or invalid 'parsers' array", a.text());
        Arena b;
        expect("no rules", b.load("{\"parsers\":[],\"root\":0}") == -2 &&
                               b.text() == "JSON missing or invalid 'rules' object", b.text());
        Arena c;
        expect("no root", c.load("{\"parsers\":[],\"rules\":{}}") == -2 &&
                              c.text() == "JSON missing 'root' field", c.text());
    }

    // ── 8: ids outside the arena (from_json validates rules and root; the port
    //        also validates children, which would otherwise trap).
    {
        ++cases;
        printf("CASE %d invalid ids\n", cases);
        Arena a;
        expect("child out of range",
               a.load("{\"parsers\":[{\"type\":\"sequence\",\"children\":[0,99]}],"
                      "\"rules\":{},\"root\":0}") == -4 &&
                   a.text() == "Child references invalid parser ID: 99",
               a.text());
        Arena b;
        expect("root out of range",
               b.load("{\"parsers\":[{\"type\":\"any\"}],\"rules\":{},\"root\":9}") == -4 &&
                   b.text() == "Root references invalid parser ID: 9",
               b.text());
    }

    // ── 9: build_parse_params copies format/prompt, forces AUTO, loads the arena.
    {
        ++cases;
        printf("CASE %d build_parse_params\n", cases);
        Arena a;
        std::vector<int64_t> out(16, 0);
        std::string json_text = JSON_TOOL;
        sl_u8 j{S(json_text)};
        sl_i64 k{a.kind.data(), (int64_t) a.kind.size()};
        sl_i64 cf{a.cfirst.data(), (int64_t) a.cfirst.size()};
        sl_i64 cc{a.ccount.data(), (int64_t) a.ccount.size()};
        sl_i64 ch{a.children.data(), (int64_t) a.children.size()};
        sl_i64 sx{a.sidx.data(), (int64_t) a.sidx.size()};
        sl_i64 ds{a.dstart.data(), (int64_t) a.dstart.size()};
        sl_i64 dc{a.dcount.data(), (int64_t) a.dcount.size()};
        sl_i64 dl{a.delims.data(), (int64_t) a.delims.size()};
        sl_i64 mn_s{a.mn.data(), (int64_t) a.mn.size()};
        sl_i64 mx_s{a.mx.data(), (int64_t) a.mx.size()};
        sl_u8 st{a.strs.data(), (int64_t) a.strs.size()};
        sl_i64 sd{a.sdir.data(), (int64_t) a.sdir.size()};
        sl_i64 o{out.data(), (int64_t) out.size()};
        sl_u8 e{a.err.data(), (int64_t) a.err.size()};
        int64_t s = pq_build_params(1 /* PEG_SIMPLE */, 7, j, k, cf, cc, ch, sx, ds, dc, dl,
                                    mn_s, mx_s, st, sd, o, e);
        expect("status", s == 0, std::to_string(s) + " " + a.text());
        expect("format kept", out[0] == 1);
        expect("generation prompt kept", out[1] == 7);
        expect("reasoning AUTO", out[2] == 1, std::to_string(out[2]));
        expect("root", out[3] == 14);
        expect("count", out[4] == 15);
    }

    // ── 10: a failed load keeps format and prompt but leaves AUTO unset
    //         (chat_parse.cpp:10-13: the load throws before the assignment).
    {
        ++cases;
        printf("CASE %d failed load and warn-once\n", cases);
        Arena a;
        std::vector<int64_t> out(16, 0);
        std::string bad_text = "{\"parsers\":[],\"root\":0}"; // rules missing
        sl_u8 j{S(bad_text)};
        sl_i64 k{a.kind.data(), (int64_t) a.kind.size()};
        sl_i64 cf{a.cfirst.data(), (int64_t) a.cfirst.size()};
        sl_i64 cc{a.ccount.data(), (int64_t) a.ccount.size()};
        sl_i64 ch{a.children.data(), (int64_t) a.children.size()};
        sl_i64 sx{a.sidx.data(), (int64_t) a.sidx.size()};
        sl_i64 ds{a.dstart.data(), (int64_t) a.dstart.size()};
        sl_i64 dc{a.dcount.data(), (int64_t) a.dcount.size()};
        sl_i64 dl{a.delims.data(), (int64_t) a.delims.size()};
        sl_i64 mn_s{a.mn.data(), (int64_t) a.mn.size()};
        sl_i64 mx_s{a.mx.data(), (int64_t) a.mx.size()};
        sl_u8 st{a.strs.data(), (int64_t) a.strs.size()};
        sl_i64 sd{a.sdir.data(), (int64_t) a.sdir.size()};
        sl_i64 o{out.data(), (int64_t) out.size()};
        sl_u8 e{a.err.data(), (int64_t) a.err.size()};
        int64_t s = pq_build_params(2 /* PEG_NATIVE */, 5, j, k, cf, cc, ch, sx, ds, dc, dl,
                                    mn_s, mx_s, st, sd, o, e);
        expect("status", s == -2, std::to_string(s) + " " + a.text());
        expect("format survives", out[0] == 2);
        expect("prompt survives", out[1] == 5);
        expect("AUTO not set", out[2] == 0, std::to_string(out[2]));

        std::vector<int64_t> w(4, 0);
        sl_i64 w_s{w.data(), (int64_t) w.size()};
        expect("first warn", pq_warn_once(w_s) == 1);
        expect("second warn", pq_warn_once(w_s) == 0);
        expect("third warn", pq_warn_once(w_s) == 0);
        expect("flag set", w[0] == 1);
    }

    printf("FG577_S4 cases=%d values=%d failures=%d\n", cases, checks, fails);
    return fails ? 1 : 0;
}
