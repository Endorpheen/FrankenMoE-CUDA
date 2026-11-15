// Differential gate for P8-09 S1 (the UI asset naming and content-type table).
//
// Native arm 1: embed.cpp itself, included with its main renamed, so
// mime_from_ext and path_basename are called as the generator defines them.
// Native arm 2: the generated ui.cpp for the synthetic bundle, compiled into
// this gate; llama_ui_find_asset(name)->type is the type the server would send
// for that asset.
// Port arm: the mime_ exports of source/mime-table.goose.
//
// The required-label table cannot be called from embed.cpp (the matchers are
// lambdas inside main), so the gate carries a transcription of those eight
// lambdas, AND checks end to end that the whole synthetic bundle satisfies all
// eight: the real generator refuses the tree otherwise, and the build would
// have stopped before this gate ran.
#include "mime.h"

#include "ui.h"

#define main embed_tool_main
#include "embed.cpp"
#undef main

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int cases = 0;
static int values = 0;
static int fails = 0;

static void eq_int(const char * what, const std::string & name, int64_t want, int64_t got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-22s %-34s want=%lld got=%lld\n", what, name.c_str(),
               (long long) want, (long long) got);
    }
}

static void eq_str(const char * what, const std::string & name, const std::string & want,
                   const std::string & got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-22s %-34s want=%s got=%s\n", what, name.c_str(), want.c_str(), got.c_str());
    }
}

static sl_u8 SL(const std::string & s) {
    return sl_u8{(uint8_t *) (void *) s.data(), (int64_t) s.size()};
}

static std::string go_type(const std::string & name) {
    uint8_t buf[64];
    sl_u8 out{buf, (int64_t) sizeof buf};
    const int64_t id = mime_of(SL(name));
    const int64_t n = mime_text(id, out);
    if (n < 0) {
        return "<bad id>";
    }
    return std::string((const char *) buf, (size_t) n);
}

static std::string go_base(const std::string & name) {
    uint8_t buf[256];
    sl_u8 out{buf, (int64_t) sizeof buf};
    const int64_t n = mime_base(SL(name), out);
    if (n < 0) {
        return "<bad base>";
    }
    return std::string((const char *) buf, (size_t) n);
}

// The eight required-bundle matchers, transcribed from embed.cpp:178-196.
static bool req_match(int label, const std::string & base) {
    auto starts = [](const std::string & s, const char * p) {
        const size_t n = strlen(p);
        return s.size() >= n && s.compare(0, n, p) == 0;
    };
    auto ends = [](const std::string & s, const char * p) {
        const size_t n = strlen(p);
        return s.size() >= n && s.compare(s.size() - n, n, p) == 0;
    };
    switch (label) {
        case 0: return base == "index.html";
        case 1: return base == "manifest.webmanifest";
        case 2: return base == "sw.js";
        case 3: return base == "build.json";
        case 4: return base == "version.json";
        case 5: return starts(base, "bundle") && ends(base, ".js");
        case 6: return starts(base, "bundle") && ends(base, ".css");
        case 7: return starts(base, "workbox") && ends(base, ".js");
        default: return false;
    }
}

static const char * req_label(int label) {
    switch (label) {
        case 0: return "index.html";
        case 1: return "manifest.webmanifest";
        case 2: return "sw.js";
        case 3: return "build.json";
        case 4: return "version.json";
        case 5: return "bundle[hash].js";
        case 6: return "bundle[hash].css";
        default: return "workbox[hash].js";
    }
}

int main() {
    goose_init(0, nullptr);

    const auto & assets = llama_ui_get_assets();

    // every asset of the real generated table, against both native arms
    for (const auto & a : assets) {
        cases++;
        const std::string name = a.name;
        eq_str("type vs embed", name, mime_from_ext(name), go_type(name));
        eq_str("type vs ui.cpp", name, a.type, go_type(name));
        eq_str("base vs embed", name, path_basename(name), go_base(name));
    }

    // names outside the table: the extension rules on their own
    const char * probes[] = {
        "", ".", "a", "a.", "a.b", "a.b.c", "x.HTML", "x.Js", "x.json5", "x.tar.gz",
        "_app/version.json", "dir.with.dot/file", "a/b.c/d", "/abs/x.css", "x.woff2.",
        "x.JPEG", "x.jpeg", "x.jpg", "x.webmanifest", "x.svgz", "x.png", "x.ico",
        "x.woff", "x.css", "x.html", "x.js", "x.json",
    };
    for (const char * p : probes) {
        cases++;
        const std::string name = p;
        eq_str("probe type", name, mime_from_ext(name), go_type(name));
        eq_str("probe base", name, path_basename(name), go_base(name));
    }

    // the required-label mask, per basename, against the transcription
    const char * bases[] = {
        "index.html", "manifest.webmanifest", "sw.js", "build.json", "version.json",
        "bundle.js", "bundle.a1b2c3.js", "bundle.css", "bundle.a1b2c3.css", "bundle.js.css",
        "bundle.css.js", "x-bundle.js", "bundle", "workbox.js", "workbox.9f8e7d.js",
        "workbox.js.css", "index.htm", "INDEX.HTML", "sw.js.map", "version.json.bak", "",
    };

    for (const char * b : bases) {
        cases++;
        const std::string base = b;
        int64_t want = 0;
        for (int i = 0; i < 8; i++) {
            if (req_match(i, base)) {
                want |= (int64_t) 1 << i;
            }
        }
        eq_int("required mask", base, want, mime_required(SL(base)));
    }

    // the shipped bundle satisfies every label, and the label texts are native
    cases++;
    int64_t all = 0;
    for (const auto & a : assets) {
        all |= mime_required(SL(path_basename(a.name)));
    }
    eq_int("bundle labels", "all eight", 255, all);

    for (int i = 0; i < 8; i++) {
        uint8_t buf[64];
        sl_u8 out{buf, (int64_t) sizeof buf};
        const int64_t n = mime_required_label(i, out);
        const std::string got = n < 0 ? "<bad bit>" : std::string((const char *) buf, (size_t) n);
        eq_str("label text", req_label(i), req_label(i), got);
    }

    printf("FGWEB_MIME cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
