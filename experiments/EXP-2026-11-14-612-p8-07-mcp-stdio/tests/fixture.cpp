// Gate for the MCP stdio transport shells (P8-07 S5).
//
// Native side: a transcription of tools/server/server-mcp.cpp on std::string,
// std::map and std::filesystem (lines cited at every function). Port side:
// the exported Goose functions. No process is spawned and no thread runs: the
// pipe, the clock and the file scan are scripted data. The only filesystem use
// is the test tree the resolver is checked against.
#include "g.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int cases = 0;
static int fails = 0;

// ── helpers ────────────────────────────────────────────────────────────────

static std::vector<std::string> split_by(const std::string & s, char sep) {
    std::vector<std::string> v;
    size_t a = 0;
    while (a <= s.size()) {
        size_t b = s.find(sep, a);
        if (b == std::string::npos) b = s.size();
        if (b > a) v.push_back(s.substr(a, b - a));
        if (b == s.size()) break;
        a = b + 1;
    }
    return v;
}

static std::string join_lines(const std::vector<std::string> & v) {
    std::string s;
    for (const auto & x : v) { s += x; s += "\n"; }
    return s;
}

static std::string esc_byte(unsigned char c) {
    static const char * hex = "0123456789abcdef";
    if (c == '\\') return "\\\\";
    if (c < 32 || c > 126) return std::string("\\x") + hex[c / 16] + hex[c % 16];
    return std::string(1, (char) c);
}

// ── test tree ──────────────────────────────────────────────────────────────

static std::string g_root;

static void make_tree() {
    g_root = (fs::temp_directory_path() / "fg_ms617_fs").string();
    std::error_code ec;
    fs::remove_all(g_root, ec);
    const char * files[] = {
        "srv", "srv.exe", "a.com", "a.exe", "b.bat", "plain.exe",
        "nested/inner.exe", "sub/tool.exe", "sub/absent", "d1/x.exe", "d2/x",
    };
    for (const char * rel : files) {
        fs::path p = fs::path(g_root) / rel;
        fs::create_directories(p.parent_path(), ec);
        std::ofstream f(p, std::ios::binary);
        f << "x";
    }
}

// Windows is case insensitive; the fixture checks the lowercased path.
static bool fs_exists(const std::string & winpath) {
    std::string p = winpath;
    for (auto & c : p) {
        if (c == '\\') c = '/';
        c = (char) std::tolower((unsigned char) c);
    }
    std::error_code ec;
    return fs::is_regular_file(p, ec);
}

static std::string P(const std::string & rel) { return g_root + "/" + rel; }
static std::string WP(const std::string & rel) { return g_root + "\\" + rel; }

// ── resolve_command (379-424) ──────────────────────────────────────────────

static bool has_sep(const std::string & s) {
    return s.find('\\') != std::string::npos || s.find('/') != std::string::npos;
}

// SearchPathW adds an extension only when the file name has none; a dot in a
// directory does not count.
static bool has_ext(const std::string & s) {
    size_t d = s.rfind('.');
    size_t p = s.rfind('\\');
    size_t q = s.rfind('/');
    if (d == std::string::npos) return false;
    size_t sep = std::string::npos;
    if (p != std::string::npos) sep = p;
    if (q != std::string::npos && (sep == std::string::npos || q > sep)) sep = q;
    return sep == std::string::npos || d > sep;
}

static std::string join_dir(const std::string & d, const std::string & name) {
    if (d.empty()) return name;
    char last = d.back();
    if (last == '\\' || last == '/') return d + name;
    return d + "\\" + name;
}

static std::vector<std::string> ext_list(const std::string & pathext) {
    if (pathext.empty()) return split_by(".COM;.EXE;.BAT;.CMD", ';');
    return split_by(pathext, ';');
}

static std::vector<std::string> candidates(const std::string & cmd,
                                           const std::string & dirs,
                                           const std::string & pathext) {
    std::vector<std::string> out;
    if (cmd.empty()) return out;
    if (has_sep(cmd)) {
        out.push_back(cmd);
        if (!has_ext(cmd)) {
            for (const auto & e : ext_list(pathext)) out.push_back(cmd + e);
        }
        return out;
    }
    for (const auto & d : split_by(dirs, ';')) out.push_back(join_dir(d, cmd));
    if (!has_ext(cmd)) {
        for (const auto & e : ext_list(pathext)) {
            for (const auto & d : split_by(dirs, ';')) out.push_back(join_dir(d, cmd + e));
        }
    }
    return out;
}

