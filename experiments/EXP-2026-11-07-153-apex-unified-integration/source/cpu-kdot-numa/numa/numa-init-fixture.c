/* worker-8/threadpool: native-vs-Goose differential for numa-init.goose.
 *
 * The native reference below is a direct transcription of
 * inputs/native/ggml/src/ggml-cpu/ggml-cpu.c:ggml_numa_init (637-723) into a
 * private result record.  Both arms run on the same machine/sysfs and the same
 * pinned thread, so node/CPU membership, getcpu node, strategy and cpuset must
 * match exactly.  A real runtime cross-check calls the qualified
 * ggml_numa_init/ggml_is_numa from libggml-cpu.
 *
 * No model, no benchmark, no sanitizer campaign. */

#include "ggml.h"
#include "ggml-cpu.h"
#include "numa-init-bridge.h"

#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

static int g_errors;

#define CHECK(x) do { \
    if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); g_errors++; } \
} while (0)

/* Mirror of ggml_get_numa_affinity() (637-635 in the snapshot). */
static void np_snapshot(cpu_set_t * set) {
    CPU_ZERO(set);
    if (pthread_getaffinity_np(pthread_self(), sizeof(*set), set) != 0) {
        CPU_ZERO(set);
    }
}

static int np_getcpu(unsigned * cpu_out, unsigned * node_out) {
#if defined(SYS_getcpu)
    unsigned cpu = 0, node = 0;
    if (syscall(SYS_getcpu, &cpu, &node, NULL) != 0) return -1;
    *cpu_out = cpu;
    *node_out = node;
    return 0;
#else
    (void) cpu_out; (void) node_out;
    return -1;
#endif
}

static const char * fixture_root(void) {
    const char * root = getenv("FG153_SYSFS_ROOT");
    return (root && root[0]) ? root : "/sys";
}

/* Native reference: exact structure of ggml_numa_init's Linux branch. */
static int64_t np_native(struct fg153_numa_result * r, int strategy) {
    r->strategy = strategy;
    np_snapshot(&r->cpuset);

    uint32_t nodes = 0;
    while (nodes < FG153_NUMA_MAX_NODES) {
        char path[256];
        int n = snprintf(path, sizeof(path), "%s/devices/system/node/node%u", fixture_root(), nodes);
        if (n <= 0 || (size_t) n >= sizeof(path)) break;
        struct stat st;
        if (stat(path, &st) != 0) break;
        ++nodes;
    }

    uint32_t cpus = 0;
    while (cpus < FG153_NUMA_MAX_CPUS) {
        char path[256];
        int n = snprintf(path, sizeof(path), "%s/devices/system/cpu/cpu%u", fixture_root(), cpus);
        if (n <= 0 || (size_t) n >= sizeof(path)) break;
        struct stat st;
        if (stat(path, &st) != 0) break;
        ++cpus;
    }

    unsigned current_cpu = 0, current_node = 0;
    int getcpu_ok = np_getcpu(&current_cpu, &current_node) == 0;

    r->total_cpus = cpus;
    r->current_node = getcpu_ok ? current_node : 0;

    if (nodes < 1 || cpus < 1 || !getcpu_ok) {
        r->n_nodes = 0;
        return FG153_NUMA_INVALID;
    }

    r->n_nodes = nodes;
    for (uint32_t n = 0; n < nodes; ++n) {
        r->nodes[n].n_cpus = 0;
        for (uint32_t c = 0; c < cpus; ++c) {
            char path[256];
            int k = snprintf(path, sizeof(path), "%s/devices/system/node/node%u/cpu%u", fixture_root(), n, c);
            if (k <= 0 || (size_t) k >= sizeof(path)) continue;
            struct stat st;
            if (stat(path, &st) == 0) {
                r->nodes[n].cpus[r->nodes[n].n_cpus++] = c;
            }
        }
    }

    if (nodes > 1) {
        FILE * f = fopen("/proc/sys/kernel/numa_balancing", "r");
        if (f != NULL) {
            char buf[42];
            if (fgets(buf, sizeof(buf), f) && strncmp(buf, "0\n", sizeof(buf)) != 0) {
                /* native only warns here; the decision is compared by
                 * fg153_np_balancing() below. */
            }
            fclose(f);
        }
    }
    return FG153_NUMA_OK;
}

static void compare(const struct fg153_numa_result * a, const struct fg153_numa_result * b, int strategy) {
    CHECK(a->strategy == b->strategy);
    CHECK(a->n_nodes == b->n_nodes);
    CHECK(a->total_cpus == b->total_cpus);
    CHECK(a->current_node == b->current_node);
    CHECK(memcmp(&a->cpuset, &b->cpuset, sizeof(a->cpuset)) == 0);
    for (uint32_t n = 0; n < a->n_nodes && n < b->n_nodes; ++n) {
        CHECK(a->nodes[n].n_cpus == b->nodes[n].n_cpus);
        for (uint32_t c = 0; c < a->nodes[n].n_cpus && c < b->nodes[n].n_cpus; ++c) {
            CHECK(a->nodes[n].cpus[c] == b->nodes[n].cpus[c]);
        }
    }
    (void) strategy;
}

static void pin_first_cpu(void) {
    cpu_set_t set;
    CPU_ZERO(&set);
    if (pthread_getaffinity_np(pthread_self(), sizeof(set), &set) != 0) return;
    int first = -1;
    for (int i = 0; i < CPU_SETSIZE; ++i) {
        if (CPU_ISSET(i, &set)) { first = i; break; }
    }
    if (first < 0) return;
    cpu_set_t one;
    CPU_ZERO(&one);
    CPU_SET(first, &one);
    (void) pthread_setaffinity_np(pthread_self(), sizeof(one), &one);
}

