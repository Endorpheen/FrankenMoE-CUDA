#define GGML_COMMON_DECL_C
#include "ggml-common.h"
#include "ggml.h"
#include "ggml-cpu.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t state=20261107;
static uint32_t next(void){state=state*1664525u+1013904223u;return state;}
static enum ggml_type types[]={GGML_TYPE_Q2_0,GGML_TYPE_Q5_0,GGML_TYPE_IQ4_NL,GGML_TYPE_IQ2_XXS,GGML_TYPE_IQ2_S,GGML_TYPE_Q4_K,GGML_TYPE_Q6_K,GGML_TYPE_F32,GGML_TYPE_F16,GGML_TYPE_BF16};
static struct ggml_threadpool*pools[3];
static uint64_t cases,values;
static int padded_ids,prequant_input,window_override=-1,mask_style;
static void fill_weights(struct ggml_tensor*t){
 if(t->type==GGML_TYPE_Q4_K||t->type==GGML_TYPE_Q6_K||t->type==GGML_TYPE_F32||t->type==GGML_TYPE_F16||t->type==GGML_TYPE_BF16){
  int64_t n=ggml_nelements(t);float*source=malloc((size_t)n*4);if(!source)abort();for(int64_t i=0;i<n;++i)source[i]=((int32_t)(next()>>8)-8388608)*0.0000003f;
  size_t got=ggml_quantize_chunk(t->type,source,t->data,0,t->ne[1]*t->ne[2],t->ne[0],NULL);if(got!=ggml_nbytes(t))abort();free(source);return;
 }
 size_t bytes=ggml_nbytes(t),block=ggml_type_size(t->type);for(size_t i=0;i<bytes;++i)((uint8_t*)t->data)[i]=(uint8_t)(next()>>24);
 for(size_t i=0;i<bytes/block;++i){uint16_t d=ggml_fp32_to_fp16(i%2?0.0625f:-0.125f);memcpy((char*)t->data+i*block,&d,2);}
}
static void write_output(FILE*f,struct ggml_tensor*out){uint64_t n=ggml_nbytes(out);if(fwrite(&n,8,1,f)!=1||fwrite(out->data,1,n,f)!=n)abort();cases++;values+=ggml_nelements(out);}
static void run_graph(struct ggml_context*ctx,struct ggml_tensor*out,int thread_index,int window){
 struct ggml_cgraph*g=ggml_new_graph_custom(ctx,32,false);ggml_build_forward_expand(g,out);
 int ts[]={1,2,8};struct ggml_cplan plan=ggml_graph_plan(g,ts[thread_index],pools[thread_index]);plan.work_data=malloc(plan.work_size);if(plan.work_size&&!plan.work_data)abort();
 memset(out->data,0xa5,ggml_nbytes(out));ggml_cpu_iq2s_fused_set_window(window);
 if(ggml_graph_compute(g,&plan)!=GGML_STATUS_SUCCESS)abort();
 ggml_cpu_iq2s_fused_set_window(false);free(plan.work_data);
}
static void moe(FILE*f,int kind,int tokens,int slots,int mode,int sequence){
 struct ggml_context*ctx=ggml_init((struct ggml_init_params){64u*1024*1024,NULL,false});if(!ctx)abort();
 int width=kind==7?33:kind>=8?64:kind<3?640:2560,outputs[]={1,17,33};int output=outputs[sequence%3];
 struct ggml_tensor*w=ggml_new_tensor_3d(ctx,types[kind],width,output,mode==2?3:5);fill_weights(w);
 struct ggml_tensor*x=ggml_new_tensor_3d(ctx,GGML_TYPE_F32,width,slots,tokens);for(int64_t i=0;i<ggml_nelements(x);++i)((float*)x->data)[i]=((int32_t)(next()>>8)-8388608)*0.0000003f;
 if(prequant_input){const struct ggml_type_traits_cpu*traits=ggml_get_type_traits_cpu(w->type);enum ggml_type target=traits->vec_dot_type;if(target!=GGML_TYPE_F32){
  struct ggml_tensor*q=ggml_new_tensor_3d(ctx,target,width,slots,tokens);ggml_from_float_t convert=ggml_get_type_traits_cpu(target)->from_float;if(!convert)abort();
  for(int t=0;t<tokens;++t)for(int j=0;j<slots;++j)convert((const float*)((char*)x->data+t*x->nb[2]+j*x->nb[1]),(char*)q->data+t*q->nb[2]+j*q->nb[1],width);x=q;
 }}
 struct ggml_tensor*ids_storage=ggml_new_tensor_2d(ctx,GGML_TYPE_I32,padded_ids?8:4,tokens);memset(ids_storage->data,0x5a,ggml_nbytes(ids_storage));
 struct ggml_tensor*ids=padded_ids?ggml_view_2d(ctx,ids_storage,4,tokens,ids_storage->nb[1],0):ids_storage;
 for(int t=0;t<tokens;++t)for(int j=0;j<4;++j)*(int32_t*)((char*)ids->data+t*ids->nb[1]+j*ids->nb[0])=j;
 struct ggml_tensor*out;
 if(mode){
  struct ggml_tensor*mask=ggml_new_tensor_1d(ctx,GGML_TYPE_I32,5);int32_t m[]={1,1,0,1,0};if(mask_style)for(int i=0;i<5;++i)m[i]=mask_style==1?0:1;memcpy(mask->data,m,sizeof(m));
  if(mode==2){struct ggml_tensor*lut=ggml_new_tensor_1d(ctx,GGML_TYPE_I32,5);int32_t l[]={2,1,-1,0,-1};if(mask_style==2)for(int i=0;i<5;++i)l[i]=0;memcpy(lut->data,l,sizeof(l));out=ggml_mul_mat_id_cold_bank(ctx,w,x,ids,mask,lut);}
  else out=ggml_mul_mat_id_cold(ctx,w,x,ids,mask);
 }else out=ggml_mul_mat_id(ctx,w,x,ids);
 int window=window_override>=0?window_override:kind==4&&!mode&&(tokens==2||tokens==3);
 run_graph(ctx,out,sequence%3,window);write_output(f,out);ggml_free(ctx);
}
static void rows(FILE*f,int kind,int profile){
 struct ggml_context*ctx=ggml_init((struct ggml_init_params){4u*1024*1024,NULL,false});if(!ctx)abort();
 int block=ggml_blck_size(types[kind]),width=profile==0?block:profile==1?block*3:profile==2?(kind<3?640:2560):(kind==2?160:5*block);
 struct ggml_tensor*w=ggml_new_tensor_2d(ctx,types[kind],width,7);fill_weights(w);struct ggml_tensor*ids=ggml_new_tensor_1d(ctx,GGML_TYPE_I32,4);int32_t selected[]={6,0,3,1};memcpy(ids->data,selected,sizeof(selected));
 struct ggml_tensor*out=ggml_get_rows(ctx,w,ids);run_graph(ctx,out,profile%3,0);write_output(f,out);ggml_free(ctx);
}
int main(int argc,char**argv){
 if(argc!=2)return 2;ggml_cpu_init();int ts[]={1,2,8};for(int i=0;i<3;++i){struct ggml_threadpool_params p=ggml_threadpool_params_default(ts[i]);p.poll=0;pools[i]=ggml_threadpool_new(&p);if(!pools[i])abort();}
 FILE*f=fopen(argv[1],"wb");if(!f)return 3;int tokens[]={1,2,3,7,16,64},slots[]={1,2,4},sequence=0;
 for(int kind=0;kind<5;++kind)for(int t=0;t<6;++t)for(int slot=0;slot<3;++slot)for(int mode=0;mode<3;++mode)moe(f,kind,tokens[t],slots[slot],mode,sequence++);
 for(int kind=0;kind<5;++kind)for(int profile=0;profile<4;++profile)rows(f,kind,profile);
 window_override=0;
 for(int kind=5;kind<10;++kind)for(int mode=0;mode<3;++mode)for(int q=0;q<2;++q)for(int pad=0;pad<2;++pad){prequant_input=q;padded_ids=pad;moe(f,kind,7,2,mode,sequence++);}
 padded_ids=1;
 for(int kind=0;kind<5;++kind)for(int mode=0;mode<3;++mode)for(int q=0;q<2;++q){prequant_input=q;moe(f,kind,7,2,mode,sequence++);}
 prequant_input=1;
 for(int mode=0;mode<3;++mode)for(int window=0;window<2;++window)for(int tok=2;tok<=3;++tok){window_override=window;moe(f,4,tok,2,mode,sequence++);}
 window_override=0;
 for(int kind=0;kind<5;++kind)for(int mode=1;mode<3;++mode)for(int style=1;style<=2;++style){mask_style=style;moe(f,kind,7,2,mode,sequence++);}
 fclose(f);for(int i=0;i<3;++i)ggml_threadpool_free(pools[i]);printf("{\"graph_cases\":%llu,\"values\":%llu,\"actual_ggml_compute\":true,\"coverage\":\"stock/cold/bank MoE, explicit fused verification windows, native tiled dispatch, GET_ROWS dequantization\"}\n",(unsigned long long)cases,(unsigned long long)values);return 0;
}
