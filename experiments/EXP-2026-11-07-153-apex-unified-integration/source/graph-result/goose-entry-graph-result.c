// Worker-7: strong entry for graph-result.goose.
// Binds the observed generated entry symbol (gg_function_1, six arguments) and
// keeps the generated main private to this TU.
#include "graph-services.h"

#define main fg153_graph_result_generated_main
#include "generated/graph-result.c"
#undef main

#ifndef FG153_GRAPH_RESULT_ENTRY
#define FG153_GRAPH_RESULT_ENTRY gg_function_1
#endif

int64_t fg153_goose_res(int64_t op, int64_t r, int64_t a, int64_t b, int64_t c, int64_t d) {
    const int64_t out = FG153_GRAPH_RESULT_ENTRY(op, r, a, b, c, d, 0);
    return out;
}
