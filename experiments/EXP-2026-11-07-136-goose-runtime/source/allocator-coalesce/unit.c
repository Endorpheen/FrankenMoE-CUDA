#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
static inline void fg_blocks_move_raw(void *data, int64_t capacity, int64_t destination, int64_t source, int64_t count) {
    assert(capacity >= 0 && capacity <= 256 && destination >= 0 && source >= 0 && count >= 0);
    assert(destination <= capacity && source <= capacity && count <= capacity - destination && count <= capacity - source);
    memmove((unsigned char *)data + (size_t)destination * 16, (unsigned char *)data + (size_t)source * 16, (size_t)count * 16);
}
#define fg_blocks_move(view, destination, source, count) fg_blocks_move_raw((view).data, (view).len, destination, source, count)
#define main fg_coalesce_unused_cli_main
#include "../../generated/allocator-coalesce.c"
#undef main
#undef fg_blocks_move
_Static_assert(sizeof(gg_type_1) == 16, "free-block ABI size");
_Static_assert(offsetof(gg_type_1, f1) == 8, "free-block size ABI offset");
int fg_selfhost_release_block(void *blocks, int count, size_t offset, size_t size) {
    assert(count >= 0 && count <= 256);
    int64_t result = gg_function_1(gg_view(blocks, 256), count, offset, size, 0);
    assert(result >= 0 && result <= 256);
    return (int)result;
}
