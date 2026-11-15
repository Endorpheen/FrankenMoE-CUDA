// Differential gate for the connectors slice (connectors.goose).
//
// Every case runs twice: through the Goose export and through a transcription of
// the native code with its lines cited. No process is spawned and no file is
// touched: the binary search is a list of directories that exist.
#include "connectors-gen.h"

#include "common.h"

static sl_u8 su8(UBuf & u) {
    return sl_u8{ u.data(), u.cap() };
}

static sl_i64 si64(IBuf & i) {
    return sl_i64{ i.data(), i.cap() };
}

static sl_i64 si64(std::vector<int64_t> & v) {
    return sl_i64{ v.data(), (int64_t) v.size() };
}

static sl_u8 su8(NameTable & t) {
    return sl_u8{ t.buf.data(), (int64_t) t.buf.size() };
}

static sl_u8 su8s(const std::string & s) {
    return sl_u8{ (uint8_t *) s.data(), (int64_t) s.size() };
}

// ── native side (tools/server/server-connect.cpp) ───────────────────────────

static const char * CONNECT_CODE_CHARS = "ABCDEFGHJKMNPQRSTUVWXYZabcdefghjkmnpqrstuvwxyz23456789";
static const size_t CONNECT_ROOM_CODE_LEN = 8;
static const size_t CONNECT_PASS_CODE_LEN = 32;

// normalize_share_code 79-100
static std::string n_normalize(const std::string & key) {
    std::string code;
    for (char c : key) {
        if (!std::isspace((unsigned char) c)) {
            code += c;
        }
    }
    if (code.size() != CONNECT_ROOM_CODE_LEN + CONNECT_PASS_CODE_LEN) {
        return "";
    }
    if (code.find_first_not_of(CONNECT_CODE_CHARS) != std::string::npos) {
        return "";
    }
    return code;
}

// format_share_code 102-112
static std::string n_format(const std::string & code) {
    std::string out;
    for (size_t i = 0; i < code.size(); i += 8) {
        if (i > 0) {
            out += ' ';
        }
        out += code.substr(i, 8);
    }
    return out;
}

// upstream_host 128-150
static std::string n_upstream(const std::vector<std::string> & hosts) {
    for (const auto & host : hosts) {
        if (host.size() >= 5 && host.compare(host.size() - 5, 5, ".sock") == 0) {
            continue;
        }
        if (host == "0.0.0.0") {
            return "127.0.0.1";
        }
        if (host == "::") {
            return "[::1]";
        }
        if (host.find(':') != std::string::npos && host[0] != '[') {
            return "[" + host + "]";
        }
        return host;
    }
    return "";
}

// unavailable_reason 168-190
static int n_unavailable(bool subproc, bool has_code, bool code_valid, bool has_tcp, bool binary_found, std::string & text) {
    if (!subproc) {
        text = "this build has subprocess support disabled, rebuild with -DLLAMA_SUBPROCESS=ON";
        return 1;
    }
    if (has_code && !code_valid) {
        text = "--connect-code must be " + std::to_string(CONNECT_ROOM_CODE_LEN + CONNECT_PASS_CODE_LEN) +
               " characters from '" + CONNECT_CODE_CHARS + "'";
        return 2;
    }
    if (!has_tcp) {
        text = "llama-connect reaches the server over TCP, add a TCP address to --host";
        return 3;
    }
    if (!binary_found) {
        text = "could not find 'llama-connect' next to llama-server or in PATH.\n"
               "    it is a separate binary: download it from https://github.com/ggml-org/llama-connect/releases\n"
               "    or build llama.cpp with -DLLAMA_CONNECT=ON to have it fetched automatically";
        return 4;
    }
    text.clear();
    return 0;
}

// forward_child_log 96-123: the tag table, first match wins
static int n_log_tag(const std::string & line, size_t & offset) {
    struct Tag {
        const char * text;
        size_t size;
        int level; // 0 error, 1 warning, 2 debug, 3 info
    };
    const Tag tags[] = {
        { "[ERROR] ", 8, 0 },
        { "[WARN] ",  7, 1 },
        { "[INFO] ",  7, 3 },
        { "[DEBUG] ", 8, 2 },
        { "[TRACE] ", 8, 2 },
    };
    for (const auto & t : tags) {
        if (line.compare(0, t.size, t.text) == 0) {
            offset = t.size;
            return t.level;
        }
    }
    offset = 0;
    return 3; // untagged lines are the startup banner, shown as info
}

