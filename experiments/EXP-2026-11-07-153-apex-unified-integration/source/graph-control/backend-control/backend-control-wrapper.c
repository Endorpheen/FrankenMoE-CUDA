// Wrapper for generated backend-control.goose C.
// The parent emits parent-review/backend-control.c. Until then bc_entry is
// absent and the fixture/wrappers fail at link time by design (no weak import,
// no PENDING). Entry symbol is the observed emitter convention for a
// single-entry module; parent must re-verify after emit.
#include "backend-bridge.h"

#if defined(__has_include)
#  if __has_include("../generated/backend-control.c")
#    define FG153_HAVE_GENERATED_BACKEND_CONTROL 1
#    define FG153_BC_GENERATED_FILE "../generated/backend-control.c"
#  elif __has_include("../parent-review/backend-control.c")
#    define FG153_HAVE_GENERATED_BACKEND_CONTROL 1
#    define FG153_BC_GENERATED_FILE "../parent-review/backend-control.c"
#  elif __has_include("parent-review/backend-control.c")
#    define FG153_HAVE_GENERATED_BACKEND_CONTROL 1
#    define FG153_BC_GENERATED_FILE "parent-review/backend-control.c"
#  endif
#endif

#ifdef FG153_HAVE_GENERATED_BACKEND_CONTROL
#define main goose_backend_control_generated_main
#include FG153_BC_GENERATED_FILE
#undef main

// Parent sets this from the actual generated C entry symbol after emit.
// No guessed emitter ID is used here.
#ifndef FG153_BC_GENERATED_ENTRY
#error "define FG153_BC_GENERATED_ENTRY from generated backend-control.c"
#endif

int64_t bc_entry(int64_t p0, int64_t p1, int64_t p2, int64_t p3,
                 int64_t p4, int64_t p5, int64_t p6, int64_t p7) {
    // The generated entry uses only scalar locals; it never touches gg_stacks.
    return FG153_BC_GENERATED_ENTRY(p0, p1, p2, p3, p4, p5, p6, p7, 1);
}
#endif
