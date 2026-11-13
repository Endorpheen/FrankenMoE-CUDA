#include "ggml.h"
#include "ggml-cpu.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ggml_tensor*(*unary_fn)(struct ggml_context*,struct ggml_tensor*);
static unary_fn unary_functions[]={ggml_abs,ggml_sgn,ggml_neg,ggml_step,ggml_tanh,ggml_elu,ggml_relu,ggml_sigmoid,ggml_hardsigmoid,ggml_exp,ggml_hardswish,ggml_sqr,ggml_sqrt,ggml_sin,ggml_cos,ggml_log,ggml_expm1,ggml_softplus,ggml_floor,ggml_ceil,ggml_round,ggml_trunc,NULL};
static struct ggml_threadpool *pools[3];
struct storage {unsigned char*raw;size_t bytes;unsigned char*copy;};
static enum ggml_type types[]={GGML_TYPE_F32,GGML_TYPE_F16,GGML_TYPE_BF16};
static uint32_t rng=20261107;
static uint64_t cases,values;
static struct storage allocate(struct ggml_tensor*t){
 struct storage s={0};s.bytes=ggml_nbytes(t);size_t total=(s.bytes+128+63)/64*64;
 s.raw=aligned_alloc(64,total);s.copy=malloc(s.bytes);if(!s.raw||!s.copy)abort();
 memset(s.raw,0xa5,total);t->data=s.raw+64;return s;
}
static void check_guard(struct storage*s){for(int i=0;i<64;++i)if(s->raw[i]!=0xa5||s->raw[64+s->bytes+i]!=0xa5)abort();}
static void layout(struct ggml_tensor*t,int pad,int strided){
 size_t size=ggml_type_size(t->type);t->nb[0]=size*(strided?2:1);
 t->nb[1]=t->nb[0]*t->ne[0]+(pad?(pad==2?size:8*size):0);
 t->nb[2]=t->nb[1]*t->ne[1]+(pad?(pad==2?size:8*size):0);
 t->nb[3]=t->nb[2]*t->ne[2]+(pad?(pad==2?size:8*size):0);
}
static void fill(struct ggml_tensor*t,int edge,int input){
 static uint32_t patterns[]={0x00000000,0x80000000,0x3f800000,0xbf800000,0x7f800000,0xff800000,0x7fc12345,0xffc23456,0x7f812345,0x00000001,0x80000001,0x3f000000,0xbf000000,0x41a00000,0x41a00001,0x41a00002,0xc1a00000,0xc1a00001,0x00800000,0x7f7fffff};
 for(int64_t z=0;z<t->ne[3];++z)for(int64_t y=0;y<t->ne[2];++y)for(int64_t x=0;x<t->ne[1];++x)for(int64_t lane=0;lane<t->ne[0];++lane){
  float value;if(edge){uint32_t bits=patterns[(lane+x*3+y*7+z*11+input*5)%(sizeof(patterns)/sizeof(patterns[0]))];memcpy(&value,&bits,4);}else{rng=rng*1664525u+1013904223u;value=((int32_t)(rng>>8)-8388608)*0.000002f;}
  unsigned char*p=(unsigned char*)t->data+z*t->nb[3]+y*t->nb[2]+x*t->nb[1]+lane*t->nb[0];
  if(t->type==GGML_TYPE_F32)memcpy(p,&value,4);
  else if(t->type==GGML_TYPE_F16){uint16_t h=ggml_fp32_to_fp16(value);memcpy(p,&h,2);}
  else {ggml_bf16_t h=ggml_fp32_to_bf16(value);memcpy(p,&h,2);}
 }
}
static void run(FILE*f,int mode,int op,int format,int profile){
 static int unary_formats[][2]={{0,0},{1,1},{2,2},{2,0},{1,0}};
 static int binary_formats[][3]={{0,0,0},{1,1,1},{2,2,2},{2,0,2},{2,0,0},{1,0,1},{1,0,0}};
 const int ak=mode?binary_formats[format][0]:unary_formats[format][0];
 const int bk=mode?binary_formats[format][1]:0;
 const int dk=mode?binary_formats[format][2]:unary_formats[format][1];
 int widths[]={1,7,32,257},threads_list[]={1,2,8};
 int width=profile==8||profile==9?4096:widths[profile%4],pad=profile==10?2:profile%2,strided=mode&&profile%4==3;
 if(!mode&&op==22)pad=0;
 int thread_index=(op+format+profile)%3;
 int inplace=(ak==dk&&profile%4==2);
 struct ggml_context*ctx=ggml_init((struct ggml_init_params){4*1024*1024,NULL,true});if(!ctx)abort();
 struct ggml_tensor*a=ggml_new_tensor_4d(ctx,types[ak],width,3,2,2);layout(a,pad,0);
 struct storage sa=allocate(a);fill(a,profile>=4&&profile<8,0);memcpy(sa.copy,a->data,sa.bytes);
 struct ggml_tensor*b=NULL,*out=NULL;struct storage sb={0},sd={0};
 if(mode){
  int bwidth=profile%4==0?1:(width%4==0?width/4:width);
  b=ggml_new_tensor_4d(ctx,types[bk],bwidth,profile%4==1?1:3,1,profile%4==3?1:2);layout(b,pad,strided);
  sb=allocate(b);fill(b,profile>=4&&profile<8,1);memcpy(sb.copy,b->data,sb.bytes);
  if(op==0)out=ggml_add(ctx,a,b);else if(op==1)out=ggml_sub(ctx,a,b);else if(op==2)out=ggml_mul(ctx,a,b);else out=ggml_div(ctx,a,b);
 }else if(op==22)out=ggml_xielu(ctx,a,0.75f,1.25f,0.5f,-0.0001f);
 else out=unary_functions[op](ctx,a);
 out->type=types[dk];layout(out,pad,0);
 if(inplace){memcpy(out->nb,a->nb,sizeof(out->nb));out->data=a->data;}else sd=allocate(out);
 struct ggml_cgraph*g=ggml_new_graph_custom(ctx,16,false);ggml_build_forward_expand(g,out);
 struct ggml_cplan plan=ggml_graph_plan(g,threads_list[thread_index],pools[thread_index]);plan.work_data=malloc(plan.work_size);
 if(plan.work_size&&!plan.work_data)abort();
 enum ggml_status status=ggml_graph_compute(g,&plan);if(status!=GGML_STATUS_SUCCESS)abort();
 check_guard(&sa);if(b){check_guard(&sb);if(memcmp(sb.copy,b->data,sb.bytes))abort();}
 if(!inplace){check_guard(&sd);if(memcmp(sa.copy,a->data,sa.bytes))abort();}
 uint64_t bytes=ggml_nbytes(out);if(fwrite(&bytes,8,1,f)!=1||fwrite(out->data,1,bytes,f)!=bytes)abort();
 printf("{\"case\":%llu,\"mode\":%d,\"op\":%d,\"format\":%d,\"profile\":%d,\"width\":%d,\"threads\":%d,\"inplace\":%d,\"bytes\":%llu}\n",(unsigned long long)cases,mode,op,format,profile,width,threads_list[thread_index],inplace,(unsigned long long)bytes);
 values+=ggml_nelements(out);cases++;
 free(plan.work_data);free(sa.raw);free(sa.copy);if(b){free(sb.raw);free(sb.copy);}if(!inplace){free(sd.raw);free(sd.copy);}ggml_free(ctx);
}
int main(int argc,char**argv){
 if(argc!=2)return 2;ggml_cpu_init();FILE*f=fopen(argv[1],"wb");if(!f)return 3;
 int ts[]={1,2,8};for(int i=0;i<3;++i){struct ggml_threadpool_params p=ggml_threadpool_params_default(ts[i]);p.poll=0;pools[i]=ggml_threadpool_new(&p);if(!pools[i])abort();}
 for(int mode=0;mode<2;++mode)for(int op=0;op<(mode?4:23);++op)for(int format=0;format<(mode?7:5);++format)for(int profile=0;profile<8;++profile)run(f,mode,op,format,profile);
 for(int mode=0;mode<2;++mode)for(int op=0;op<(mode?4:23);++op)for(int profile=8;profile<11;++profile)run(f,mode,op,0,profile);
 fclose(f);for(int i=0;i<3;++i)ggml_threadpool_free(pools[i]);
 printf("{\"graph_cases\":%llu,\"values\":%llu,\"actual_ggml_compute\":true,\"input_preserved_when_out_of_place\":true,\"canaries_preserved\":true}\n",(unsigned long long)cases,(unsigned long long)values);return 0;
}
