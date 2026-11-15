// Differential gate for the platform-path slice.
//
// Oracle: a transcription of common/common.cpp with the platform lifted from a
// compile-time branch to a parameter, because the host can only compile one
// branch at a time and the point of the slice is that all of them agree:
//   fs_get_cache_directory   common.cpp:1019-1071
//   fs_get_config_directory  common.cpp:1073-1116
//   fs_get_cache_file        common.cpp:1118-1127
//   common_set_env           common.cpp:1007-1017
//   set_process_priority     common.cpp:232-283 (both branches)
//   tty_can_use_colors       common.cpp:1185-1202
//   DIRECTORY_SEPARATOR      common.h:25-29
//
// Port: source/plat-paths.goose.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>

#include "plat.h"

static int g_checks = 0, g_fails = 0;

static sl_u8 S(const char * s) {
    return sl_u8{(uint8_t *) const_cast<char *>(s), (int64_t) std::strlen(s)};
}

static void expect(const char * group, const char * what, int64_t got, int64_t want) {
    ++g_checks;
    if (got != want) {
        ++g_fails;
        if (g_fails <= 40) {
            std::printf("FAIL %s / %s: got %lld want %lld\n", group, what,
                        (long long) got, (long long) want);
        }
    }
}

static void expect_str(const char * group, const char * what, const std::string & got,
                       const std::string & want) {
    ++g_checks;
    if (got != want) {
        ++g_fails;
        if (g_fails <= 40) {
            std::printf("FAIL %s / %s: got '%s' want '%s'\n", group, what,
                        got.c_str(), want.c_str());
        }
    }
}

// ---- transcription --------------------------------------------------------

enum { NAT_LINUX = 0, NAT_BSD = 1, NAT_APPLE = 2, NAT_WIN = 3, NAT_EMSCRIPTEN = 4 };

static char nat_sep(int plat) { return plat == NAT_WIN ? '\\' : '/'; }

static std::string nat_ensure(std::string p, char sep) {
    if (p.empty() || p.back() != sep) { p += sep; } // common.cpp:1022-1024
    return p;
}

// common.cpp:1019-1071.
static std::string nat_cache_dir(int plat, const std::string & ll, const std::string & xdg,
                                 const std::string & home, const std::string & pw,
                                 const std::string & lad, int64_t & status) {
    const char sep = nat_sep(plat);
    std::string c = ll;
    status = 0;
    if (c.empty()) {
        if (plat == NAT_LINUX || plat == NAT_BSD) {
            if (!xdg.empty()) {
                c = xdg;
            } else if (!home.empty()) {
                c = home + "/.cache/";
            } else if (plat == NAT_LINUX) {
                if (pw.empty()) { status = -1; return {}; } // getpwuid failure
                c = pw + "/.cache/";
            } else {
                status = -1; return {};
            }
        } else if (plat == NAT_APPLE) {
            c = home;
            if (c.empty()) { status = -1; return {}; }
            c += "/Library/Caches/";
        } else if (plat == NAT_WIN) {
            c = lad;
            if (c.empty()) { status = -1; return {}; }
        } else {
            status = -2; return {}; // GGML_ABORT
        }
        c = nat_ensure(c, sep);
        c += "llama.cpp";
    }
    return nat_ensure(c, sep);
}

// common.cpp:1073-1116.
static std::string nat_config_dir(int plat, const std::string & xdg, const std::string & home,
                                  const std::string & pw, const std::string & appdata,
                                  int64_t & status) {
    const char sep = nat_sep(plat);
    std::string c;
    status = 0;
    if (plat == NAT_LINUX || plat == NAT_BSD || plat == NAT_APPLE) {
        if (!xdg.empty()) {
            c = xdg;
        } else if (!home.empty()) {
            c = home + "/.config/";
        } else if (plat == NAT_LINUX) {
            if (pw.empty()) { status = -1; return {}; }
            c = pw + "/.config/";
        } else {
            status = -1; return {};
        }
    } else if (plat == NAT_WIN) {
        c = appdata;
        if (c.empty()) { status = -1; return {}; }
    } else {
        status = -2; return {}; // "not implemented on this platform"
    }
    c = nat_ensure(c, sep);
    c += "llama.cpp";
    return nat_ensure(c, sep);
}

