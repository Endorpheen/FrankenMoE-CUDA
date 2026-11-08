#ifndef FG164_SCHEDULER_FFI_H
#define FG164_SCHEDULER_FFI_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define FFI __attribute__((visibility("hidden")))
FFI uint64_t fg164_arg(int64_t,int64_t);
FFI uint64_t fg164_get(int64_t,int64_t,int64_t,int64_t);
FFI void fg164_put(int64_t,int64_t,int64_t,int64_t,uint64_t);
FFI uint64_t fg164_constant(int64_t);
FFI int64_t fg164_record(int64_t,int64_t,int64_t);
FFI uint64_t fg164_ptr(int64_t,uint64_t);
FFI void fg164_ptr_put(int64_t,uint64_t,uint64_t);
FFI int64_t fg164_i32(int64_t,uint64_t);
FFI void fg164_i32_put(int64_t,uint64_t,int64_t);
FFI int64_t fg164_heap(int64_t,uint64_t,int64_t);
FFI void fg164_drop(int64_t,int64_t);
FFI void fg164_mem(int64_t,int64_t,uint64_t,int64_t);
FFI uint64_t fg164_hash_slot(int64_t,int64_t);
FFI uint64_t fg164_dep_slot(int64_t,int64_t);
FFI uint64_t fg164_component(int64_t,int64_t,int64_t,int64_t,uint64_t,uint64_t);
FFI void fg164_view(int64_t,int64_t,int64_t,int64_t);
FFI void fg164_name(int64_t,int64_t,int64_t,int64_t);
FFI void fg164_optimize(int64_t,int64_t,int64_t);
FFI uint64_t fg164_callback(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
FFI uint64_t fg164_expert(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
FFI uint64_t fg164_expert_proc(int64_t);
FFI int64_t fg164_async_present(int64_t);
FFI uint64_t fg164_try_async(int64_t,int64_t,int64_t,int64_t);
FFI int64_t fg164_env(int64_t);
FFI int64_t fg164_atoi(int64_t);
FFI void fg164_debug(int64_t,int64_t,int64_t);
FFI void fg164_fail(int64_t);
FFI void fg164_mark(int64_t);
FFI uint64_t fg164_goose_entry(int64_t);
#undef FFI
#ifdef __cplusplus
}
#endif
#endif
