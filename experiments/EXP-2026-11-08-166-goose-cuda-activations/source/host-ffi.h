#ifndef FG166_HOST_FFI_H
#define FG166_HOST_FFI_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define FFI __attribute__((visibility("hidden")))
FFI int64_t fg166_arg(int64_t,int64_t);
FFI int64_t fg166_info(int64_t,int64_t);
FFI int64_t fg166_ti(int64_t,int64_t,int64_t);
FFI float fg166_tf(int64_t,int64_t);
FFI int64_t fg166_pred(int64_t,int64_t,int64_t);
FFI void fg166_hi(int64_t,int64_t,int64_t);
FFI void fg166_hf(int64_t,int64_t,float);
FFI int64_t fg166_constant(int64_t);
FFI float fg166_half_round(float);
FFI void fg166_launch(int64_t);
FFI void fg166_fail(int64_t);
FFI void fg166_host_entry(int64_t);
#undef FFI
#ifdef __cplusplus
}
#endif
#endif