// common.cpp:1118-1127.
static std::string nat_cache_file(int plat, const std::string & ll, const std::string & xdg,
                                  const std::string & home, const std::string & pw,
                                  const std::string & lad, const std::string & filename,
                                  int64_t & status) {
    if (filename.find(nat_sep(plat)) != std::string::npos) { status = -3; return {}; }
    std::string c = nat_cache_dir(plat, ll, xdg, home, pw, lad, status);
    if (status != 0) { return {}; }
    return c + filename;
}

// ---- gate drivers ---------------------------------------------------------

static void gate_cache(const char * id, int plat, const char * ll, const char * xdg,
                       const char * home, const char * pw, const char * lad) {
    int64_t want_status = 0;
    const std::string want = nat_cache_dir(plat, ll, xdg, home, pw, lad, want_status);

    uint8_t out[512];
    std::memset(out, 0, sizeof(out));
    const int64_t got_status = plat_cache_dir(plat, S(ll), S(xdg), S(home), S(pw), S(lad),
                                              sl_u8{out, (int64_t) sizeof(out)});
    expect(id, "status", got_status >= 0 ? 0 : got_status, want_status);
    if (want_status == 0) {
        expect_str(id, "path", std::string((const char *) out, (size_t) got_status), want);
    }
}

static void gate_config(const char * id, int plat, const char * xdg, const char * home,
                        const char * pw, const char * appdata) {
    int64_t want_status = 0;
    const std::string want = nat_config_dir(plat, xdg, home, pw, appdata, want_status);

    uint8_t out[512];
    std::memset(out, 0, sizeof(out));
    const int64_t got_status = plat_config_dir(plat, S(xdg), S(home), S(pw), S(appdata),
                                               sl_u8{out, (int64_t) sizeof(out)});
    expect(id, "status", got_status >= 0 ? 0 : got_status, want_status);
    if (want_status == 0) {
        expect_str(id, "path", std::string((const char *) out, (size_t) got_status), want);
    }
}

static void gate_file(const char * id, int plat, const char * ll, const char * xdg,
                      const char * home, const char * pw, const char * lad,
                      const char * filename) {
    int64_t want_status = 0;
    const std::string want = nat_cache_file(plat, ll, xdg, home, pw, lad, filename, want_status);

    uint8_t out[512];
    std::memset(out, 0, sizeof(out));
    const int64_t got_status = plat_cache_file(plat, S(ll), S(xdg), S(home), S(pw), S(lad), S(filename),
                                               sl_u8{out, (int64_t) sizeof(out)});
    expect(id, "status", got_status >= 0 ? 0 : got_status, want_status);
    if (want_status == 0) {
        expect_str(id, "path", std::string((const char *) out, (size_t) got_status), want);
    }
}

