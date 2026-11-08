/* worker-8/threadpool: O(1) OS primitives for numa-init.goose.
 *
 * Everything here is a single syscall or a single file open.  The enumeration
 * loops, already-initialized decision, validity decision and warning decision
 * live in Goose (numa-init.goose). */

#include "numa-init-bridge.h"

#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

static bool w8_numa_enabled = true;

__attribute__((constructor)) static void w8_numa_config(void) {
    const char * value = getenv("GGML_GOOSE_CPU_NUMA");
    if (value && atoi(value) == 0) w8_numa_enabled = false;
}

int fg153_numa_enabled(void) { return w8_numa_enabled ? 1 : 0; }

static void w8_numa_require(bool ok) {
    if (!ok) {
        fprintf(stderr, "worker-8/threadpool numa: invariant failed\n");
        abort();
    }
}

/* Test seam for the OS path formatter.  Defaults to the real sysfs; the local
 * corpus points it at a synthetic tree so the enumeration loops are covered
 * even when the isolated sandbox does not mount /sys. */
static const char * w8_sysfs_root(void) {
    const char * root = getenv("FG153_SYSFS_ROOT");
    return (root && root[0]) ? root : "/sys";
}

static struct fg153_numa_result * w8_result(int64_t handle) {
    struct fg153_numa_result * r = (struct fg153_numa_result *)(intptr_t) handle;
    w8_numa_require(r != NULL);
    return r;
}

int64_t fg153_np_set_strategy(int64_t result, int64_t strategy) {
    struct fg153_numa_result * r = w8_result(result);
    w8_numa_require(strategy >= 0 && strategy <= 4);
    r->strategy = (int32_t) strategy;
    return 0;
}

int64_t fg153_np_snapshot_cpuset(int64_t result) {
    struct fg153_numa_result * r = w8_result(result);
    cpu_set_t set;
    CPU_ZERO(&set);
    if (pthread_getaffinity_np(pthread_self(), sizeof(set), &set) != 0) {
        CPU_ZERO(&set);
    }
    r->cpuset = set;
    return 0;
}

int64_t fg153_np_node_exists(int64_t node) {
    w8_numa_require(node >= 0 && node < FG153_NUMA_MAX_NODES);
    char path[256];
    int n = snprintf(path, sizeof(path), "%s/devices/system/node/node%ld", w8_sysfs_root(), (long) node);
    w8_numa_require(n > 0 && (size_t) n < sizeof(path));
    struct stat st;
    return stat(path, &st) == 0 ? 1 : 0;
}

int64_t fg153_np_cpu_exists(int64_t cpu) {
    w8_numa_require(cpu >= 0 && cpu < FG153_NUMA_MAX_CPUS);
    char path[256];
    int n = snprintf(path, sizeof(path), "%s/devices/system/cpu/cpu%ld", w8_sysfs_root(), (long) cpu);
    w8_numa_require(n > 0 && (size_t) n < sizeof(path));
    struct stat st;
    return stat(path, &st) == 0 ? 1 : 0;
}

int64_t fg153_np_node_cpu_exists(int64_t node, int64_t cpu) {
    w8_numa_require(node >= 0 && node < FG153_NUMA_MAX_NODES);
    w8_numa_require(cpu >= 0 && cpu < FG153_NUMA_MAX_CPUS);
    char path[256];
    int n = snprintf(path, sizeof(path), "%s/devices/system/node/node%ld/cpu%ld", w8_sysfs_root(), (long) node, (long) cpu);
    w8_numa_require(n > 0 && (size_t) n < sizeof(path));
    struct stat st;
    return stat(path, &st) == 0 ? 1 : 0;
}

int64_t fg153_np_getcpu(int64_t result) {
    struct fg153_numa_result * r = w8_result(result);
#if defined(SYS_getcpu)
    unsigned cpu = 0;
    unsigned node = 0;
    long rc = syscall(SYS_getcpu, &cpu, &node, NULL);
    if (rc != 0) return -1;
    r->current_node = (uint32_t) node;
    return (int64_t) cpu;
#else
    (void) r;
    return -1;
#endif
}

int64_t fg153_np_set_counts(int64_t result, int64_t nodes, int64_t cpus) {
    struct fg153_numa_result * r = w8_result(result);
    w8_numa_require(nodes >= 0 && nodes <= FG153_NUMA_MAX_NODES);
    w8_numa_require(cpus >= 0 && cpus <= FG153_NUMA_MAX_CPUS);
    r->n_nodes = (uint32_t) nodes;
    r->total_cpus = (uint32_t) cpus;
    return 0;
}

int64_t fg153_np_set_node_ncpus(int64_t result, int64_t node, int64_t count) {
    struct fg153_numa_result * r = w8_result(result);
    w8_numa_require(node >= 0 && node < FG153_NUMA_MAX_NODES);
    w8_numa_require(count >= 0 && count <= FG153_NUMA_MAX_CPUS);
    r->nodes[node].n_cpus = (uint32_t) count;
    return 0;
}

int64_t fg153_np_set_node_cpu(int64_t result, int64_t node, int64_t index, int64_t cpu) {
    struct fg153_numa_result * r = w8_result(result);
    w8_numa_require(node >= 0 && node < FG153_NUMA_MAX_NODES);
    w8_numa_require(index >= 0 && index < FG153_NUMA_MAX_CPUS);
    w8_numa_require(cpu >= 0 && cpu < FG153_NUMA_MAX_CPUS);
    r->nodes[node].cpus[index] = (uint32_t) cpu;
    return 0;
}

int64_t fg153_np_balancing(void) {
    FILE * f = fopen("/proc/sys/kernel/numa_balancing", "r");
    if (f == NULL) return 0;
    char buf[42];
    int enabled = 0;
    if (fgets(buf, sizeof(buf), f) && strncmp(buf, "0\n", sizeof(buf)) != 0) {
        enabled = 1;
    }
    fclose(f);
    return enabled;
}

void fg153_np_warn(int64_t code) {
    if (code == 0) {
        fprintf(stderr, "ggml_numa_init: NUMA already initialized\n");
    } else if (code == 2) {
        fprintf(stderr, "worker-8/threadpool: /proc/sys/kernel/numa_balancing is enabled\n");
    }
}

#ifdef W8_WITNESS
static unsigned long long w8_modes[8];
#endif

void fg153_np_witness(int64_t mode) {
    w8_numa_require(mode >= 0 && mode < 8);
#ifdef W8_WITNESS
    w8_modes[mode]++;
#endif
}

#ifdef W8_WITNESS
__attribute__((destructor)) static void w8_numa_report(void) {
    for (int i = 0; i < 8; ++i) {
        if (w8_modes[i]) {
            fprintf(stderr, "FG153_NUMA mode=%d calls=%llu\n", i, w8_modes[i]);
        }
    }
}
#endif
