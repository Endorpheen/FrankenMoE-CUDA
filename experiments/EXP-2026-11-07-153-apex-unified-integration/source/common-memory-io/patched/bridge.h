/* bridge.h - extern contract for the common memory/IO Goose module. */
#ifndef FG153_COMMON_BRIDGE_H
#define FG153_COMMON_BRIDGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* generic box field read/write (storage primitive only) */
int64_t fg153_cg(int64_t p, int64_t k, int64_t i);
void    fg153_cp(int64_t p, int64_t k, int64_t i, int64_t v);
/* IO/string/visitor primitives; 1 = ok, 0 = C++ exception captured */
int64_t fg153_cs(int64_t op, int64_t a, int64_t b, int64_t c);
void    fg153_cw(int64_t mode);

#ifdef __cplusplus
}
#endif
#endif /* FG153_COMMON_BRIDGE_H */
