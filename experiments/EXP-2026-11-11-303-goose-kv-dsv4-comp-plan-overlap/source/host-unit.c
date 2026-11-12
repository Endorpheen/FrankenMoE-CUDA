#include "host-ffi.h"
#define main fg303ds_generated_main
#include "host-qualified.c"
#undef main
uint8_t fg303dh_run(int64_t p){return fg303ds_gg_function_1(p,1);}
