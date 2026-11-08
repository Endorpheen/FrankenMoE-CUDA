#include "ggml.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <math.h>
#include <sys/resource.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"fixture failure at %d: %s\n",__LINE__,#x);exit(2);}}while(0)
struct record {uint64_t values[32+GGML_MAX_SRC];unsigned char params[GGML_MAX_OP_PARAMS];char name[GGML_MAX_NAME];};
struct snapshot {struct record records[96];unsigned count;int result;};
static struct ggml_tensor* leaf(struct ggml_context*c,enum ggml_type ty,int64_t a,int64_t b,int64_t d,int64_t e){struct ggml_tensor*t=ggml_new_tensor_4d(c,ty,a,b,d,e);ggml_set_name(t,"fixture input");return t;}
static int index_of(struct ggml_tensor**all,unsigned count,const struct ggml_tensor*t){if(!t)return -1;for(unsigned i=0;i<count;i++)if(all[i]==t)return i;return -2;}
static void capture(struct ggml_context*c,struct ggml_tensor*t,struct snapshot*s){
 struct ggml_tensor*all[96];memset(s,0,sizeof(*s));for(struct ggml_tensor*p=ggml_get_first_tensor(c);p;p=ggml_get_next_tensor(c,p)){CHECK(s->count<96);all[s->count++]=p;}s->result=index_of(all,s->count,t);
 for(unsigned i=0;i<s->count;i++){struct ggml_tensor*p=all[i];struct record*r=s->records+i;unsigned k=0;r->values[k++]=p->type;r->values[k++]=p->op;r->values[k++]=p->flags;r->values[k++]=p->view_offs;r->values[k++]=index_of(all,s->count,p->view_src);r->values[k++]=p->data!=NULL;r->values[k++]=ggml_nbytes(p);r->values[k++]=ggml_nelements(p);r->values[k++]=ggml_nrows(p);
  for(int j=0;j<4;j++){r->values[k++]=p->ne[j];r->values[k++]=p->nb[j];}for(int j=0;j<GGML_MAX_SRC;j++)r->values[k++]=index_of(all,s->count,p->src[j]);
  if(p->data)r->values[k++]=(uintptr_t)p->data-(uintptr_t)ggml_get_mem_buffer(c);
  CHECK(k<=sizeof(r->values)/sizeof(*r->values));memcpy(r->params,p->op_params,sizeof(r->params));memcpy(r->name,p->name,sizeof(r->name));
  if(p->view_src&&p->data)CHECK(p->data==(char*)p->view_src->data+p->view_offs);
 }
}
static struct ggml_tensor* build(struct ggml_context*c,int mode,int variant){
 struct ggml_tensor*a=leaf(c,GGML_TYPE_F32,8,4,2,2),*b=leaf(c,GGML_TYPE_F32,8,1,1,1),*d=NULL,*t=NULL;
 float eps=variant==0?1e-6f:variant==1?-0.0f:NAN;
 switch(mode){
 case 0:return ggml_dup_tensor(c,a);
 case 1:return ggml_view_tensor(c,a);
 case 2:return variant?ggml_add_inplace(c,a,b):ggml_add(c,a,b);
 case 3:return variant?ggml_sub_inplace(c,a,b):ggml_sub(c,a,b);
 case 4:return variant?ggml_mul_inplace(c,a,b):ggml_mul(c,a,b);
 case 5:return variant?ggml_div_inplace(c,a,b):ggml_div(c,a,b);
 case 6:return variant==0?ggml_silu(c,a):variant==1?ggml_sigmoid_inplace(c,a):ggml_softplus(c,a);
 case 7:return variant?ggml_norm_inplace(c,a,eps):ggml_norm(c,a,eps);
 case 8:return variant?ggml_rms_norm_inplace(c,a,eps):ggml_rms_norm(c,a,eps);
 case 9:return variant?ggml_l2_norm_inplace(c,a,eps):ggml_l2_norm(c,a,eps);
 case 10:b=leaf(c,GGML_TYPE_F32,8,4,2,2);return variant==0?ggml_swiglu(c,a):variant==1?ggml_swiglu_swapped(c,a):ggml_swiglu_split(c,a,b);
 case 11:a=leaf(c,GGML_TYPE_Q4_K,256,16,1,1);b=leaf(c,GGML_TYPE_F32,256,3,2,2);return ggml_mul_mat(c,a,b);
 case 12:a=leaf(c,GGML_TYPE_Q4_K,256,16,256,1);b=leaf(c,GGML_TYPE_F32,256,variant?8:1,3,1);d=leaf(c,GGML_TYPE_I32,8,3,1,1);return ggml_mul_mat_id(c,a,b,d);
 case 13:return ggml_sum(c,a);
 case 14:return ggml_sum_rows(c,a);
 case 15:return ggml_mean(c,a);
 case 16:a=leaf(c,GGML_TYPE_F32,2,1,1,1);return ggml_repeat(c,a,b);
 case 17:a=leaf(c,GGML_TYPE_F32,2,1,1,1);return ggml_repeat_4d(c,a,8,4,2,2);
 case 18:return ggml_concat(c,a,a,variant);
 case 19:return variant?ggml_scale_bias_inplace(c,a,.5f,-1.25f):ggml_scale_bias(c,a,.5f,-1.25f);
 case 20:return variant?ggml_clamp_inplace(c,a,-3.5f,4.25f):ggml_clamp(c,a,-3.5f,4.25f);
 case 21:b=leaf(c,GGML_TYPE_F16,8,4,2,2);if(variant)ggml_set_name(b,"");return ggml_cpy(c,a,b);
 case 22:return ggml_cast(c,a,variant?GGML_TYPE_F16:GGML_TYPE_F32);
 case 23:return ggml_cont(c,ggml_transpose(c,a));
 case 24:return ggml_cont_4d(c,a,16,4,2,1);
 case 25:b=leaf(c,GGML_TYPE_F32,16,4,2,1);return ggml_reshape(c,a,b);
 case 26:return ggml_reshape_1d(c,a,128);
 case 27:return ggml_reshape_2d(c,a,16,8);
 case 28:return ggml_reshape_3d(c,a,16,4,2);
 case 29:return ggml_reshape_4d(c,a,4,4,4,2);
 case 30:return ggml_view_1d(c,a,4,variant*4);
 case 31:return ggml_view_2d(c,a,4,4,32,variant*4);
 case 32:return ggml_view_3d(c,a,4,4,2,32,128,variant*4);
 case 33:return ggml_view_4d(c,a,4,4,2,2,32,128,256,variant*4);
 case 34:return variant?ggml_permute(c,a,0,2,1,3):ggml_permute(c,a,3,2,0,1);
 case 35:return ggml_transpose(c,a);
 case 36:if(variant==2)a=leaf(c,GGML_TYPE_I32,8,4,2,2);d=leaf(c,GGML_TYPE_I32,3,2,2,1);return ggml_get_rows(c,a,d);
 case 37:a=leaf(c,GGML_TYPE_F16,8,8,2,2);b=leaf(c,GGML_TYPE_F32,8,4,2,2);d=leaf(c,variant?GGML_TYPE_I32:GGML_TYPE_I64,4,1,1,1);return ggml_set_rows(c,a,b,d);
 case 38:b=leaf(c,GGML_TYPE_F16,8,4,1,1);return variant?ggml_soft_max_ext_inplace(c,a,b,.125f,4):ggml_soft_max_ext(c,a,NULL,.125f,0);
 case 39:{int sections[4]={2,2,2,2};b=leaf(c,GGML_TYPE_I32,variant?8:2,1,1,1);d=variant==2?leaf(c,GGML_TYPE_F32,4,1,1,1):NULL;return variant?ggml_rope_multi(c,a,b,d,8,sections,GGML_ROPE_TYPE_MROPE,32768,1000000,1,1,.75f,32,1):ggml_rope_ext(c,a,b,d,8,0,32768,1000000,1,1,.75f,32,1);}
 case 40:return ggml_argsort(c,a,variant?GGML_SORT_ORDER_ASC:GGML_SORT_ORDER_DESC);
 case 41:return ggml_top_k(c,a,variant?1:8);
 case 42:a=leaf(c,GGML_TYPE_F32,6,16,2,1);b=leaf(c,GGML_TYPE_F32,4,16,1,1);return ggml_ssm_conv(c,a,b);
 case 43:{a=leaf(c,GGML_TYPE_F32,8,2,3,2);b=leaf(c,GGML_TYPE_F32,8,2,3,2);d=leaf(c,GGML_TYPE_F32,8,4,3,2);struct ggml_tensor*g=leaf(c,GGML_TYPE_F32,variant?8:1,4,3,2),*beta=leaf(c,GGML_TYPE_F32,1,4,3,2),*state=leaf(c,GGML_TYPE_F32,8,8,4,2);return ggml_gated_delta_net(c,a,b,d,g,beta,state,variant+1);}
 case 44:{a=leaf(c,GGML_TYPE_F32,8,3,4,2);b=leaf(c,GGML_TYPE_F16,8,5,2,2);d=leaf(c,GGML_TYPE_F16,6,5,2,2);struct ggml_tensor*mask=variant?leaf(c,GGML_TYPE_F16,5,3,1,1):NULL;return ggml_flash_attn_ext(c,a,b,d,mask,.125f,variant?4:0,10);}
 case 45:return variant?ggml_fill_inplace(c,a,eps):ggml_fill(c,a,eps);
 case 46:return ggml_cumsum(c,a);
 case 47:a=leaf(c,GGML_TYPE_F32,8,1,2,2);return ggml_diag(c,a);
 case 48:a=leaf(c,GGML_TYPE_F32,8,8,2,2);return ggml_tri(c,a,variant?GGML_TRI_TYPE_UPPER:GGML_TRI_TYPE_LOWER_DIAG);
 case 49:a=leaf(c,GGML_TYPE_F32,8,8,2,2);b=leaf(c,GGML_TYPE_F32,3,8,2,2);return ggml_solve_tri(c,a,b,true,true,false);
 case 50:return variant==2?ggml_pad_ext(c,a,-1,0,0,0,0,0,0,0):ggml_pad_ext(c,a,1,2,3,4,1,0,0,1);
 case 51:return variant?ggml_set_inplace(c,a,b,32,128,256,4):ggml_set(c,a,b,32,128,256,4);
 case 52:b=leaf(c,GGML_TYPE_F32,8,256,1,1);d=leaf(c,GGML_TYPE_I32,4,2,1,1);return ggml_add_id(c,a,b,d);
 case 53:return variant?ggml_dup_inplace(c,a):ggml_dup(c,a);
 case 54:return variant?ggml_sqr_inplace(c,a):ggml_sqr(c,a);
 case 55:return variant?ggml_sqrt_inplace(c,a):ggml_sqrt(c,a);
 case 56:return ggml_argsort_top_k(c,a,variant?1:8);
 case 57:b=leaf(c,GGML_TYPE_F32,8,3,2,2);t=ggml_mul_mat(c,a,b);CHECK(ggml_prec_set_acc(t,GGML_PREC_F32));return t;
 case 58:b=leaf(c,GGML_TYPE_F32,8,3,2,2);t=ggml_mul_mat(c,a,b);CHECK(ggml_prec_set_src(t,GGML_PREC_F32,1));CHECK(!ggml_prec_set_src(t,GGML_PREC_F32,0));return t;
 case 59:b=leaf(c,GGML_TYPE_F32,8,3,2,2);t=ggml_mul_mat(c,a,b);ggml_mul_mat_set_prec(t,GGML_PREC_F32);return t;
 case 60:b=leaf(c,GGML_TYPE_F32,8,3,2,2);t=ggml_mul_mat(c,a,b);ggml_mul_mat_set_hint(t,GGML_HINT_SRC0_IS_HADAMARD);return t;
 case 61:case 62:case 64:{a=leaf(c,GGML_TYPE_F32,8,3,4,1);b=leaf(c,GGML_TYPE_F16,8,5,2,1);d=leaf(c,GGML_TYPE_F16,6,5,2,1);t=ggml_flash_attn_ext(c,a,b,d,NULL,.125f,0,0);if(mode==61)ggml_flash_attn_ext_set_prec(t,GGML_PREC_F32);else if(mode==62)ggml_flash_attn_ext_set_n_kv_max(t,123);else{struct ggml_tensor*s=leaf(c,GGML_TYPE_F32,4,1,1,1);ggml_flash_attn_ext_add_sinks(t,variant?s:NULL);}return t;}
 case 63:t=ggml_soft_max(c,a);b=leaf(c,GGML_TYPE_F32,2,1,1,1);ggml_soft_max_add_sinks(t,variant?b:NULL);return t;
 case 65:b=leaf(c,GGML_TYPE_F32,1,1,1,1);return variant?ggml_add1_inplace(c,a,b):ggml_add1(c,a,b);
 case 66:return variant?ggml_acc_inplace(c,a,b,32,128,256,4):ggml_acc(c,a,b,32,128,256,4);
 }
 CHECK(0);return NULL;
}
static void negative(int kind){struct ggml_context*c=ggml_init((struct ggml_init_params){1048576,NULL,true});struct ggml_tensor*a=leaf(c,GGML_TYPE_F32,8,4,2,2),*b=leaf(c,GGML_TYPE_F32,7,4,2,2),*d;
 switch(kind){
 case 0:ggml_add(c,a,b);break;
 case 1:ggml_mul_mat(c,a,b);break;
 case 2:ggml_view_1d(c,a,128,4);break;
 case 3:ggml_reshape_2d(c,a,3,7);break;
 case 4:ggml_permute(c,a,0,0,2,3);break;
 case 5:ggml_get_rows(c,a,b);break;
 case 6:ggml_set_rows(c,a,b,b);break;
 case 7:ggml_soft_max_ext(c,a,NULL,1,3);break;
 case 8:b=leaf(c,GGML_TYPE_I32,3,1,1,1);ggml_rope_ext(c,a,b,NULL,8,0,1,10000,1,1,1,32,1);break;
 case 9:ggml_top_k(c,a,9);break;
 case 10:b=leaf(c,GGML_TYPE_F32,16,4,1,1);ggml_ssm_conv(c,a,b);break;
 case 11:d=leaf(c,GGML_TYPE_F32,8,2,3,2);ggml_gated_delta_net(c,d,d,d,d,d,d,1);break;
 case 12:ggml_flash_attn_ext(c,a,a,a,b,1,0,0);break;
 case 13:a=leaf(c,GGML_TYPE_F32,8,8,1,1);ggml_solve_tri(c,a,a,false,true,false);break;
 case 14:ggml_mul_mat_set_prec(a,GGML_PREC_F32);break;
 case 15:ggml_prec_set_src(a,GGML_PREC_F32,GGML_MAX_SRC);break;
 }
 ggml_free(c);_exit(0);
}
int main(void){struct rlimit limit={0,0};CHECK(setrlimit(RLIMIT_CORE,&limit)==0);setenv("GGML_NO_BACKTRACE","1",1);uint64_t hash=1469598103934665603ULL;unsigned cases=0;for(int mode=0;mode<67;mode++)for(int no_alloc=0;no_alloc<2;no_alloc++)for(int variant=0;variant<3;variant++){
 struct snapshot native,goose;setenv("GGML_GOOSE_APEX_OPS","0",1);struct ggml_context*c=ggml_init((struct ggml_init_params){33554432,NULL,no_alloc});capture(c,build(c,mode,variant),&native);ggml_free(c);
 setenv("GGML_GOOSE_APEX_OPS","1",1);c=ggml_init((struct ggml_init_params){33554432,NULL,no_alloc});capture(c,build(c,mode,variant),&goose);ggml_free(c);
 if(memcmp(&native,&goose,sizeof(native))){fprintf(stderr,"DIFFERENTIAL MISMATCH mode=%d no_alloc=%d variant=%d tensors=%u/%u result=%d/%d\n",mode,no_alloc,variant,native.count,goose.count,native.result,goose.result);for(unsigned i=0;i<native.count&&i<goose.count;i++)if(memcmp(native.records+i,goose.records+i,sizeof(struct record))){fprintf(stderr,"tensor=%u native name=%s goose name=%s\n",i,native.records[i].name,goose.records[i].name);for(unsigned j=0;j<32+GGML_MAX_SRC;j++)if(native.records[i].values[j]!=goose.records[i].values[j])fprintf(stderr,"value[%u]=%llu/%llu\n",j,(unsigned long long)native.records[i].values[j],(unsigned long long)goose.records[i].values[j]);for(unsigned j=0;j<GGML_MAX_OP_PARAMS;j++)if(native.records[i].params[j]!=goose.records[i].params[j])fprintf(stderr,"param[%u]=%u/%u\n",j,native.records[i].params[j],goose.records[i].params[j]);}return 3;}
 const unsigned char*p=(const unsigned char*)&goose;for(size_t i=0;i<sizeof(goose);i++){hash^=p[i];hash*=1099511628211ULL;}cases++;
 }
 for(int kind=0;kind<16;kind++)for(int enabled=0;enabled<2;enabled++){pid_t pid=fork();CHECK(pid>=0);if(pid==0){freopen("/dev/null","w",stderr);setenv("GGML_GOOSE_APEX_OPS",enabled?"1":"0",1);negative(kind);}int status;CHECK(waitpid(pid,&status,0)==pid);CHECK(WIFSIGNALED(status)&&WTERMSIG(status)==SIGABRT);}
 printf("APEX_OPS_DIFFERENTIAL_OK entries=67 cases=%u invalid_pairs=16 hash=%016llx\n",cases,(unsigned long long)hash);return 0;
}
