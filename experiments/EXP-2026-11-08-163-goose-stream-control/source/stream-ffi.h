#ifndef FG163_STREAM_FFI_H
#define FG163_STREAM_FFI_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define FFI __attribute__((visibility("hidden")))
FFI uint64_t fg163_arg(int64_t,int64_t);
FFI uint64_t fg163_get(int64_t,int64_t,int64_t,int64_t);
FFI uint64_t fg163_callback(int64_t,int64_t,int64_t);
FFI uint64_t fg163_constant(int64_t);
FFI int64_t fg163_io(int64_t,int64_t,int64_t,int64_t,int64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
FFI uint64_t fg163_backend_call(int64_t,int64_t,int64_t,int64_t,int64_t);
FFI uint64_t fg163_device_call(int64_t,int64_t,int64_t,uint64_t,uint64_t);
FFI uint64_t fg163_reg_call(int64_t,int64_t,uint64_t);
FFI void fg163_zero(int64_t,uint64_t);
FFI uint64_t fg163_nbytes(int64_t);
FFI int64_t fg163_is_host(int64_t);
FFI int64_t fg163_buffer_copy(int64_t,int64_t);
FFI int64_t fg163_heap(uint64_t);
FFI void fg163_drop(int64_t);
FFI void fg163_fail(int64_t);
FFI void fg163_mark(int64_t);
FFI uint64_t fg163_goose_entry(int64_t);
#undef FFI
#ifdef __cplusplus
}
#endif
#endif
