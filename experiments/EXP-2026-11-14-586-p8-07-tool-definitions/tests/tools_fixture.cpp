// Gate for the P8-07 preparatory slice: tool definitions, argument validation,
// result formation (pure parts, no file system).
//
// Expected values and messages are transcribed from
// upstream/llama.cpp/tools/server/server-tools.cpp with the line cited in each
// case; the JSON shapes follow the source order of the object literals.
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

static std::string vis(const std::string & s, size_t max = 220) {
    std::string r;
    size_t n = 0;
    for (unsigned char c : s) {
        if (n++ > max) { r += "..."; break; }
        if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if (c == '\t') r += "\\t";
        else if (c == '"') r += "\\\"";
        else if (c >= 32 && c < 127) r.push_back((char) c);
        else { char b[8]; snprintf(b, sizeof b, "\\x%02X", c); r += b; }
    }
    return r;
}

static sl_u8 S(const std::string & s) {
    return sl_u8{(uint8_t *) s.data(), (int64_t) s.size()};
}

static void expect(const char * what, bool ok, const std::string & detail = "") {
    ++checks;
    if (!ok) {
        ++fails;
        printf("  FAIL %s%s\n", what, detail.empty() ? "" : (" : " + detail).c_str());
    }
}

struct Val {
    std::vector<int64_t> st = std::vector<int64_t>(512, 0);
    std::vector<uint8_t> sbuf = std::vector<uint8_t>(1 << 16, 0);
    std::vector<uint8_t> msg = std::vector<uint8_t>(1 << 12, 0);

    int64_t run(int64_t idx, const std::string & params) {
        sl_u8 p{S(params)};
        sl_i64 st_s{st.data(), (int64_t) st.size()};
        sl_u8 sb{sbuf.data(), (int64_t) sbuf.size()};
        sl_u8 m{msg.data(), (int64_t) msg.size()};
        return tool_validate(idx, p, st_s, sb, m);
    }
    std::string msg_text() const { return std::string((const char *) msg.data(), (size_t) st[19]); }
    std::string str(int slot) const {
        return std::string((const char *) sbuf.data() + st[20 + 2 * slot], (size_t) st[21 + 2 * slot]);
    }
};

static std::string name_of(int64_t idx) {
    std::vector<uint8_t> b(32, 0);
    sl_u8 s{b.data(), (int64_t) b.size()};
    int64_t n = tool_name(idx, s);
    return std::string((const char *) b.data(), (size_t) n);
}

static int64_t index_of(const std::string & n) {
    return tool_index(S(n));
}

static std::string read_text(const std::string & content, int64_t start, int64_t end, int64_t loc) {
    std::vector<uint8_t> o(1 << 17, 0);
    sl_u8 c{S(content)};
    sl_u8 d{o.data(), (int64_t) o.size()};
    int64_t n = tool_read_text(c, start, end, loc, d);
    return std::string((const char *) o.data(), (size_t) n);
}

static std::string guard(int64_t size, int64_t end_line, int64_t b64, const std::string & path,
                         int64_t stat_ok) {
    std::vector<uint8_t> m(1024, 0);
    sl_u8 p{S(path)};
    sl_u8 mm{m.data(), (int64_t) m.size()};
    int64_t r = tool_read_guard(size, end_line, b64, p, stat_ok, mm);
    return r == 0 ? std::string() : std::string((const char *) m.data(), strlen((char *) m.data()));
}

static std::string edit_check(Val & v, const std::string & content) {
    sl_i64 st_s{v.st.data(), (int64_t) v.st.size()};
    sl_u8 sb{v.sbuf.data(), (int64_t) v.sbuf.size()};
    sl_u8 c{S(content)};
    sl_u8 m{v.msg.data(), (int64_t) v.msg.size()};
    int64_t r = tool_edit_check(st_s, sb, c, m);
    return r == 0 ? std::string() : v.msg_text();
}