// model_hf_repo 692-698
static std::string n_hf_repo(bool has_opt, const std::string & value, int source, const std::string & name) {
    if (has_opt && !value.empty()) {
        return value;
    }
    return source == 2 ? name : "";
}

// common/download.cpp 618-623
static const char * NATIVE_SIDECARS[] = { "mmproj", "mtp", "dflash", "eagle3", "dspark" };
static const bool NATIVE_DRAFT[] = { false, true, true, true, true };

// parse_sidecars 665-678 and check_sidecar 655-663
static std::vector<std::string> n_sidecars(bool absent, const std::string & list, std::string & err) {
    err.clear();
    std::vector<std::string> out;
    if (absent) {
        for (int i = 0; i < 5; i++) {
            if (!NATIVE_DRAFT[i]) {
                out.push_back(NATIVE_SIDECARS[i]);
            }
        }
        return out;
    }
    std::vector<std::string> kinds;
    size_t pos = 0;
    while (true) {
        size_t e = list.find(',', pos);
        if (e == std::string::npos) {
            e = list.size();
        }
        kinds.push_back(list.substr(pos, e - pos));
        if (e == list.size()) {
            break;
        }
        pos = e + 1;
    }
    for (const auto & kind : kinds) {
        bool known = false;
        for (int i = 0; i < 5; i++) {
            if (kind == NATIVE_SIDECARS[i]) {
                known = true;
            }
        }
        if (!known) {
            err = "unknown sidecar '" + kind + "', expected one of mmproj, mtp, dflash, eagle3, dspark";
            return {};
        }
        out.push_back(kind);
    }
    return out;
}

// the download child environment, load() 1427-1445
static void n_sidecar_env(const std::vector<bool> & sel, std::string & spec, bool & mmproj) {
    mmproj = false;
    spec.clear();
    for (int i = 0; i < 5; i++) {
        if (!sel[i]) {
            continue;
        }
        if (NATIVE_DRAFT[i]) {
            spec += (spec.empty() ? "" : ",") + std::string("draft-") + NATIVE_SIDECARS[i];
        } else {
            mmproj = true;
        }
    }
    if (spec.empty()) {
        spec = "none";
    }
}

// ── gate ───────────────────────────────────────────────────────────────────

static const std::string CODE = "ABCDEFGHJKMNPQRSTUVWXYZ23456789abcdefghj";

static void gate_normalize(Gate & g) {
    UBuf o;
    g.begin("normalize");
    std::string spaced;
    for (size_t i = 0; i < CODE.size(); i++) {
        if (i == 8 || i == 16 || i == 24 || i == 32) {
            spaced += ' ';
        }
        spaced += CODE[i];
    }
    const std::vector<std::string> cases = {
        CODE,
        spaced,
        " " + CODE + "\n",
        CODE.substr(0, 39),
        CODE + "Z",
        std::string(CODE).replace(0, 1, "I"),
        std::string(CODE).replace(0, 1, "0"),
        "",
        "ABCDEFGH",
    };
    for (const auto & c : cases) {
        o.zero();
        const int64_t n = cn_normalize_code(su8s(c), su8(o));
        const std::string want = n_normalize(c);
        g.value("length", n, want.empty() ? (int64_t) -1 : (int64_t) want.size());
        g.value("text", o.str(n < 0 ? 0 : n), want);
    }
    o.zero();
    g.expect("alphabet_len", cn_normalize_code(su8s(CODE), su8(o)), (int64_t) 40);
}

static void gate_format(Gate & g) {
    UBuf o;
    g.begin("format");
    for (const std::string & c : { CODE, std::string("ABCDEFGH"), std::string("") }) {
        o.zero();
        const int64_t n = cn_format_code(su8s(c), su8(o));
        g.value("text", o.str(n), n_format(c));
    }
}

static void gate_resolve(Gate & g) {
    UBuf o;
    g.begin("resolve_code");
    struct Case {
        int has;
        std::string code;
    };
    const Case cases[] = { { 0, "" }, { 1, CODE }, { 1, "bad" }, { 1, CODE.substr(0, 39) } };
    for (const auto & c : cases) {
        o.zero();
        const int64_t r = cn_resolve_code(c.has, su8s(c.code), su8(o));
        int64_t want = 1;
        if (c.has != 0) {
            want = n_normalize(c.code).empty() ? 0 : 2;
        }
        g.value("verdict", r, want);
    }
    g.begin("resolve_golden");
    o.zero();
    g.expect("no_code", cn_resolve_code(0, su8s(""), su8(o)), (int64_t) 1);
}

