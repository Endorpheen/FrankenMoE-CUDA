// Real CPU integration TU for the worker-6 provider block.
//
// Source-bound port of the three integration points in inputs/active-cpu:
//   1. native-bridge.cpp : fg143_try              -> dispatcher
//   2. ggml-cpu-integrated.c : ggml_graph_plan    -> workspace planner
//   3. control/services.c : fg143_ctl_goose_work  -> control runtime
//
// Compiled as a preloadable CPU integration object together with the real
// generated provider unit (provider-unit.c / provider-generated.c) and
// provider-dispatch.c. Normal ggml_backend graph execution enters the
// provider through fg143_try; there is no direct test-only provider call.
#define _GNU_SOURCE
#include "../provider.h"
#include "ggml.h"
#include "ggml-cpu.h"
#include "ggml-cpu-impl.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct fg006_integration_stats {
    uint64_t dispatcher_calls;
    uint64_t planner_calls;
    uint64_t planner_increases;
    uint64_t control_calls;
    uint64_t control_threads;
    size_t planner_old;
    size_t planner_need;
    size_t provider_workspace_n1;
    size_t provider_workspace_n8;
    int enabled;
};
struct fg006_integration_stats fg006_integration_stats;

static int integration_enabled = 1;
static int fg006_plan_threads = 0;
__attribute__((constructor)) static void fg006_integration_config(void) {
    const char * v = getenv("FG006_NATIVE_ROLLBACK");
    integration_enabled = !v || strcmp(v, "1") != 0;
    fg006_integration_stats.enabled = integration_enabled;
}

typedef uint8_t (*try_fn)(const struct ggml_compute_params *, struct ggml_tensor *);
typedef struct ggml_cplan (*plan_fn)(const struct ggml_cgraph *, int, struct ggml_threadpool *);
typedef int64_t (*goose_work_fn)(int64_t, int64_t, int64_t);
typedef size_t (*extra_work_fn)(const struct ggml_tensor *, int);

static void * next_symbol(const char * name) {
    void * p = dlsym(RTLD_NEXT, name);
    if (!p) {
        fprintf(stderr, "FG006_INTEGRATION missing symbol %s\n", name);
        abort();
    }
    return p;
}

// 1. Dispatcher: inputs/active-cpu/native-bridge.cpp:40-46.
uint8_t fg143_try(const struct ggml_compute_params * params, struct ggml_tensor * t) {
    static try_fn original;
    if (!original) original = (try_fn) next_symbol("fg143_try");
    if (integration_enabled && fg006_try(params, t)) {
        ++fg006_integration_stats.dispatcher_calls;
        return 1;
    }
    return original(params, t);
}

// 2. Workspace planner: inputs/active-cpu/ggml-cpu-integrated.c:3231-3482.
// The native planner sizes Goose scratch from the native n_tasks value. GET_ROWS
// has n_tasks == 1, while the Goose GetRows kernel splits by params->nth, so the
// provider scratch must be requested with the final plan.n_threads.
struct ggml_cplan ggml_graph_plan(const struct ggml_cgraph * cgraph, int n_threads,
        struct ggml_threadpool * threadpool) {
    static plan_fn original;
    if (!original) original = (plan_fn) next_symbol("ggml_graph_plan");
    fg006_plan_threads = n_threads > 0 ? n_threads : 1;
    struct ggml_cplan plan = original(cgraph, n_threads, threadpool);
    ++fg006_integration_stats.planner_calls;
    if (!integration_enabled) return plan;

    int threads = plan.n_threads > 0 ? plan.n_threads : (n_threads > 0 ? n_threads : 1);
    size_t need = 0, need1 = 0;
    for (int i = 0; i < cgraph->n_nodes; ++i) {
        size_t cur = fg006_extra_workspace(cgraph->nodes[i], threads);
        size_t cur1 = fg006_extra_workspace(cgraph->nodes[i], 1);
        if (cur > need) need = cur;
        if (cur1 > need1) need1 = cur1;
    }
    fg006_integration_stats.planner_need = need;
    fg006_integration_stats.provider_workspace_n8 = need;
    fg006_integration_stats.provider_workspace_n1 = need1;
    fg006_integration_stats.planner_old = plan.work_size;
    if (need > 0) {
        size_t total = need + 64u * (size_t) threads;
        if (total > plan.work_size) {
            fg006_integration_stats.planner_increases++;
            plan.work_size = total;
            fprintf(stderr, "FG006_PLAN threads=%d old=%zu need=%zu new=%zu\n",
                    threads, fg006_integration_stats.planner_old, need, total);
        }
    }
    return plan;
}

// 3. Control runtime: inputs/active-cpu/control/services.c:2,7,32.
struct fg143_ctl_context {
    const struct ggml_cgraph * graph;
    struct ggml_tensor * node;
    struct ggml_threadpool * pool;
    struct ggml_compute_params * params;
};
static struct ggml_tensor * ctl_node(struct fg143_ctl_context * x, int64_t index) {
    if (x->graph) {
        if (index < 0 || index >= x->graph->n_nodes) return NULL;
        return x->graph->nodes[index];
    }
    return index == 0 ? x->node : NULL;
}
int64_t fg143_ctl_goose_work(int64_t context, int64_t index, int64_t tasks) {
    static goose_work_fn original;
    static extra_work_fn original_extra;
    if (!original) original = (goose_work_fn) next_symbol("fg143_ctl_goose_work");
    if (!original_extra) original_extra = (extra_work_fn) next_symbol("fg143_extra_workspace");
    struct fg143_ctl_context * x = (struct fg143_ctl_context *)(uintptr_t) context;
    struct ggml_tensor * node = ctl_node(x, index);
    if (!integration_enabled || !node) return original(context, index, tasks);
    int threads = fg006_plan_threads > 0 ? fg006_plan_threads : (int) tasks;
    size_t size = fg006_extra_workspace(node, threads);
    if (size == 0) size = original_extra(node, threads);
    ++fg006_integration_stats.control_calls;
    fg006_integration_stats.control_threads = (uint64_t) threads;
    if (size > (size_t) INT64_MAX) abort();
    return (int64_t) size;
}

__attribute__((destructor)) static void fg006_integration_report(void) {
    fprintf(stderr,
            "FG006_INTEGRATION enabled=%d dispatcher=%llu planner=%llu increases=%llu "
            "control=%llu threads=%llu old=%zu need=%zu n1=%zu n8=%zu\n",
            fg006_integration_stats.enabled,
            (unsigned long long) fg006_integration_stats.dispatcher_calls,
            (unsigned long long) fg006_integration_stats.planner_calls,
            (unsigned long long) fg006_integration_stats.planner_increases,
            (unsigned long long) fg006_integration_stats.control_calls,
            (unsigned long long) fg006_integration_stats.control_threads,
            fg006_integration_stats.planner_old, fg006_integration_stats.planner_need,
            fg006_integration_stats.provider_workspace_n1,
            fg006_integration_stats.provider_workspace_n8);
}
