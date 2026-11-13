#include "fg180q-host-ffi.h"
#define main fg180qs_generated_main
#include "fg180q-host-qualified.c"
#undef main
uint8_t fg180qh_run(int64_t p){return fg180qs_gg_function_1(p,1);}