static void gate_upstream(Gate & g) {
    UBuf o;
    g.begin("upstream_host");
    const std::vector<std::vector<std::string>> cases = {
        { "/tmp/server.sock" },
        { "0.0.0.0" },
        { "::" },
        { "192.168.1.5" },
        { "fe80::1" },
        { "[::1]" },
        { "/tmp/a.sock", "127.0.0.1" },
        { "/tmp/a.sock", "::", "0.0.0.0" },
        {},
    };
    for (const auto & c : cases) {
        NameTable t;
        for (const auto & h : c) {
            t.add(h);
        }
        o.zero();
        const int64_t n = cn_upstream_host(t.n(), su8(t), si64(t.off), si64(t.len), su8(o));
        g.value("host", o.str(n), n_upstream(c));
    }
}

static void gate_unavailable(Gate & g) {
    UBuf o;
    g.begin("unavailable_reason");
    struct Case {
        int subproc, has_code, code_valid, has_tcp, binary;
    };
    const Case cases[] = {
        { 1, 0, 0, 1, 1 },
        { 0, 1, 1, 0, 0 },
        { 1, 1, 0, 1, 1 },
        { 1, 1, 0, 0, 0 },
        { 1, 1, 1, 0, 0 },
        { 1, 0, 0, 0, 0 },
        { 1, 1, 1, 1, 0 },
        { 0, 0, 0, 0, 0 },
    };
    for (const auto & c : cases) {
        o.zero();
        const int64_t reason = cn_unavailable(c.subproc, c.has_code, c.code_valid, c.has_tcp, c.binary, su8(o));
        std::string text;
        const int want = n_unavailable(c.subproc != 0, c.has_code != 0, c.code_valid != 0, c.has_tcp != 0, c.binary != 0, text);
        g.value("reason", reason, want);
        g.value("text", o.str(want == 0 ? 0 : (int64_t) text.size()), text);
    }
    // the four texts are fixed contracts, so the important ones are pinned too
    g.begin("unavailable_golden");
    o.zero();
    cn_unavailable(1, 0, 0, 1, 1, su8(o));
    g.expect("none", o.str(0).empty() ? (int64_t) 1 : (int64_t) 0, (int64_t) 1);
    o.zero();
    g.expect("subproc", cn_unavailable(0, 0, 0, 1, 1, su8(o)), (int64_t) 1);
    o.zero();
    const int64_t n = cn_unavailable(1, 1, 1, 1, 0, su8(o));
    g.expect("binary", n, (int64_t) 4);
    g.expect("binary_lines", (int64_t) std::count(o.b.data(), o.b.data() + 200, '\n'), (int64_t) 2);
}

static void gate_binary(Gate & g) {
    g.begin("find_binary");
    struct Case {
        int sibling;
        std::vector<int> exists;
    };
    const Case cases[] = {
        { 1, {} },
        { 0, { 1, 0, 0 } },
        { 0, { 0, 1, 1 } },
        { 0, { 0, 0, 0 } },
        { 1, { 1 } },
        { 0, {} },
    };
    for (const auto & c : cases) {
        std::vector<int64_t> ex(c.exists.begin(), c.exists.end());
        const int64_t r = cn_find_binary(c.sibling, (int64_t) ex.size(), si64(ex));
        int64_t want = -1;
        if (c.sibling != 0) {
            want = 0;
        } else {
            for (size_t i = 0; i < c.exists.size(); i++) {
                if (c.exists[i] != 0) {
                    want = (int64_t) i + 1;
                    break;
                }
            }
        }
        g.value("candidate", r, want);
    }
}

