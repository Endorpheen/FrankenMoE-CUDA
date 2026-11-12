#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#define main fg_lifetime_unused_cli_main
#include "../../generated/tensor-lifetime.c"
#undef main
_Static_assert(sizeof(gg_type_1) == 40, "GGML hash-node ABI size");
_Static_assert(offsetof(gg_type_1, f0) == 0, "children ABI offset");
_Static_assert(offsetof(gg_type_1, f1) == 4, "views ABI offset");
_Static_assert(offsetof(gg_type_1, f6) == 24, "buffer offset ABI");
_Static_assert(offsetof(gg_type_1, f7) == 32, "allocated flag ABI");
int fg_selfhost_parent_done(void *node, bool is_view) { return gg_function_1((gg_type_1 *)node, is_view, 0); }
bool fg_selfhost_view_done(void *node) { return gg_function_2((gg_type_1 *)node, 0); }
