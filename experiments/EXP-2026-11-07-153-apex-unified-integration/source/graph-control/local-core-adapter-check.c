// LOCAL CHECK ONLY: compile the copied native core in one TU and append the
// arena adapter, proving the adapter sees the real ggml_object/context and
// internal static helpers. This is -fsyntax-only, not a full build.
#include "build/ggml-native-core.copy.c"
#include "graph-arena-adapter.inc"
