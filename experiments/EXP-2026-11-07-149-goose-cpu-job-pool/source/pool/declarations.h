struct fg149_request {
    struct ggml_threadpool * pool;
    struct ggml_compute_state * state;
    const struct ggml_threadpool_params * params;
    struct ggml_cgraph * graph;
    struct ggml_cplan * plan;
    int64_t value;
    int32_t cursor;
    void * temporary;
};
static bool fg149_enabled = true;
static bool fg149_tiled_enabled = true;
__attribute__((constructor)) static void fg149_config(void) {
    const char * value = getenv("GGML_GOOSE_CPU_POOL");
    fg149_enabled = !value || strcmp(value,"0");
    const char*tiled=getenv("GGML_CPU_TILED_MM");fg149_tiled_enabled=!tiled||atoi(tiled)!=0;
}
extern int64_t fg149_pool_control(int64_t,int64_t,const uint8_t*,int64_t,uint8_t*,int64_t);
static int64_t fg149_call(int mode,struct fg149_request * q,const bool * mask,bool * local) {
    return fg149_pool_control(mode,(int64_t)(uintptr_t)q,(const uint8_t*)mask,mask?GGML_MAX_N_THREADS:0,(uint8_t*)local,local?GGML_MAX_N_THREADS:0);
}
