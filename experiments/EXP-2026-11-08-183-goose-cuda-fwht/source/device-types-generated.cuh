#pragma once
#include <stdint.h>
// Types the emitted body needs for its local fixed array. They mirror the compiler's
// own declarations in the generated C. gg_index and the byte-view helpers come from
// the mechanical extraction, so they are not repeated here.
typedef struct { void * data; int64_t len; } fg183fd_gg_slice;
typedef struct { float data[16]; } fg183fd_gg_type_7;
static __device__ __forceinline__ fg183fd_gg_slice fg183fd_gg_view(void *p,int64_t n){
 fg183fd_gg_slice v;v.data=p;v.len=n;return v;}
