#ifndef FG165_REGISTRY_FFI_H
#define FG165_REGISTRY_FFI_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define FFI __attribute__((visibility("hidden")))
FFI uint64_t fg165_arg(int64_t,int64_t);
FFI int64_t fg165_state(void);
FFI uint64_t fg165_get(int64_t,int64_t);
FFI void fg165_put(int64_t,int64_t,uint64_t);
FFI uint64_t fg165_ptr(int64_t,uint64_t);
FFI void fg165_ptr_put(int64_t,uint64_t,uint64_t);
FFI int64_t fg165_alloc(uint64_t);
FFI void fg165_free(int64_t);
FFI void fg165_copy(int64_t,int64_t,uint64_t);
FFI uint64_t fg165_byte(int64_t,uint64_t);
FFI void fg165_byte_put(int64_t,uint64_t,uint64_t);
FFI uint64_t fg165_lower(uint64_t);
FFI int64_t fg165_builtin(int64_t);
FFI int64_t fg165_literal(int64_t);
FFI uint64_t fg165_effect(int64_t,int64_t,int64_t,int64_t);
FFI int64_t fg165_env(int64_t);
FFI void fg165_mark(int64_t);
FFI void fg165_log(int64_t,int64_t,int64_t);
FFI void fg165_fail(int64_t);
FFI uint64_t fg165_goose_entry(int64_t);
#undef FFI
#ifdef __cplusplus
}
#endif
#endif
