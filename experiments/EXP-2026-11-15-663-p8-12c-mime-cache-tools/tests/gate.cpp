// Differential gate for the promoted P8-12 server call sites. No model, no GPU,
// no server: the gate links the shipped decision functions
// (tools/server/fg-goose-server/fg-goose-sites.h) together with the shipped
// bridge and switches objects, and compares every answer with a frozen copy of
// the native bodies:
//
//   server-models.h:55   server_model_status_to_string   the 7-case switch
//   server-http.cpp:77   origin_is_localhost             parse + three compares
//   ui-assets.cmake:28   mime_from_ext                   the extension table
//   server-http.cpp:403  cache_immutable / cache_revalidate   the route literals
//
// The last two bodies were deleted with EXP662, so their references live here,
// frozen, and nothing else carries them any more.
//   server-tools.cpp:1936 find_tool                      the linear scan
//
// Both arms are measured through one binary. "ref" means the native answer (the
// gate must reproduce it byte for byte through the Goose arm); "unknown" and
// "stub" are the documented answers of a build without the unit, used to prove
// that the OFF path no longer carries the native body.
//
// Two sites measure the module contract directly, not through a call site,
// because the call sites fall back to the native value when the module answers
// nothing: "mime-raw" and "cache-raw" call fgs_mime_text / fgs_cache_text and
// require an answer where one is owed. Without them a mutation that makes the
// module return -1 would hide behind the fallback.
//
// Usage:
//   gate [--status ref|unknown] [--cors ref|false] [--mime ref|stub]
//        [--cache ref|stub] [--cache-raw ref|none] [--mime-raw ref|none]
//        [--tools ref] [--tools-raw ref|none] [--native]
//
// "--native" exports the two remaining *_NATIVE variables before the first
// call, so the same run measures each native arm instead of the Goose arm.
//
// Exit code 0 when every case matches its expectation, 1 otherwise. Every line
// is TSV: site, case, answer, expectation, verdict.

#include "fg-goose-sites.h"
#include "ui.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_cases = 0;
int g_bad   = 0;

std::string printable(const std::string & s) {
    if (s.empty()) {
        return "<empty>";
    }
    std::string out;
    for (char c : s) {
        if (c == '\t' || c == '\n') {
            out += '?';
        } else {
            out += c;
        }
    }
    return out;
}

void emit(const char * site, const std::string & id, const std::string & got, const std::string & want) {
    ++g_cases;
    const bool ok = got == want;
    if (!ok) {
        ++g_bad;
    }
    printf("%s\t%s\t%s\t%s\t%s\n", site, printable(id).c_str(), printable(got).c_str(),
           printable(want).c_str(), ok ? "OK" : "DIFF");
}

// Frozen native body: tools/server/server-models.h:55.
std::string ref_status(int64_t status) {
    switch (status) {
        case 0:  return "downloading";
        case 1:  return "incomplete";
        case 2:  return "downloaded";
        case 3:  return "unloaded";
        case 4:  return "loading";
        case 5:  return "loaded";
        case 6:  return "sleeping";
        default: return "unknown";
    }
}

// Frozen native body: tools/server/server-http.cpp:77.
bool ref_origin_is_localhost(const std::string & origin) {
    try {
        const std::string host = common_http_parse_url(origin).host;
        return host == "localhost" || host == "127.0.0.1" || host == "::1";
    } catch (const std::exception &) {
        return false;
    }
}

// Frozen native body: scripts/ui-assets.cmake:28 mime_from_ext. The extension is
// everything after the LAST '.' of the whole name; the compare is byte exact and
// case sensitive; anything unknown is the octet-stream fallback, which is also
// the answer for a name without a dot and for an empty extension.
std::string ref_mime_from_ext(const std::string & name) {
    std::string ext;
    const size_t dot = name.rfind('.');
    if (dot != std::string::npos) {
        ext = name.substr(dot + 1);
    }
    if (ext == "html")        return "text/html; charset=utf-8";
    if (ext == "css")         return "text/css";
    if (ext == "js")          return "application/javascript";
    if (ext == "json")        return "application/json";
    if (ext == "webmanifest") return "application/manifest+json";
    if (ext == "svg")         return "image/svg+xml";
    if (ext == "png")         return "image/png";
    if (ext == "jpg")         return "image/jpeg";
    if (ext == "jpeg")        return "image/jpeg";
    if (ext == "ico")         return "image/x-icon";
    if (ext == "woff")        return "font/woff";
    if (ext == "woff2")       return "font/woff2";
    return "application/octet-stream";
}

