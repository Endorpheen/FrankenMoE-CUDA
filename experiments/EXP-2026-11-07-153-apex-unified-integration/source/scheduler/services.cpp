#include "bridge.h"
#include <atomic>
struct fg153_sched_deps { std::vector<std::pair<ggml_tensor *,std::vector<ggml_tensor *>>> rows; };
static void fg153_add_dep(void * context,ggml_tensor * tensor,ggml_tensor * until) {
 auto &rows=((fg153_sched_deps*)context)->rows;
 auto it=std::find_if(rows.begin(),rows.end(),[until](const auto&r){return r.first==until;});
 if(it==rows.end()){rows.push_back({until,{tensor}});return;}
 if(std::find(it->second.begin(),it->second.end(),tensor)==it->second.end())it->second.push_back(tensor);
}
int fg153_scheduler_enabled(void){const char*v=getenv("GGML_GOOSE_SCHEDULER");return !v||strcmp(v,"0")!=0;}
static std::atomic<uint64_t> fg153_scheduler_counts[14];
void fg153_sw(int64_t mode){GGML_ASSERT(mode>=0&&mode<14);fg153_scheduler_counts[mode].fetch_add(1,std::memory_order_relaxed);}
extern "C" __attribute__((visibility("default"))) uint64_t fg153_scheduler_count(int mode){GGML_ASSERT(mode>=0&&mode<14);return fg153_scheduler_counts[mode].load(std::memory_order_relaxed);}
int64_t fg153_sg(int64_t p,int64_t k,int64_t i){auto*s=(ggml_backend_sched*)(intptr_t)p;auto*t=(ggml_tensor*)(intptr_t)p;auto*g=(ggml_cgraph*)(intptr_t)p;auto*sp=(ggml_backend_sched_split*)(intptr_t)p;auto*buf=(ggml_backend_buffer*)(intptr_t)p;switch(k){
case 0:return(int64_t)(intptr_t)(s->is_reset);
case 1:return(int64_t)(intptr_t)(s->is_alloc);
case 2:return(int64_t)(intptr_t)(s->n_backends);
case 3:return(int64_t)(intptr_t)(s->backends[i]);
case 4:return(int64_t)(intptr_t)(s->bufts[i]);
case 5:return(int64_t)(intptr_t)(s->galloc);
case 6:return(int64_t)(intptr_t)(s->hash_set.size);
case 7:return(int64_t)(intptr_t)(s->hv_tensor_backend_ids[i]);
case 8:return(int64_t)(intptr_t)(s->hv_tensor_copies[i]);
case 9:if(i==-1)return(intptr_t)s->node_backend_ids;return(int64_t)(intptr_t)s->node_backend_ids[i];
case 10:if(i==-1)return(intptr_t)s->leaf_backend_ids;return(int64_t)(intptr_t)s->leaf_backend_ids[i];
case 11:if(i==-1)return(intptr_t)s->prev_node_backend_ids;return(int64_t)(intptr_t)s->prev_node_backend_ids[i];
case 12:if(i==-1)return(intptr_t)s->prev_leaf_backend_ids;return(int64_t)(intptr_t)s->prev_leaf_backend_ids[i];
case 14:return(int64_t)(intptr_t)(&s->graph);
case 15:if(i==-1)return(intptr_t)s->splits;return(intptr_t)&s->splits[i];
case 16:return(int64_t)(intptr_t)(s->n_splits);
case 17:return(int64_t)(intptr_t)(s->splits_capacity);
case 18:return(int64_t)(intptr_t)(s->n_copies);
case 19:return(int64_t)(intptr_t)(s->cur_copy);
case 20:return(int64_t)(intptr_t)(s->next_copy);
case 21:return(int64_t)(intptr_t)(s->events[i/GGML_SCHED_MAX_COPIES][i%GGML_SCHED_MAX_COPIES]);
case 22:if(i==-1)return(intptr_t)s->graph_inputs;return(intptr_t)s->graph_inputs[i];
case 23:return(int64_t)(intptr_t)(s->n_graph_inputs);
case 24:return(int64_t)(intptr_t)(s->graph_inputs_capacity);
case 25:return(int64_t)(intptr_t)(s->ctx);
case 26:return(int64_t)(intptr_t)(s->callback_eval);
case 28:return(int64_t)(intptr_t)(s->expert_pipeline_enabled);
case 29:return(int64_t)(intptr_t)(s->callback_copy);
case 31:return(int64_t)(intptr_t)(s->op_offload);
case 32:return(int64_t)(intptr_t)(s->debug);
case 33:return(int64_t)(intptr_t)(s->graph_size);
case 34:return(int64_t)(intptr_t)(s->prev_graph_size);
case 35:return(int64_t)(intptr_t)(s->debug_realloc);
case 36:return(int64_t)(intptr_t)(s->expert_pipeline[i]);
case 37:return(int64_t)(intptr_t)(s->goose_expert_size);
case 38:return(intptr_t)s->context_buffer;case 39:return s->context_buffer_size;
case 101:return(int64_t)(intptr_t)(t->view_src);
case 102:return(int64_t)(intptr_t)(t->buffer);
case 103:return(int64_t)(intptr_t)(t->op);
case 104:return(int64_t)(intptr_t)(t->flags);
case 105:return(int64_t)(intptr_t)(t->src[i]);
case 106:return(int64_t)(intptr_t)(t->ne[i]);
case 110:return(int64_t)(intptr_t)(buf->usage);
case 111:return(int64_t)(intptr_t)(buf->buft);
case 200:return(int64_t)(intptr_t)(g->n_nodes);
case 201:return(int64_t)(intptr_t)(g->n_leafs);
case 202:if(i==-1)return(intptr_t)g->nodes;return(intptr_t)g->nodes[i];
case 203:if(i==-1)return(intptr_t)g->leafs;return(intptr_t)g->leafs[i];
case 204:return(int64_t)(intptr_t)(g->size);
case 205:return(int64_t)(intptr_t)(g->uid);
case 300:return(int64_t)(intptr_t)(sp->backend_id);
case 301:return(int64_t)(intptr_t)(sp->i_start);
case 302:return(int64_t)(intptr_t)(sp->i_end);
case 303:if(i==-1)return(intptr_t)sp->inputs;return(int64_t)(intptr_t)sp->inputs[i];
case 304:return(int64_t)(intptr_t)(sp->n_inputs);
case 305:return(int64_t)(intptr_t)(sp->inputs_capacity);
case 306:return(int64_t)(intptr_t)(&sp->graph);
case 400:return(int64_t)(intptr_t)(((int64_t*)(intptr_t)p)[i]);
case 402:return GGML_MAX_SRC;
case 403:return GGML_SCHED_MAX_COPIES;
case 404:return GGML_BACKEND_BUFFER_USAGE_WEIGHTS;
case 405:return GGML_TENSOR_FLAG_INPUT;
case 406:return GGML_OP_ROPE;
case 407:return GGML_OP_FLASH_ATTN_EXT;
case 408:return GGML_TENSOR_FLAG_OUTPUT;
case 409:return GGML_OP_MUL_MAT_ID;
case 410:return GGML_EXPERT_QUERY;
case 411:return GGML_EXPERT_RESERVE;
case 412:return GGML_EXPERT_BIND;
case 413:return GGML_EXPERT_BEGIN;
case 414:return GGML_EXPERT_END;
case 415:return GGML_EXPERT_CANCEL;
case 416:return GGML_STATUS_SUCCESS;
case 417:return GGML_STATUS_ALLOC_FAILED;
case 421:return sizeof(ggml_backend_sched);case 422:return sizeof(ggml_tensor);case 423:return sizeof(ggml_backend_sched_split);case 424:return GGML_SCHED_MAX_BACKENDS;case 425:return GGML_SCHED_MAX_SPLIT_INPUTS;
case 420:{void* ptrs[]={s->splits,s->graph_inputs,s->hv_tensor_backend_ids,s->hv_tensor_copies,s->node_backend_ids,s->leaf_backend_ids,s->prev_node_backend_ids,s->prev_leaf_backend_ids,s->context_buffer,s->graph.nodes,s->graph.leafs};GGML_ASSERT(i>=0&&i<11);return(intptr_t)ptrs[i];}
default:GGML_ABORT("bad Goose scheduler field");}}
void fg153_sp(int64_t p,int64_t k,int64_t i,int64_t v){auto*s=(ggml_backend_sched*)(intptr_t)p;auto*t=(ggml_tensor*)(intptr_t)p;auto*g=(ggml_cgraph*)(intptr_t)p;auto*sp=(ggml_backend_sched_split*)(intptr_t)p;switch(k){
case 0:s->is_reset=v;return;
case 1:s->is_alloc=v;return;
case 2:s->n_backends=v;return;
case 3:s->backends[i]=(ggml_backend_t)(intptr_t)v;return;
case 4:s->bufts[i]=(ggml_backend_buffer_type_t)(intptr_t)v;return;
case 5:s->galloc=(ggml_gallocr_t)(intptr_t)v;return;
case 7:if(i==-1){s->hv_tensor_backend_ids=(int*)(intptr_t)v;return;}s->hv_tensor_backend_ids[i]=v;return;
case 8:if(i==-1){s->hv_tensor_copies=(ggml_tensor**)(intptr_t)v;return;}s->hv_tensor_copies[i]=(ggml_tensor *)(intptr_t)v;return;
case 9:if(i==-1){s->node_backend_ids=(int*)(intptr_t)v;return;}s->node_backend_ids[i]=(int)v;return;
case 10:if(i==-1){s->leaf_backend_ids=(int*)(intptr_t)v;return;}s->leaf_backend_ids[i]=(int)v;return;
case 11:if(i==-1){s->prev_node_backend_ids=(int*)(intptr_t)v;return;}s->prev_node_backend_ids[i]=(int)v;return;
case 12:if(i==-1){s->prev_leaf_backend_ids=(int*)(intptr_t)v;return;}s->prev_leaf_backend_ids[i]=(int)v;return;
case 16:s->n_splits=v;return;
case 17:s->splits_capacity=v;return;
case 18:s->n_copies=v;return;
case 19:s->cur_copy=v;return;
case 20:s->next_copy=v;return;
case 21:s->events[i/GGML_SCHED_MAX_COPIES][i%GGML_SCHED_MAX_COPIES]=(ggml_backend_event_t)(intptr_t)v;return;
case 22:if(i==-1){s->graph_inputs=(ggml_tensor**)(intptr_t)v;return;}s->graph_inputs[i]=(ggml_tensor *)(intptr_t)v;return;
case 23:s->n_graph_inputs=v;return;
case 24:s->graph_inputs_capacity=v;return;
case 25:s->ctx=(ggml_context *)(intptr_t)v;return;
case 28:s->expert_pipeline_enabled=v;return;
case 31:s->op_offload=v;return;
case 32:s->debug=v;return;
case 33:s->graph_size=v;return;
case 34:s->prev_graph_size=v;return;
case 35:s->debug_realloc=v;return;
case 36:s->expert_pipeline[i]=(ggml_expert_pipeline_fn)(intptr_t)v;return;
case 37:s->goose_expert_size=v;return;
case 15:GGML_ASSERT(i==-1);s->splits=(ggml_backend_sched_split*)(intptr_t)v;return;case 38:s->context_buffer=(char*)(intptr_t)v;return;case 39:s->context_buffer_size=v;return;
case 101:t->view_src=(ggml_tensor *)(intptr_t)v;return;
case 102:t->buffer=(ggml_backend_buffer_t)(intptr_t)v;return;
case 103:t->op=(ggml_op)v;return;
case 104:t->flags=v;return;
case 105:t->src[i]=(ggml_tensor *)(intptr_t)v;return;
case 106:t->ne[i]=v;return;
case 200:g->n_nodes=v;return;
case 201:g->n_leafs=v;return;
case 202:if(i==-1){g->nodes=(ggml_tensor**)(intptr_t)v;return;}g->nodes[i]=(ggml_tensor *)(intptr_t)v;return;
case 203:if(i==-1){g->leafs=(ggml_tensor**)(intptr_t)v;return;}g->leafs[i]=(ggml_tensor *)(intptr_t)v;return;
case 204:g->size=v;return;
case 205:g->uid=v;return;
case 300:sp->backend_id=v;return;
case 301:sp->i_start=v;return;
case 302:sp->i_end=v;return;
case 303:if(i==-1){sp->inputs=(ggml_tensor**)(intptr_t)v;return;}sp->inputs[i]=(ggml_tensor *)(intptr_t)v;return;
case 304:sp->n_inputs=v;return;
case 305:sp->inputs_capacity=v;return;
case 400:((int64_t*)(intptr_t)p)[i]=v;return;
default:GGML_ABORT("bad Goose scheduler write");}}
__attribute__((noinline)) static void *fg153_scheduler_realloc(void*p,size_t n){return realloc(p,n);}
int64_t fg153_ss(int64_t op,int64_t a,int64_t b,int64_t c,int64_t d,int64_t e){
 auto*s=(ggml_backend_sched*)(intptr_t)a;auto*sp=(ggml_backend_sched_split*)(intptr_t)a;
 auto*deps=(fg153_sched_deps*)(intptr_t)a;
 auto backend=(ggml_backend_t)(intptr_t)a;auto*t=(ggml_tensor*)(intptr_t)b;
 switch(op){
 case 0:GGML_ABORT("Goose scheduler invariant failed");
 case 1:return(intptr_t)calloc(1,(size_t)a);
 case 2:free((void*)(intptr_t)a);return 0;
 case 3:return ggml_hash_find_or_insert(&s->hash_set,t);
 case 4:return ggml_is_view_op((ggml_op)a);
 case 5:return ggml_backend_buffer_is_host((ggml_backend_buffer_t)(intptr_t)a);
 case 6:return ggml_backend_supports_buft(backend,(ggml_backend_buffer_type_t)(intptr_t)b);
 case 7:return ggml_backend_supports_op(backend,t);
 case 8:return ggml_backend_offload_op(backend,t);
 case 9:{auto*cp=ggml_dup_tensor_layout((ggml_context*)(intptr_t)a,t);ggml_format_name(cp,"%s#%s#%d",ggml_backend_name((ggml_backend_t)(intptr_t)c),t->name,(int)d);return(intptr_t)cp;}
 case 10:{ggml_free(s->ctx);ggml_init_params params={s->context_buffer_size,s->context_buffer,true};s->ctx=ggml_init(params);GGML_ASSERT(s->ctx);return 0;}
 case 11:return ggml_graph_next_uid();
 case 14:ggml_backend_sched_print_assignments(s,(ggml_cgraph*)(intptr_t)b);return 0;
 case 16:return(intptr_t)new fg153_sched_deps();
 case 17:sp->graph=ggml_graph_view((ggml_cgraph*)(intptr_t)b,(int)c,(int)d);return 0;
 case 18:{ggml_backend_graph_optimize_params params={fg153_add_dep,(void*)(intptr_t)c};ggml_backend_graph_optimize(backend,(ggml_cgraph*)(intptr_t)b,&params);return 0;}
 case 19:return deps->rows.size();
 case 20:return deps->rows.at((size_t)b).second.size();
 case 22:return(intptr_t)ggml_view_tensor((ggml_context*)(intptr_t)a,t);
 case 23:{auto it=std::find_if(deps->rows.begin(),deps->rows.end(),[t](const auto&r){return r.first==t;});return it==deps->rows.end()?-1:it-deps->rows.begin();}
 case 24:return(intptr_t)deps->rows.at((size_t)b).second.at((size_t)c);
 case 25:delete deps;return 0;
 case 26:return(intptr_t)ggml_backend_reg_get_proc_address(ggml_backend_dev_backend_reg(ggml_backend_get_device(backend)),"ggml_backend_expert_pipeline_v1");
 case 27:return s->expert_pipeline[b](s->backends[b],(decltype(GGML_EXPERT_QUERY))c,(ggml_tensor*)(intptr_t)d,(ggml_tensor*)(intptr_t)e,c>=GGML_EXPERT_BEGIN?nullptr:&s->goose_expert_size);
 case 28:return ggml_gallocr_alloc_graph(s->galloc,&s->graph);
 case 29:if(s->callback_realloc)s->callback_realloc(s,&s->graph,b!=0,c!=0,s->callback_realloc_user_data);return 0;
 case 30:ggml_backend_synchronize(backend);return 0;
 case 31:return ggml_gallocr_reserve_n(s->galloc,&s->graph,s->node_backend_ids,s->leaf_backend_ids);
 case 32:ggml_backend_event_synchronize((ggml_backend_event_t)(intptr_t)a);return 0;
 case 33:ggml_backend_tensor_copy((ggml_tensor*)(intptr_t)a,t);return 0;
 case 34:return s->callback_copy(s->backends[((ggml_backend_sched_split*)(intptr_t)b)->backend_id],(ggml_tensor*)(intptr_t)c,(ggml_tensor*)(intptr_t)d,&((ggml_backend_sched_split*)(intptr_t)b)->graph,s->callback_copy_user_data);
 case 35:ggml_backend_event_wait(backend,(ggml_backend_event_t)(intptr_t)b);return 0;
 case 36:{auto dst=(ggml_backend_t)(intptr_t)b;return dst->iface.cpy_tensor_async&&dst->iface.cpy_tensor_async(backend,dst,(ggml_tensor*)(intptr_t)c,(ggml_tensor*)(intptr_t)d);}
 case 37:{auto*g=(ggml_cgraph*)(intptr_t)b;if(c==0&&d==g->n_nodes)return ggml_backend_graph_compute_async(backend,g);auto gv=ggml_graph_view(g,(int)c,(int)d);return ggml_backend_graph_compute_async(backend,&gv);}
 case 38:return s->callback_eval(t,c!=0,s->callback_eval_user_data);
 case 39:ggml_backend_event_record((ggml_backend_event_t)(intptr_t)a,(ggml_backend_t)(intptr_t)b);return 0;
 case 40:ggml_hash_set_reset(&s->hash_set);return 0;
 case 41:ggml_backend_event_free((ggml_backend_event_t)(intptr_t)a);return 0;
 case 42:ggml_gallocr_free((ggml_gallocr_t)(intptr_t)a);return 0;
 case 43:ggml_free((ggml_context*)(intptr_t)a);return 0;
 case 44:ggml_hash_set_free(&s->hash_set);return 0;
 case 45:return(intptr_t)ggml_backend_get_default_buffer_type(backend);
 case 46:return(intptr_t)ggml_backend_event_new(backend->device);
 case 47:return(intptr_t)ggml_gallocr_new_n(s->bufts,s->n_backends);
 case 48:ggml_gallocr_reserve_n_size(s->galloc,&s->graph,s->node_backend_ids,s->leaf_backend_ids,(size_t*)(intptr_t)b);return 0;
 case 49:s->hash_set=ggml_hash_set_new((size_t)b);return 0;
 case 50:return ggml_graph_overhead_custom((size_t)a,false);
 case 51:{const char*v=getenv(a?"GGML_SCHED_DEBUG_REALLOC":"GGML_SCHED_DEBUG");int def=0;
#ifdef GGML_SCHED_NO_REALLOC
 if(a)def=1;
#endif
 return v?atoi(v):def;}
 case 52:return ggml_backend_dev_type(backend->device)==GGML_BACKEND_DEVICE_TYPE_CPU;
 case 53:return(intptr_t)fg153_scheduler_realloc((void*)(intptr_t)a,(size_t)b);
 case 54:memset((void*)(intptr_t)a,0,(size_t)b);return 0;
 default:GGML_ABORT("bad Goose scheduler primitive");}}

