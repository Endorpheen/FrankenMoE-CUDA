#ifndef FG167_HOST_FFI_H
#define FG167_HOST_FFI_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define FFI __attribute__((visibility("hidden")))
FFI int64_t fg167_arg(int64_t,int64_t);
FFI int64_t fg167_plan_ptr(int64_t);
FFI uint64_t fg167_hget(int64_t,int64_t,int64_t);
FFI void fg167_hput(int64_t,int64_t,int64_t,uint64_t);
FFI int64_t fg167_ti(int64_t,int64_t,int64_t);
FFI uint8_t fg167_pred(int64_t,int64_t,int64_t);
FFI int64_t fg167_constant(int64_t);
FFI void fg167_launch(int64_t);
FFI void fg167_fail(int64_t);
FFI void fg167_host_entry(int64_t);
#undef FFI
#ifdef __cplusplus
}
#endif
#endif
