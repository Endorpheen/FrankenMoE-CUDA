#include "ggml.h"
#include "ggml-cpu.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t rng=20261107;
static float random_value(void){rng=rng*1664525u+1013904223u;return ((int32_t)(rng>>8)-8388608)*0.000001f;}
static uint64_t cases,values;
static int fixture_experts=17;
static void run(FILE *file,enum ggml_type type,int width,int output,int tokens,int slots,int threads) {
 struct ggml_init_params params={128u*1024*1024,NULL,false};struct ggml_context *ctx=ggml_init(params);if(!ctx)abort();
 const int experts=fixture_experts,used=4;
 struct ggml_tensor *w=ggml_new_tensor_3d(ctx,type,width,output,experts);
 size_t nw=(size_t)width*output*experts;float *wf=malloc(nw*4);if(!wf)abort();for(size_t i=0;i<nw;++i)wf[i]=random_value();
 const size_t written=ggml_quantize_chunk(type,wf,w->data,0,(int64_t)output*experts,width,NULL);if(written!=ggml_nbytes(w))abort();free(wf);
 struct ggml_tensor *x=ggml_new_tensor_3d(ctx,GGML_TYPE_F32,width,slots,tokens);for(int64_t i=0;i<ggml_nelements(x);++i)((float *)x->data)[i]=random_value();
 struct ggml_tensor *ids=ggml_new_tensor_2d(ctx,GGML_TYPE_I32,used,tokens);for(int t=0;t<tokens;++t)for(int j=0;j<used;++j)((int32_t *)ids->data)[t*used+j]=(t*13+j*7)%experts;
 struct ggml_tensor *out=ggml_mul_mat_id(ctx,w,x,ids);struct ggml_cgraph *g=ggml_new_graph_custom(ctx,32,false);ggml_build_forward_expand(g,out);
 struct ggml_threadpool_params tp=ggml_threadpool_params_default(threads);tp.poll=0;
 struct ggml_threadpool *pool=ggml_threadpool_new(&tp);if(!pool)abort();
 struct ggml_cplan plan=ggml_graph_plan(g,threads,pool);plan.work_data=malloc(plan.work_size);if(plan.work_size&&!plan.work_data)abort();
 enum ggml_status status=ggml_graph_compute(g,&plan);
 float *expected=malloc(ggml_nbytes(out));if(!expected)abort();memcpy(expected,out->data,ggml_nbytes(out));
 for(int warm=0;warm<2;++warm)if(ggml_graph_compute(g,&plan)!=GGML_STATUS_SUCCESS)abort();
 for(int repeat=0;repeat<5;++repeat){int64_t begin=ggml_time_us();for(int pass=0;pass<20;++pass)if(ggml_graph_compute(g,&plan)!=GGML_STATUS_SUCCESS)abort();double us=(ggml_time_us()-begin)/20.0;if(memcmp(expected,out->data,ggml_nbytes(out)))abort();printf("{\"kind\":\"operation\",\"type\":%d,\"width\":%d,\"output\":%d,\"tokens\":%d,\"slots\":%d,\"threads\":%d,\"repeat\":%d,\"microseconds\":%.9g,\"preliminary\":true}\n",type,width,output,tokens,slots,threads,repeat,us);}
 free(expected);free(plan.work_data);ggml_threadpool_free(pool);
if(status!=GGML_STATUS_SUCCESS){fprintf(stderr,"graph compute failed: %d\n",status);abort();}
 uint64_t bytes=ggml_nbytes(out);if(fwrite(&bytes,8,1,file)!=1||fwrite(out->data,1,(size_t)bytes,file)!=bytes)abort();++cases;values+=bytes/4;ggml_free(ctx);
}
int main(int argc,char **argv){if(argc!=2)return 2;ggml_cpu_init();FILE *file=fopen(argv[1],"wb");if(!file)return 3;int sequence=0;int widths[]={256,512,2048};int token_counts[]={1,7,128};int thread_counts[]={1,2,8};int outputs[]={1,17,65};int slots[]={1,2,4};
 for(int type=0;type<2;++type)for(int w=0;w<3;++w)for(int t=0;t<3;++t)for(int h=0;h<3;++h){int i=sequence++;run(file,type?GGML_TYPE_Q6_K:GGML_TYPE_Q4_K,widths[w],outputs[i%3],token_counts[t],slots[(i/3)%3],thread_counts[h]);}
 fixture_experts=5;for(int type=0;type<2;++type)for(int thread=0;thread<2;++thread)run(file,type?GGML_TYPE_Q6_K:GGML_TYPE_Q4_K,256,4097,1,1,thread?8:2);
 fclose(file);printf("{\"graph_cases\":%llu,\"values\":%llu,\"actual_ggml_compute\":true}\n",(unsigned long long)cases,(unsigned long long)values);return 0;}
