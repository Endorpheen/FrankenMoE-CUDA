#pragma once
// worker-4: per-decode call frame for context-execution.goose.
// Mirrors the parent-graph-input-frame.hpp principle: a stack-owned frame with
// explicit pointers and deterministic destruction, passed to the Goose entry
// as an opaque handle. No process-wide scratch, no shared mutable statics.
#include "llama-context.h"
#include "llama-graph.h"
#include "llama-io.h"
#include "llama-memory.h"

#include <cstdint>
#include <memory>
#include <vector>

struct FG107ExecFrame {
    explicit FG107ExecFrame(llama_context & ctx) : lctx(&ctx) {}
    ~FG107ExecFrame() = default;

    FG107ExecFrame(const FG107ExecFrame &)            = delete;
    FG107ExecFrame & operator=(const FG107ExecFrame &) = delete;

    // Parent glue usage (inside llama_context::decode):
    //   FG107ExecFrame frame(*this);
    //   const int rc = fg107_goose_decode((int64_t)(intptr_t)&frame,
    //                                     (int64_t)(intptr_t)&batch_inp);
    // The frame destructor frees mctx/gparams before returning to the caller.
    llama_context * lctx = nullptr;

    llama_memory_context_ptr          mctx;     // owned per decode call
    std::unique_ptr<llm_graph_params> gparams;  // owned per graph_params call
    std::vector<int32_t>              sampler_counts;
    int                               last_error = 0;
    int                               last_ret   = 0;

    // State/checkpoint IO objects (owned per call, deterministic cleanup).
    std::unique_ptr<llama_io_write_i> wio;
    std::unique_ptr<llama_io_read_i>  rio;

    // Constructor-parameter values needed by resource_init (filled by parent
    // glue before fg107_goose_resource_init; not global state).
    void * abort_cb   = nullptr;
    void * abort_data = nullptr;
    int    type_k     = 0;
    int    type_v     = 0;
    bool   swa_full   = false;
    const void * params = nullptr; // const llama_context_params* for cparams policy
};