__attribute__((destructor)) static void fg153_scheduler_report(void){if(!getenv("GGML_GOOSE_SCHEDULER_TRACE"))return;for(int i=0;i<14;i++){auto n=fg153_scheduler_counts[i].load(std::memory_order_relaxed);if(n)fprintf(stderr,"FG153_SCHEDULER mode=%d calls=%llu\n",i,(unsigned long long)n);}}

static_assert(sizeof(void*)==8,"Goose scheduler requires 64-bit pointer ABI");
static_assert(sizeof(size_t)==8,"Goose scheduler requires 64-bit size ABI");
static_assert(sizeof(int)==4,"Goose scheduler backend IDs require 32-bit int ABI");
extern "C" __attribute__((visibility("default"))) int fg153_scheduler_validate_layout(void){
 ggml_backend_sched s={};ggml_backend_sched_split sp={};ggml_cgraph g={};ggml_tensor t={};
 int ids[3]={};ggml_tensor*ptrs[3]={};ggml_backend_sched_split splits[2]={};
 fg153_sp((intptr_t)&s,9,-1,(intptr_t)ids);GGML_ASSERT(fg153_sg((intptr_t)&s,9,-1)==(intptr_t)ids);
 fg153_sp((intptr_t)&s,9,1,7);GGML_ASSERT(ids[1]==7&&fg153_sg((intptr_t)&s,9,1)==7);
 fg153_sp((intptr_t)&s,15,-1,(intptr_t)splits);GGML_ASSERT(fg153_sg((intptr_t)&s,15,1)==(intptr_t)&splits[1]);
 fg153_sp((intptr_t)&s,22,-1,(intptr_t)ptrs);GGML_ASSERT(fg153_sg((intptr_t)&s,22,-1)==(intptr_t)ptrs);
 fg153_sp((intptr_t)&sp,303,-1,(intptr_t)ptrs);GGML_ASSERT(fg153_sg((intptr_t)&sp,303,-1)==(intptr_t)ptrs);
 fg153_sp((intptr_t)&g,202,-1,(intptr_t)ptrs);fg153_sp((intptr_t)&g,203,-1,(intptr_t)ptrs);
 GGML_ASSERT(fg153_sg((intptr_t)&g,202,-1)==(intptr_t)ptrs&&fg153_sg((intptr_t)&g,203,-1)==(intptr_t)ptrs);
 fg153_sp((intptr_t)&g,202,1,(intptr_t)&t);GGML_ASSERT(fg153_sg((intptr_t)&g,202,1)==(intptr_t)&t);
 fg153_sp((intptr_t)&t,105,2,(intptr_t)&t);GGML_ASSERT(t.src[2]==&t&&fg153_sg((intptr_t)&t,105,2)==(intptr_t)&t);
 printf("{\"status\":\"PASS_SCHEDULER_TYPED_LAYOUT\",\"scheduler_size\":%zu,\"split_size\":%zu,\"tensor_size\":%zu,\"graph_size\":%zu,\"scheduler_graph_offset\":%zu,\"tensor_src_offset\":%zu}\n",sizeof(s),sizeof(sp),sizeof(t),sizeof(g),offsetof(ggml_backend_sched,graph),offsetof(ggml_tensor,src));return 1;
}
