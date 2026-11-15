// Transcribed JSON consumer, one body instantiated for two carriers.
// Provenance: reference/gguf-model-data.cpp, detect_gguf_filename().
// tests/anchors.py checks that the anchors below still exist in that copy.

#include "hf-pick.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

// Both carriers are compiled in an ON build: nlohmann stays the oracle, the
// Goose adapter is the dispatched path. The flag only selects the dispatch.
#define JSON_ASSERT(x) ((void) 0)
#include <nlohmann/json.hpp>

#if defined(FG_GOOSE_JSON_REPLACE) && FG_GOOSE_JSON_REPLACE
#include "goose-json-adapter.h"
#endif

namespace {

// The body of detect_gguf_filename() below the HTTP call, with the parse
// failure and the format failure mapped onto result codes instead of fprintf.
// Anchors: "if (!j.contains(\"siblings\")", "sibling[\"rfilename\"].get<std::string>()",
// "m.find(\"-00001-of-\")".
template <class J>
hf_pick_result pick_with(const std::string & body, const std::string & quant) {
    hf_pick_result out;

    J j;
    try {
        j = J::parse(body.begin(), body.end());
    } catch (...) {
        out.error = 1;
        return out;
    }

    if (!j.contains("siblings") || !j["siblings"].is_array()) {
        out.error = 2;
        return out;
    }

    std::vector<std::string> matches;
    std::string quant_upper = quant;
    for (char & c : quant_upper) {
        c = (char) toupper((unsigned char) c);
    }

    try {
        for (const auto & sibling : j["siblings"]) {
            if (!sibling.contains("rfilename")) {
                continue;
            }
            // `.template` is required only because this body is a template; the
            // call is the original `sibling["rfilename"].get<std::string>()`.
            std::string fname = sibling["rfilename"].template get<std::string>();
            if (fname.size() < 5 || fname.substr(fname.size() - 5) != ".gguf") {
                continue;
            }

            std::string fname_upper = fname;
            for (char & c : fname_upper) {
                c = (char) toupper((unsigned char) c);
            }
            if (fname_upper.find(quant_upper) != std::string::npos) {
                matches.push_back(fname);
            }
        }
    } catch (...) {
        // nlohmann throws type_error when rfilename is not a string; the
        // carrier adapter throws the same way. Neither path can continue.
        out.error = 4;
        out.matches.assign(matches.begin(), matches.end());
        return out;
    }

    if (matches.empty()) {
        out.error = 3;
        return out;
    }

    std::sort(matches.begin(), matches.end());
    out.matches.assign(matches.begin(), matches.end());

    // Prefer non-split, non-supplementary file
    for (const auto & m : matches) {
        if (m.find("-of-") == std::string::npos && m.find("mmproj") == std::string::npos) {
            out.chosen = m;
            return out;
        }
    }

    // Return the first shard (00001-of-) and extract the prefix
    for (const auto & m : matches) {
        auto pos = m.find("-00001-of-");
        if (pos != std::string::npos) {
            out.split_prefix = m.substr(0, pos);
            out.chosen = m;
            return out;
        }
    }

    out.chosen = matches[0];
    return out;
}

} // namespace

int hf_pick_calls_native = 0;
int hf_pick_calls_goose = 0;

hf_pick_result hf_pick_native(const std::string & body, const std::string & quant) {
    ++hf_pick_calls_native;
    return pick_with<nlohmann::json>(body, quant);
}

#if defined(FG_GOOSE_JSON_REPLACE) && FG_GOOSE_JSON_REPLACE
hf_pick_result hf_pick_goose(const std::string & body, const std::string & quant) {
    ++hf_pick_calls_goose;
    return pick_with<goose_json::json>(body, quant);
}
#endif

hf_pick_result hf_pick(const std::string & body, const std::string & quant) {
#if defined(FG_GOOSE_JSON_REPLACE) && FG_GOOSE_JSON_REPLACE
    return hf_pick_goose(body, quant);
#else
    return hf_pick_native(body, quant);
#endif
}