static std::string resolve_from(const std::vector<std::string> & c, const std::string & cmd) {
    for (const auto & x : c) {
        if (fs_exists(x)) return x;
    }
    return cmd;
}

// ── build_env (426-460) ────────────────────────────────────────────────────

static std::string env_native(const std::string & parent, const std::string & overrides) {
    std::map<std::string, std::string> ov;
    for (const auto & o : split_by(overrides, '\n')) {
        size_t eq = o.find('=');
        if (eq == std::string::npos) {
            ov[o] = "";
        } else {
            ov[o.substr(0, eq)] = o.substr(eq + 1);
        }
    }
    std::string out;
    for (const auto & e : split_by(parent, '\n')) {
        size_t eq = e.find('=');
        std::string key = eq == std::string::npos ? e : e.substr(0, eq);
        if (ov.find(key) == ov.end()) out += e + "\n";
    }
    for (const auto & [k, v] : ov) out += k + "=" + v + "\n";
    return out;
}

// ── write_all (537-602) ────────────────────────────────────────────────────

struct WRes {
    int64_t rc = 2, total = 0, steps = 0, polls = 0, timeouts = 0;
};

static WRes write_native(int64_t size, const std::string & script) {
    WRes r;
    if (size <= 0) { r.rc = 0; return r; }
    bool pending = false;
    for (const auto & tok : split_by(script, ' ')) {
        if (r.total >= size) { r.rc = 0; break; }
        if (pending) {
            pending = false;
            if (tok == "t") { r.timeouts++; continue; }
            if (tok == "p" || tok == "q") continue;
            if (tok == "h" || tok == "x" || tok == "n" || tok == "e") { r.rc = 1; break; }
            r.rc = 2;
            break;
        }
        if (tok == "0") { r.steps++; r.rc = 1; break; }
        if (tok == "i") { r.steps++; continue; }
        if (tok == "s") { r.rc = r.total >= size ? 0 : 1; break; }
        if (tok == "g") { r.steps++; r.polls++; pending = true; continue; }
        if (!tok.empty() && tok[0] == 'w') {
            int64_t n = atoll(tok.c_str() + 1);
            if (n > size - r.total) n = size - r.total;
            if (n > 0) r.total += n;
            r.steps++;
            if (r.total >= size) { r.rc = 0; break; }
            continue;
        }
        r.rc = 2;
        break;
    }
    return r;
}

static std::string write_text(const WRes & r) {
    return "rc=" + std::to_string(r.rc) + " total=" + std::to_string(r.total) +
           " steps=" + std::to_string(r.steps) + " polls=" + std::to_string(r.polls) +
           " timeouts=" + std::to_string(r.timeouts);
}

// ── mcp_pump_ndjson (24-129) ───────────────────────────────────────────────

struct PRes {
    int64_t state = 5, warns = 0;
    std::vector<std::string> lines;
};

static PRes pump_native(const std::string & script, int64_t consume, int64_t max_line) {
    PRes r;
    std::string buf;
    bool skipping = false;
    size_t t = 0;

    while (t < script.size() && r.state == 5) {
        size_t e = script.find('\n', t);
        if (e == std::string::npos) e = script.size();
        std::string step = script.substr(t, e - t);
        t = e + 1;
        if (step.empty()) continue;
        char code = step[0];

        if (code == 't' || code == 'i' || code == 'g' || code == 'h' || code == 'q') {
            continue;
        } else if (code == 'e') {
            r.state = 1;
        } else if (code == 'r' || code == 'x' || code == 'n' || code == 'p') {
            r.state = 2;
        } else if (code == 's') {
            r.state = 0;
        } else if (code == 'd') {
            std::string chunk;
            for (size_t i = 1; i < step.size(); i++) {
                char ch = step[i];
                if (ch == '\\' && i + 1 < step.size()) {
                    char nx = step[i + 1];
                    if (nx == 'n') { chunk += '\n'; i++; }
                    else if (nx == 'r') { chunk += '\r'; i++; }
                    else if (nx == '\\') { chunk += '\\'; i++; }
                    else chunk += ch;
                } else {
                    chunk += ch;
                }
            }
            buf += chunk;
            if (buf.size() > 1024) buf.resize(1024);

            bool phase = false;
            if (skipping) {
                size_t nl = buf.find('\n');
                if (nl == std::string::npos) {
                    if ((int64_t) buf.size() > max_line) buf.clear();
                    phase = true;
                } else {
                    buf.erase(0, nl + 1);
                    skipping = false;
                }
            }
            if (!phase) {
                size_t nl;
                while ((nl = buf.find('\n')) != std::string::npos) {
                    std::string line = buf.substr(0, nl);
                    buf.erase(0, nl + 1);
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    if (!line.empty()) {
                        r.lines.push_back(line);
                        if ((int64_t) r.lines.size() == consume) r.state = 3;
                    }
                    if (r.state == 3) break;
                }
                if (r.state != 3 && (int64_t) buf.size() > max_line) {
                    r.warns++;
                    buf.clear();
                    skipping = true;
                }
            }
        }
    }
    if (r.state == 5) r.state = 4;
    return r;
}

