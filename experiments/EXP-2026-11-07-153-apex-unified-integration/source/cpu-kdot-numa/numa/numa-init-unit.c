/* worker-8/threadpool: wrapper TU for generated/numa-init.c.
 * entry() is gg_function_1; this gives it the stable C ABI used by the hook. */

#include "numa-init-bridge.h"

#include <stdint.h>

#define main fg153_numa_unused_main
#include "generated/numa-init.c"
#undef main

int64_t fg153_numa_run(int64_t result, int64_t strategy, int64_t existing_nodes) {
    return gg_function_1(result, strategy, existing_nodes, 0);
}
