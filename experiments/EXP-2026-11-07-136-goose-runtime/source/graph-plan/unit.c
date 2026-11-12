#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
void *fg_graph_hash(uint64_t, uint64_t);
uint64_t fg_graph_source(uint64_t, int64_t);
uint64_t fg_graph_owner(uint64_t);
bool fg_graph_is_view(uint64_t);
uint32_t fg_graph_flags(uint64_t);
int32_t fg_graph_op(uint64_t);
void fg_graph_allocate(uint64_t, uint64_t, int32_t);
void fg_graph_release(uint64_t, uint64_t);
void fg_graph_verify(const void *, int32_t, int32_t, bool, int32_t, int32_t, bool);
#define main fg_graph_unused_cli_main
#include "../../generated/graph-plan.c"
#undef main
_Static_assert(sizeof(gg_type_13) == 40, "hash-node ABI size");
_Static_assert(offsetof(gg_type_13, f7) == 32, "allocation flag ABI offset");
_Static_assert(sizeof(uintptr_t) == 8 && sizeof(void *) == 8, "opaque tensor-handle ABI");
_Static_assert(sizeof(int) == 4, "buffer-index ABI");
void fg_selfhost_graph_plan(void *allocator, const void *nodes, int node_count, const void *leafs, int leaf_count,
                            const int *node_buffers, const int *leaf_buffers, int max_sources,
                            unsigned input_flag, int op_none, bool verify) {
    gg_function_1((uintptr_t)allocator, gg_view((void *)nodes,node_count),gg_view((void *)leafs,leaf_count),
        gg_view((void *)node_buffers,node_buffers ? node_count : 0),gg_view((void *)leaf_buffers,leaf_buffers ? leaf_count : 0),
        max_sources,input_flag,op_none,verify,0);
}
