#include "host-ffi.h"
#define main fg381ds_generated_main
#include "host-qualified.c"
#undef main
uint8_t fg381dh_run(int64_t p){return fg381ds_gg_function_1(p,1);}
