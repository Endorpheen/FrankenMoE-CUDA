#include <stdint.h>
extern void fg153_graph_trace(int,int64_t);
// Exported wrapper for parent-review/graph-storage.c.
// The generated C is a translation unit with static gg_function_N and its own
// main; this file renames main and exports the exact entry ABI.
// Entry symbol is the observed emitter entry in the saved generated C
// (gg_function_1, seven parameters, called by generated main with base 1).
// Parent must re-verify this symbol after re-emitting graph-storage.goose.
#include "graph-bridge.h"

#if defined(__has_include)
#  if __has_include("generated/graph-storage.c")
#    define FG153_GS_GENERATED_FILE "generated/graph-storage.c"
#  else
#    define FG153_GS_GENERATED_FILE "parent-review/graph-storage.c"
#  endif
#endif
#ifndef FG153_GS_GENERATED_FILE
#  define FG153_GS_GENERATED_FILE "parent-review/graph-storage.c"
#endif
#define main goose_graph_storage_generated_main
#include FG153_GS_GENERATED_FILE
#undef main

#ifndef FG153_GS_GENERATED_ENTRY
#define FG153_GS_GENERATED_ENTRY gg_function_1
#endif

int64_t gs_entry(int64_t p0, int64_t p1, int64_t p2, int64_t p3,
                 int64_t p4, int64_t p5, int64_t p6) {
    // The generated entry uses only scalar locals; it never touches gg_stacks.
    fg153_graph_trace(1, p0);
    return FG153_GS_GENERATED_ENTRY(p0, p1, p2, p3, p4, p5, p6, 1);
}
