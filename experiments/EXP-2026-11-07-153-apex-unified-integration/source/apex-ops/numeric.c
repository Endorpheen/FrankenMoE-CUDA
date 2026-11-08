#include "ggml.h"
#include "ggml-cpu.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"numeric fixture failure %d: %s\n",__LINE__,#x);exit(2);}}while(0)
static struct ggml_tensor* input(struct ggml_context*c,int64_t a,int64_t b,int64_t d,int64_t e,float offset){struct ggml_tensor*t=ggml_new_tensor_4d(c,GGML_TYPE_F32,a,b,d,e);float*p=t->data;for(int64_t i=0;i<ggml_nelements(t);i++)p[i]=offset+(float)((i*7)%17-8)*.03125f;return t;}
static void compute(struct ggml_context*c,struct ggml_tensor*t,void*out,size_t bytes){struct ggml_cgraph*g=ggml_new_graph(c);ggml_build_forward_expand(g,t);CHECK(ggml_graph_compute_with_ctx(c,g,1)==GGML_STATUS_SUCCESS);CHECK(ggml_nbytes(t)==bytes);memcpy(out,t->data,bytes);}
static void run(int enabled,int kind,void*out,size_t bytes){setenv("GGML_GOOSE_APEX_OPS",enabled?"1":"0",1);struct ggml_context*c=ggml_init((struct ggml_init_params){4194304,NULL,false});struct ggml_tensor*t;
 if(kind==0){struct ggml_tensor*w=input(c,8,16,1,1,.0625f),*x=input(c,8,3,1,1,.125f);t=ggml_mul_mat(c,w,x);struct ggml_tensor*r=t;t=ggml_rms_norm(c,t,1e-6f);t=ggml_add(c,r,ggml_scale(c,ggml_silu(c,t),.5f));t=ggml_soft_max(c,t);t=ggml_cont(c,ggml_argsort_top_k(c,t,4));t=ggml_reshape_1d(c,t,12);struct ggml_tensor*embedding=input(c,8,16,1,1,-.125f);t=ggml_get_rows(c,embedding,t);t=ggml_l2_norm(c,t,1e-6f);}
 else if(kind==1){struct ggml_tensor*w=input(c,8,16,4,1,.03125f),*x=input(c,8,1,3,1,.0625f),*ids=ggml_new_tensor_2d(c,GGML_TYPE_I32,2,3);int32_t*id=ids->data;for(int i=0;i<6;i++)id[i]=(i*3)%4;t=ggml_mul_mat_id(c,w,x,ids);t=ggml_swiglu(c,t);t=ggml_sum_rows(c,t);}
 else{struct ggml_tensor*q=input(c,4,2,3,1,.0625f),*k=input(c,4,2,3,1,.03125f),*v=input(c,4,2,3,1,.125f),*g=input(c,1,2,3,1,-.5f),*beta=input(c,1,2,3,1,.5f),*state=input(c,4,4,2,1,0);t=ggml_gated_delta_net(c,q,k,v,g,beta,state,1);}
 compute(c,t,out,bytes);ggml_free(c);
}
int main(void){const size_t sizes[]={8*12*4,1*2*3*4,8*7*4};uint64_t hash=1469598103934665603ULL;for(int kind=0;kind<3;kind++){unsigned char native[1024],goose[1024];run(0,kind,native,sizes[kind]);run(1,kind,goose,sizes[kind]);CHECK(memcmp(native,goose,sizes[kind])==0);const float*p=(const float*)goose;for(size_t i=0;i<sizes[kind]/4;i++)CHECK(isfinite(p[i]));for(size_t i=0;i<sizes[kind];i++){hash^=goose[i];hash*=1099511628211ULL;}}printf("APEX_OPS_NUMERIC_DIFFERENTIAL_OK graphs=3 bytes=632 hash=%016llx\n",(unsigned long long)hash);return 0;}