static std::string pump_text(const PRes & r) {
    std::string s = "state=" + std::to_string(r.state) + " warns=" + std::to_string(r.warns) +
                    " lines=" + std::to_string(r.lines.size()) + "\n";
    for (const auto & l : r.lines) {
        s += "L";
        for (unsigned char c : l) s += esc_byte(c);
        s += "\n";
    }
    return s;
}

// ── errlog tail (618-632) ──────────────────────────────────────────────────

static std::string err_native(const std::string & text, int64_t cap) {
    std::string tail;
    for (const auto & line : split_by(text, '\n')) {
        tail += line;
        tail += '\n';
        if ((int64_t) tail.size() > cap) tail.erase(0, tail.size() - (size_t) cap);
    }
    return tail;
}

// ── manager get_or_create (771-819) ────────────────────────────────────────

static std::string manager_native(const std::string & args) {
    std::vector<int64_t> v;
    for (const auto & t : split_by(args, ' ')) v.push_back(atoll(t.c_str()));
    while (v.size() < 9) v.push_back(0);
    int64_t stopping = v[0], now = v[1], dead = v[2], ddl = v[3], alive = v[4];
    int64_t is_alive = v[5], cfg = v[6], sok = v[7], salive = v[8];
    int64_t action = 5, close_old = 0, close_new = 0, cool = 0;
    bool done = false;

    if (stopping) { action = 0; done = true; }
    else if (dead && now < ddl) { action = 1; done = true; }
    if (!done && alive) {
        if (is_alive) { action = 2; done = true; }
        else close_old = 1;
    }
    if (!done) {
        if (!cfg) action = 5;
        else if (sok && salive) action = 3;
        else { action = 4; close_new = 1; cool = now + 5000; }
    }
    return "action=" + std::to_string(action) + " close_old=" + std::to_string(close_old) +
           " close_new=" + std::to_string(close_new) + " cooldown=" + std::to_string(cool);
}

// ── warmup (693-749) ───────────────────────────────────────────────────────

static std::string warmup_native(const std::string & rows) {
    if (rows.empty()) return "";
    std::vector<std::string> seen;
    std::string out;
    int64_t reg = 0;

    for (const auto & row : split_by(rows, '\n')) {
        auto f = split_by(row, '|');
        while (f.size() < 6) f.push_back("0");
        const std::string & name = f[0];
        int64_t spawn = atoll(f[1].c_str());
        int64_t start_ok = atoll(f[2].c_str());
        int64_t query = atoll(f[3].c_str());
        int64_t stopping = atoll(f[4].c_str());
        int64_t tools = atoll(f[5].c_str());
        bool dup = std::find(seen.begin(), seen.end(), name) != seen.end();

        if (dup) {
            out += "skip|" + name + "\n";
        } else {
            seen.push_back(name);
            out += name;
            if (!start_ok) {
                out += "|failed|0|0";
            } else {
                int64_t dl = spawn + 10000;
                if (stopping || query >= dl) {
                    out += "|aborted|" + std::to_string(dl) + "|0";
                } else {
                    out += "|ok|" + std::to_string(dl) + "|" + std::to_string(tools);
                    reg += tools;
                }
            }
            out += "\n";
        }
    }
    out += "registry=" + std::to_string(reg) + "\n";
    return out;
}

// ── port calls ─────────────────────────────────────────────────────────────

