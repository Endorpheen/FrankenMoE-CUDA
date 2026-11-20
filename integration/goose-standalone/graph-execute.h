#pragma once

#include "ggml.h"

// The graph and its tensor descriptors must outlive the returned storage.
void * fg_standalone_graph_alloc(struct ggml_cgraph * graph);
enum ggml_status fg_standalone_graph_compute(struct ggml_cgraph * graph);
