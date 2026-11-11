// Bridge for EXP253: tensor handles cross as opaque int64; the four gating primitives are ggml calls.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct ggml_context ggml_context;
typedef struct ggml_tensor ggml_tensor;
extern ggml_tensor * ggml_soft_max(ggml_context *, ggml_tensor *);
extern ggml_tensor * ggml_sigmoid(ggml_context *, ggml_tensor *);
extern ggml_tensor * ggml_sqrt(ggml_context *, ggml_tensor *);
extern ggml_tensor * ggml_softplus(ggml_context *, ggml_tensor *);

static ggml_context * g_ctx;
void fg253s_set_ctx(void * c) { g_ctx = (ggml_context *) c; }


extern ggml_tensor * ggml_reshape_2d(ggml_context *, ggml_tensor *, int64_t, int64_t);
extern ggml_tensor * ggml_reshape_3d(ggml_context *, ggml_tensor *, int64_t, int64_t, int64_t);
extern ggml_tensor * ggml_sum_rows(ggml_context *, ggml_tensor *);
extern ggml_tensor * ggml_clamp(ggml_context *, ggml_tensor *, float, float);
extern ggml_tensor * ggml_div(ggml_context *, ggml_tensor *, ggml_tensor *);
static float fg253s_bits2f(int64_t b){float f;int32_t u=(int32_t)b;memcpy(&f,&u,4);return f;}
static int64_t fg253s_reshape_2d(int64_t t,int64_t n,int64_t m){return (int64_t)(uintptr_t) ggml_reshape_2d(g_ctx,(ggml_tensor*)(uintptr_t)t,n,m);}
static int64_t fg253s_reshape_3d(int64_t t,int64_t a,int64_t b,int64_t c){return (int64_t)(uintptr_t) ggml_reshape_3d(g_ctx,(ggml_tensor*)(uintptr_t)t,a,b,c);}
static int64_t fg253s_sum_rows(int64_t t){return (int64_t)(uintptr_t) ggml_sum_rows(g_ctx,(ggml_tensor*)(uintptr_t)t);}
static int64_t fg253s_clamp(int64_t t,int64_t lo,int64_t hi){return (int64_t)(uintptr_t) ggml_clamp(g_ctx,(ggml_tensor*)(uintptr_t)t,fg253s_bits2f(lo),fg253s_bits2f(hi));}
static int64_t fg253s_div(int64_t a,int64_t b){return (int64_t)(uintptr_t) ggml_div(g_ctx,(ggml_tensor*)(uintptr_t)a,(ggml_tensor*)(uintptr_t)b);}

extern ggml_tensor * ggml_argsort_top_k(ggml_context *, ggml_tensor *, int);
extern ggml_tensor * ggml_get_rows(ggml_context *, ggml_tensor *, ggml_tensor *);
static int64_t fg253s_argsort_top_k(int64_t t,int64_t k){return (int64_t)(uintptr_t) ggml_argsort_top_k(g_ctx,(ggml_tensor*)(uintptr_t)t,(int)k);}
static int64_t fg253s_get_rows(int64_t a,int64_t b){return (int64_t)(uintptr_t) ggml_get_rows(g_ctx,(ggml_tensor*)(uintptr_t)a,(ggml_tensor*)(uintptr_t)b);}
static void fg253s_fail(int64_t c){fprintf(stderr,"FG253S_FAIL code=%lld\n",(long long)c);abort();}
static void fg253s_put(int64_t p,int64_t k,int64_t i,uint64_t v){((int64_t*)(uintptr_t)p)[k*16+i]=(int64_t)v;}
static uint64_t fg253s_get(int64_t p,int64_t k,int64_t i){return (uint64_t)((const int64_t*)(uintptr_t)p)[k*16+i];}
static int64_t fg253s_soft_max(int64_t t){return (int64_t)(uintptr_t) ggml_soft_max(g_ctx,(ggml_tensor*)(uintptr_t)t);}
static int64_t fg253s_sigmoid(int64_t t){return (int64_t)(uintptr_t) ggml_sigmoid(g_ctx,(ggml_tensor*)(uintptr_t)t);}
static int64_t fg253s_sqrt(int64_t t){return (int64_t)(uintptr_t) ggml_sqrt(g_ctx,(ggml_tensor*)(uintptr_t)t);}
static int64_t fg253s_softplus(int64_t t){return (int64_t)(uintptr_t) ggml_softplus(g_ctx,(ggml_tensor*)(uintptr_t)t);}

#include "host-qualified.c"

int64_t fg253s_probs(int64_t logits, int64_t op) {
    int64_t pkt[2*16] = {0}; pkt[0]=logits; pkt[1]=op;
    (void) fg253ds_gg_function_1((int64_t)(uintptr_t)pkt,1);
    return pkt[16+0];
}

int64_t fg253s_norm(int64_t weights, int64_t n_used, int64_t n_tokens) {
    int64_t pkt[2*16] = {0}; pkt[0]=weights; pkt[1]=n_used; pkt[2]=2; pkt[3]=n_tokens;
    (void) fg253ds_gg_function_1((int64_t)(uintptr_t)pkt,1);
    return pkt[16+0];
}

void fg253s_topk(int64_t selection, int64_t probs, int64_t n_used, int64_t n_expert, int64_t n_tokens, int64_t out[2]) {
    int64_t pkt[2*16] = {0};
    pkt[0]=selection; pkt[1]=probs; pkt[2]=3; pkt[3]=n_used; pkt[4]=n_expert; pkt[5]=n_tokens;
    (void) fg253ds_gg_function_1((int64_t)(uintptr_t)pkt,1);
    out[0]=pkt[16+0]; out[1]=pkt[16+1];
}
