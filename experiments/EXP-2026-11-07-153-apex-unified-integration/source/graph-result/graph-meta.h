#pragma once
// worker-7: integration helper for the llm_graph_context resource metadata.
//
// The C++ constructor initializer list is a typed POD projection; the numeric
// metadata derivation and validation live in graph-context.goose. The parent can
// call fg153_graph_meta_fill() from a delegating constructor or from a replay
// hook that keeps the original constructor body for rollback.
#include "graph-bridge.h"

#ifdef __cplusplus
#include "llama-graph.h"

// Returns 0 on success, non-zero if the Goose entry failed or the projected
// pointer fields disagree with the source params.
int fg153_graph_meta_fill(const llm_graph_params & params, fg153_graph_meta & meta);
#endif
