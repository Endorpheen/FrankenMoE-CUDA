#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#define main fg_allocator_unused_cli_main
#include "../../generated/allocator-reuse.c"
#undef main
_Static_assert(sizeof(size_t) == 8, "64-bit allocator ABI");
_Static_assert(sizeof(gg_type_1) == 16, "free-block ABI size");
_Static_assert(offsetof(gg_type_1, f1) == 8, "free-block size ABI offset");
int64_t fg_selfhost_choose_reuse(const void *blocks, int count, size_t requested, size_t *available) {
    assert(count >= 0 && count <= 256);
    gg_type_6 result = gg_function_1(gg_view((void *)blocks, count), requested, 0);
    *available = result.f3;
    return result.f2;
}
