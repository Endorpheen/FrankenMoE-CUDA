// Shadow comparison: the Goose parser and dumper against nlohmann for the same texts.
//
// The requirement (P10-07) asks for a shadow comparison before any adapter exists. The tree bridge
// is not needed for that part: parse a text with nlohmann, dump it, parse the same text through the
// Goose doors in the shipped library, dump it, and compare the two byte strings. Ordering and number
// normalisation are the two things this catches, and both are promises common_json makes.
//
// nlohmann is header-only and already vendored, so nothing new is linked. The Goose side is reached
// through the shipped library, exactly as the two existing JSON tests do it.
#include <dlfcn.h>

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

namespace {

struct sl8  { const void * data; int64_t len; };
struct sl64 { int64_t * data; int64_t len; };
struct slc64 { const int64_t * data; int64_t len; };
struct slf  { double * data; int64_t len; };
struct slcf { const double * data; int64_t len; };

using parse_fn = int64_t (*)(sl8, sl64, sl64, slf, sl64, sl64, sl64, sl64, sl64, sl64, sl64, sl64,
                             sl64, sl8, sl64, sl64, sl64, sl64);
using dump_fn  = int64_t (*)(int64_t, slc64, slc64, slcf, slc64, slc64, slc64, slc64, slc64, slc64,
                             sl64, sl8, sl8, int64_t, int64_t);

parse_fn g_parse = nullptr;
dump_fn  g_dump  = nullptr;

constexpr int N = 512;
int64_t jk[N], jv[N], js[N], jl[N], jf[N], jn[N], jh[N];
int64_t ev[512], eko[512], ekl[512], en[512], jkey[512], jkl[512], jchild[512], jst[16];
double jd[N];
char sbuf[4096];
char out[4096];

std::string goose_roundtrip(const std::string & text, int64_t * rc) {
    std::memset(jk, 0, sizeof jk); std::memset(jv, 0, sizeof jv); std::memset(js, 0, sizeof js);
    std::memset(jl, 0, sizeof jl); std::memset(jf, 0, sizeof jf); std::memset(jn, 0, sizeof jn);
    std::memset(jh, 0, sizeof jh); std::memset(ev, 0, sizeof ev); std::memset(eko, 0, sizeof eko);
    std::memset(ekl, 0, sizeof ekl); std::memset(en, 0, sizeof en); std::memset(sbuf, 0, sizeof sbuf);
    std::memset(jkey, 0, sizeof jkey); std::memset(jkl, 0, sizeof jkl);
    std::memset(jchild, 0, sizeof jchild); std::memset(jst, 0, sizeof jst);
    std::memset(out, 0, sizeof out);

    const int64_t r = g_parse(sl8{ text.data(), (int64_t) text.size() },
        sl64{ jk, N }, sl64{ jv, N }, slf{ jd, N }, sl64{ js, N }, sl64{ jl, N }, sl64{ jf, N },
        sl64{ jn, N }, sl64{ jh, N }, sl64{ ev, N }, sl64{ eko, N }, sl64{ ekl, N }, sl64{ en, N },
        sl8{ sbuf, sizeof sbuf - 1 }, sl64{ jkey, N }, sl64{ jkl, N }, sl64{ jchild, N },
        sl64{ jst, 16 });
    if (r != 0) { *rc = r; return "<parse failed>"; }
    const int64_t n = g_dump(0, slc64{ jk, N }, slc64{ jv, N }, slcf{ jd, N }, slc64{ js, N },
        slc64{ jl, N }, slc64{ jf, N }, slc64{ jn, N }, slc64{ jkey, N }, slc64{ jkl, N },
        sl64{ jchild, N }, sl8{ sbuf, sizeof sbuf - 1 }, sl8{ out, sizeof out - 1 }, -1, 0);
    *rc = n;
    return std::string(out);
}

} // namespace

int main(int argc, char ** argv) {
    void * h = dlopen(argc > 1 ? argv[1] : "libllama.so.0", RTLD_LAZY);
    if (!h) { std::printf("cannot open the library: %s\n", dlerror()); return 2; }
    g_parse = (parse_fn) dlsym(h, "json_parse");
    g_dump  = (dump_fn)  dlsym(h, "json_dump");
    void (*init)(int, char **) = (void (*)(int, char **)) dlsym(h, "gs_json_dump_init");
    if (!g_parse || !g_dump) { std::printf("a door is missing\n"); return 2; }
    if (init) init(0, nullptr);

    const std::vector<std::string> texts = {
        "{\"a\":1,\"b\":[true,null]}",
        "{\"z\":1,\"a\":2}",
        "[1,2,3]",
        "{\"s\":\"x y\",\"n\":-1.5e3}",
        "null",
        "[]",
        "{}",
        "[{\"k\":[1,[2,[3]]]}]",
        "{\"nested\":{\"deep\":{\"deeper\":{\"v\":1}}}}",
        "{\"u\":\"\\u00e9\\u00e8\",\"q\":\"a\\\"b\"}",
        "{\"big\":1234567890123,\"neg\":-42}",
        "[true,false,null,0,-1,\"\",{},[]]",
        "{\"a\":0.5,\"b\":1e-3,\"c\":2.5E+2}",
        "  {  \"spaced\" :  [ 1 , 2 ]  }  ",
    };

    int failures = 0, compared = 0;
    for (const auto & text : texts) {
        std::string nl;
        try {
            nl = nlohmann::ordered_json::parse(text).dump();
        } catch (const std::exception & e) {
            std::printf("nlohmann rejected a text the fixture expects valid: %s (%s)\n", text.c_str(), e.what());
            ++failures;
            continue;
        }
        int64_t rc = 0;
        const std::string g = goose_roundtrip(text, &rc);
        ++compared;
        if (rc <= 0 || g != nl) {
            ++failures;
            std::printf("MISMATCH %s\n  nlohmann: %s\n  goose:    %s (rc=%lld)\n",
                        text.c_str(), nl.c_str(), g.c_str(), (long long) rc);
        }
    }
    std::printf("json shadow: texts=%zu compared=%d failures=%d\n", texts.size(), compared, failures);
    return failures == 0 ? 0 : 1;
}