// The asset type both arms answer a build without the unit.
const char * const STUB_MIME = "application/octet-stream";

// The Cache-Control text both arms answer a build without the unit: the
// conservative revalidate answer, never the immutable one.
const char * const STUB_CACHE = "no-cache";

// The route literals of server-http.cpp:403-404 and 466.
const char * const CACHE_IMMUTABLE  = "public, max-age=31536000, immutable";
const char * const CACHE_REVALIDATE = "no-cache";
const char * const CACHE_NOCACHE    = "no-cache";

// Frozen copy of the native tool table order, server-tools.cpp:1957-1963.
const char * const TOOL_NAMES[7] = {
    "read_file", "file_glob_search", "grep_search", "exec_shell_command",
    "write_file", "edit_file", "get_info",
};

// Frozen copy of the native find_tool loop: the position of `name` in `table`,
// or -1 when the table does not hold it.
int64_t native_tool_slot(const std::vector<std::string> & table, const std::string & name) {
    for (size_t i = 0; i < table.size(); ++i) {
        if (table[i] == name) {
            return (int64_t) i;
        }
    }
    return -1;
}

// Frozen copy of the call-site rule at server-tools.cpp:1937-1950: the module's
// index is trusted only while the table still holds that name at that position;
// otherwise the native loop answers, which is -1 in this model only when the
// table truly lacks the name.
int64_t site_tool_slot(const std::vector<std::string> & table, const std::string & name) {
    const int64_t idx = fgs_tool_index(name.data(), (int64_t) name.size());
    if (idx >= 0 && (size_t) idx < table.size() && table[idx] == name) {
        return idx;
    }
    return native_tool_slot(table, name);
}

std::string bool_text(bool v) {
    return v ? "true" : "false";
}

// One bridge answer as text: "ERR" for the -1 the unit or the call site returns
// when it has nothing to say, the bytes otherwise.
std::string answer_text(int64_t n, const char * buf) {
    if (n < 0) {
        return "ERR";
    }
    return std::string(buf, (size_t) n);
}

} // namespace

