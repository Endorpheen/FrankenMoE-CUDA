// The four build-info symbols that gguf-split.cpp prints in --version. The
// generated header is not part of a source checkout, and the tool never uses
// these values for anything else.
#include <cstdint>

int llama_build_number(void) { return 0; }
const char * llama_commit(void) { return "05048b02"; }
const char * llama_compiler(void) { return "gcc-13"; }
const char * llama_build_target(void) { return "x86_64-linux"; }
