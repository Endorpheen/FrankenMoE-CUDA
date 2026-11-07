#define GGML_COMMON_DECL_C
#include "ggml-common.h"
#include "ggml.h"
#include "ggml-cpu.h"
#include "ggml-cpu/quants.h"
#include "ggml-quants.h"
#include "matrix-bridge.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
typedef void(*dot_fn)(int,float*,size_t,const void*,size_t,const void*,size_t,int);
typedef void(*deq_fn)(const void*,float*,int64_t);
static dot_fn dots[]={ggml_vec_dot_q2_0_q8_0,ggml_vec_dot_q5_0_q8_0,ggml_vec_dot_iq4_nl_q8_0,ggml_vec_dot_iq2_xxs_q8_K,ggml_vec_dot_iq2_s_q8_K};
static deq_fn deqs[]={(deq_fn)dequantize_row_q2_0,(deq_fn)dequantize_row_q5_0,(deq_fn)dequantize_row_iq4_nl,(deq_fn)dequantize_row_iq2_xxs,(deq_fn)dequantize_row_iq2_s};
static int quant_size[]={64,32,32,256,256},block_size[]={18,22,18,66,82};
static atomic_uint_fast64_t cases,rows_checked,values_checked;
static uint32_t random_next(uint32_t*s){*s=*s*1664525u+1013904223u;return*s;}
struct buf {unsigned char*raw,*data;size_t size;};
static struct buf create(size_t n){struct buf b={malloc(n+128),NULL,n};if(!b.raw)abort();b.data=b.raw+64;memset(b.raw,0xa5,n+128);return b;}
static void guard(struct buf b){for(int i=0;i<64;++i)if(b.raw[i]!=0xa5||b.data[b.size+i]!=0xa5)abort();}
static void compare(int kind,int width,int nrows,int pattern,int mode,int slots,int columns,int contiguous,uint32_t seed){
 int blocks=width/quant_size[kind],yb=width/(kind<3?32:256),ys=kind<3?34:292;
 int first=mode?2:0,total_rows=mode?11:nrows,xstride=blocks+(mode?1:0);
 int ystride=yb+(mode&&!contiguous?1:0),yrows=mode?slots*3:1;
 struct buf x=create((size_t)xstride*total_rows*block_size[kind]);
 struct buf y=create((size_t)ystride*yrows*ys);
 for(size_t i=0;i<x.size;++i)x.data[i]=(unsigned char)(random_next(&seed)>>24);
 for(int row=0;row<total_rows;++row)for(int ib=0;ib<blocks;++ib){
  uint16_t h=ggml_fp32_to_fp16(pattern==1?0.0f:((ib+row)%2?-0.125f:0.0625f));
  memcpy(x.data+(row*xstride+ib)*block_size[kind],&h,2);
  if(pattern>=2)memset(x.data+(row*xstride+ib)*block_size[kind]+2,pattern==2?0:pattern==3?255:pattern==4?85:170,block_size[kind]-2);
 }
 for(int row=0;row<yrows;++row)for(int ib=0;ib<yb;++ib){
  unsigned char*p=y.data+(row*ystride+ib)*ys;
  if(kind<3){uint16_t h=ggml_fp32_to_fp16(ib%2?0.03125f:-0.0625f);memcpy(p,&h,2);for(int j=0;j<32;++j)p[j+2]=(unsigned char)(j*47+ib*13+row*31);}
  else {float d=ib%2?0.03125f:-0.0625f;memcpy(p,&d,4);for(int j=0;j<256;++j)p[j+4]=(unsigned char)(j*47+ib*13+row*31);for(int j=0;j<16;++j){int sum=0;for(int k=0;k<16;++k)sum+=(int8_t)p[4+j*16+k];int16_t s=(int16_t)sum;memcpy(p+260+2*j,&s,2);}}
 }
 unsigned char*before_x=malloc(x.size),*before_y=malloc(y.size);if(!before_x||!before_y)abort();memcpy(before_x,x.data,x.size);memcpy(before_y,y.data,y.size);
 int outputs=mode?265:nrows;struct buf a=create(outputs*4),b=create(outputs*4);
 struct fg139_route route[4]={{0,0},{1,0},{0,1},{3,2}};
 if(mode){
  for(int c=0;c<columns;++c){int input=((route[c+1].slot%slots)+route[c+1].token*slots)*ystride;int output=route[c+1].slot*16+route[c+1].token*67+first;
   for(int row=0;row<nrows;++row)dots[kind](width,(float*)a.data+output+row,0,x.data+(first+row)*xstride*block_size[kind],0,y.data+input*ys,0,1);
  }
 }else for(int row=0;row<nrows;++row)dots[kind](width,(float*)a.data+row,0,x.data+row*xstride*block_size[kind],0,y.data,0,1);
 struct fg139_task p={0};p.mode=mode;p.kind=kind;p.blocks=blocks;p.rows=nrows;p.weight_stride=xstride;p.first=first;p.cols=mode?columns:1;p.slots=slots;p.dst_stride1=16;p.dst_stride2=67;p.route_first=mode?1:0;p.input_contiguous=contiguous;p.input_row_blocks=yb;p.input_stride1=ystride;p.input_stride2=ystride*slots;p.output_stride=1;
 fg139_execute(&p,x.data,xstride*total_rows,y.data,ystride*yrows,(float*)b.data,outputs,route,4);
 if(memcmp(a.raw,b.raw,a.size+128)){fprintf(stderr,"matrix mismatch kind=%d width=%d rows=%d pattern=%d mode=%d slots=%d cols=%d contiguous=%d\n",kind,width,nrows,pattern,mode,slots,columns,contiguous);abort();}
 guard(x);guard(y);guard(a);guard(b);if(memcmp(before_x,x.data,x.size)||memcmp(before_y,y.data,y.size))abort();
 if(!mode){
  struct buf da=create(width*4),db=create(width*4);deqs[kind](x.data,(float*)da.data,width);fg139_dequantize(kind,x.data,(float*)db.data,width);
  if(memcmp(da.raw,db.raw,da.size+128)){fprintf(stderr,"dequant mismatch %d %d\n",kind,width);abort();}
  guard(da);guard(db);free(da.raw);free(db.raw);atomic_fetch_add(&values_checked,width);
 }
 atomic_fetch_add(&cases,1);atomic_fetch_add(&rows_checked,nrows*(mode?columns:1));
 free(before_x);free(before_y);free(b.raw);free(a.raw);free(y.raw);free(x.raw);
}
static void*worker(void*arg){intptr_t id=(intptr_t)arg;for(int i=0;i<20;++i){int kind=(int)((id+i)%5);compare(kind,kind<3?640:2560,7,i%6,0,1,1,1,20261107u+(uint32_t)(id*997+i));}return NULL;}
static void empty_spans(void){
 const int spans[][2]={{-2,1},{0,1},{1,-2},{1,0},{-2,-3},{0,0}};
 for(int kind=0;kind<5;++kind)for(int i=0;i<6;++i){
  struct fg139_task p={0};p.mode=3;p.kind=kind;p.blocks=10;p.rows=spans[i][0];p.cols=spans[i][1];
  struct buf out=create(64);fg139_execute(&p,NULL,0,NULL,0,(float*)out.data,16,NULL,0);
  for(size_t j=0;j<out.size+128;++j)if(out.raw[j]!=0xa5)abort();free(out.raw);
  atomic_fetch_add(&cases,1);
 }
}
int main(void){
 ggml_cpu_init();empty_spans();int rs[]={1,2,7,16};
 for(int kind=0;kind<5;++kind)for(int w=0;w<6;++w)for(int rr=0;rr<4;++rr)for(int pattern=0;pattern<6;++pattern){int width=w==0?0:w==1?quant_size[kind]:w==2?3*quant_size[kind]:w==3?5*quant_size[kind]:w==4?(kind<3?640:2560):(kind<3?2560:4096);compare(kind,width,rs[rr],pattern,0,1,1,1,20261107u+kind*1301u+width+rs[rr]);}
 for(int kind=0;kind<5;++kind)for(int mode=1;mode<=3;mode+=2){if(mode==1&&kind!=4)continue;for(int ss=0;ss<3;++ss)for(int columns=2;columns<=3;++columns)for(int contiguous=0;contiguous<2;++contiguous)compare(kind,kind<3?640:2560,7,0,mode,1<<ss,columns,contiguous,12);}
 pthread_t threads[8];for(intptr_t i=0;i<8;++i)if(pthread_create(threads+i,NULL,worker,(void*)i))abort();for(int i=0;i<8;++i)pthread_join(threads[i],NULL);
 printf("{\"status\":\"PASS_ALL_FIVE_MATRIX_DEQUANT_ROUTED_FUSED_CANARIES\",\"cases\":%llu,\"matrix_rows\":%llu,\"dequant_values\":%llu,\"concurrent_threads\":8}\n",(unsigned long long)atomic_load(&cases),(unsigned long long)atomic_load(&rows_checked),(unsigned long long)atomic_load(&values_checked));return 0;
}
