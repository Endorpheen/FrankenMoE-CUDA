#include "host-ffi.h"
#define main fg300ds_generated_main
#include "host-qualified.c"
#undef main
uint8_t fg300dh_run(int64_t p){return fg300ds_gg_function_1(p,1);}
