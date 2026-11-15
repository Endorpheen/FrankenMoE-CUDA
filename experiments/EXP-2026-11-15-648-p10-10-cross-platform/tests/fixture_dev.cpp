// Differential gate for the backend/device selection slice.
//
// Oracle: a transcription of ggml/src/ggml-backend-reg.cpp with the registry
// replaced by plain vectors, because the native code needs real dl handles and
// real devices. The file-name test is the exception: it calls the real
// std::filesystem::path, so extension edge cases are compared against the
// standard library rather than retyped.
//
// Port: source/dev-backend-select.goose.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>

#include "dev.h"

static int g_checks = 0, g_fails = 0;

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

static sl_u8 S(const char * s) {
    return sl_u8{(uint8_t *) const_cast<char *>(s), (int64_t) std::strlen(s)};
}

// ---- transcription --------------------------------------------------------

enum { NAT_LINUX = 0, NAT_BSD = 1, NAT_APPLE = 2, NAT_WIN = 3 };
enum { NAT_CPU = 0, NAT_GPU = 1, NAT_IGPU = 2, NAT_ACCEL = 3, NAT_META = 4 };

// ggml-backend-reg.cpp:307-315.
static bool nat_striequals(const char * a, const char * b) {
    for (; *a && *b; a++, b++) {
        if ((unsigned char) std::tolower((unsigned char) *a) !=
            (unsigned char) std::tolower((unsigned char) *b)) {
            return false;
        }
    }
    return *a == *b;
}

// ggml-backend-reg.cpp:344-363.
static int64_t nat_by_name(const std::vector<std::string> & names, const std::string & n) {
    for (size_t i = 0; i < names.size(); ++i) {
        if (nat_striequals(names[i].c_str(), n.c_str())) { return (int64_t) i; }
    }
    return -1;
}

static int64_t nat_by_type(const std::vector<int64_t> & types, int64_t want) {
    for (size_t i = 0; i < types.size(); ++i) {
        if (types[i] == want) { return (int64_t) i; }
    }
    return -1;
}

// ggml-backend-reg.cpp:381-390.
static int64_t nat_init_best(const std::vector<int64_t> & types) {
    int64_t d = nat_by_type(types, NAT_GPU);
    if (d < 0) { d = nat_by_type(types, NAT_IGPU); }
    if (d < 0) { d = nat_by_type(types, NAT_CPU); }
    return d;
}

// ggml-backend-reg.cpp:531-547.
static int64_t nat_best_score(const std::vector<int64_t> & has, const std::vector<int64_t> & scores) {
    int64_t best = 0;
    int64_t best_i = -1;
    for (size_t i = 0; i < scores.size(); ++i) {
        if (has[i] && scores[i] > best) {
            best = scores[i];
            best_i = (int64_t) i;
        }
    }
    return best_i;
}

// ggml-backend-reg.cpp:468-482.
static std::string nat_filename(int plat, const std::string & name) {
    const char * prefix = plat == NAT_WIN ? "ggml-" : "libggml-";
    const char * ext    = plat == NAT_WIN ? ".dll" : ".so";
    return std::string(prefix) + name + ext;
}

// ggml-backend-reg.cpp:521-526, over the real std::filesystem::path.
static bool nat_match(int plat, const std::string & name, const std::string & filename) {
    const char * prefix = plat == NAT_WIN ? "ggml-" : "libggml-";
    const char * ext    = plat == NAT_WIN ? ".dll" : ".so";
    const std::filesystem::path p(filename);
    const std::string file_prefix = std::string(prefix) + name + "-";
    return p.filename().native().find(file_prefix) == 0 && p.extension() == ext;
}

// ggml-backend-reg.cpp:404-466.
static std::string nat_exe_prefix(int plat, const std::string & path, bool resolved) {
    const char sep = plat == NAT_WIN ? '\\' : '/';
    if (!resolved) {
        return plat == NAT_LINUX ? std::string("./") : std::string();
    }
    std::string base = path;
    auto last = base.find_last_of(sep);
    if (last != std::string::npos) {
        base = base.substr(0, last);
    }
    return base + sep;
}

// ggml-backend-reg.cpp:489-503.
static std::vector<std::string> nat_search_paths(const std::string & user, bool has_dir,
                                                const std::string & dir, const std::string & exe,
                                                const std::string & cwd, bool cwd_ok) {
    std::vector<std::string> v;
    if (!user.empty()) {
        v.push_back(user);
        return v;
    }
    if (has_dir) { v.push_back(dir); }
    v.push_back(exe);
    if (cwd_ok) { v.push_back(cwd); }
    return v;
}

