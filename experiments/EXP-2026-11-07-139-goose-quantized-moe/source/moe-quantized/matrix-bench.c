#define _GNU_SOURCE
#define GGML_COMMON_DECL_C
#include "ggml-common.h"
#include "ggml.h"
#include "ggml-cpu.h"
#include "ggml-cpu/quants.h"
#include "matrix-bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <math.h>
typedef void(*dot_fn)(int,float*,size_t,const void*,size_t,const void*,size_t,int);
static dot_fn dots[]={ggml_vec_dot_q2_0_q8_0,ggml_vec_dot_q5_0_q8_0,ggml_vec_dot_iq4_nl_q8_0,ggml_vec_dot_iq2_xxs_q8_K,ggml_vec_dot_iq2_s_q8_K};
static int qk[]={64,32,32,256,256},bs[]={18,22,18,66,82};
static uint32_t rnd=20261107;
static unsigned char byte(void){rnd=rnd*1664525+1013904223;return rnd>>24;}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
struct fixture{struct fg139_task p;struct fg139_route routes[3];void*x,*y;float*out;int width;};
static void run(struct fixture*f,int goose){
 if(goose){fg139_execute(&f->p,f->x,f->p.rows*f->p.blocks,f->y,f->p.cols*f->p.input_row_blocks,f->out,f->p.rows*f->p.cols,f->routes,f->p.cols);return;}
 int kind=f->p.kind,ybs=kind<3?34:292;
 for(int col=0;col<f->p.cols;++col)for(int row=0;row<f->p.rows;++row)dots[kind](f->width,f->out+col*f->p.rows+row,0,(char*)f->x+row*f->p.blocks*bs[kind],0,(char*)f->y+col*f->p.input_row_blocks*ybs,0,1);
}
static double measure(struct fixture*f,int goose,int iters){double start=now();for(int i=0;i<iters;++i)run(f,goose);return(now()-start)/iters;}
static int cmp(const void*a,const void*b){double x=*(const double*)a,y=*(const double*)b;return(x>y)-(x<y);}
int main(void){
 ggml_cpu_init();cpu_set_t allowed,chosen;int core=-1;if(sched_getaffinity(0,sizeof(allowed),&allowed)==0){for(int i=0;i<CPU_SETSIZE;++i)if(CPU_ISSET(i,&allowed)){core=i;break;}CPU_ZERO(&chosen);CPU_SET(core,&chosen);if(sched_setaffinity(0,sizeof(chosen),&chosen))abort();}
 printf("{\"cpu\":%d,\"rounds\":5,\"counter_free\":true,\"cases\":[",core);int count=0;
 for(int kind=0;kind<5;++kind)for(int shape=0;shape<3;++shape){
  struct fixture f={0};f.width=2560;f.p.kind=kind;f.p.mode=3;f.p.blocks=f.width/qk[kind];f.p.rows=shape?640:1;f.p.cols=shape==2?3:1;f.p.weight_stride=f.p.blocks;f.p.slots=1;f.p.input_contiguous=1;f.p.input_row_blocks=f.width/(kind<3?32:256);f.p.dst_stride1=f.p.rows;f.p.dst_stride2=f.p.rows;f.p.output_stride=1;
  size_t nx=f.p.rows*f.p.blocks*bs[kind],ny=f.p.cols*f.p.input_row_blocks*(kind<3?34:292);f.x=malloc(nx);f.y=malloc(ny);f.out=calloc(f.p.cols*f.p.rows,4);float*gold=calloc(f.p.cols*f.p.rows,4);if(!f.x||!f.y||!f.out||!gold)abort();
  for(size_t i=0;i<nx;++i)((unsigned char*)f.x)[i]=byte();for(int row=0;row<f.p.rows;++row)for(int ib=0;ib<f.p.blocks;++ib){uint16_t h=ggml_fp32_to_fp16(0.0625f);memcpy((char*)f.x+(row*f.p.blocks+ib)*bs[kind],&h,2);}
  float*input=malloc(f.width*4);for(int col=0;col<f.p.cols;++col){f.routes[col]=(struct fg139_route){0,col};for(int i=0;i<f.width;++i)input[i]=((int)byte()-128)*0.0625f;if(kind<3)quantize_row_q8_0(input,(char*)f.y+col*f.p.input_row_blocks*34,f.width);else quantize_row_q8_K(input,(char*)f.y+col*f.p.input_row_blocks*292,f.width);}free(input);
  run(&f,0);memcpy(gold,f.out,f.p.rows*f.p.cols*4);run(&f,1);if(memcmp(gold,f.out,f.p.rows*f.p.cols*4))abort();
  int iters=1;while(measure(&f,0,iters)<0.002/iters&&iters<65536)iters*=2;
  double values[2][5];for(int round=0;round<5;++round)for(int j=0;j<2;++j){int arm=(round+j)%2;values[arm][round]=measure(&f,arm,iters);}
  qsort(values[0],5,sizeof(double),cmp);qsort(values[1],5,sizeof(double),cmp);
  printf("%s{\"kind\":%d,\"width\":%d,\"rows\":%lld,\"cols\":%lld,\"iters\":%d,\"native_us\":%.6f,\"goose_us\":%.6f,\"ratio\":%.6f}",count++?",":"",kind,f.width,(long long)f.p.rows,(long long)f.p.cols,iters,values[0][2]*1e6,values[1][2]*1e6,values[1][2]/values[0][2]);fflush(stdout);free(gold);free(f.out);free(f.y);free(f.x);
 }
 puts("]}");return 0;
}
