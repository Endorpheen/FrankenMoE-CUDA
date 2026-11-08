#include "ggml-backend-impl.h"
#ifdef FG160_CANDIDATE
#include "transport.h"
#else
#include "ggml-impl.h"
#endif
#include <cstdint>
#include <cstring>
extern "C" __attribute__((visibility("default"))) uint64_t fg160_probe(int mode,const uint64_t *v){
auto*g=(const ggml_cgraph*)(uintptr_t)v[0];auto*t=(ggml_tensor*)(uintptr_t)v[0];switch(mode){
case 0:return (uint64_t)(int64_t)ggml_node_get_use_count(g,(int)v[1]);case 1:return ggml_node_has_n_uses(g,(int)v[1],(int32_t)v[2]);
case 2:return ggml_can_fuse_ext(g,(const int*)(uintptr_t)v[1],(const ggml_op*)(uintptr_t)v[2],(int)v[3]);
case 3:return ggml_can_fuse(g,(int)v[1],(const ggml_op*)(uintptr_t)v[2],(int)v[3]);
case 4:return ggml_can_fuse_subgraph_ext(g,(const int*)(uintptr_t)v[1],(int)v[2],(const ggml_op*)(uintptr_t)v[3],(const int*)(uintptr_t)v[4],(int)v[5]);
case 5:return ggml_can_fuse_subgraph(g,(int)v[1],(int)v[2],(const ggml_op*)(uintptr_t)v[3],(const int*)(uintptr_t)v[4],(int)v[5]);
case 6:{auto*p=(const std::array<int,3>*)(uintptr_t)v[2];switch(v[3]){case 0:return ggml_check_edges(g,(int)v[1],{});case 1:return ggml_check_edges(g,(int)v[1],{p[0]});case 2:return ggml_check_edges(g,(int)v[1],{p[0],p[1]});case 3:return ggml_check_edges(g,(int)v[1],{p[0],p[1],p[2]});default:abort();}}
case 9:ggml_set_op_params(t,(const void*)(uintptr_t)v[1],v[2]);return 0;
case 10:return (uint32_t)ggml_get_op_params_i32(t,(uint32_t)v[1]);case 11:{float f=ggml_get_op_params_f32(t,(uint32_t)v[1]);uint32_t u;memcpy(&u,&f,4);return u;}
case 12:ggml_set_op_params_i32(t,(uint32_t)v[1],(int32_t)v[2]);return 0;
case 13:{uint32_t u=(uint32_t)v[2];float f;memcpy(&f,&u,4);ggml_set_op_params_f32(t,(uint32_t)v[1],f);return 0;}
default:abort();}}
