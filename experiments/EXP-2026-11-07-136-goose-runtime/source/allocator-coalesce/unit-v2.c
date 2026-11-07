#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#define main fg_coalesce_unused_cli_main
#include "../../generated/allocator-coalesce.c"
#undef main
_Static_assert(sizeof(gg_type_1) == 16, "free-block ABI size");
_Static_assert(offsetof(gg_type_1, f1) == 8, "free-block size ABI offset");
int fg_selfhost_release_block(void *blocks, int count, size_t offset, size_t size) {
    assert(count >= 0 && count <= 256);
    int64_t result = gg_function_1(gg_view(blocks, 256), count, offset, size, 0);
    assert(result >= 0 && result <= 256);
    return (int)result;
}
