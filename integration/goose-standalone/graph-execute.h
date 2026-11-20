#pragma once

#include "ggml.h"

// The graph and its tensor descriptors must outlive the returned storage.
void * fg_standalone_graph_alloc(struct ggml_cgraph * graph);
enum ggml_status fg_standalone_graph_compute(struct ggml_cgraph * graph);
// Only the selected graph output needs to be visible to the host.
enum ggml_status fg_standalone_graph_compute_output(struct ggml_cgraph * graph, const struct ggml_tensor * output);
