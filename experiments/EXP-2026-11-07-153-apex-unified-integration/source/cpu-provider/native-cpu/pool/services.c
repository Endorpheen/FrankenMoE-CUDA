_Static_assert(sizeof(bool)==1,"mask ABI requires byte booleans");
void * fg149_system_alloc(int64_t kind,int64_t bytes) {UNUSED(kind);GGML_ASSERT(bytes>0);return ggml_aligned_malloc((size_t)bytes);}
void fg149_system_free(int64_t kind,void * pointer,int64_t bytes) {UNUSED(kind);GGML_ASSERT(bytes>=0);ggml_aligned_free(pointer,(size_t)bytes);}
#ifndef GGML_USE_OPENMP
int64_t fg149_system_start(struct ggml_compute_state * worker) {return ggml_thread_create(&worker->thrd,NULL,ggml_graph_compute_secondary_thread,worker);}
#endif
int64_t fg149_meta(int64_t context,int64_t field) {
    struct fg149_request * q=(struct fg149_request*)(uintptr_t)context;
    struct ggml_threadpool * p=q->pool;
    if(field>=100&&field<108)return g_state.numa.nodes[field-100].n_cpus;
    if(field>=500000&&field<500000+8*512){int64_t at=field-500000;return g_state.numa.nodes[at/512].cpus[at%512];}
    switch(field){
    case 0:return(int64_t)(uintptr_t)p;
    case 1:return q->params?q->params->n_threads:0;
    case 2:return q->params->paused;
    case 3:return q->params->poll;
    case 4:return q->params->prio;
    case 5:return q->params->strict_cpu;
    case 6:return sizeof(struct ggml_threadpool);
    case 7:return sizeof(struct ggml_compute_state);
    case 8:return GGML_MAX_N_THREADS;
    case 9:
#ifdef GGML_USE_OPENMP
        return 1;
#else
        return 0;
#endif
    case 10:return p->n_threads;
    case 11:return atomic_load_explicit(&p->pause,memory_order_seq_cst);
    case 12:return atomic_load_explicit(&p->stop,memory_order_seq_cst);
    case 13:return p->poll;
    case 14:return p->prio;
#ifndef GGML_USE_OPENMP
    case 15:return q->state->pending;
    case 16:return q->state->ith;
    case 17:return q->state->last_graph;
#endif
    case 18:return atomic_load_explicit(&p->n_graph,memory_order_relaxed);
    case 19:return atomic_load_explicit(&p->n_barrier_passed,memory_order_relaxed);
    case 20:return q->value;
    case 21:return q->cursor;
    case 22:return GGML_EXIT_SUCCESS;
    case 23:return GGML_EXIT_ABORTED;
    case 70:return g_state.numa.n_nodes;
    case 71:return g_state.numa.numa_strategy;
    case 72:return g_state.numa.current_node;
    case 73:return g_state.numa.total_cpus;
#if defined(__linux__)
    case 80:return SCHED_OTHER;
    case 81:return SCHED_BATCH;
    case 82:return SCHED_FIFO;
#else
    case 80:return 0;
    case 81:return 0;
    case 82:return 0;
#endif
    default:GGML_ABORT("invalid Goose pool metadata");
    }
}
void fg149_set(int64_t context,int64_t field,int64_t value) {
    struct fg149_request * q=(struct fg149_request*)(uintptr_t)context;
    struct ggml_threadpool * p=q->pool;
    if(field>=10000&&field<10000+(int64_t)GGML_MAX_N_THREADS*GGML_MAX_N_THREADS){int64_t at=field-10000;GGML_ASSERT(at/512<p->n_threads);p->workers[at/512].cpumask[at%512]=value!=0;return;}
    switch(field){
    case 0:p->cgraph=q->graph;return;
    case 1:p->cplan=q->plan;return;
    case 2:atomic_store_explicit(&p->n_graph,(int)value,memory_order_relaxed);return;
    case 3:atomic_store_explicit(&p->n_barrier,(int)value,memory_order_relaxed);return;
    case 4:atomic_store_explicit(&p->n_barrier_passed,(int)value,memory_order_relaxed);return;
    case 5:atomic_store_explicit(&p->current_chunk,(int)value,memory_order_relaxed);return;
    case 6:atomic_store_explicit(&p->stop,value!=0,memory_order_seq_cst);return;
    case 7:atomic_store_explicit(&p->pause,value!=0,memory_order_seq_cst);return;
    case 8:atomic_store_explicit(&p->abort,(int)value,memory_order_relaxed);return;
    case 9:p->n_threads=(int)value;return;
    case 10:p->poll=(uint32_t)value;return;
    case 11:p->prio=(int32_t)value;return;
    case 12:p->ec=(enum ggml_status)value;return;
#ifndef GGML_USE_OPENMP
    case 13:q->state->pending=value!=0;return;
    case 14:q->state->last_graph=(int)value;return;
#endif
    case 16:GGML_ASSERT(value>=0&&value<p->n_threads);p->workers[value].threadpool=p;p->workers[value].ith=(int)value;return;
    case 17:p->workers=NULL;return;
    default:GGML_ABORT("invalid Goose pool field store");
    }
}
int64_t fg149_action(int64_t context,int64_t op,int64_t a,int64_t b) {
    struct fg149_request * q=(struct fg149_request*)(uintptr_t)context;
    struct ggml_threadpool * p=q->pool;
    switch(op){
    case 0:q->pool=fg149_system_alloc(0,a);return(int64_t)(uintptr_t)q->pool;
    case 1:fg149_system_free(0,p,a);q->pool=NULL;return 0;
    case 2:p->workers=fg149_system_alloc(1,a);return(int64_t)(uintptr_t)p->workers;
    case 3:fg149_system_free(1,p->workers,a);p->workers=NULL;return 0;
    case 4:memset(p->workers,0,(size_t)a);return 0;
    case 5:
#ifdef _WIN32
        ggml_mutex_init(&p->mutex);return 0;
#else
        return ggml_mutex_init(&p->mutex);
#endif
    case 6:
#ifdef _WIN32
        ggml_cond_init(&p->cond);return 0;
#else
        return ggml_cond_init(&p->cond);
#endif
    case 7:ggml_mutex_destroy(&p->mutex);return 0;
    case 8:ggml_cond_destroy(&p->cond);return 0;
    case 9:if(a)ggml_mutex_lock_shared(&p->mutex);else ggml_mutex_lock(&p->mutex);return 0;
    case 10:if(a)ggml_mutex_unlock_shared(&p->mutex);else ggml_mutex_unlock(&p->mutex);return 0;
    case 11:ggml_cond_wait(&p->cond,&p->mutex);return 0;
    case 12:ggml_cond_broadcast(&p->cond);return 0;
#ifndef GGML_USE_OPENMP
    case 13:GGML_ASSERT(a>0&&a<p->n_threads);return fg149_system_start(p->workers+a);
    case 14:GGML_ASSERT(a>0&&a<p->n_threads);return ggml_thread_join(p->workers[a].thrd,NULL);
#endif
    case 15:ggml_thread_cpu_relax();return 0;
    case 16:return atomic_fetch_add_explicit(&p->n_barrier,(int)a,memory_order_seq_cst);
    case 17:atomic_store_explicit(&p->n_barrier,(int)a,memory_order_relaxed);return 0;
    case 18:return atomic_fetch_add_explicit(&p->n_barrier_passed,(int)a,memory_order_seq_cst);
    case 19:
#ifdef GGML_TSAN_ENABLED
        if(a)atomic_fetch_add_explicit(&p->n_graph,0,memory_order_seq_cst);else atomic_fetch_add_explicit(&p->n_barrier_passed,0,memory_order_seq_cst);
#else
        atomic_thread_fence(memory_order_seq_cst);
#endif
        return 0;
    case 20:{
#ifdef GGML_USE_OPENMP
        #pragma omp barrier
        return 0;
#else
        GGML_ABORT("OpenMP primitive called by pthread policy");
#endif
    }
    case 21:atomic_store_explicit(&p->current_chunk,(int)a,memory_order_relaxed);return 0;
    case 22:return atomic_fetch_add_explicit(&p->current_chunk,(int)a,memory_order_relaxed);
    case 23:atomic_store_explicit(&p->n_graph,(int)a,memory_order_seq_cst);return 0;
    case 25:GGML_ASSERT(a>=0&&a<p->n_threads);ggml_graph_compute_thread(p->workers+a);return 0;
    case 26:
#if defined(__linux__)
        {struct sched_param param={0};param.sched_priority=(int)b;int rc=pthread_setschedparam(pthread_self(),(int)a,&param);if(rc)GGML_LOG_WARN("thread priority primitive failed: %s\n",strerror(rc));return rc==0;}
#else
        return ggml_thread_apply_priority(q->pool?q->pool->prio:(int32_t)q->value);
#endif
    case 27:return ggml_thread_apply_affinity(q->state?q->state->cpumask:p->workers[0].cpumask);
#if defined(__gnu_linux__)
    case 40:q->temporary=CPU_ALLOC((int)a);if(q->temporary)CPU_ZERO_S(CPU_ALLOC_SIZE(g_state.numa.total_cpus),(cpu_set_t*)q->temporary);return q->temporary!=NULL;
    case 41:CPU_SET_S((int)a,CPU_ALLOC_SIZE(g_state.numa.total_cpus),(cpu_set_t*)q->temporary);return 0;
    case 42:{int rc=pthread_setaffinity_np(pthread_self(),CPU_ALLOC_SIZE(g_state.numa.total_cpus),(cpu_set_t*)q->temporary);if(rc)GGML_LOG_WARN("NUMA affinity primitive failed: %s\n",strerror(rc));return rc;}
    case 43:CPU_FREE(q->temporary);q->temporary=NULL;return 0;
    case 44:{int rc=pthread_setaffinity_np(pthread_self(),CPU_ALLOC_SIZE(g_state.numa.total_cpus),&g_state.numa.cpuset);if(rc)GGML_LOG_WARN("NUMA affinity primitive failed: %s\n",strerror(rc));return rc;}
#endif
    default:GGML_ABORT("invalid Goose pool primitive");
    }
}
