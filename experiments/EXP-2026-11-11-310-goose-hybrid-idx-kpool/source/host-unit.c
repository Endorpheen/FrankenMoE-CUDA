#include "host-ffi.h"
#define main fg310ds_generated_main
#include "host-qualified.c"
#undef main
uint8_t fg310dh_run(int64_t p){return fg310ds_gg_function_1(p,1);}
