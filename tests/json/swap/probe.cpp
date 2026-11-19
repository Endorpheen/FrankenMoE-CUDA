// The swap itself: common_json::parse with FG_JSON_GOOSE_PARSE on and off.
//
// This is the only test that exercises the flag path in vendor/flint/common/json.cpp rather than the
// adapter alone. libllama is loaded first so the door lookup (RTLD_NOLOAD) can find json_parse.
#include <dlfcn.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "json.h"
#include "nlohmann/json.hpp"

extern "C" void ggml_abort(const char *, int, const char *, ...) { std::abort(); }

int main(int argc, char ** argv) {
    const char * lib = argc > 1 ? argv[1] : "libllama.so.0";
    if (!dlopen(lib, RTLD_LAZY)) { std::printf("cannot open %s: %s\n", lib, dlerror()); return 2; }

    const std::vector<std::string> texts = {
        "{\"a\":1,\"b\":[true,null]}",
        "{\"z\":1,\"a\":2}",
        "[1,2,3]",
        "{\"s\":\"x y\",\"n\":-1.5e3}",
        "[{\"k\":[1,[2,[3]]]}]",
        "{\"big\":1234567890123,\"neg\":-42}",
        "{\"a\":0.5,\"b\":1e-3,\"c\":2.5E+2}",
    };

    const bool want_goose = std::getenv("FG_JSON_GOOSE_PARSE") != nullptr;
    int failures = 0;
    for (const auto & text : texts) {
        const std::string want = nlohmann::ordered_json::parse(text).dump();
        std::string got;
        try {
            got = common_json::parse(text).dump();
        } catch (const std::exception & e) {
            std::printf("parse threw for %s: %s\n", text.c_str(), e.what());
            ++failures;
            continue;
        }
        if (got != want) {
            ++failures;
            std::printf("MISMATCH %s\n  nlohmann: %s\n  parse():  %s\n", text.c_str(), want.c_str(), got.c_str());
        }
    }
    std::printf("json swap: goose=%d texts=%zu failures=%d\n", want_goose ? 1 : 0, texts.size(), failures);
    return failures == 0 ? 0 : 1;
}
