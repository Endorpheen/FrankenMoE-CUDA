#pragma once
// Local syntax-check shim: the parent frame against the native header tree.
// Production uses the real inputs/parent-graph-input-frame.hpp.
#include "llama-graph.h"
#include "llama-memory-hybrid.h"
#include "bridge.h"

struct FG153HybridInputFrame {
    const llm_graph_context *graph = nullptr;
    llm_graph_input_mem_hybrid *input = nullptr;
    const llama_ubatch *batch = nullptr;
    const llm_graph_params *params = nullptr;
    const llama_memory_hybrid_context *memory = nullptr;
    std::unique_ptr<llm_graph_input_rs> rs;
    std::unique_ptr<llm_graph_input_attn_kv> attn;
    explicit FG153HybridInputFrame(const llm_graph_context & g)
        : graph(&g), batch(&g.ubatch), memory(static_cast<const llama_memory_hybrid_context *>(g.mctx)) {}
    FG153HybridInputFrame(llm_graph_input_mem_hybrid * i, const llama_ubatch * u, const llm_graph_params * p = nullptr)
        : input(i), batch(u), params(p), memory(i->mctx) {}
};
