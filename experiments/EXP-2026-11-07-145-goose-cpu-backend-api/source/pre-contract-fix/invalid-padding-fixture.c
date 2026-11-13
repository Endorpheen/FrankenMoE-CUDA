#include "ggml.h"
#include "ggml-cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static FILE*out;
static struct ggml_context*ctx;
static struct ggml_tensor*make(enum ggml_type type,int64_t a,int64_t b,int64_t c,int64_t d){return ggml_new_tensor_4d(ctx,type,a,b,c,d);}
static void fill(struct ggml_tensor*t,int positive){int64_t n=ggml_nelements(t);if(t->type==GGML_TYPE_F32){float*p=t->data;for(int64_t i=0;i<n;++i)p[i]=((int)((i*13+7)%31)-15)/19.0f+(positive?1.25f:0.0f);}else if(t->type==GGML_TYPE_I32){int32_t*p=t->data;for(int64_t i=0;i<n;++i)p[i]=i%3;}else if(t->type==GGML_TYPE_I64){int64_t*p=t->data;for(int64_t i=0;i<n;++i)p[i]=i%3;}}
static struct ggml_tensor*node(enum ggml_op op,enum ggml_type type,int64_t a,int64_t b,int64_t c,int64_t d,struct ggml_tensor*x,struct ggml_tensor*y,struct ggml_tensor*z){struct ggml_tensor*t=make(type,a,b,c,d);t->op=op;t->src[0]=x;t->src[1]=y;t->src[2]=z;memset(t->op_params,0,sizeof(t->op_params));memset(t->data,0,ggml_nbytes(t));return t;}
static void fp(struct ggml_tensor*t,int at,float v){memcpy((char*)t->op_params+4*at,&v,4);}
static void run(const char*name,struct ggml_tensor*t,int threads){struct ggml_cgraph*g=ggml_new_graph(ctx);ggml_build_forward_expand(g,t);enum ggml_status s=ggml_graph_compute_with_ctx(ctx,g,threads);if(s!=GGML_STATUS_SUCCESS){fprintf(stderr,"graph failed %s status%d\n",name,s);exit(3);}size_t n=ggml_nbytes(t);if(fwrite(t->data,1,n,out)!=n)abort();printf("{\"name\":\"%s\",\"type\":%d,\"elements\":%lld,\"bytes\":%zu,\"threads\":%d}\n",name,t->type,(long long)ggml_nelements(t),n,threads);fflush(stdout);}
static void begin(void){struct ggml_init_params p={16*1024*1024,NULL,false};ctx=ggml_init(p);if(!ctx)abort();}
static void end(void){ggml_free(ctx);}
int main(int argc,char**argv){if(argc!=2)return 2;out=fopen(argv[1],"wb");if(!out)return 2;
 const enum ggml_op element[]={GGML_OP_ADD,GGML_OP_ADD1,GGML_OP_SUB,GGML_OP_MUL,GGML_OP_DIV,GGML_OP_SCALE,GGML_OP_SQR,GGML_OP_SQRT,GGML_OP_SIN,GGML_OP_COS,GGML_OP_LOG,GGML_OP_CLAMP,GGML_OP_LEAKY_RELU,GGML_OP_SILU_BACK};
 for(int threads=1;threads<=3;threads+=2){
 for(size_t k=0;k<sizeof(element)/sizeof(*element);++k){begin();struct ggml_tensor*x=make(GGML_TYPE_F32,17,3,1,1),*y=make(GGML_TYPE_F32,17,3,1,1);fill(x,element[k]==GGML_OP_LOG||element[k]==GGML_OP_SQRT);fill(y,1);if(element[k]==GGML_OP_ADD1)y=make(GGML_TYPE_F32,1,1,1,1),fill(y,1);struct ggml_tensor*t=node(element[k],GGML_TYPE_F32,17,3,1,1,x,y,NULL);fp(t,0,element[k]==GGML_OP_CLAMP?-.3f:.7f);fp(t,1,element[k]==GGML_OP_CLAMP?.4f:.1f);char name[80];snprintf(name,sizeof(name),"pointwise-%d-t%d",element[k],threads);run(name,t,threads);end();}
 for(int u=0;u<GGML_UNARY_OP_COUNT;++u){begin();struct ggml_tensor*x=make(GGML_TYPE_F32,17,3,1,1);fill(x,0);struct ggml_tensor*t=node(GGML_OP_UNARY,GGML_TYPE_F32,17,3,1,1,x,NULL,NULL);t->op_params[0]=u;fp(t,1,.6f);fp(t,2,1.6f);fp(t,3,.5f);fp(t,4,12.f);char name[80];snprintf(name,sizeof(name),"unary-%d-t%d",u,threads);run(name,t,threads);end();}
 for(int u=0;u<GGML_GLU_OP_COUNT;++u){begin();struct ggml_tensor*x=make(GGML_TYPE_F32,17,3,1,1),*y=make(GGML_TYPE_F32,17,3,1,1);fill(x,0);fill(y,1);struct ggml_tensor*t=node(GGML_OP_GLU,GGML_TYPE_F32,17,3,1,1,x,y,NULL);t->op_params[0]=u;fp(t,2,1.702f);fp(t,3,7.f);char name[80];snprintf(name,sizeof(name),"glu-%d-t%d",u,threads);run(name,t,threads);end();}
 const enum ggml_op reduce[]={GGML_OP_SUM,GGML_OP_SUM_ROWS,GGML_OP_MEAN,GGML_OP_CUMSUM,GGML_OP_ARGMAX,GGML_OP_NORM,GGML_OP_RMS_NORM,GGML_OP_L2_NORM,GGML_OP_SOFT_MAX};
 for(size_t k=0;k<sizeof(reduce)/sizeof(*reduce);++k){begin();struct ggml_tensor*x=make(GGML_TYPE_F32,17,3,1,1);fill(x,0);enum ggml_op op=reduce[k];int64_t w=op==GGML_OP_SUM||op==GGML_OP_SUM_ROWS||op==GGML_OP_MEAN?1:op==GGML_OP_ARGMAX?3:17;int64_t h=op==GGML_OP_SUM||op==GGML_OP_ARGMAX?1:3;struct ggml_tensor*t=node(op,op==GGML_OP_ARGMAX?GGML_TYPE_I32:GGML_TYPE_F32,w,h,1,1,x,NULL,NULL);fp(t,0,op==GGML_OP_SOFT_MAX?.7f:1e-5f);char name[80];snprintf(name,sizeof(name),"reduce-norm-%d-t%d",op,threads);run(name,t,threads);end();}
 for(int mode=0;mode<=2;mode+=2){begin();struct ggml_tensor*x=make(GGML_TYPE_F32,16,2,3,1),*y=make(GGML_TYPE_I32,3,1,1,1);fill(x,0);fill(y,0);struct ggml_tensor*t=node(GGML_OP_ROPE,GGML_TYPE_F32,16,2,3,1,x,y,NULL);t->op_params[1]=8;t->op_params[2]=mode;t->op_params[4]=2048;fp(t,5,10000.f);fp(t,6,1.f);fp(t,7,.4f);fp(t,8,1.f);fp(t,9,32.f);fp(t,10,1.f);char name[80];snprintf(name,sizeof(name),"rope-%d-t%d",mode,threads);run(name,t,threads);end();}
 {begin();struct ggml_tensor*x=make(GGML_TYPE_F32,32,7,1,1),*y=make(GGML_TYPE_F32,32,5,1,1);fill(x,0);fill(y,1);struct ggml_tensor*t=node(GGML_OP_MUL_MAT,GGML_TYPE_F32,7,5,1,1,x,y,NULL);char name[80];snprintf(name,sizeof(name),"dense-f32-t%d",threads);run(name,t,threads);end();}
 {begin();struct ggml_tensor*x=make(GGML_TYPE_F32,17,3,1,1);fill(x,0);struct ggml_tensor*t=node(GGML_OP_DUP,GGML_TYPE_F32,17,3,1,1,x,NULL,NULL);char name[80];snprintf(name,sizeof(name),"dup-t%d",threads);run(name,t,threads);end();}
 {begin();struct ggml_tensor*x=make(GGML_TYPE_F32,7,3,1,1);fill(x,0);struct ggml_tensor*t=node(GGML_OP_REPEAT,GGML_TYPE_F32,14,6,1,1,x,NULL,NULL);char name[80];snprintf(name,sizeof(name),"repeat-t%d",threads);run(name,t,threads);end();}
 {begin();struct ggml_tensor*x=make(GGML_TYPE_F32,7,3,1,1),*y=make(GGML_TYPE_F32,5,3,1,1);fill(x,0);fill(y,1);struct ggml_tensor*t=node(GGML_OP_CONCAT,GGML_TYPE_F32,12,3,1,1,x,y,NULL);char name[80];snprintf(name,sizeof(name),"concat-t%d",threads);run(name,t,threads);end();}
 {begin();struct ggml_tensor*x=make(GGML_TYPE_F32,7,3,1,1);fill(x,0);struct ggml_tensor*t=node(GGML_OP_PAD,GGML_TYPE_F32,10,5,1,1,x,NULL,NULL);char name[80];snprintf(name,sizeof(name),"pad-t%d",threads);run(name,t,threads);end();}
 {begin();struct ggml_tensor*x=make(GGML_TYPE_F32,7,3,1,1);fill(x,0);struct ggml_tensor*t=node(GGML_OP_ROLL,GGML_TYPE_F32,7,3,1,1,x,NULL,NULL);t->op_params[0]=2;t->op_params[1]=-1;char name[80];snprintf(name,sizeof(name),"roll-t%d",threads);run(name,t,threads);end();}
 {begin();struct ggml_tensor*x=make(GGML_TYPE_F32,7,5,1,1),*y=make(GGML_TYPE_I32,3,1,1,1);fill(x,0);fill(y,0);struct ggml_tensor*t=node(GGML_OP_GET_ROWS,GGML_TYPE_F32,7,3,1,1,x,y,NULL);char name[80];snprintf(name,sizeof(name),"gather-t%d",threads);run(name,t,threads);end();}
 {begin();struct ggml_tensor*x=make(GGML_TYPE_F32,5,3,2,1),*y=make(GGML_TYPE_F32,3,3,1,1);fill(x,0);fill(y,1);struct ggml_tensor*t=node(GGML_OP_SSM_CONV,GGML_TYPE_F32,3,3,2,1,x,y,NULL);char name[80];snprintf(name,sizeof(name),"ssm-conv-t%d",threads);run(name,t,threads);end();}
 }
 fclose(out);return 0;
}