static void gate_log(Gate & g) {
    IBuf o(8);
    g.begin("log_tag");
    const std::vector<std::string> cases = {
        "[ERROR] failed to spawn\n",
        "[WARN] nobody\n",
        "[INFO] ready\n",
        "[DEBUG] detail\n",
        "[TRACE] deep\n",
        "llama-connect 1.2.3\n",
        "",
        " [ERROR] leading space\n",
        "[error] lower\n",
    };
    for (const auto & c : cases) {
        o.zero();
        const int64_t off = cn_log_tag(su8s(c), si64(o));
        size_t want_off = 0;
        const int want_level = n_log_tag(c, want_off);
        g.value("level", o.v[0], (int64_t) want_level);
        g.value("offset", off, (int64_t) want_off);
        g.value("offset_word", o.v[1], (int64_t) want_off);
    }
}

static void gate_hf_repo(Gate & g) {
    UBuf o;
    g.begin("model_hf_repo");
    struct Case {
        int has;
        std::string value;
        int source;
        std::string name;
    };
    const Case cases[] = {
        { 1, "ggml-org/qwen3-8b-GGUF", 0, "preset_a" },
        { 0, "", 2, "ggml-org/llama-7b" },
        { 0, "", 0, "preset_a" },
        { 0, "", 1, "local/thing.gguf" },
        { 1, "", 2, "ggml-org/llama-7b" },
        { 1, "own/repo", 2, "ggml-org/llama-7b" },
    };
    for (const auto & c : cases) {
        o.zero();
        const int64_t n = cn_model_hf_repo(c.has, su8s(c.value), c.source, su8s(c.name), su8(o));
        g.value("repo", o.str(n), n_hf_repo(c.has != 0, c.value, c.source, c.name));
    }
}

static void gate_sidecars(Gate & g) {
    UBuf o;
    g.begin("sidecars");
    struct Case {
        int absent;
        std::string list;
    };
    const Case cases[] = {
        { 1, "" },
        { 0, "mmproj" },
        { 0, "mmproj,mtp" },
        { 0, "mmproj,bogus" },
        { 0, " mmproj " },
        { 0, "" },
        { 0, "mmproj,mtp,dflash,eagle3,dspark" },
        { 0, "dspark,mmproj" },
    };
    for (const auto & c : cases) {
        o.zero();
        IBuf meta(4);
        const int64_t n = cn_sidecars(c.absent, su8s(c.list), su8(o), si64(meta));
        std::string err;
        const std::vector<std::string> want = n_sidecars(c.absent != 0, c.list, err);
        const std::string text = err.empty() ? join(want, ",") : err;
        g.value("count", meta.v[0], (int64_t) want.size());
        g.value("verdict", n < 0 ? (int64_t) -1 : (int64_t) 0, err.empty() ? (int64_t) 0 : (int64_t) -1);
        g.value("text", o.str((int64_t) text.size()), text);
    }
}

static void gate_sidecar_env(Gate & g) {
    IBuf o(8);
    UBuf spec;
    g.begin("sidecar_env");
    const std::vector<std::vector<bool>> cases = {
        { false, false, false, false, false },
        { true, false, false, false, false },
        { false, true, false, false, false },
        { false, true, true, false, true },
        { true, true, true, true, true },
        { false, false, false, true, false },
    };
    for (const auto & c : cases) {
        std::vector<int64_t> sel;
        for (bool b : c) {
            sel.push_back(b ? 1 : 0);
        }
        o.zero();
        spec.zero();
        const int64_t n = cn_sidecar_env(si64(sel), su8(spec), si64(o));
        std::string want_spec;
        bool want_mmproj = false;
        n_sidecar_env(c, want_spec, want_mmproj);
        g.value("spec", spec.str(n), want_spec);
        g.value("mmproj", o.v[0], (int64_t) want_mmproj);
    }
    g.begin("sidecar_env_golden");
    std::vector<int64_t> sel = { 1, 1, 0, 0, 0 };
    o.zero();
    spec.zero();
    cn_sidecar_env(si64(sel), su8(spec), si64(o));
    g.expect("mmproj_and_mtp", spec.str(9) == "draft-mtp" ? (int64_t) 1 : (int64_t) 0, (int64_t) 1);
    g.expect("mmproj_flag", o.v[0], (int64_t) 1);
}

int main() {
    goose_init(0, nullptr);

    Gate g("CN");
    gate_normalize(g);
    gate_format(g);
    gate_resolve(g);
    gate_upstream(g);
    gate_unavailable(g);
    gate_binary(g);
    gate_log(g);
    gate_hf_repo(g);
    gate_sidecars(g);
    gate_sidecar_env(g);
    return g.finish() == 0 ? 0 : 1;
}
