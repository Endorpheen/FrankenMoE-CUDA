// The staged replacement carrier for one nlohmann::json consumer.
//
// Consumer (reference/gguf-model-data.cpp, lines 454-530): detect_gguf_filename()
// reads a HuggingFace /api/models response and picks the GGUF file whose name
// contains the requested quant.
//
// The transcription below is the JSON half of that function: parse the body,
// require "siblings" to be an array, walk it for "rfilename" strings that end
// in .gguf and contain the quant, sort, prefer a non-split/non-mmproj file,
// else the first shard and its split prefix.
//
// One body, two carriers, selected per translation unit:
//   OFF (default) nlohmann::json  - the canon path, always compiled;
//   ON            goose_json::json - the Goose parser's flat table through
//                                   source/goose-json-adapter.h.
// Both always compile into every binary except that the Goose adapter is only
// linked in an ON build; the flag selects which one hf_pick() dispatches to.

#pragma once

#include <string>
#include <vector>

struct hf_pick_result {
    int                      error = 0; // 0 ok, 1 parse, 2 format, 3 no match, 4 type
    std::string              chosen;
    std::string              split_prefix;
    std::vector<std::string> matches;   // gate observability: sorted match list
};

// Call counters: the gate proves the dispatch is real, not that the flag was
// silently ignored.
extern int hf_pick_calls_native;
extern int hf_pick_calls_goose;

// The canon carrier, always compiled.
hf_pick_result hf_pick_native(const std::string & body, const std::string & quant);

#if defined(FG_GOOSE_JSON_REPLACE) && FG_GOOSE_JSON_REPLACE
// The Goose carrier, compiled and linked only in an ON build.
hf_pick_result hf_pick_goose(const std::string & body, const std::string & quant);
#endif

// The dispatched entry point.
hf_pick_result hf_pick(const std::string & body, const std::string & quant);