// ggml-backend-reg.cpp:186-224.
static std::vector<int64_t> nat_registered(const std::vector<int64_t> & reg_ids,
                                           const std::vector<int64_t> & dev_reg,
                                           const std::vector<int64_t> & dev_id) {
    std::vector<int64_t> seen_reg, out;
    for (size_t r = 0; r < reg_ids.size(); ++r) {
        const int64_t id = reg_ids[r];
        if (id < 0) { continue; }
        bool dup = false;
        for (auto s : seen_reg) { if (s == id) { dup = true; } }
        if (dup) { continue; }
        seen_reg.push_back(id);
        for (size_t k = 0; k < dev_reg.size(); ++k) {
            if (dev_reg[k] != (int64_t) r) { continue; }
            bool have = false;
            for (auto o : out) { if (o == dev_id[k]) { have = true; } }
            if (!have) { out.push_back(dev_id[k]); }
        }
    }
    return out;
}

// ---- gate drivers ---------------------------------------------------------

static void gate_names(const char * id, const std::vector<std::string> & names,
                       const char * query, int which) {
    std::string blob;
    std::vector<int64_t> off, len;
    for (auto & n : names) {
        off.push_back((int64_t) blob.size());
        len.push_back((int64_t) n.size());
        blob += n;
    }
    uint8_t b[256];
    int64_t o[16], l[16];
    std::memcpy(b, blob.data(), blob.size());
    for (size_t i = 0; i < off.size(); ++i) { o[i] = off[i]; l[i] = len[i]; }

    const int64_t n = (int64_t) names.size();
    const int64_t got = which == 0
        ? dev_by_name(sl_u8{b, (int64_t) blob.size()}, sl_i64{o, n}, sl_i64{l, n}, n, S(query))
        : dev_reg_by_name(sl_u8{b, (int64_t) blob.size()}, sl_i64{o, n}, sl_i64{l, n}, n, S(query));

    expect(id, "index", got, nat_by_name(names, query));
}