static int balancing_native(void) {
    FILE * f = fopen("/proc/sys/kernel/numa_balancing", "r");
    if (!f) return 0;
    char buf[42];
    int enabled = 0;
    if (fgets(buf, sizeof(buf), f) && strncmp(buf, "0\n", sizeof(buf)) != 0) enabled = 1;
    fclose(f);
    return enabled;
}

/* Synthetic sysfs scenarios cover the enumeration/membership loops even when
 * the isolated sandbox does not mount /sys.  The OS path formatter reads
 * FG153_SYSFS_ROOT; all decisions remain in Goose. */
static void scenario(const char * base, const char * name, int expect_nodes, int expect_cpus) {
    char root[512];
    CHECK(snprintf(root, sizeof(root), "%s/%s", base, name) > 0);
    CHECK(setenv("FG153_SYSFS_ROOT", root, 1) == 0);

    for (int strategy = 0; strategy <= 4; ++strategy) {
        struct fg153_numa_result native;
        struct fg153_numa_result goose;
        struct fg153_numa_result again;
        memset(&native, 0, sizeof(native));
        memset(&goose, 0, sizeof(goose));
        memset(&again, 0, sizeof(again));

        int64_t cn = np_native(&native, strategy);
        int64_t cg = fg153_numa_run((int64_t)(intptr_t) &goose, strategy, 0);
        CHECK(cn == cg);
        compare(&native, &goose, strategy);
        CHECK(fg153_numa_run((int64_t)(intptr_t) &again, strategy, 0) == cg);
        compare(&goose, &again, strategy);
        CHECK(goose.n_nodes == (uint32_t) expect_nodes);
        CHECK(goose.total_cpus == (uint32_t) expect_cpus);
        CHECK(fg153_np_balancing() == balancing_native());
    }
}

int main(int argc, char ** argv) {
    pin_first_cpu();

    const char * base = argc > 1 ? argv[1] : "/work/threadpool/fake-sysfs";
    scenario(base, "empty", 0, 0);
    scenario(base, "nodes-only", 0, 0);
    scenario(base, "one", 1, 3);
    scenario(base, "two", 2, 4);
    scenario(base, "eight", 8, 8);

    /* explicit membership check for the two-node tree */
    {
        char root[512];
        CHECK(snprintf(root, sizeof(root), "%s/two", base) > 0);
        CHECK(setenv("FG153_SYSFS_ROOT", root, 1) == 0);
        struct fg153_numa_result goose;
        memset(&goose, 0, sizeof(goose));
        CHECK(fg153_numa_run((int64_t)(intptr_t) &goose, 0, 0) == FG153_NUMA_OK);
        CHECK(goose.n_nodes == 2 && goose.total_cpus == 4);
        CHECK(goose.nodes[0].n_cpus == 2 && goose.nodes[0].cpus[0] == 0 && goose.nodes[0].cpus[1] == 1);
        CHECK(goose.nodes[1].n_cpus == 2 && goose.nodes[1].cpus[0] == 2 && goose.nodes[1].cpus[1] == 3);
    }

    /* real /sys path on this machine (invalid in the sandbox) */
    unsetenv("FG153_SYSFS_ROOT");
    for (int strategy = 0; strategy <= 4; ++strategy) {
        struct fg153_numa_result native;
        struct fg153_numa_result goose;
        memset(&native, 0, sizeof(native));
        memset(&goose, 0, sizeof(goose));

        int64_t cn = np_native(&native, strategy);
        int64_t cg = fg153_numa_run((int64_t)(intptr_t) &goose, strategy, 0);
        CHECK(cn == cg);
        compare(&native, &goose, strategy);
        CHECK(fg153_np_balancing() == balancing_native());
    }

    /* already-initialized early return must not touch the result */
    {
        struct fg153_numa_result untouched;
        memset(&untouched, 0xA5, sizeof(untouched));
        int64_t code = fg153_numa_run((int64_t)(intptr_t) &untouched, 0, 3);
        CHECK(code == FG153_NUMA_ALREADY);
        int unchanged = 1;
        const unsigned char * p = (const unsigned char *) &untouched;
        for (size_t i = 0; i < sizeof(untouched); ++i) if (p[i] != 0xA5) unchanged = 0;
        CHECK(unchanged);
    }

    /* null-handle guard in the wrapper entry */
    CHECK(fg153_numa_run(0, 0, 0) == 0);

    /* real runtime cross-check: the runtime g_state must agree on is_numa */
    {
        struct fg153_numa_result goose;
        memset(&goose, 0, sizeof(goose));
        int64_t code = fg153_numa_run((int64_t)(intptr_t) &goose, GGML_NUMA_STRATEGY_DISABLED, 0);
        CHECK(code == FG153_NUMA_OK || code == FG153_NUMA_INVALID);
        ggml_numa_init(GGML_NUMA_STRATEGY_DISABLED);
        bool runtime_numa = ggml_is_numa();
        CHECK(runtime_numa == (goose.n_nodes > 1));
        printf("NUMA machine: nodes=%u cpus=%u current_node=%u is_numa=%d\n",
               goose.n_nodes, goose.total_cpus, goose.current_node, runtime_numa ? 1 : 0);
    }

    if (g_errors) {
        fprintf(stderr, "{\"status\":\"FAIL_LOCAL_NUMA_INIT\",\"errors\":%d}\n", g_errors);
        return 1;
    }
    printf("{\"status\":\"PASS_LOCAL_NUMA_INIT\",\"scenarios\":5,\"strategies\":5,\"deterministic\":true}\n");
    return 0;
}
