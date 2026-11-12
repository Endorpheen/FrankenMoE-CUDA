#ifndef FG161_ALLOCATOR_API_H
#define FG161_ALLOCATOR_API_H
#include <stdint.h>
static int fg161_enabled;
static inline uint64_t fg161_run(const uint64_t *args);
#define FG161_A(x) ((uint64_t)(uintptr_t)(x))
#define FG161_CALL(...) fg161_run((const uint64_t[]){__VA_ARGS__})
#endif
