#pragma once
/* worker-8/threadpool: result record for the NUMA affinity enumeration that
 * remains native in inputs/native/ggml/src/ggml-cpu/ggml-cpu.c
 * (ggml_numa_init, lines 637-723).
 *
 * This is an internal POD owned by the hook and the Goose module.  It is not a
 * mirror of a private ggml struct: the patched TU copies these fields into
 * g_state.numa with explicit typed assignments, so the Goose side never
 * guesses layout of ggml_numa_nodes.
 *
 * The dimensions match GGML_NUMA_MAX_NODES / GGML_NUMA_MAX_CPUS in ggml-cpu.c
 * (8 and 512). */

#include <sched.h>
#include <stdint.h>

#define FG153_NUMA_MAX_NODES 8
#define FG153_NUMA_MAX_CPUS 512

struct fg153_numa_node {
    uint32_t cpus[FG153_NUMA_MAX_CPUS];
    uint32_t n_cpus;
};

struct fg153_numa_result {
    int32_t strategy;
    int32_t _pad0;
    struct fg153_numa_node nodes[FG153_NUMA_MAX_NODES];
    uint32_t n_nodes;
    uint32_t total_cpus;
    uint32_t current_node;
    uint32_t _pad1;
    cpu_set_t cpuset;
};

/* Return codes of fg153_numa_run. */
#define FG153_NUMA_ALREADY 0 /* n_nodes was already non-zero */
#define FG153_NUMA_OK 1      /* enumeration succeeded */
#define FG153_NUMA_INVALID 2 /* no nodes/cpus or getcpu failed */
