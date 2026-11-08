// Worker-7: strong entry for graph-context.goose.
#include "graph-services.h"

#define main fg153_graph_context_generated_main
#include "generated/graph-context.c"
#undef main

#ifndef FG153_GRAPH_CONTEXT_ENTRY
#define FG153_GRAPH_CONTEXT_ENTRY gg_function_1
#endif

int64_t fg153_goose_ctx(int64_t op, int64_t params, int64_t out, int64_t a, int64_t b, int64_t c) {
    const int64_t result = FG153_GRAPH_CONTEXT_ENTRY(op, params, out, a, b, c, 0);
    return result;
}
