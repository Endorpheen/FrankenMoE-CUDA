#include <cstdlib>
#include <cstring>
#include <iostream>
#include "binding-owner.hpp"
int main(){llama_model_params p={};llama_model_qwen35moe m(p);m.name="Apex-35B-V1.0";m.arch=LLM_ARCH_QWEN35MOE;m.hparams={};m.hparams.n_layer_all=41;m.hparams.n_layer_nextn=1;m.hparams.n_embd=2048;unsetenv("FG153_APEX_MODEL");if(!fg153_apex_enabled(m))return 1;setenv("FG153_APEX_MODEL","0",1);if(fg153_apex_enabled(m))return 2;setenv("FG153_APEX_MODEL","1",1);if(!fg153_apex_enabled(m))return 3;m.name="unmatched";if(fg153_apex_enabled(m))return 4;std::cout<<"PASS exact Apex default-on, explicit native rollback, unsupported-name exclusion\n";}
