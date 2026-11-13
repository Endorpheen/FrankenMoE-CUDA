#ifndef FG162_BUFFERS_FFI_H
#define FG162_BUFFERS_FFI_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define FFI __attribute__((visibility("hidden")))
FFI void fg162_mark(int64_t);
FFI uint64_t fg162_arg(int64_t,int64_t);
FFI void fg162_arg_put(int64_t,int64_t,uint64_t);
FFI uint64_t fg162_constant(int64_t);
FFI int64_t fg162_heap(int64_t,uint64_t);
FFI void fg162_drop(int64_t,int64_t);
FFI uint64_t fg162_get(int64_t,int64_t,int64_t,int64_t);
FFI void fg162_put(int64_t,int64_t,int64_t,int64_t,uint64_t);
FFI uint64_t fg162_callback(int64_t,int64_t,int64_t);
FFI uint64_t fg162_buft_call(int64_t,int64_t,int64_t,uint64_t);
FFI uint64_t fg162_buffer_call(int64_t,int64_t,int64_t,uint64_t);
FFI void fg162_iface_copy(int64_t,int64_t);
FFI int64_t fg162_workspace(int64_t,int64_t);
FFI int64_t fg162_multi_iface(void);
FFI uint64_t fg162_multi_free_marker(void);
FFI uint64_t fg162_nbytes(int64_t);
FFI int64_t fg162_quantized(int64_t);
FFI int64_t fg162_is_meta(int64_t);
FFI void fg162_meta_usage(int64_t,int64_t);
FFI void fg162_tallocr_init(int64_t,int64_t);
FFI int64_t fg162_tallocr_step(int64_t,int64_t);
FFI void fg162_snapshot(int64_t,int64_t,int64_t,int64_t);
FFI uint64_t fg162_ptr(int64_t,int64_t);
FFI void fg162_ptr_put(int64_t,int64_t,uint64_t);
FFI void fg162_fail(int64_t);
FFI uint64_t fg162_goose_entry(int64_t);
#undef FFI
#ifdef __cplusplus
}
#endif
#endif