int main() {
    goose_init(0, 0);

    // ── 1: dispatch and flags (server-tools.cpp:1957-1963, 869-1963).
    {
        ++cases;
        printf("CASE %d dispatch and flags\n", cases);
        expect("read_file index", index_of("read_file") == 0);
        expect("grep index", index_of("grep_search") == 2);
        expect("get_info index", index_of("get_info") == 6);
        expect("unknown index", index_of("nope") == -1);
        expect("name of 3", name_of(3) == "exec_shell_command");
        expect("name out of range", name_of(9).empty());
        // exec: write + cwd + stream = 7 (:1250-1253); read_file: cwd only = 2; write: write+cwd = 3
        expect("exec flags", tool_flags(3) == 7);
        expect("read_file flags", tool_flags(0) == 2);
        expect("write_file flags", tool_flags(4) == 3);
    }

    // ── 2: read_file defaults (server-tools.cpp:899-902).
    {
        ++cases;
        printf("CASE %d read_file defaults\n", cases);
        Val v;
        int64_t s = v.run(0, "{\"path\":\"/tmp/a.txt\"}");
        expect("status", s == 0, std::to_string(s));
        expect("path", v.str(0) == "/tmp/a.txt", v.str(0));
        expect("start_line", v.st[10] == 1, std::to_string(v.st[10]));
        expect("end_line", v.st[11] == -1, std::to_string(v.st[11]));
        expect("append_loc", v.st[12] == 0);
        expect("as_base64", v.st[13] == 0);
        Val b;
        b.run(0, "{\"path\":\"a\",\"start_line\":3,\"end_line\":9,\"append_loc\":true,"
                "\"resp_type\":\"base64\"}");
        expect("start 3", b.st[10] == 3);
        expect("end 9", b.st[11] == 9);
        expect("loc 1", b.st[12] == 1);
        expect("base64 1", b.st[13] == 1);
    }

    // ── 3: missing required member -> params.at throws -> 400 (:899).
    {
        ++cases;
        printf("CASE %d missing required members\n", cases);
        Val v;
        expect("read_file no path", v.run(0, "{}") == -2);
        expect("read_file message", v.msg_text() == "missing required member: path", v.msg_text());
        Val g;
        expect("grep no pattern", g.run(2, "{\"path\":\"x\"}") == -2);
        Val w;
        expect("write no content", w.run(4, "{\"path\":\"x\"}") == -2);
        Val e;
        expect("edit no edits", e.run(5, "{\"path\":\"x\"}") == -2);
        Val bad;
        expect("not an object", bad.run(0, "\"text\"") == -1);
        expect("not json", bad.run(0, "not json") == -1);
    }

    // ── 4: read_file text: range, line numbers, truncation (:920-966).
    {
        ++cases;
        printf("CASE %d read_file text\n", cases);
        const std::string c = "alpha\nbeta\ngamma\n";
        expect("whole file", read_text(c, 1, -1, 0) == "alpha\nbeta\ngamma\n");
        expect("range 2..2", read_text(c, 2, 2, 0) == "beta\n");
        expect("range 3..", read_text(c, 3, -1, 0) == "gamma\n");
        expect("line numbers", read_text(c, 2, 2, 1) == "2\xe2\x86\x92" "beta\n",
               vis(read_text(c, 2, 2, 1)));
        expect("start beyond eof", read_text(c, 9, -1, 0).empty());
        expect("end before start", read_text(c, 3, 1, 0).empty());

        // 20 KB of 10-byte lines: the result stops below 16 KiB and gains the marker.
        std::string big;
        for (int i = 0; i < 2000; i++) big += "0123456789\n";
        std::string out = read_text(big, 1, -1, 0);
        bool marker = out.size() > 18 && out.compare(out.size() - 18, 18, "[output truncated]") == 0;
        expect("truncation marker", marker, std::to_string(out.size()));
        expect("truncation budget", out.size() <= 16384 + 18, std::to_string(out.size()));
    }

    // ── 5: read_file size guards (:907-928) and base64 ceiling (:910-914).
    {
        ++cases;
        printf("CASE %d read_file size guards\n", cases);
        expect("stat failed", guard(0, -1, 0, "/nope", 0) == "cannot stat file: /nope",
               guard(0, -1, 0, "/nope", 0));
        expect("too large unlimited",
               guard(20000, -1, 0, "f", 1) ==
                   "file too large (20000 bytes, max 16384). Use start_line/end_line to read a portion.",
               guard(20000, -1, 0, "f", 1));
        expect("too large with range", guard(20000, 10, 0, "f", 1).empty());
        expect("base64 ceiling",
               guard(40000000, -1, 1, "f", 1) == "file too large (40000000 bytes, max 33554432)",
               guard(40000000, -1, 1, "f", 1));
        expect("base64 under ceiling", guard(1000, -1, 1, "f", 1).empty());
    }

    // ── 6: file_glob_search validation (:1030-1046).
    {
        ++cases;
        printf("CASE %d file_glob validation\n", cases);
        Val v;
        expect("ok", v.run(1, "{\"path\":\"/w\"}") == 0);
        expect("include default", v.str(1) == "**", v.str(1));
        expect("limit default 100", v.st[16] == 100);
        Val z;
        expect("limit 0", z.run(1, "{\"path\":\"/w\",\"limit\":0}") == -3);
        expect("limit message", z.msg_text() == "invalid limit: 0 (expected 1 or more)",
               z.msg_text());
        Val h;
        h.run(1, "{\"path\":\"/w\",\"limit\":5000,\"max_depth\":-3}");
        expect("limit clamped", h.st[16] == 100, std::to_string(h.st[16]));
        expect("max_depth clamped", h.st[15] == 0, std::to_string(h.st[15]));
        Val t;
        expect("bad type", t.run(1, "{\"path\":\"/w\",\"type\":\"banana\"}") == -3);
        expect("type message",
               t.msg_text() == "invalid type: banana (expected \"file\", \"dir\" or \"all\")",
               t.msg_text());
        Val d;
        d.run(1, "{\"path\":\"/w\",\"type\":\"dir\"}");
        expect("dir kind", d.st[14] == 1);
    }

    // ── 7: grep_search validation (:1130-1145) and literal escaping (:1157-1165).
    {
        ++cases;
        printf("CASE %d grep validation\n", cases);
        Val v;
        expect("ok", v.run(2, "{\"path\":\"x\",\"pattern\":\"a.b\"}") == 0);
        expect("include default", v.str(2) == "**");
        expect("literal 0", v.st[11] == 0);
        expect("context 0", v.st[13] == 0);
        Val c;
        c.run(2, "{\"path\":\"x\",\"pattern\":\"p\",\"context_lines\":-2}");
        expect("context clamped", c.st[13] == 0);
        Val l;
        l.run(2, "{\"path\":\"x\",\"pattern\":\"p\",\"literal\":true}");
        expect("literal 1", l.st[11] == 1);

        std::vector<uint8_t> o(256, 0);
        sl_u8 pat{S("a.b*c(d)[e]")};
        sl_u8 out{o.data(), (int64_t) o.size()};
        int64_t n = tool_literal_regex(pat, out);
        std::string got((const char *) o.data(), (size_t) n);
        expect("escaped", got == "a\\.b\\*c\\(d\\)\\[e\\]", got);
        std::vector<uint8_t> o2(64, 0);
        sl_u8 plain{S("plain-text_1")};
        sl_u8 out2{o2.data(), (int64_t) o2.size()};
        n = tool_literal_regex(plain, out2);
        expect("plain untouched", std::string((const char *) o2.data(), (size_t) n) == "plain-text_1");
    }

    // ── 8: exec_shell_command clamps (:1273-1281).
    {
        ++cases;
        printf("CASE %d exec clamps\n", cases);
        Val v;
        expect("ok", v.run(3, "{\"command\":\"ls\"}") == 0);
        expect("timeout default", v.st[10] == 10, std::to_string(v.st[10]));
        expect("max default", v.st[11] == 16384, std::to_string(v.st[11]));
        Val c;
        c.run(3, "{\"command\":\"ls\",\"timeout\":999,\"max_output_size\":999999}");
        expect("timeout clamped", c.st[10] == 60, std::to_string(c.st[10]));
        expect("max clamped", c.st[11] == 16384, std::to_string(c.st[11]));
        Val s;
        s.run(3, "{\"command\":\"ls\",\"timeout\":3,\"max_output_size\":100}");
        expect("timeout small", s.st[10] == 3);
        expect("max small", s.st[11] == 100);
    }

    // ── 9: write_file arguments (:1350-1355).
    {
        ++cases;
        printf("CASE %d write_file args\n", cases);
        Val v;
        expect("ok", v.run(4, "{\"path\":\"a/b.txt\",\"content\":\"hi\"}") == 0);
        expect("path", v.str(0) == "a/b.txt");
        expect("content", v.str(1) == "hi");
        expect("bytes", v.st[10] == 2);
        Val e;
        expect("empty content accepted", e.run(4, "{\"path\":\"a\",\"content\":\"\"}") == 0);
        expect("bytes 0", e.st[10] == 0);
    }

    // ── 10: edit_file argument checks (:1412-1432).
    {
        ++cases;
        printf("CASE %d edit_file args\n", cases);
        Val v;
        expect("ok", v.run(5, "{\"path\":\"f\",\"edits\":[{\"old_text\":\"a\",\"new_text\":\"b\"}]}") == 0);
        expect("count", v.st[10] == 1);
        Val e;
        expect("empty array", e.run(5, "{\"path\":\"f\",\"edits\":[]}") == -3);
        expect("empty message", e.msg_text() == "\"edits\" must be a non-empty array", e.msg_text());
        Val z;
        expect("empty old_text", z.run(5, "{\"path\":\"f\",\"edits\":[{\"old_text\":\"\",\"new_text\":\"b\"}]}") == -3);
        expect("old_text message", z.msg_text() == "edits[0].old_text must not be empty", z.msg_text());
        Val two;
        two.run(5, "{\"path\":\"f\",\"edits\":[{\"old_text\":\"a\",\"new_text\":\"b\"},"
                   "{\"old_text\":\"cc\",\"new_text\":\"d\"}]}");
        expect("two edits", two.st[10] == 2);
    }

    // ── 11: edit_file matching (:1436-1489): not found, duplicates, overlap.
    {
        ++cases;
        printf("CASE %d edit matching\n", cases);
        const std::string content = "line one\nline two\nline three\n";
        {
            Val v;
            v.run(5, "{\"path\":\"f\",\"edits\":[{\"old_text\":\"absent\",\"new_text\":\"x\"}]}");
            expect("not found",
                   edit_check(v, content) ==
                       "could not find edits[0].old_text in f, it must match the file's current "
                       "content exactly",
                   edit_check(v, content));
        }
        {
            Val v;
            v.run(5, "{\"path\":\"f\",\"edits\":[{\"old_text\":\"line\",\"new_text\":\"x\"}]}");
            std::string m = edit_check(v, content);
            expect("duplicates", m == "found 3 occurrences of edits[0].old_text in f, it must be unique", m);
        }
        {
            Val v;
            v.run(5, "{\"path\":\"f\",\"edits\":[{\"old_text\":\"line two\",\"new_text\":\"x\"},"
                     "{\"old_text\":\"two\\nline\",\"new_text\":\"y\"}]}");
            std::string m = edit_check(v, content);
            expect("overlap", m == "edits[0] and edits[1] overlap in f; merge them into one edit or "
                                  "target disjoint regions", m);
        }
        {
            Val v;
            v.run(5, "{\"path\":\"f\",\"edits\":[{\"old_text\":\"line one\",\"new_text\":\"x\"},"
                     "{\"old_text\":\"line three\",\"new_text\":\"y\"}]}");
            expect("disjoint ok", edit_check(v, content).empty());
            expect("positions", v.st[11] == 0 && v.st[13] == 18, std::to_string(v.st[11]) + "," +
                                                                    std::to_string(v.st[13]));
        }
    }

    // ── 12: result JSON shapes, in the native key order.
    {
        ++cases;
        printf("CASE %d result json\n", cases);
        std::vector<uint8_t> o(4096, 0);
        sl_u8 out{o.data(), (int64_t) o.size()};
        auto text = [&](int64_t n) { return std::string((const char *) o.data(), (size_t) n); };

        expect("error",
               text(tool_json_error(S("bad \"x\""), out)) == "{\"error\":\"bad \\\"x\\\"\"}",
               text(tool_json_error(S("bad \"x\""), out)));
        expect("plain", text(tool_json_plain(S("l1\nl2"), out)) ==
                             "{\"plain_text_response\":\"l1\\nl2\"}");
        expect("write", text(tool_json_write(S("a/b"), 2, out)) ==
                             "{\"result\":\"file written successfully\",\"path\":\"a/b\",\"bytes\":2}",
               text(tool_json_write(S("a/b"), 2, out)));
        expect("edit", text(tool_json_edit(S("f"), 1, out)) ==
                            "{\"result\":\"file edited successfully\",\"path\":\"f\",\"edits_applied\":1}");
        expect("info", text(tool_json_info(S("Linux"), S("/w"), out)) ==
                            "{\"os\":\"Linux\",\"cwd\":\"/w\"}");
        expect("base64", text(tool_json_base64(5, out)) == "{\"base64\":\"\",\"size_bytes\":5}");
        expect("exec tail", text(tool_exec_tail(S("out"), 0, 0, out)) == "out\n[exit code: 0]");
        expect("exec tail timeout",
               text(tool_exec_tail(S("out"), 2, 1, out)) == "out\n[exit code: 2] [exit due to timed out]");
        expect("glob tail", text(tool_glob_tail(5, 5, 0, out)) == "\n---\nTotal matches: 5\n");
        expect("glob tail limit",
               text(tool_glob_tail(250, 100, 0, out)) ==
                   "\n---\nTotal matches: 250\n[100 results limit reached (250 total matches). "
                   "Refine the glob pattern to narrow the search.]\n");
    }

    printf("FG577_TOOLS cases=%d values=%d failures=%d\n", cases, checks, fails);
    return fails ? 1 : 0;
}
