#pragma once
/* worker-8/threadpool: C contract for numa-init.goose.
 *
 * The Goose module owns the affinity enumeration decisions and loops:
 * already-initialized early return, strategy/cpuset publication, node and CPU
 * probing loops, per-node CPU membership loops, validity decision, and the
 * numa_balancing warning decision.  The services provide only O(1) OS
 * primitives: sysfs existence probes, pthread affinity snapshot, getcpu and
 * diagnostics.  No path formatting or enumeration loop lives in C. */

#include "numa-result.h"

#ifdef __cplusplus
extern "C" {
#endif

int fg153_numa_enabled(void);
int64_t fg153_numa_run(int64_t result, int64_t strategy, int64_t existing_nodes);

/* Typed OS primitives implemented in numa-init-services.c. */
int64_t fg153_np_set_strategy(int64_t result, int64_t strategy);
int64_t fg153_np_snapshot_cpuset(int64_t result);
int64_t fg153_np_node_exists(int64_t node);
int64_t fg153_np_cpu_exists(int64_t cpu);
int64_t fg153_np_node_cpu_exists(int64_t node, int64_t cpu);
int64_t fg153_np_getcpu(int64_t result);
int64_t fg153_np_set_counts(int64_t result, int64_t nodes, int64_t cpus);
int64_t fg153_np_set_node_ncpus(int64_t result, int64_t node, int64_t count);
int64_t fg153_np_set_node_cpu(int64_t result, int64_t node, int64_t index, int64_t cpu);
int64_t fg153_np_balancing(void);
void    fg153_np_warn(int64_t code);
void    fg153_np_witness(int64_t mode);

#ifdef __cplusplus
}
#endif
