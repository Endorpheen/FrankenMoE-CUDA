#pragma once
#include <cstdint>
#include <vector>
#include <map>
struct llama_model;
struct fg151_request {
 void*state=nullptr;const llama_model*model=nullptr;
 const std::vector<ggml_backend_t>*backends=nullptr;
 const std::vector<ggml_backend_buffer_type_t>*bufts=nullptr;
 size_t bytes=0;std::vector<ggml_tensor*>experts;std::vector<size_t>tensors;std::vector<int64_t>model_indices;size_t host_tensors=0;
 ggml_backend_t backend=nullptr;const ggml_tensor*src=nullptr;ggml_tensor*dst=nullptr;const ggml_tensor*weight=nullptr;ggml_cgraph*graph=nullptr;
 const ggml_tensor*lookup=nullptr;const ggml_tensor*selection=nullptr;
 int64_t layer=-1,group=-1,bank=-1,expert=0,last=0,offset=0,transfer_size=0;size_t hits=0;
 std::map<ggml_backend_buffer_type_t,size_t>*memory=nullptr;
 int64_t(*read)(fg151_request*,int64_t,int64_t,int64_t)=nullptr;
 void(*write)(fg151_request*,int64_t,int64_t,int64_t,int64_t)=nullptr;
 int64_t(*service)(fg151_request*,int64_t,int64_t,int64_t,int64_t)=nullptr;
};
extern "C" int64_t fg151_lifecycle(int64_t,int64_t);
extern "C" int64_t fg151_meta(int64_t,int64_t,int64_t,int64_t);
extern "C" void fg151_put(int64_t,int64_t,int64_t,int64_t,int64_t);
extern "C" int64_t fg151_action(int64_t,int64_t,int64_t,int64_t,int64_t);
