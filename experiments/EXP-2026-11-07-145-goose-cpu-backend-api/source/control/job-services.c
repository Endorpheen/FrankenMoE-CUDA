#ifdef GGML_USE_OPENMP
struct fg143_job_context {struct ggml_cgraph*graph;struct ggml_cplan*plan;struct ggml_threadpool*pool;};
int64_t fg143_job_get(int64_t context,int64_t field){struct fg143_job_context*x=(struct fg143_job_context*)(uintptr_t)context;switch(field){case 0:return x->plan!=NULL;case 1:return x->plan->n_threads;case 2:GGML_ASSERT(x->plan->work_size<=INT64_MAX);return(int64_t)x->plan->work_size;case 3:return x->plan->work_data!=NULL;case 4:return x->pool->n_threads;case 6:return x->pool!=NULL;case 7:return x->pool->ec;case 10:return omp_get_thread_num();case 11:return omp_get_num_threads();case 12:return GGML_STATUS_SUCCESS;default:GGML_ABORT("invalid Goose job field");}}
void fg143_job_set(int64_t context,int64_t field,int64_t value){struct fg143_job_context*x=(struct fg143_job_context*)(uintptr_t)context;switch(field){case 0:x->pool->cgraph=x->graph;break;case 1:x->pool->cplan=x->plan;break;case 2:atomic_store_explicit(&x->pool->current_chunk,(int)value,memory_order_seq_cst);break;case 3:atomic_store_explicit(&x->pool->abort,(int)value,memory_order_seq_cst);break;case 4:x->pool->ec=(enum ggml_status)value;break;case 5:GGML_ASSERT(value>0&&value<=INT_MAX);atomic_store_explicit(&x->pool->n_graph,(int)value,memory_order_relaxed);break;default:GGML_ABORT("invalid Goose job write");}}
void fg143_job_create(int64_t context,int64_t threads){struct fg143_job_context*x=(struct fg143_job_context*)(uintptr_t)context;GGML_ASSERT(threads>0&&threads<=INT_MAX);struct ggml_threadpool_params params=ggml_threadpool_params_default((int)threads);x->pool=ggml_threadpool_new_impl(&params,x->graph,x->plan);GGML_ASSERT(x->pool);}
void fg143_job_free(int64_t context){struct fg143_job_context*x=(struct fg143_job_context*)(uintptr_t)context;ggml_threadpool_free(x->pool);x->pool=NULL;}
void fg143_job_team(int64_t context,int64_t threads){
    #pragma omp parallel num_threads(threads)
    {fg143_cpu_control(4,context,0,0,NULL);}
}
void fg143_job_barrier(void){
    #pragma omp barrier
}
void fg143_job_place(int64_t context,int64_t thread){struct fg143_job_context*x=(struct fg143_job_context*)(uintptr_t)context;GGML_ASSERT(thread>=0&&thread<x->pool->n_threads);ggml_thread_apply_priority(x->pool->prio);if(ggml_thread_cpumask_is_valid(x->pool->workers[thread].cpumask))ggml_thread_apply_affinity(x->pool->workers[thread].cpumask);}
void fg143_job_compute(int64_t context,int64_t thread){struct fg143_job_context*x=(struct fg143_job_context*)(uintptr_t)context;GGML_ASSERT(thread>=0&&thread<x->pool->n_threads);ggml_graph_compute_thread(x->pool->workers+thread);}
void fg143_job_clear_affinity(void){clear_numa_thread_affinity();}
bool fg143_control_supports_jobs(void){return true;}
enum ggml_status fg143_control_graph_compute(struct ggml_cgraph*g,struct ggml_cplan*p){struct fg143_job_context context={g,p,p?p->threadpool:NULL};int64_t out[2]={0};fg143_cpu_control(3,(int64_t)(uintptr_t)&context,0,0,out);return(enum ggml_status)out[0];}
#else
int64_t fg143_job_get(int64_t context,int64_t field){UNUSED(context);UNUSED(field);GGML_ABORT("Goose OpenMP job module unavailable");}
void fg143_job_set(int64_t c,int64_t f,int64_t v){UNUSED(c);UNUSED(f);UNUSED(v);GGML_ABORT("Goose OpenMP job module unavailable");}
void fg143_job_create(int64_t c,int64_t t){UNUSED(c);UNUSED(t);GGML_ABORT("Goose OpenMP job module unavailable");}
void fg143_job_free(int64_t c){UNUSED(c);GGML_ABORT("Goose OpenMP job module unavailable");}
void fg143_job_team(int64_t c,int64_t t){UNUSED(c);UNUSED(t);GGML_ABORT("Goose OpenMP job module unavailable");}
void fg143_job_barrier(void){GGML_ABORT("Goose OpenMP job module unavailable");}
void fg143_job_place(int64_t c,int64_t t){UNUSED(c);UNUSED(t);GGML_ABORT("Goose OpenMP job module unavailable");}
void fg143_job_compute(int64_t c,int64_t t){UNUSED(c);UNUSED(t);GGML_ABORT("Goose OpenMP job module unavailable");}
void fg143_job_clear_affinity(void){GGML_ABORT("Goose OpenMP job module unavailable");}
bool fg143_control_supports_jobs(void){return false;}
enum ggml_status fg143_control_graph_compute(struct ggml_cgraph*g,struct ggml_cplan*p){UNUSED(g);UNUSED(p);GGML_ABORT("Goose OpenMP job module unavailable");}
#endif
