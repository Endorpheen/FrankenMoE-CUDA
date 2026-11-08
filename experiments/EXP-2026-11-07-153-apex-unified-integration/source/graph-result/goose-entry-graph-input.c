// Worker-7: strong entry for graph-input.goose.
#include "graph-services.h"

#define main fg153_graph_input_generated_main
#include "generated/graph-input.c"
#undef main

#ifndef FG153_GRAPH_INPUT_ENTRY
#define FG153_GRAPH_INPUT_ENTRY gg_function_1
#endif

int64_t fg153_goose_inp(int64_t op, int64_t inp, int64_t x, int64_t y, int64_t z, int64_t w) {
    const int64_t out = FG153_GRAPH_INPUT_ENTRY(op, inp, x, y, z, w, 0);
    return out;
}
