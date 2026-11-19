// Through-the-tree shadow comparison: Goose parser -> adapter -> common_json dump, against nlohmann.
//
// Unlike tests/json/nlohmann-shadow, which compares the Goose dumper with nlohmann, this one
// exercises the adapter: the same parse result is turned into a common_json tree and dumped by
// common_json itself. If the adapter loses key order or a number's class, the bytes differ.
//
// Build: g++ -O2 -std=c++17 -Ivendor/flint/common -Ivendor/flint/vendor/nlohmann
//            probe.cpp ../adapter/json_adapter.cpp vendor/flint/common/json.cpp -ldl
#include <dlfcn.h>

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "json.h"
#include "nlohmann/json.hpp"

// common_json's implementation calls ggml_abort for its invariants; this probe never triggers one
// and links the minimal stub rather than the whole ggml runtime.
extern "C" void ggml_abort(const char *, int, const char *, ...) { std::abort(); }
#include "nlohmann/json.hpp"
#include "../adapter/json_adapter.cpp"

namespace {

struct sl8  { const void * data; int64_t len; };
struct sl64 { int64_t * data; int64_t len; };

using parse_fn = int64_t (*)(sl8, sl64, sl64, sl64, sl64, sl64, sl64, sl64, sl64, sl64, sl64, sl64,
                             sl64, sl8, sl64, sl64, sl64, sl64);

parse_fn g_parse = nullptr;

constexpr int N = 512;
int64_t jk[N], jv[N], js[N], jl[N], jf[N], jn[N], jh[N];
int64_t ev[512], eko[512], ekl[512], en[512], jkey[512], jkl[512], jchild[512], jst[16];
double jd[N];
char sbuf[4096];

bool goose_tree(const std::string & text, nlohmann::ordered_json & out) {
    std::memset(jk, 0, sizeof jk); std::memset(jv, 0, sizeof jv); std::memset(js, 0, sizeof js);
    std::memset(jl, 0, sizeof jl); std::memset(jf, 0, sizeof jf); std::memset(jn, 0, sizeof jn);
    std::memset(jh, 0, sizeof jh); std::memset(ev, 0, sizeof ev); std::memset(eko, 0, sizeof eko);
    std::memset(ekl, 0, sizeof ekl); std::memset(en, 0, sizeof en); std::memset(sbuf, 0, sizeof sbuf);
    const int64_t r = g_parse(sl8{ text.data(), (int64_t) text.size() },
        sl64{ jk, N }, sl64{ jv, N }, sl64{ (int64_t *) jd, N }, sl64{ js, N }, sl64{ jl, N },
        sl64{ jf, N }, sl64{ jn, N }, sl64{ jh, N }, sl64{ ev, N }, sl64{ eko, N }, sl64{ ekl, N },
        sl64{ en, N }, sl8{ sbuf, sizeof sbuf - 1 }, sl64{ jkey, N }, sl64{ jkl, N },
        sl64{ jchild, N }, sl64{ jst, 16 });
    if (r != 0) { return false; }
    fg_json_adapter::arrays a{ jk, jv, js, jl, jd, jh, ev, eko, ekl, en, sbuf, 0 };
    out = fg_json_adapter::to_tree(a, 0);
    return true;
}

} // namespace

int main(int argc, char ** argv) {
    void * h = dlopen(argc > 1 ? argv[1] : "libllama.so.0", RTLD_LAZY);
    if (!h) { std::printf("cannot open the library: %s\n", dlerror()); return 2; }
    g_parse = (parse_fn) dlsym(h, "json_parse");
    if (!g_parse) { std::printf("json_parse is missing\n"); return 2; }

    const std::vector<std::string> texts = {
        "{\"a\":1,\"b\":[true,null]}",
        "{\"z\":1,\"a\":2}",
        "[1,2,3]",
        "{\"s\":\"x y\",\"n\":-1.5e3}",
        "[]",
        "{}",
        "[{\"k\":[1,[2,[3]]]}]",
        "{\"nested\":{\"deep\":{\"deeper\":{\"v\":1}}}}",
        "{\"big\":1234567890123,\"neg\":-42}",
        "[true,false,null,0,-1,\"\",{},[]]",
        "{\"a\":0.5,\"b\":1e-3,\"c\":2.5E+2}",
    };

    int failures = 0, compared = 0;
    for (const auto & text : texts) {
        const std::string want = nlohmann::ordered_json::parse(text).dump();
        nlohmann::ordered_json tree;
        if (!goose_tree(text, tree)) {
            std::printf("Goose refused a text nlohmann accepts: %s\n", text.c_str());
            ++failures;
            continue;
        }
        std::string got;
        try { got = tree.dump(); } catch (const std::exception & e) {
            std::printf("dump threw for %s: %s\n", text.c_str(), e.what());
            ++failures;
            continue;
        }
        ++compared;
        if (got != want) {
            ++failures;
            std::printf("MISMATCH %s\n  nlohmann:    %s\n  adapter tree: %s\n", text.c_str(), want.c_str(), got.c_str());
        }
    }
    std::printf("json adapter shadow: texts=%zu compared=%d failures=%d\n", texts.size(), compared, failures);
    return failures == 0 ? 0 : 1;
}
