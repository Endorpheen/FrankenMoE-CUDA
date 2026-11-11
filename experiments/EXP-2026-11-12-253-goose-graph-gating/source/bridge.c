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
