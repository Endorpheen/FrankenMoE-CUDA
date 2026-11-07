#include "ggml.h"
#include "ggml-cpu.h"
#include "ggml-backend.h"
#include "ggml-backend-impl.h"
#include "ggml-impl.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE*out;
static const enum ggml_type kinds[]={ACTIVE_TYPES};
static void record(const char*name,const void*data,size_t bytes,int kind){if(fwrite(data,1,bytes,out)!=bytes)abort();printf("{\"name\":\"%s\",\"bytes\":%zu,\"type\":%d}\n",name,bytes,kind);fflush(stdout);}
static struct ggml_tensor shape(enum ggml_type type){struct ggml_tensor t={0};t.type=type;t.ne[0]=256;t.ne[1]=8;t.ne[2]=1;t.ne[3]=1;t.nb[0]=ggml_type_size(type);t.nb[1]=ggml_row_size(type,256);t.nb[2]=t.nb[1]*8;t.nb[3]=t.nb[2];return t;}
static bool abort_once(void*value){++*(int64_t*)value;return true;}
static void fill(struct ggml_tensor*t,int variant){for(int64_t i=0;i<ggml_nelements(t);++i)((float*)t->data)[i]=(float)((i*13+variant*7)%47-23)/31.f;}
static void graph_cases(ggml_backend_reg_t reg,int threads){
 ggml_backend_t backend=ggml_backend_cpu_init();if(!backend)abort();ggml_backend_set_n_threads_t set_threads=(ggml_backend_set_n_threads_t)ggml_backend_reg_get_proc_address(reg,"ggml_backend_set_n_threads");ggml_backend_set_abort_callback_t set_abort=(ggml_backend_set_abort_callback_t)ggml_backend_reg_get_proc_address(reg,"ggml_backend_set_abort_callback");if(!set_threads||!set_abort)abort();set_threads(backend,threads);
 struct ggml_threadpool_params config=ggml_threadpool_params_default(threads);config.poll=0;ggml_threadpool_t pool_a=ggml_threadpool_new(&config),pool_b=ggml_threadpool_new(&config);if(!pool_a||!pool_b)abort();
 for(int cycle=0;cycle<7;++cycle){
  ggml_backend_cpu_set_threadpool(backend,cycle<2?pool_a:cycle==2?pool_b:cycle==5?pool_a:NULL);
  struct ggml_init_params init={8*1024*1024,NULL,false};struct ggml_context*ctx=ggml_init(init);if(!ctx)abort();int width=cycle==1?1024:cycle==2?32:64;
  struct ggml_tensor*x=ggml_new_tensor_2d(ctx,GGML_TYPE_F32,width,3);fill(x,cycle);struct ggml_tensor*t;
  if(cycle==2)t=ggml_scale(ctx,x,.75f);
  else if(cycle==5){struct ggml_tensor*norm=ggml_rms_norm(ctx,x,1e-5f);struct ggml_tensor*w=ggml_new_tensor_1d(ctx,GGML_TYPE_F32,width);fill(w,1);t=ggml_swiglu_split(ctx,ggml_mul(ctx,norm,w),x);}
  else t=ggml_soft_max(ctx,x);
  memset(t->data,0,ggml_nbytes(t));struct ggml_cgraph*g=ggml_new_graph(ctx);ggml_build_forward_expand(g,t);int64_t calls=0;set_abort(backend,cycle==5?abort_once:NULL,cycle==5?&calls:NULL);ggml_backend_cpu_set_use_ref(backend,cycle==6);
  enum ggml_status status;
  if(cycle==4){ggml_backend_graph_plan_t plan=ggml_backend_graph_plan_create(backend,g);if(!plan)abort();status=ggml_backend_graph_plan_compute(backend,plan);if(status!=GGML_STATUS_SUCCESS)abort();fill(x,7);status=ggml_backend_graph_plan_compute(backend,plan);ggml_backend_graph_plan_free(backend,plan);}
  else status=ggml_backend_graph_compute(backend,g);
  if(status!=(cycle==5?GGML_STATUS_ABORTED:GGML_STATUS_SUCCESS))abort();int64_t state[2]={status,calls};char name[128];snprintf(name,sizeof(name),"backend-status-cycle%d-t%d",cycle,threads);record(name,state,sizeof(state),GGML_TYPE_I64);snprintf(name,sizeof(name),"backend-output-cycle%d-t%d",cycle,threads);record(name,t->data,ggml_nbytes(t),GGML_TYPE_F32);ggml_free(ctx);
 }
 set_abort(backend,NULL,NULL);ggml_backend_cpu_set_threadpool(backend,NULL);ggml_backend_free(backend);ggml_threadpool_free(pool_a);ggml_threadpool_free(pool_b);
}
int main(int argc,char**argv){if(argc!=2)return 2;out=fopen(argv[1],"wb");if(!out)return 2;ggml_backend_reg_t reg=ggml_backend_cpu_reg();ggml_backend_dev_t dev=ggml_backend_reg_dev_get(reg,0);
 struct ggml_backend_dev_props props;memset(&props,0,sizeof(props));ggml_backend_dev_get_props(dev,&props);uint64_t info[]={ggml_backend_reg_dev_count(reg),props.type,props.memory_total,props.memory_free,props.caps.async,props.caps.host_buffer,props.caps.buffer_from_host_ptr,props.caps.events,props.caps.mmap_support};record("device-properties",info,sizeof(info),GGML_TYPE_I64);record("device-name",props.name,strlen(props.name)+1,GGML_TYPE_I8);record("device-description",props.description,strlen(props.description)+1,GGML_TYPE_I8);
 const char*names[]={PROC_NAMES,"not-a-known-procedure","ggml_backend_set_n_threads_extra",""};uint8_t found[64]={0};for(size_t i=0;i<sizeof(names)/sizeof(*names);++i)found[i]=ggml_backend_reg_get_proc_address(reg,names[i])!=NULL;record("procedure-presence",found,sizeof(names)/sizeof(*names),GGML_TYPE_I8);
 ggml_backend_get_features_t get_features=(ggml_backend_get_features_t)ggml_backend_reg_get_proc_address(reg,"ggml_backend_get_features");if(!get_features)abort();struct ggml_backend_feature*features=get_features(reg);char feature_text[4096];size_t at=0;for(size_t i=0;features[i].name;++i){size_t a=strlen(features[i].name)+1,b=strlen(features[i].value)+1;if(at+a+b>sizeof(feature_text))abort();memcpy(feature_text+at,features[i].name,a);at+=a;memcpy(feature_text+at,features[i].value,b);at+=b;}record("feature-list",feature_text,at,GGML_TYPE_I8);
 ggml_backend_dev_get_extra_bufts_t get_extras=(ggml_backend_dev_get_extra_bufts_t)ggml_backend_reg_get_proc_address(reg,"ggml_backend_dev_get_extra_bufts");if(!get_extras)abort();ggml_backend_buffer_type_t*extras=get_extras(dev);for(int i=0;extras[i];++i){char name[80];snprintf(name,sizeof(name),"extra-name-%d",i);const char*n=ggml_backend_buft_name(extras[i]);record(name,n,strlen(n)+1,GGML_TYPE_I8);uint8_t supported=ggml_backend_dev_supports_buft(dev,extras[i]);snprintf(name,sizeof(name),"extra-support-%d",i);record(name,&supported,1,GGML_TYPE_I8);ggml_backend_buffer_t buffer=ggml_backend_buft_alloc_buffer(extras[i],4096);if(buffer){struct ggml_tensor a=shape(GGML_TYPE_Q4_K),b=shape(GGML_TYPE_F32),t=shape(GGML_TYPE_F32);a.buffer=buffer;t.op=GGML_OP_MUL_MAT;t.src[0]=&a;t.src[1]=&b;supported=ggml_backend_dev_supports_op(dev,&t);snprintf(name,sizeof(name),"extra-operation-%d",i);record(name,&supported,1,GGML_TYPE_I8);ggml_backend_buffer_free(buffer);}}
 ggml_backend_buffer_type_t standard=ggml_backend_dev_buffer_type(dev);uint64_t alignment[]={ggml_backend_buft_get_alignment(standard),ggml_backend_dev_supports_buft(dev,standard)};record("standard-buffer",alignment,sizeof(alignment),GGML_TYPE_I64);void*host=aligned_alloc(64,4096);if(!host)abort();memset(host,0x35,4096);ggml_backend_buffer_t wrapped=ggml_backend_dev_buffer_from_host_ptr(dev,host,4096,1024);if(!wrapped)abort();ggml_backend_buffer_free(wrapped);record("wrapped-host-data",host,64,GGML_TYPE_I8);free(host);
 for(int op=0;op<GGML_OP_COUNT;++op){uint8_t values[1024];size_t at=0;for(size_t kind=0;kind<sizeof(kinds)/sizeof(*kinds);++kind)for(int variant=0;variant<8;++variant){enum ggml_type ys[]={GGML_TYPE_F32,GGML_TYPE_F16,GGML_TYPE_BF16,GGML_TYPE_Q8_0};struct ggml_tensor a=shape(kinds[kind]),b=shape(ys[variant%4]),third=shape(GGML_TYPE_F32),t=shape(variant==1?GGML_TYPE_IQ3_XXS:variant==2?kinds[kind]:variant==3?GGML_TYPE_F16:GGML_TYPE_F32);if(variant==2||variant==4)a.ne[2]=2;if(variant==3)a.nb[1]+=64;third.ne[0]=variant==1?1:16;t.op=op;t.src[0]=&a;t.src[1]=&b;t.src[2]=&third;t.src[3]=&third;ggml_set_op_params_i32(&t,0,variant==0?1:3);if(variant==5||variant==6)ggml_set_op_params_i32(&t,1,GGML_HINT_SRC0_IS_HADAMARD);if(op==GGML_OP_SOFT_MAX_BACK){float bias=variant==0?-0.f:variant==7?NAN:variant==4?.5f:0.f;memcpy((char*)t.op_params+4,&bias,4);}values[at++]=ggml_backend_dev_supports_op(dev,&t);if(at>=sizeof(values))abort();}char name[64];snprintf(name,sizeof(name),"capability-op%d",op);record(name,values,at,GGML_TYPE_I8);}
 for(int threads=1;threads<=16;threads=threads==1?2:threads==2?8:32){graph_cases(reg,threads);if(threads==8)graph_cases(reg,16);}
 fclose(out);return 0;
}
