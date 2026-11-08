#pragma once
#include "models.h"
struct NativeProbe:llm_graph_context {
 const llama_model &model;
 NativeProbe(const llama_model &m,const llm_graph_params &p):llm_graph_context(p),model(m){}
 ggml_tensor *build_layer_ffn(ggml_tensor *,int);
};
struct NativeFlow:NativeProbe {
 using NativeProbe::NativeProbe;
 std::unique_ptr<llm_graph_input_mem_hybrid> input;
 llm_graph_input_mem_hybrid *build_inp_mem_hybrid(){input=std::make_unique<llm_graph_input_mem_hybrid>(cparams,nullptr,nullptr,nullptr);return input.get();}
 ggml_tensor *build_layer_attn_linear(llm_graph_input_rs *,ggml_tensor *x,int il){auto *out=ggml_scale(ctx0,x,1.0f);cb(out,"fixture_recurrent",il);return out;}
 ggml_tensor *build_layer_attn(llm_graph_input_attn_kv *,ggml_tensor *x,ggml_tensor *,int *,int il){auto *out=ggml_scale(ctx0,x,1.0f);cb(out,"fixture_attention",il);return out;}
 void flow();
};
