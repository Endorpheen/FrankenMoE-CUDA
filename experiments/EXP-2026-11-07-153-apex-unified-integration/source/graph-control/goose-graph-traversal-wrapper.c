#include <stdint.h>
extern void fg153_graph_trace(int,int64_t);
// Exported wrapper for generated graph-traversal.c.
// The parent emits parent-review/graph-traversal.c after the recursive fn fix.
// Until then this file defines no gt_entry, so fixture/unit fail at link time
// (no weak lookup, no PENDING). Entry symbol follows the observed emitter
// convention (gg_function_1, seven parameters); parent must re-verify after
// emit and may override via -DFG153_GT_GENERATED_ENTRY.
#include "graph-bridge.h"

#if defined(__has_include)
#  if __has_include("generated/graph-traversal.c")
#    define FG153_HAVE_GENERATED_TRAVERSAL 1
#    define FG153_GT_GENERATED_FILE "generated/graph-traversal.c"
#  elif __has_include("parent-review/graph-traversal.c")
#    define FG153_HAVE_GENERATED_TRAVERSAL 1
#    define FG153_GT_GENERATED_FILE "parent-review/graph-traversal.c"
#  endif
#endif

#ifdef FG153_HAVE_GENERATED_TRAVERSAL
#define main goose_graph_traversal_generated_main
#include FG153_GT_GENERATED_FILE
#undef main

#ifndef FG153_GT_GENERATED_ENTRY
#define FG153_GT_GENERATED_ENTRY gg_function_1
#endif

int64_t gt_entry(int64_t p0, int64_t p1, int64_t p2, int64_t p3,
                 int64_t p4, int64_t p5, int64_t p6) {
    fg153_graph_trace(0, p0);
    return FG153_GT_GENERATED_ENTRY(p0, p1, p2, p3, p4, p5, p6, 1);
}
#endif
