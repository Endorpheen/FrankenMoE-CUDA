#pragma once
#include "models.h"
#include "bridge.h"
struct ApexLoadOwner {llama_model_base & model;llama_model_loader & loader;int trunk;uint32_t interval=4;};
inline bool fg153_apex_enabled(const llama_model &m){const char *v=std::getenv("FG153_APEX_MODEL");return (!v||std::strcmp(v,"0")!=0)&&m.arch==LLM_ARCH_QWEN35MOE&&m.name=="Apex-35B-V1.0"&&m.hparams.n_layer()==40&&m.hparams.n_embd==2048;}
