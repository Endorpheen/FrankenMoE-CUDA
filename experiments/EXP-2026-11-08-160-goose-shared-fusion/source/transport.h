#pragma once

#define ggml_node_get_use_count fg160_native_ggml_node_get_use_count
#define ggml_node_has_n_uses fg160_native_ggml_node_has_n_uses
#define ggml_can_fuse_ext fg160_native_ggml_can_fuse_ext
#define ggml_can_fuse fg160_native_ggml_can_fuse
#define ggml_can_fuse_subgraph fg160_native_ggml_can_fuse_subgraph
#define ggml_set_op_params fg160_native_ggml_set_op_params
#define ggml_get_op_params_i32 fg160_native_ggml_get_op_params_i32
#define ggml_get_op_params_f32 fg160_native_ggml_get_op_params_f32
#define ggml_set_op_params_i32 fg160_native_ggml_set_op_params_i32
#define ggml_set_op_params_f32 fg160_native_ggml_set_op_params_f32
#define ggml_bitset_size fg160_native_ggml_bitset_size
#define ggml_bitset_get fg160_native_ggml_bitset_get
#define ggml_bitset_set fg160_native_ggml_bitset_set
#define ggml_bitset_clear fg160_native_ggml_bitset_clear
#define ggml_hash fg160_native_ggml_hash
#define ggml_hash_find fg160_native_ggml_hash_find
#define ggml_hash_contains fg160_native_ggml_hash_contains
#define ggml_hash_insert fg160_native_ggml_hash_insert
#define ggml_hash_find_or_insert fg160_native_ggml_hash_find_or_insert
#define ggml_check_edges fg160_native_ggml_check_edges
#include "/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/src/ggml-impl.h"
#undef ggml_node_get_use_count
#undef ggml_node_has_n_uses
#undef ggml_can_fuse_ext
#undef ggml_can_fuse
#undef ggml_can_fuse_subgraph
#undef ggml_set_op_params
#undef ggml_get_op_params_i32
#undef ggml_get_op_params_f32
#undef ggml_set_op_params_i32
#undef ggml_set_op_params_f32
#undef ggml_bitset_size
#undef ggml_bitset_get
#undef ggml_bitset_set
#undef ggml_bitset_clear
#undef ggml_hash
#undef ggml_hash_find
#undef ggml_hash_contains
#undef ggml_hash_insert
#undef ggml_hash_find_or_insert
#undef ggml_check_edges
#include "service-api.h"
static inline int32_t ggml_node_get_use_count(const struct ggml_cgraph *cgraph,int node_idx){if(fg160_fusion_enabled){return (int32_t)fg160_packet(0,FG160_A(cgraph),FG160_A(node_idx),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0));}return fg160_native_ggml_node_get_use_count(cgraph,node_idx);}
static inline bool ggml_node_has_n_uses(const struct ggml_cgraph *cgraph,int node_idx,int32_t n_uses){if(fg160_fusion_enabled){return (bool)fg160_packet(1,FG160_A(cgraph),FG160_A(node_idx),FG160_A(n_uses),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0));}return fg160_native_ggml_node_has_n_uses(cgraph,node_idx,n_uses);}
static inline bool ggml_can_fuse_ext(const struct ggml_cgraph *cgraph,const int *node_idxs,const enum ggml_op *ops,int num_ops){if(fg160_fusion_enabled){return (bool)fg160_packet(2,FG160_A(cgraph),FG160_A(node_idxs),FG160_A(ops),FG160_A(num_ops),FG160_A(0),FG160_A(0),FG160_A(0));}return fg160_native_ggml_can_fuse_ext(cgraph,node_idxs,ops,num_ops);}
static inline bool ggml_can_fuse(const struct ggml_cgraph *cgraph,int node_idx,const enum ggml_op *ops,int num_ops){if(fg160_fusion_enabled){return (bool)fg160_packet(3,FG160_A(cgraph),FG160_A(node_idx),FG160_A(ops),FG160_A(num_ops),FG160_A(0),FG160_A(0),FG160_A(0));}return fg160_native_ggml_can_fuse(cgraph,node_idx,ops,num_ops);}
static inline bool ggml_can_fuse_subgraph(const struct ggml_cgraph *cgraph,int node_idx,int count,const enum ggml_op *ops,const int *outputs,int num_outputs){if(fg160_fusion_enabled){return (bool)fg160_packet(5,FG160_A(cgraph),FG160_A(node_idx),FG160_A(count),FG160_A(ops),FG160_A(outputs),FG160_A(num_outputs),FG160_A(0));}return fg160_native_ggml_can_fuse_subgraph(cgraph,node_idx,count,ops,outputs,num_outputs);}
static inline void ggml_set_op_params(struct ggml_tensor *tensor,const void *params,size_t params_size){if(fg160_fusion_enabled){fg160_packet(9,FG160_A(tensor),FG160_A(params),FG160_A(params_size),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0));return;}fg160_native_ggml_set_op_params(tensor,params,params_size);}
static inline int32_t ggml_get_op_params_i32(const struct ggml_tensor *tensor,uint32_t i){if(fg160_fusion_enabled){return (int32_t)fg160_packet(10,FG160_A(tensor),FG160_A(i),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0));}return fg160_native_ggml_get_op_params_i32(tensor,i);}
static inline float ggml_get_op_params_f32(const struct ggml_tensor *tensor,uint32_t i){if(fg160_fusion_enabled){return fg160_float_value(fg160_packet(11,FG160_A(tensor),FG160_A(i),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0)));}return fg160_native_ggml_get_op_params_f32(tensor,i);}
static inline void ggml_set_op_params_i32(struct ggml_tensor *tensor,uint32_t i,int32_t value){if(fg160_fusion_enabled){fg160_packet(12,FG160_A(tensor),FG160_A(i),FG160_A(value),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0));return;}fg160_native_ggml_set_op_params_i32(tensor,i,value);}
static inline void ggml_set_op_params_f32(struct ggml_tensor *tensor,uint32_t i,float value){if(fg160_fusion_enabled){fg160_packet(13,FG160_A(tensor),FG160_A(i),FG160_A(fg160_float_bits(value)),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0));return;}fg160_native_ggml_set_op_params_f32(tensor,i,value);}
static inline size_t ggml_bitset_size(size_t n){if(fg160_graph_enabled){return (size_t)fg160_packet(36,FG160_A(n),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0));}return fg160_native_ggml_bitset_size(n);}
static inline bool ggml_bitset_get(const ggml_bitset_t *bitset,size_t i){if(fg160_graph_enabled){return (bool)fg160_packet(37,FG160_A(bitset),FG160_A(i),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0));}return fg160_native_ggml_bitset_get(bitset,i);}
static inline void ggml_bitset_set(ggml_bitset_t *bitset,size_t i){if(fg160_graph_enabled){fg160_packet(38,FG160_A(bitset),FG160_A(i),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0));return;}fg160_native_ggml_bitset_set(bitset,i);}
static inline void ggml_bitset_clear(ggml_bitset_t *bitset,size_t i){if(fg160_graph_enabled){fg160_packet(39,FG160_A(bitset),FG160_A(i),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0));return;}fg160_native_ggml_bitset_clear(bitset,i);}
static inline size_t ggml_hash(const struct ggml_tensor *p){if(fg160_graph_enabled){return (size_t)fg160_packet(40,FG160_A(p),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0));}return fg160_native_ggml_hash(p);}
static inline size_t ggml_hash_find(const struct ggml_hash_set *hash_set,const struct ggml_tensor *key){if(fg160_graph_enabled){return (size_t)fg160_packet(41,FG160_A(hash_set),FG160_A(key),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0));}return fg160_native_ggml_hash_find(hash_set,key);}
static inline bool ggml_hash_contains(const struct ggml_hash_set *hash_set,struct ggml_tensor *key){if(fg160_graph_enabled){return (bool)fg160_packet(42,FG160_A(hash_set),FG160_A(key),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0));}return fg160_native_ggml_hash_contains(hash_set,key);}
static inline size_t ggml_hash_insert(struct ggml_hash_set *hash_set,struct ggml_tensor *key){if(fg160_graph_enabled){return (size_t)fg160_packet(43,FG160_A(hash_set),FG160_A(key),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0));}return fg160_native_ggml_hash_insert(hash_set,key);}
static inline size_t ggml_hash_find_or_insert(struct ggml_hash_set *hash_set,struct ggml_tensor *key){if(fg160_graph_enabled){return (size_t)fg160_packet(44,FG160_A(hash_set),FG160_A(key),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0),FG160_A(0));}return fg160_native_ggml_hash_find_or_insert(hash_set,key);}
#ifdef __cplusplus
inline bool ggml_can_fuse(const struct ggml_cgraph*g,int first,std::initializer_list<enum ggml_op>ops){return ggml_can_fuse(g,first,ops.begin(),(int)ops.size());}
inline bool ggml_can_fuse_subgraph(const struct ggml_cgraph*g,int first,std::initializer_list<enum ggml_op>ops,std::initializer_list<int>outputs={}){return ggml_can_fuse_subgraph(g,first,(int)ops.size(),ops.begin(),outputs.begin(),(int)outputs.size());}
inline bool ggml_check_edges(const struct ggml_cgraph*g,int first,std::initializer_list<std::array<int,3>>edges){if(fg160_fusion_enabled){static_assert(sizeof(std::array<int,3>)==3*sizeof(int),"Edge tuple ABI");return (bool)fg160_packet(6,FG160_A(g),FG160_A(first),FG160_A(edges.begin()),FG160_A(edges.size()),0,0,0);}return fg160_native_ggml_check_edges(g,first,edges);}

#endif
