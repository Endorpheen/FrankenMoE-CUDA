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
static int hot_routes,padded_input,prequant_input,zero_input;
extern void quantize_row_q8_K(const float*,void*,int64_t);
static void run(FILE *file,enum ggml_type type,int width,int output,int tokens,int slots,int threads) {
 struct ggml_init_params params={128u*1024*1024,NULL,false};struct ggml_context *ctx=ggml_init(params);if(!ctx)abort();
 const int experts=fixture_experts,used=4;
 struct ggml_tensor *w=ggml_new_tensor_3d(ctx,type,width,output,experts);
 size_t nw=(size_t)width*output*experts;float *wf=malloc(nw*4);if(!wf)abort();for(size_t i=0;i<nw;++i)wf[i]=random_value();
 const size_t written=ggml_quantize_chunk(type,wf,w->data,0,(int64_t)output*experts,width,NULL);if(written!=ggml_nbytes(w))abort();free(wf);
 struct ggml_tensor *storage=ggml_new_tensor_3d(ctx,GGML_TYPE_F32,width+(padded_input?16:0),slots,tokens);
 for(int64_t i=0;i<ggml_nelements(storage);++i)((float*)storage->data)[i]=zero_input?0.0f:random_value();
 struct ggml_tensor *x=padded_input?ggml_view_3d(ctx,storage,width,slots,tokens,storage->nb[1],storage->nb[2],0):storage;
 if(prequant_input){struct ggml_tensor*q=ggml_new_tensor_3d(ctx,GGML_TYPE_Q8_K,width,slots,tokens);for(int token=0;token<tokens;++token)for(int slot=0;slot<slots;++slot)quantize_row_q8_K((const float*)((const char*)x->data+token*x->nb[2]+slot*x->nb[1]),(char*)q->data+token*q->nb[2]+slot*q->nb[1],width);x=q;}

 struct ggml_tensor *ids=ggml_new_tensor_2d(ctx,GGML_TYPE_I32,used,tokens);for(int t=0;t<tokens;++t)for(int j=0;j<used;++j)((int32_t *)ids->data)[t*used+j]=hot_routes?j:(t*13+j*7)%experts;
 struct ggml_tensor *out=ggml_mul_mat_id(ctx,w,x,ids);struct ggml_cgraph *g=ggml_new_graph_custom(ctx,32,false);ggml_build_forward_expand(g,out);
 enum ggml_status status=ggml_graph_compute_with_ctx(ctx,g,threads);if(status!=GGML_STATUS_SUCCESS){fprintf(stderr,"graph compute failed: %d\n",status);abort();}
 uint64_t bytes=ggml_nbytes(out);if(fwrite(&bytes,8,1,file)!=1||fwrite(out->data,1,(size_t)bytes,file)!=bytes)abort();++cases;values+=bytes/4;ggml_free(ctx);
}
int main(int argc,char **argv){if(argc!=2)return 2;ggml_cpu_init();FILE *file=fopen(argv[1],"wb");if(!file)return 3;int sequence=0;int widths[]={256,512,2048};int token_counts[]={1,7,128};int thread_counts[]={1,2,8};int outputs[]={1,17,65};int slots[]={1,2,4};
 for(int type=0;type<2;++type)for(int w=0;w<3;++w)for(int t=0;t<3;++t)for(int h=0;h<3;++h){int i=sequence++;run(file,type?GGML_TYPE_Q6_K:GGML_TYPE_Q4_K,widths[w],outputs[i%3],token_counts[t],slots[(i/3)%3],thread_counts[h]);}
 fixture_experts=5;for(int type=0;type<2;++type)for(int thread=0;thread<2;++thread)run(file,type?GGML_TYPE_Q6_K:GGML_TYPE_Q4_K,256,4097,1,1,thread?8:2);
 fixture_experts=5;hot_routes=1;
 int tails[]={8,16,17,255,256,257,273};int wide[]={256,512,2048,4096};int outputs2[]={1,17,65,129};int profile=0;
 for(int type=0;type<2;++type)for(int wi=0;wi<4;++wi)for(int ti=0;ti<7;++ti){int n=profile++;padded_input=n%2;prequant_input=n%3==0;zero_input=n%7==0;run(file,type?GGML_TYPE_Q6_K:GGML_TYPE_Q4_K,wide[wi],outputs2[n%4],tails[ti],slots[n%3],thread_counts[(n/3)%3]);}
 fclose(file);printf("{\"graph_cases\":%llu,\"values\":%llu,\"actual_ggml_compute\":true}\n",(unsigned long long)cases,(unsigned long long)values);return 0;}
