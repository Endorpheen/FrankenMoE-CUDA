struct fg143_exec_context {struct ggml_compute_state*state;struct ggml_threadpool*pool;const struct ggml_cgraph*graph;const struct ggml_cplan*plan;struct ggml_compute_params params;};
extern uint8_t fg143_backend_is_enabled(void);
static bool fg143_executor_enabled=true;
__attribute__((constructor))static void fg143_executor_config(void){const char*v=getenv("GGML_GOOSE_CPU_EXECUTOR");fg143_executor_enabled=!v||strcmp(v,"0");}
extern int64_t fg143_execute_graph(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
extern uint8_t fg143_apply_fused(const struct ggml_compute_params*,const struct ggml_tensor*,struct ggml_tensor*,int);
int64_t fg143_ex_meta(int64_t c,int64_t index,int64_t field){
 struct fg143_exec_context*x=(struct fg143_exec_context*)(uintptr_t)c;GGML_ASSERT(index>=0&&index<x->graph->n_nodes);const struct ggml_tensor*t=x->graph->nodes[index];
 switch(field){case 0:return t->op;case 1:return t->flags;case 2:return(int64_t)(uintptr_t)t;case 3:return(int64_t)(uintptr_t)t->src[0];case 4:return(int64_t)(uintptr_t)t->src[1];case 5:return(int64_t)(uintptr_t)t->view_src;case 6:return ggml_node_get_use_count(x->graph,index);case 7:return t->type;case 8:return t->src[0]?t->src[0]->type:-1;case 9:return t->src[1]?t->src[1]->type:-1;case 10:case 11:case 12:case 13:return t->ne[field-10];case 14:return t->src[0]?t->src[0]->ne[0]:0;case 15:return t->src[1]?t->src[1]->ne[0]:0;case 16:return t->src[0]?t->src[0]->nb[0]:0;case 17:return t->src[1]?t->src[1]->nb[0]:0;default:GGML_ABORT("invalid Goose executor metadata field");}
}
int64_t fg143_ex_abort_value(int64_t c){struct fg143_exec_context*x=(struct fg143_exec_context*)(uintptr_t)c;return atomic_load_explicit(&x->pool->abort,memory_order_relaxed);}
void fg143_ex_compute(int64_t c,int64_t index){struct fg143_exec_context*x=(struct fg143_exec_context*)(uintptr_t)c;GGML_ASSERT(index>=0&&index<x->graph->n_nodes);ggml_compute_forward(&x->params,x->graph->nodes[index]);}
void fg143_ex_fused(int64_t c,int64_t index,int64_t slot){struct fg143_exec_context*x=(struct fg143_exec_context*)(uintptr_t)c;GGML_ASSERT(index>=0&&index+1<x->graph->n_nodes&&slot>=0&&slot<2);GGML_ASSERT(fg143_apply_fused(&x->params,x->graph->nodes[index],x->graph->nodes[index+1],slot));}
uint8_t fg143_ex_abort_requested(int64_t c){struct fg143_exec_context*x=(struct fg143_exec_context*)(uintptr_t)c;return x->plan->abort_callback?x->plan->abort_callback(x->plan->abort_callback_data):0;}
void fg143_ex_abort_at(int64_t c,int64_t index){struct fg143_exec_context*x=(struct fg143_exec_context*)(uintptr_t)c;GGML_ASSERT(index>=0&&index<=INT_MAX);atomic_store_explicit(&x->pool->abort,(int)index,memory_order_relaxed);x->pool->ec=GGML_STATUS_ABORTED;}
void fg143_ex_barrier(int64_t c){struct fg143_exec_context*x=(struct fg143_exec_context*)(uintptr_t)c;ggml_barrier(x->pool);}
static thread_ret_t fg143_run_executor(void*data){
 struct ggml_compute_state*state=(struct ggml_compute_state*)data;struct ggml_threadpool*tp=state->threadpool;
#ifdef GGML_USE_CPU_RISCV64_SPACEMIT
 ggml_backend_cpu_riscv64_spacemit_set_numa_thread_affinity(state->ith);
#else
 set_numa_thread_affinity(state->ith);
#endif
 struct fg143_exec_context x={0};x.state=state;x.pool=tp;x.graph=tp->cgraph;x.plan=tp->cplan;x.params.ith=state->ith;x.params.nth=atomic_load_explicit(&tp->n_graph,memory_order_relaxed)&GGML_THREADPOOL_N_THREADS_MASK;x.params.wsize=x.plan->work_size;x.params.wdata=x.plan->work_data;x.params.threadpool=tp;x.params.use_ref=x.plan->use_ref;
 fg143_execute_graph((int64_t)(uintptr_t)&x,x.graph->n_nodes,state->ith,x.params.nth,x.plan->use_ref,!ggml_cpu_disable_fusion&&fg143_backend_is_enabled(),GGML_TENSOR_FLAG_COMPUTE,GGML_TENSOR_FLAG_OUTPUT);
#ifdef GGML_USE_CPU_RISCV64_SPACEMIT
 ggml_backend_cpu_riscv64_spacemit_clear_numa_thread_affinity_threaded(state->ith);
#endif
 return 0;
}