int main(int argc, char ** argv) {
    std::string status_expect = "ref";
    std::string cors_expect   = "ref";
    std::string mime_expect   = "ref";
    std::string cache_expect  = "ref";
    std::string tools_expect  = "ref";
    std::string cache_raw     = "ref";
    std::string mime_raw      = "ref";
    std::string tools_raw     = "ref";
    bool force_native         = false;

    for (int i = 1; i < argc; ++i) {
        const char * a = argv[i];
        const char * v = (i + 1 < argc) ? argv[i + 1] : nullptr;
        if (strcmp(a, "--native") == 0) {
            force_native = true;
        } else if (strcmp(a, "--status") == 0 && v) {
            status_expect = v; ++i;
        } else if (strcmp(a, "--cors") == 0 && v) {
            cors_expect = v; ++i;
        } else if (strcmp(a, "--mime") == 0 && v) {
            mime_expect = v; ++i;
        } else if (strcmp(a, "--cache") == 0 && v) {
            cache_expect = v; ++i;
        } else if (strcmp(a, "--tools") == 0 && v) {
            tools_expect = v; ++i;
        } else if (strcmp(a, "--cache-raw") == 0 && v) {
            cache_raw = v; ++i;
        } else if (strcmp(a, "--mime-raw") == 0 && v) {
            mime_raw = v; ++i;
        } else if (strcmp(a, "--tools-raw") == 0 && v) {
            tools_raw = v; ++i;
        } else {
            fprintf(stderr, "unknown argument: %s\n", a);
            return 2;
        }
    }

    // Before the first switched decision: the switches are cached on first use.
    // Only two sites still have a native arm; the other three lost theirs with
    // the body it could restore.
    if (force_native) {
        setenv(FG_GOOSE_ENV_CORS_NATIVE,  "1", 1);
        setenv(FG_GOOSE_ENV_TOOLS_NATIVE, "1", 1);
    }

    // ── model status: 7 ids, plus one below and one above the enum ───────────
    const int64_t statuses[9] = { 0, 1, 2, 3, 4, 5, 6, -1, 7 };
    for (int i = 0; i < 9; ++i) {
        const int64_t id = statuses[i];
        const std::string got = fg_goose_model_status_text(id);
        const std::string want = status_expect == "ref" ? ref_status(id) : "unknown";
        emit("status", std::to_string(id), got, want);
    }

    // ── Origin host: 3 local hosts and 3 refusals ────────────────────────────
    const char * origins[6] = {
        "http://localhost:8080", "https://127.0.0.1", "http://[::1]:9000",
        "https://example.com", "http://127.0.0.1.evil.com", "ftp://localhost",
    };
    for (int i = 0; i < 6; ++i) {
        const bool got  = fg_goose_origin_is_localhost(origins[i]);
        const bool want = cors_expect == "ref" && ref_origin_is_localhost(origins[i]);
        emit("cors", origins[i], bool_text(got), bool_text(want));
    }

    // ── asset content type: one name per extension plus the edge cases ───────
    const char * mime_names[] = {
        "index.html",
        "_app/immutable/entry.js",
        "sw.js",
        "manifest.webmanifest",
        "build.json",
        "_app/version.json",
        "favicon.svg",
        "logo.png",
        "photo.jpg",
        "photo.jpeg",
        "favicon.ico",
        "font.woff",
        "font.woff2",
        "noext",          // no dot at all
        "index.",         // empty extension
        "INDEX.HTML",     // case sensitive table
        "_app/x.y/z",     // the last dot lives in a directory component
        "_app/x.y/z.js",  // two dots: only the last one counts
        "",               // empty name
    };
    for (const char * n : mime_names) {
        const std::string got  = fg_goose_asset_type(n);
        const std::string want = mime_expect == "ref" ? ref_mime_from_ext(n) : STUB_MIME;
        emit("mime", n, got, want);
    }

    // ── asset content type: every name the build actually embedded ───────────
    // The expectation is the type the table carries, i.e. what the native serve
    // path answered before the switch.
    for (const auto & a : llama_ui_get_assets()) {
        const std::string got  = fg_goose_asset_type(a.name);
        const std::string want = mime_expect == "ref" ? a.type : STUB_MIME;
        emit("mime-asset", a.name, got, want);
    }

    // ── Cache-Control: the three routes the server registers, plus ids no
    // route uses (the module must not return garbage for one) ────────────────
    struct cache_case { int64_t policy; const char * native_text; const char * id; };
    const cache_case caches[10] = {
        { FGS_POLICY_IMMUTABLE,  CACHE_IMMUTABLE,  "0 immutable" },
        { FGS_POLICY_REVALIDATE, CACHE_REVALIDATE, "1 revalidate" },
        { FGS_POLICY_NOCACHE,    CACHE_NOCACHE,    "2 nocache" },
        { -1,                    CACHE_NOCACHE,    "-1 out of range" },
        { -2,                    CACHE_NOCACHE,    "-2 out of range" },
        { 3,                     CACHE_NOCACHE,    "3 out of range" },
        { 4,                     CACHE_NOCACHE,    "4 out of range" },
        { 100,                   CACHE_NOCACHE,    "100 out of range" },
        { INT64_MIN,             CACHE_NOCACHE,    "INT64_MIN out of range" },
        { INT64_MAX,             CACHE_NOCACHE,    "INT64_MAX out of range" },
    };
    for (const cache_case & c : caches) {
        const std::string got  = fg_goose_cache_text(c.policy);
        const std::string want = cache_expect == "ref" ? c.native_text : STUB_CACHE;
        emit("cache", c.id, got, want);
    }

    // ── tool dispatch: module answer against the native scan ─────────────────
    const std::vector<std::string> builtin(TOOL_NAMES, TOOL_NAMES + 7);
    const char * tool_names[] = {
        "read_file", "file_glob_search", "grep_search", "exec_shell_command",
        "write_file", "edit_file", "get_info",
        "Read_File",          // case differs
        "read_fil",           // prefix
        "read_file ",         // trailing space
        "",                   // empty name
        "mcp__fs__read_file", // MCP-only name
    };
    for (const char * n : tool_names) {
        const int64_t got  = site_tool_slot(builtin, n);
        const int64_t want = native_tool_slot(builtin, n);
        emit("tools", n, std::to_string(got), std::to_string(want));
    }

    // ── tool dispatch: the table shapes the call site must survive ───────────
    {
        std::vector<std::string> reversed = builtin;
        std::reverse(reversed.begin(), reversed.end());

        std::vector<std::string> appended = builtin;
        appended.push_back("mcp__fs__read_file");

        const std::vector<std::string> empty;

        struct table_case { const char * id; const std::vector<std::string> * table; const char * name; };
        const table_case tables[4] = {
            { "builtin order, grep_search",   &builtin,  "grep_search" },
            { "reordered table, grep_search", &reversed, "grep_search" },
            { "MCP appended, mcp name",       &appended, "mcp__fs__read_file" },
            { "empty table, read_file",       &empty,    "read_file" },
        };
        for (const table_case & t : tables) {
            const int64_t got  = site_tool_slot(*t.table, t.name);
            const int64_t want = native_tool_slot(*t.table, t.name);
            emit("tools-table", t.id, std::to_string(got), std::to_string(want));
        }
    }

    // ── module contracts: an answer must exist, not fall back ────────────────
    for (const char * n : { "index.html", "sw.js", "noext", "INDEX.HTML", "" }) {
        char buf[64];
        const int64_t got_n = fgs_mime_text(n, (int64_t) strlen(n), buf, (int64_t) sizeof(buf));
        const std::string got = answer_text(got_n, buf);
        const std::string want = mime_raw == "ref" ? ref_mime_from_ext(n) : "ERR";
        emit("mime-raw", n, got, want);
    }

    for (int64_t policy : { (int64_t) 0, (int64_t) 1, (int64_t) 2, (int64_t) 3, (int64_t) -1 }) {
        char buf[64];
        const int64_t got_n = fgs_cache_text(policy, buf, (int64_t) sizeof(buf));
        const std::string got = answer_text(got_n, buf);
        std::string want;
        if (cache_raw != "ref") {
            want = "ERR";
        } else if (policy == 0) {
            want = CACHE_IMMUTABLE;
        } else if (policy == 1 || policy == 2) {
            want = CACHE_NOCACHE;
        } else {
            want = "ERR";  // no route uses such an id; the module must not invent one
        }
        emit("cache-raw", std::to_string(policy), got, want);
    }

    for (const char * n : { "read_file", "file_glob_search", "grep_search",
                            "exec_shell_command", "write_file", "edit_file", "get_info",
                            "nope", "", "read_fil" }) {
        const int64_t got_n = fgs_tool_index(n, (int64_t) strlen(n));
        const std::string got = got_n < 0 ? "ERR" : std::to_string(got_n);
        std::string want;
        if (tools_raw != "ref") {
            want = "ERR";
        } else {
            const int64_t idx = native_tool_slot(builtin, n);
            want = idx < 0 ? "ERR" : std::to_string(idx);
        }
        emit("tools-raw", n, got, want);
    }

    printf("# cases=%d divergences=%d\n", g_cases, g_bad);
    (void) tools_expect; // the tool site has one expectation: the native scan
    return g_bad == 0 ? 0 : 1;
}