int main() {
    goose_init(0, nullptr);
    setvbuf(stdout, nullptr, _IONBF, 0);

    // ---- name lookup: 9 cases --------------------------------------------
    const std::vector<std::string> devs = { "CUDA0", "CUDA1", "CPU", "", "Metal" };
    gate_names("N0 exact",       devs, "CUDA1", 0);
    gate_names("N1 lower",       devs, "cuda0", 0);
    gate_names("N2 mixed",       devs, "MeTaL", 0);
    gate_names("N3 missing",     devs, "CUDA9", 0);
    gate_names("N4 empty query", devs, "",      0);
    gate_names("N5 empty table", {},   "CUDA0", 0);
    gate_names("N6 reg exact",   devs, "CPU",   1);
    gate_names("N7 reg lower",   devs, "cpu",   1);
    gate_names("N8 reg missing", devs, "Vulkan0", 1);

    // ---- type lookup: 5 cases --------------------------------------------
    {
        const int64_t types[4] = { NAT_CPU, NAT_GPU, NAT_GPU, NAT_ACCEL };
        struct { const char * id; int64_t want, want_rc; } c[] = {
            { "T0 gpu first",  NAT_GPU,   1 },
            { "T1 igpu none",  NAT_IGPU, -1 },
            { "T2 cpu",        NAT_CPU,   0 },
            { "T3 accel",      NAT_ACCEL, 3 },
            { "T4 meta none",  NAT_META, -1 },
        };
        for (auto & k : c) {
            expect(k.id, "index", dev_by_type(sl_i64{(int64_t *) types, 4}, 4, k.want), k.want_rc);
        }
    }

    // ---- init_best: 5 cases ----------------------------------------------
    {
        struct { const char * id; std::vector<int64_t> types; } c[] = {
            { "B0 accel then igpu", { NAT_ACCEL, NAT_IGPU, NAT_CPU } },
            { "B1 cpu only",        { NAT_CPU } },
            { "B2 accel only",      { NAT_ACCEL } },
            { "B3 gpu and igpu",    { NAT_GPU, NAT_IGPU } },
            { "B4 accel then cpu",  { NAT_ACCEL, NAT_CPU } },
        };
        for (auto & k : c) {
            int64_t t[8];
            for (size_t i = 0; i < k.types.size(); ++i) { t[i] = k.types[i]; }
            expect(k.id, "index", dev_init_best(sl_i64{t, (int64_t) k.types.size()},
                                               (int64_t) k.types.size()),
                   nat_init_best(k.types));
        }
    }

    // ---- best score: 7 cases ---------------------------------------------
    {
        struct { const char * id; std::vector<int64_t> has, scores; } c[] = {
            { "S0 middle wins",    { 1, 1, 1 }, { 0, 5, 3 } },
            { "S1 all zero",       { 1, 1, 1 }, { 0, 0, 0 } },
            { "S2 no symbol",      { 0, 0 },    { 9, 9 } },
            { "S3 tie keeps first", { 1, 1 },   { 4, 4 } },
            { "S4 later wins",     { 1, 1, 1 }, { 2, 7, 7 } },
            { "S5 missing symbol", { 0, 1 },    { 9, 1 } },
            { "S6 single",         { 1 },       { 3 } },
        };
        for (auto & k : c) {
            int64_t h[4], s[4];
            for (size_t i = 0; i < k.scores.size(); ++i) { h[i] = k.has[i]; s[i] = k.scores[i]; }
            expect(k.id, "index", dev_best_score(sl_i64{h, (int64_t) k.scores.size()},
                                                 sl_i64{s, (int64_t) k.scores.size()},
                                                 (int64_t) k.scores.size()),
                   nat_best_score(k.has, k.scores));
        }
    }

    // ---- backend file name: 4 cases --------------------------------------
    {
        struct { const char * id; int64_t plat; const char * name; } c[] = {
            { "F0 posix cpu", NAT_LINUX, "cpu" },
            { "F1 win cpu",   NAT_WIN,   "cpu" },
            { "F2 posix cuda", NAT_LINUX, "cuda" },
            { "F3 empty",      NAT_WIN,   "" },
        };
        for (auto & k : c) {
            uint8_t out[128];
            std::memset(out, 0, sizeof(out));
            const int64_t n = dev_backend_filename(k.plat, S(k.name), sl_u8{out, (int64_t) sizeof(out)});
            expect_str(k.id, "name", std::string((const char *) out, (size_t) n),
                       nat_filename(k.plat, k.name));
        }
    }

    // ---- candidate match: 8 cases ----------------------------------------
    {
        struct { const char * id; int64_t plat; const char * name; const char * file; } c[] = {
            { "M0 base name",     NAT_LINUX, "cpu",  "libggml-cpu.so" },
            { "M1 versioned so",  NAT_LINUX, "cpu",  "libggml-cpu.so.0" },
            { "M2 wrong name",    NAT_LINUX, "cpu",  "libggml-cuda.so" },
            { "M3 dll",           NAT_WIN,   "cpu",  "ggml-cpu.dll" },
            { "M4 dll on posix",  NAT_LINUX, "cpu",  "ggml-cpu.dll" },
            { "M5 no dash",       NAT_LINUX, "cpu",  "libggml-cpu.so" },
            { "M6 prefix only",   NAT_LINUX, "cpu",  "libggml-" },
            { "M7 variant name",  NAT_LINUX, "cpu",  "libggml-cpu-armv8.so" },
            { "M8 variant .so.1",  NAT_LINUX, "cpu",  "libggml-cpu-armv8.so.1" },
            { "M9 variant no ext", NAT_LINUX, "cpu",  "libggml-cpu-armv8" },
        };
        for (auto & k : c) {
            expect(k.id, "match", dev_match_candidate(k.plat, S(k.name), S(k.file)),
                   nat_match(k.plat, k.name, k.file) ? 1 : 0);
        }
    }

    // ---- executable prefix: 7 cases --------------------------------------
    {
        struct { const char * id; int64_t plat; const char * path; int64_t resolved; } c[] = {
            { "E0 linux",         NAT_LINUX, "/usr/local/bin/llama-server", 1 },
            { "E1 linux failed",  NAT_LINUX, "",                            0 },
            { "E2 apple",         NAT_APPLE, "/Applications/llama",         1 },
            { "E3 win",           NAT_WIN,   "C:\\bin\\llama.exe",          1 },
            { "E4 win failed",    NAT_WIN,   "",                            0 },
            { "E5 no separator",  NAT_LINUX, "llama",                       1 },
            { "E6 bsd failed",    NAT_BSD,   "",                            0 },
        };
        for (auto & k : c) {
            uint8_t out[256];
            std::memset(out, 0, sizeof(out));
            const int64_t n = dev_exe_prefix(k.plat, S(k.path), k.resolved,
                                             sl_u8{out, (int64_t) sizeof(out)});
            expect_str(k.id, "prefix", std::string((const char *) out, (size_t) n),
                       nat_exe_prefix(k.plat, k.path, k.resolved != 0));
        }
    }

    // ---- search paths: 6 cases -------------------------------------------
    {
        struct { const char * id; const char * user; int64_t has_dir; const char * dir;
                 const char * exe; const char * cwd; int64_t cwd_ok; } c[] = {
            { "D0 user only",   "/opt/lib", 1, "/ggml", "/bin/", "/work", 1 },
            { "D1 full list",   "",         1, "/ggml", "/bin/", "/work", 1 },
            { "D2 no dir",      "",         0, "/ggml", "/bin/", "/work", 1 },
            { "D3 cwd error",   "",         0, "/ggml", "/bin/", "/work", 0 },
            { "D4 empty exe",   "",         0, "/ggml", "",      "/work", 1 },
            { "D5 empty all",   "",         0, "",      "",      "",      0 },
        };
        for (auto & k : c) {
            const std::vector<std::string> want = nat_search_paths(k.user, k.has_dir != 0, k.dir,
                                                                   k.exe, k.cwd, k.cwd_ok != 0);
            uint8_t blob[512];
            int64_t off[8], len[8];
            std::memset(blob, 0, sizeof(blob));
            const int64_t n = dev_search_paths(S(k.user), k.has_dir, S(k.dir), S(k.exe), S(k.cwd),
                                               k.cwd_ok, sl_u8{blob, (int64_t) sizeof(blob)},
                                               sl_i64{off, 8}, sl_i64{len, 8});
            expect(k.id, "count", n, (int64_t) want.size());
            if (n == (int64_t) want.size()) {
                for (int64_t i = 0; i < n; ++i) {
                    expect_str(k.id, "entry", std::string((const char *) blob + off[i], (size_t) len[i]),
                               want[(size_t) i]);
                }
            }
        }
    }

    // ---- registration order: 6 cases -------------------------------------
    {
        struct { const char * id; std::vector<int64_t> reg; std::vector<int64_t> dreg, did; } c[] = {
            { "R0 plain",         { 10, 11 },       { 0, 0, 1 }, { 100, 101, 200 } },
            { "R1 duplicate reg", { 10, 10 },       { 0, 0, 1 }, { 100, 101, 200 } },
            { "R2 duplicate dev", { 10, 11 },       { 0, 0, 1 }, { 100, 100, 200 } },
            { "R3 null reg",      { -1, 11 },       { 0, 1 },    { 100, 200 } },
            { "R4 all null",      { -1, -1 },       { 0, 1 },    { 100, 200 } },
            { "R5 three regs",    { 10, 11, 12 },   { 0, 1, 2 }, { 100, 200, 300 } },
        };
        for (auto & k : c) {
            const std::vector<int64_t> want = nat_registered(k.reg, k.dreg, k.did);
            int64_t reg[8], dreg[8], did[8], seen[8], out[16];
            for (size_t i = 0; i < k.reg.size(); ++i)  { reg[i] = k.reg[i]; }
            for (size_t i = 0; i < k.dreg.size(); ++i) { dreg[i] = k.dreg[i]; did[i] = k.did[i]; }
            std::memset(seen, 0, sizeof(seen));
            std::memset(out, 0, sizeof(out));
            const int64_t n = dev_registered_ids(sl_i64{reg, (int64_t) k.reg.size()},
                                                (int64_t) k.reg.size(),
                                                sl_i64{dreg, (int64_t) k.dreg.size()},
                                                sl_i64{did, (int64_t) k.dreg.size()},
                                                (int64_t) k.dreg.size(),
                                                sl_i64{seen, 8}, sl_i64{out, 16});
            expect(k.id, "count", n, (int64_t) want.size());
            if (n == (int64_t) want.size()) {
                for (int64_t i = 0; i < n; ++i) {
                    expect(k.id, "device", out[i], want[(size_t) i]);
                }
            }
        }
    }

    std::printf("dev: %d checks, %d failures\n", g_checks, g_fails);
    return g_fails == 0 ? 0 : 1;
}