static char g_buf[262144];

static std::string call4(int64_t (*fn)(sl_u8, sl_u8, sl_u8, sl_u8),
                         const std::string & a, const std::string & b, const std::string & c) {
    sl_u8 sa{(uint8_t *) (void *) a.data(), (int64_t) a.size()};
    sl_u8 sb{(uint8_t *) (void *) b.data(), (int64_t) b.size()};
    sl_u8 sc{(uint8_t *) (void *) c.data(), (int64_t) c.size()};
    sl_u8 o{(uint8_t *) g_buf, (int64_t) sizeof g_buf};
    int64_t n = fn(sa, sb, sc, o);
    return std::string(g_buf, (size_t) n);
}

static std::string call3(int64_t (*fn)(sl_u8, sl_u8, sl_u8),
                         const std::string & a, const std::string & b) {
    sl_u8 sa{(uint8_t *) (void *) a.data(), (int64_t) a.size()};
    sl_u8 sb{(uint8_t *) (void *) b.data(), (int64_t) b.size()};
    sl_u8 o{(uint8_t *) g_buf, (int64_t) sizeof g_buf};
    int64_t n = fn(sa, sb, o);
    return std::string(g_buf, (size_t) n);
}

static std::string call_pump(const std::string & script, int64_t consume, int64_t max_line) {
    sl_u8 sa{(uint8_t *) (void *) script.data(), (int64_t) script.size()};
    sl_u8 o{(uint8_t *) g_buf, (int64_t) sizeof g_buf};
    int64_t n = ms_pump_lines(sa, consume, max_line, o);
    return std::string(g_buf, (size_t) n);
}

static std::string call_plan(int64_t size, const std::string & script) {
    sl_u8 sa{(uint8_t *) (void *) script.data(), (int64_t) script.size()};
    sl_u8 o{(uint8_t *) g_buf, (int64_t) sizeof g_buf};
    int64_t n = ms_write_plan(size, sa, o);
    return std::string(g_buf, (size_t) n);
}

static std::string call_err(const std::string & text, int64_t cap) {
    sl_u8 sa{(uint8_t *) (void *) text.data(), (int64_t) text.size()};
    sl_u8 o{(uint8_t *) g_buf, (int64_t) sizeof g_buf};
    int64_t n = ms_err_tail(sa, cap, o);
    return std::string(g_buf, (size_t) n);
}

static std::string call_one(int64_t (*fn)(sl_u8, sl_u8), const std::string & a) {
    sl_u8 sa{(uint8_t *) (void *) a.data(), (int64_t) a.size()};
    sl_u8 o{(uint8_t *) g_buf, (int64_t) sizeof g_buf};
    int64_t n = fn(sa, o);
    return std::string(g_buf, (size_t) n);
}

static std::string call_out(int64_t (*fn)(sl_u8), const std::string & a) {
    sl_u8 o{(uint8_t *) g_buf, (int64_t) sizeof g_buf};
    int64_t n = fn(o);
    return std::string(g_buf, (size_t) n);
}

static void eq(const char * name, const std::string & want, const std::string & got) {
    cases++;
    if (want == got) {
        printf("OK   %s\n", name);
        return;
    }
    fails++;
    printf("FAIL %s\n  native: %s\n  port:   %s\n", name, want.c_str(), got.c_str());
}

// ── cases ──────────────────────────────────────────────────────────────────

static void resolve_case(const char * name, const std::string & cmd,
                         const std::string & dirs, const std::string & pe) {
    auto native = resolve_from(candidates(cmd, dirs, pe), cmd);
    auto port = resolve_from(split_by(call4(ms_resolve_candidates, cmd, dirs, pe), '\n'), cmd);
    eq(name, native, port);
}

static void cands_case(const char * name, const std::string & cmd,
                       const std::string & dirs, const std::string & pe) {
    auto native = join_lines(candidates(cmd, dirs, pe));
    eq(name, native, call4(ms_resolve_candidates, cmd, dirs, pe));
}

