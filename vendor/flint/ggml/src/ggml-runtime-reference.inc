//
// timing
//

#if defined(_MSC_VER) || defined(__MINGW32__)
static int64_t timer_freq, timer_start;
static BOOL CALLBACK ggml_time_init_once(PINIT_ONCE once, PVOID param, PVOID *ctx) {
    UNUSED(once);
    UNUSED(param);
    UNUSED(ctx);

    LARGE_INTEGER t;
    QueryPerformanceFrequency(&t);
    timer_freq = t.QuadPart;

    // The multiplication by 1000 or 1000000 below can cause an overflow if timer_freq
    // and the uptime is high enough.
    // We subtract the program start time to reduce the likelihood of that happening.
    QueryPerformanceCounter(&t);
    timer_start = t.QuadPart;

    return TRUE;
}
void ggml_time_init(void) {
    static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
    InitOnceExecuteOnce(&once, ggml_time_init_once, NULL, NULL);
}
int64_t ggml_time_ms(void) {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return ((t.QuadPart-timer_start) * 1000) / timer_freq;
}
int64_t ggml_time_us(void) {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return ((t.QuadPart-timer_start) * 1000000) / timer_freq;
}
#else
void ggml_time_init(void) {}
int64_t ggml_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec*1000 + (int64_t)ts.tv_nsec/1000000;
}

int64_t ggml_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec*1000000 + (int64_t)ts.tv_nsec/1000;
}
#endif

void ggml_unravel_index(const struct ggml_tensor * tensor, int64_t i, int64_t * i0, int64_t * i1, int64_t * i2, int64_t * i3) {
    const int64_t ne2 = tensor->ne[2];
    const int64_t ne1 = tensor->ne[1];
    const int64_t ne0 = tensor->ne[0];

    const int64_t i3_ = (i/(ne2*ne1*ne0));
    const int64_t i2_ = (i - i3_*ne2*ne1*ne0)/(ne1*ne0);
    const int64_t i1_ = (i - i3_*ne2*ne1*ne0 - i2_*ne1*ne0)/ne0;
    const int64_t i0_ = (i - i3_*ne2*ne1*ne0 - i2_*ne1*ne0 - i1_*ne0);

    if (i0) {
        * i0 = i0_;
    }
    if (i1) {
        * i1 = i1_;
    }
    if (i2) {
        * i2 = i2_;
    }
    if (i3) {
        * i3 = i3_;
    }
}

void ggml_threadpool_params_init(struct ggml_threadpool_params * p, int n_threads) {
    p->n_threads  = n_threads;
    p->prio       = 0;     // default priority (usually means normal or inherited)
    p->poll       = 50;    // hybrid-polling enabled
    p->strict_cpu = false; // no strict placement (all threads share same cpumask)
    p->paused     = false; // threads are ready to go
    memset(p->cpumask, 0, GGML_MAX_N_THREADS); // all-zero means use the default affinity (usually inherited)
}

struct ggml_threadpool_params ggml_threadpool_params_default(int n_threads) {
    struct ggml_threadpool_params p;
    ggml_threadpool_params_init(&p, n_threads);
    return p;
}

bool ggml_threadpool_params_match(const struct ggml_threadpool_params * p0, const struct ggml_threadpool_params * p1) {
    if (p0->n_threads  != p1->n_threads  ) return false;
    if (p0->prio       != p1->prio       ) return false;
    if (p0->poll       != p1->poll       ) return false;
    if (p0->strict_cpu != p1->strict_cpu ) return false;
    return memcmp(p0->cpumask, p1->cpumask, GGML_MAX_N_THREADS) == 0;
}

struct ggml_hash_set ggml_hash_set_new(size_t size) {
    size = ggml_hash_size(size);
    struct ggml_hash_set result;
    result.size = size;
    result.keys = GGML_MALLOC(sizeof(struct ggml_tensor *) * size);
    result.used = GGML_CALLOC(ggml_bitset_size(size), sizeof(ggml_bitset_t));
    return result;
}

void ggml_hash_set_free(struct ggml_hash_set * hash_set) {
    GGML_FREE(hash_set->used);
    GGML_FREE(hash_set->keys);
}

void * ggml_new_buffer(struct ggml_context * ctx, size_t nbytes) {
    struct ggml_object * obj = ggml_new_object(ctx, GGML_OBJECT_TYPE_WORK_BUFFER, nbytes);

    return (uint8_t *)ctx->mem_buffer + obj->offs;
}

struct ggml_tensor * ggml_get_tensor(struct ggml_context * ctx, const char * name) {
    struct ggml_object * obj = ctx->objects_begin;

    char * const mem_buffer = ctx->mem_buffer;

    while (obj != NULL) {
        if (obj->type == GGML_OBJECT_TYPE_TENSOR) {
            struct ggml_tensor * cur = (struct ggml_tensor *)(mem_buffer + obj->offs);
            if (strcmp(cur->name, name) == 0) {
                return cur;
            }
        }

        obj = obj->next;
    }

    return NULL;
}

void * ggml_get_mem_buffer(const struct ggml_context * ctx) {
    return ctx->mem_buffer;
}

const char * ggml_op_desc(const struct ggml_tensor * t) {
    if (t->op == GGML_OP_UNARY) {
        enum ggml_unary_op uop = ggml_get_unary_op(t);
        return ggml_unary_op_name(uop);
    }
    if (t->op == GGML_OP_GLU) {
        enum ggml_glu_op gop = ggml_get_glu_op(t);
        return ggml_glu_op_name(gop);
    }
    return ggml_op_name(t->op);
}

struct ggml_cgraph * ggml_new_graph(struct ggml_context * ctx) {
    return ggml_new_graph_custom(ctx, GGML_DEFAULT_GRAPH_SIZE, false);
}

struct ggml_tensor * ggml_graph_node(struct ggml_cgraph * cgraph, int i) {
    if (i < 0) {
        GGML_ASSERT(cgraph->n_nodes + i >= 0);
        return cgraph->nodes[cgraph->n_nodes + i];
    }

    GGML_ASSERT(i < cgraph->n_nodes);
    return cgraph->nodes[i];
}

int ggml_graph_n_nodes(struct ggml_cgraph * cgraph) {
    return cgraph->n_nodes;
}
