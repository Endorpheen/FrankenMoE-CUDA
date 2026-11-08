#pragma once
#include "models.h"
#include "bridge.h"
struct ApexOwner final : llm_graph_context {
 const llama_model & model;
 std::array<int,4> rope_sections;
 std::array<ggml_tensor *,8> probe={};
 std::unique_ptr<llm_graph_input_embd> pending_embedding;
 std::vector<llm_graph_input_ptr> pending_inputs;
 explicit ApexOwner(const llama_model & m,const llm_graph_params & p,bool execute=true):llm_graph_context(p),model(m),rope_sections(hparams.rope_sections) {if(execute)fg153_apex_build((int64_t)(intptr_t)this);}
};