int main() {
    goose_init(0, nullptr);
    make_tree();

    // resolve_command (379-424)
    const std::string path = P("") + ";" + P("nested") + ";" + P("sub") + ";" + P("d1") + ";" + P("d2");
    resolve_case("res_empty", "", path, "");
    resolve_case("res_exact", "srv", P(""), "");
    resolve_case("res_ext", "tool", P("sub"), "");
    resolve_case("res_exe_found", "plain", P(""), "");
    resolve_case("res_path_exact", WP("nested\\inner.exe"), path, "");
    resolve_case("res_path_ext", WP("nested\\inner"), path, "");
    resolve_case("res_path_gives_up", WP("nested\\absent"), path, "");
    resolve_case("res_order", "x", P("d1") + ";" + P("d2"), "");
    resolve_case("res_default_pe", "b", P(""), "");
    resolve_case("res_pe_exe_first", "a", P(""), ".EXE;.COM");
    resolve_case("res_pe_com_first", "a", P(""), ".COM;.EXE");
    resolve_case("res_pe_upper", "srv", P(""), ".EXE");
    resolve_case("res_pe_empty_tokens", "b", P(""), ";;");
    resolve_case("res_dir_trailing_sep", "srv", P("") + "\\", "");
    resolve_case("res_no_dirs", "srv", "", "");
    resolve_case("res_not_found", "nope", path, "");
    cands_case("res_cands_path", WP("nested\\inner"), path, ".EXE;.COM");
    cands_case("res_cands_dirs", "x", P("d1") + ";" + P("d2"), ".EXE");

    // build_env (426-460)
    eq("env_none", env_native("A=1\nB=2", ""), call3(ms_env_build, "A=1\nB=2", ""));
    eq("env_override", env_native("A=1\nB=2\nA=3", "A=9"), call3(ms_env_build, "A=1\nB=2\nA=3", "A=9"));
    eq("env_noeq", env_native("NOEQ\nX=1", ""), call3(ms_env_build, "NOEQ\nX=1", ""));
    eq("env_noeq_override", env_native("NOEQ\nX=1", "NOEQ=5"), call3(ms_env_build, "NOEQ\nX=1", "NOEQ=5"));
    eq("env_value_eq", env_native("URL=a=b\nX=1", ""), call3(ms_env_build, "URL=a=b\nX=1", ""));
    eq("env_sorted", env_native("A=1", "Z=1\nA=2\nM=3"), call3(ms_env_build, "A=1", "Z=1\nA=2\nM=3"));
    eq("env_empty_key", env_native("A=1", "=v"), call3(ms_env_build, "A=1", "=v"));
    eq("env_empty_parent", env_native("", "B=2\nA=1"), call3(ms_env_build, "", "B=2\nA=1"));

    // write_all (537-602)
    auto wcase = [](const char * name, int64_t size, const std::string & script) {
        eq(name, write_text(write_native(size, script)), call_plan(size, script));
    };
    wcase("w_zero_size", 0, "");
    wcase("w_one", 4, "w4");
    wcase("w_partial", 4, "w2 w2");
    wcase("w_zero_return", 4, "w2 0");
    wcase("w_eintr", 4, "i w4");
    wcase("w_pollout", 4, "w2 g p w2");
    wcase("w_timeout", 5, "w2 g t w3");
    wcase("w_hup", 4, "w2 g h");
    wcase("w_err", 4, "g x");
    wcase("w_nval", 4, "w1 g n");
    wcase("w_poll_fail", 4, "g e");
    wcase("w_poll_intr", 4, "g q w4");
    wcase("w_stop", 4, "w2 s w2");
    wcase("w_stalled", 4, "w2 g p");
    wcase("w_clamp", 4, "w9");

    // mcp_pump_ndjson (24-129)
    auto pcase = [](const char * name, const std::string & script, int64_t consume, int64_t max_line) {
        eq(name, pump_text(pump_native(script, consume, max_line)), call_pump(script, consume, max_line));
    };
    pcase("p_two_lines", "dhello\\nworld\\n", 1000, 64);
    pcase("p_crlf_full", "dab\\r\\ncd\\r\\n", 1000, 64);
    pcase("p_crlf_split", "dab\\r\nd\\ncd\\r\\n", 1000, 64);
    pcase("p_empty_lines", "d\\n\\nx\\n", 1000, 64);
    pcase("p_cr_only_line", "d\\r\\n\\r\\nx\\n", 1000, 64);
    pcase("p_partial_eof", "dabc\ne", 1000, 64);
    pcase("p_chunk_split", "dhe\ndllo\\n", 1000, 64);
    pcase("p_consumer_one", "da\\nb\\n", 1, 64);
    pcase("p_consumer_second", "da\\nb\\nc\\n", 2, 64);
    pcase("p_eof", "da\\ne", 1000, 64);
    pcase("p_read_error", "r", 1000, 64);
    pcase("p_hup_continues", "hda\\n", 1000, 64);
    pcase("p_poerr", "x", 1000, 64);
    pcase("p_pnval", "n", 1000, 64);
    pcase("p_poll_fail", "p", 1000, 64);
    pcase("p_eintr", "ida\\n", 1000, 64);
    pcase("p_eagain", "gda\\n", 1000, 64);
    pcase("p_timeout", "tda\\n", 1000, 64);
    pcase("p_poll_intr", "qda\\n", 1000, 64);
    pcase("p_stopped", "da\\ns", 1000, 64);
    pcase("p_oversize_drop", "d0123456789", 1000, 8);
    pcase("p_oversize_resync", "d0123456789\ndjunk\\nok\\n", 1000, 8);
    pcase("p_oversize_then_bound", "d0123456789\nd0123456789", 1000, 8);
    pcase("p_cap_exact", "dabc\\n", 1000, 3);
    pcase("p_cap_plus_one_line", "dabcd\\n", 1000, 3);
    pcase("p_cap_no_newline", "dabcd", 1000, 3);
    pcase("p_many_chunks", "da\nb\nc\\n", 1000, 64);
    pcase("p_escape_backslash", "da\\\\\\\\b\\n", 1000, 64);

    // errlog tail (618-632)
    auto ecase = [](const char * name, const std::string & text, int64_t cap) {
        eq(name, err_native(text, cap), call_err(text, cap));
    };
    ecase("err_empty", "", 8);
    ecase("err_two", "a\nb", 100);
    ecase("err_truncate", "abc\ndef", 5);
    ecase("err_exact_cap", "ab\ncd", 6);
    ecase("err_cap_zero", "abc", 0);

    // manager get_or_create (771-819)
    auto mcase = [](const char * name, const std::string & args) {
        eq(name, manager_native(args), call_one(ms_manager_resolve, args));
    };
    mcase("mgr_stopped", "1 1000 0 0 0 0 1 1 1");
    mcase("mgr_cooldown", "0 1000 1 6000 0 0 1 1 1");
    mcase("mgr_cooldown_expired", "0 6000 1 6000 0 0 1 1 1");
    mcase("mgr_cooldown_just_below", "0 5999 1 6000 0 0 1 1 1");
    mcase("mgr_reuse", "0 1000 0 0 1 1 1 1 1");
    mcase("mgr_dead_recreate", "0 1000 0 0 1 0 1 1 1");
    mcase("mgr_failed_start", "0 1000 0 0 1 0 1 0 1");
    mcase("mgr_started_not_alive", "0 2000 0 0 0 0 1 1 0");
    mcase("mgr_no_config", "0 1000 0 0 0 0 0 0 0");
    mcase("mgr_stopped_beats_cooldown", "1 1000 1 6000 0 0 1 1 1");
    mcase("mgr_expired_no_config", "0 9000 1 6000 0 0 0 0 0");

    // warmup (693-749)
    auto ucase = [](const char * name, const std::string & rows) {
        eq(name, warmup_native(rows), call_one(ms_warmup_plan, rows));
    };
    ucase("warm_empty", "");
    ucase("warm_one_ok", "a|1000|1|2000|0|3");
    ucase("warm_deadline_hit", "a|1000|1|11000|0|3");
    ucase("warm_just_below", "a|1000|1|10999|0|3");
    ucase("warm_stopping", "a|1000|1|2000|1|3");
    ucase("warm_failed", "a|1000|0|0|0|0");
    ucase("warm_duplicate", "a|0|1|10|0|1\nb|0|1|10|0|2\na|0|1|10|0|9");
    ucase("warm_two", "a|0|1|10|0|3\nb|500|1|6000|0|4");

    // constants (657-658, 24-32, 619, 676-678)
    eq("limits", "cooldown=5\nwarmup=10\npoll_ms=50\nmax_line=8388608\nerr_tail=4096\ntransport=stdio\n",
       call_out(ms_limits, ""));

    printf("FG_MSTDIO cases=%d failures=%d\n", cases, fails);
    return fails == 0 ? 0 : 1;
}