int main() {
    goose_init(0, nullptr);
    setvbuf(stdout, nullptr, _IONBF, 0);

    // ---- separator: 4 cases ----------------------------------------------
    expect("sep linux", "47", plat_sep(NAT_LINUX), '/');
    expect("sep bsd",   "47", plat_sep(NAT_BSD), '/');
    expect("sep apple", "47", plat_sep(NAT_APPLE), '/');
    expect("sep win",   "92", plat_sep(NAT_WIN), '\\');

    // ---- cache directory: 15 cases ---------------------------------------
    gate_cache("C0  llama_cache",        NAT_LINUX, "/cache/lc", "", "", "", "");
    gate_cache("C1  llama_cache slash",  NAT_LINUX, "/cache/lc/", "", "", "", "");
    gate_cache("C2  xdg",                NAT_LINUX, "", "/x/c", "", "", "");
    gate_cache("C3  xdg trailing",       NAT_LINUX, "", "/x/c/", "", "", "");
    gate_cache("C4  home",               NAT_LINUX, "", "", "/home/u", "", "");
    gate_cache("C5  getpwuid",           NAT_LINUX, "", "", "", "/home/pw", "");
    gate_cache("C6  no home",            NAT_LINUX, "", "", "", "", "");
    gate_cache("C7  bsd no pw fallback", NAT_BSD,   "", "", "", "/home/pw", "");
    gate_cache("C8  bsd xdg",            NAT_BSD,   "", "/x", "", "", "");
    gate_cache("C9  apple home",         NAT_APPLE, "", "", "/Users/u", "", "");
    gate_cache("C10 apple no home",      NAT_APPLE, "", "", "", "", "");
    gate_cache("C11 win localappdata",   NAT_WIN,   "", "", "", "", "C:\\Users\\u\\AppData\\Local");
    gate_cache("C12 win no localappdata", NAT_WIN,  "", "", "", "", "");
    gate_cache("C13 win llama_cache",    NAT_WIN,   "C:\\cache", "", "", "", "");
    gate_cache("C14 emscripten",         NAT_EMSCRIPTEN, "", "", "", "", "");

    // ---- config directory: 11 cases -------------------------------------
    gate_config("G0  linux xdg",        NAT_LINUX, "/x/cfg", "", "", "");
    gate_config("G1  linux home",       NAT_LINUX, "", "/home/u", "", "");
    gate_config("G2  linux getpwuid",   NAT_LINUX, "", "", "/home/pw", "");
    gate_config("G3  linux none",       NAT_LINUX, "", "", "", "");
    gate_config("G4  bsd no pw",        NAT_BSD,   "", "", "/home/pw", "");
    gate_config("G5  apple xdg",        NAT_APPLE, "/x/cfg", "", "", "");
    gate_config("G6  apple home",       NAT_APPLE, "", "/Users/u", "", "");
    gate_config("G7  apple none",       NAT_APPLE, "", "", "", "");
    gate_config("G8  win appdata",      NAT_WIN,   "", "", "", "C:\\Users\\u\\AppData\\Roaming");
    gate_config("G9  win none",         NAT_WIN,   "", "", "", "");
    gate_config("G10 emscripten",       NAT_EMSCRIPTEN, "", "", "", "");

    // ---- cache file: 6 cases --------------------------------------------
    gate_file("F0  file",          NAT_LINUX, "/cache/lc", "", "", "", "", "x.bin");
    gate_file("F1  file slash",    NAT_LINUX, "/cache/lc", "", "", "", "", "a/b");
    gate_file("F2  file backslash", NAT_WIN,  "C:\\lc", "", "", "", "", "a\\b");
    gate_file("F3  file win",      NAT_WIN,   "C:\\lc", "", "", "", "", "x.bin");
    gate_file("F4  file home",     NAT_LINUX, "", "", "/home/u", "", "", "k.json");
    gate_file("F5  file no cache", NAT_LINUX, "", "", "", "", "", "k.json");

    // ---- environment write: 3 cases -------------------------------------
    expect("E0 win empty",      "putenv_s", plat_set_env(NAT_WIN, 1), 2);
    expect("E1 posix empty",    "unsetenv", plat_set_env(NAT_LINUX, 1), 0);
    expect("E2 posix non-empty", "setenv",  plat_set_env(NAT_LINUX, 0), 1);

    // ---- priority: 12 cases ---------------------------------------------
    const int64_t prios[] = { 0, 1, 2, 3, 4 };
    const int64_t posix_want[] = { 5, 0, -5, -10, -20 };
    const int64_t win_want[] = { 16384, 32, 32768, 128, 256 };
    for (int i = 0; i < 5; ++i) {
        char id[32];
        std::snprintf(id, sizeof(id), "P%d posix prio", i);
        expect(id, "value", plat_priority(NAT_LINUX, prios[i]), posix_want[i]);
    }
    for (int i = 0; i < 5; ++i) {
        char id[32];
        std::snprintf(id, sizeof(id), "P%d win prio", i);
        expect(id, "value", plat_priority(NAT_WIN, prios[i]), win_want[i]);
    }
    expect("P10 normal noop", "1", plat_priority_noop(1), 1);
    expect("P11 low noop",    "0", plat_priority_noop(0), 0);

    // ---- tty colours: 6 cases -------------------------------------------
    expect("T0 no_color",      "0", plat_tty_colors(S("1"), S("xterm"), 1, 1), 0);
    expect("T1 term dumb",     "0", plat_tty_colors(S(""), S("dumb"), 1, 1), 0);
    expect("T2 stdout tty",    "1", plat_tty_colors(S(""), S("xterm"), 1, 0), 1);
    expect("T3 stderr tty",    "1", plat_tty_colors(S(""), S("xterm-256color"), 0, 1), 1);
    expect("T4 no tty",        "0", plat_tty_colors(S(""), S("xterm"), 0, 0), 0);
    expect("T5 no_color wins", "0", plat_tty_colors(S("0"), S("dumb"), 1, 1), 0);

    std::printf("plat: %d checks, %d failures\n", g_checks, g_fails);
    return g_fails == 0 ? 0 : 1;
}
