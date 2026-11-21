#pragma once

#include "ggml.h"

// The graph and its tensor descriptors must outlive the returned storage.
void * fg_standalone_graph_alloc(struct ggml_cgraph * graph);
struct fg_standalone_graph_plan;
// The graph topology and bindings must stay fixed while the plan is live.
// Only the selected output, if any, is published from the plan's temporary arena.
struct fg_standalone_graph_plan * fg_standalone_graph_plan_alloc(struct ggml_cgraph * graph, const struct ggml_tensor * output);
void fg_standalone_graph_plan_free(struct fg_standalone_graph_plan * plan);
enum ggml_status fg_standalone_graph_plan_compute(struct fg_standalone_graph_plan * plan);
enum ggml_status fg_standalone_graph_compute(struct ggml_cgraph * graph);
// Only the selected graph output needs to be visible to the host.
enum ggml_status fg_standalone_graph_compute_output(struct ggml_cgraph * graph, const struct ggml_tensor * output);
int64_t fg_standalone_configure_cpu_batch(int64_t threads);
int64_t fg_standalone_cpu_batch_threads(void);
