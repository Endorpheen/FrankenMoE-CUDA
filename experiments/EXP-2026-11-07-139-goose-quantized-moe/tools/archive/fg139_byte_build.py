from pathlib import Path
import json,subprocess
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/moe-quantized-byte';v=r/'variants/moe-quantized-byte';(v/'build').mkdir(parents=True,exist_ok=True)
fields=['mode','kind','blocks','rows','weight_stride','first','cols','slots','dst_stride1','dst_stride2','route_first','input_contiguous','input_row_blocks','input_stride1','input_stride2','output_stride']
header='#pragma once\n#include <stdint.h>\nstruct fg139_task {'+','.join(['int64_t '+fields[0]]+fields[1:])+';};\nstruct fg139_route {int32_t slot,token;};\n#ifdef __cplusplus\nextern "C" {\n#endif\nvoid fg139_execute(const struct fg139_task*,const void*,int64_t,const void*,int64_t,float*,int64_t,const void*,int64_t);\nvoid fg139_dot(int,int,const void*,const void*,float*);\nvoid fg139_dequantize(int,const void*,float*,int64_t);\n#ifdef __cplusplus\n}\n#endif\n'
(s/'matrix-bridge.h').write_text(header)
unit='''#define GGML_COMMON_IMPL_C
#include "ggml-common.h"
#include "ggml-quants.h"
#include "ggml-impl.h"
#include "matrix-bridge.h"
#include <immintrin.h>
#include <stddef.h>
#include <stdatomic.h>
#include <assert.h>
#include <stdio.h>
#ifndef FG139_MATH_TRACE
#define FG139_MATH_TRACE 0
#endif
#ifndef FG139_IQ4_TAIL_FMA
#define FG139_IQ4_TAIL_FMA 0
#endif
static uint8_t fg139_iq4_tail_fused(void){return FG139_IQ4_TAIL_FMA;}
static float fg139_half_float(uint16_t x){
#if defined(__F16C__)
 return _cvtsh_ss(x);
#else
 return GGML_FP16_TO_FP32(x);
#endif
}
#if FG139_MATH_TRACE
static atomic_uint_fast64_t fg139_math_calls[4][5],fg139_math_rows[4][5],fg139_math_blocks[4][5];
#endif
static void fg139_math_witness(int64_t mode,int64_t kind,int64_t blocks,int64_t rows,int64_t cols){
#if FG139_MATH_TRACE
 assert(mode>=0&&mode<=3&&kind>=0&&kind<5);
 atomic_fetch_add_explicit(&fg139_math_calls[mode][kind],1,memory_order_relaxed);
 atomic_fetch_add_explicit(&fg139_math_rows[mode][kind],rows*cols,memory_order_relaxed);
 atomic_fetch_add_explicit(&fg139_math_blocks[mode][kind],blocks*rows*cols,memory_order_relaxed);
#else
 (void)mode;(void)kind;(void)blocks;(void)rows;(void)cols;
#endif
}
#define main fg139_matrix_unused_main
#include "../../generated/moe-matrix-byte.c"
#undef main
_Static_assert(sizeof(gg_type_1)==sizeof(struct fg139_task),"Task ABI");
_Static_assert(sizeof(gg_type_3)==sizeof(block_q2_0),"Q2_0 ABI");
_Static_assert(sizeof(gg_type_8)==sizeof(block_q5_0),"Q5_0 ABI");
_Static_assert(sizeof(gg_type_11)==sizeof(block_iq4_nl),"IQ4_NL ABI");
_Static_assert(sizeof(gg_type_13)==sizeof(block_iq2_xxs),"IQ2_XXS ABI");
_Static_assert(sizeof(gg_type_16)==sizeof(block_iq2_s),"IQ2_S ABI");
_Static_assert(sizeof(gg_type_19)==sizeof(block_q8_0),"Q8_0 ABI");
_Static_assert(sizeof(gg_type_23)==sizeof(block_q8_K),"Q8_K ABI");
_Static_assert(sizeof(gg_type_33)==sizeof(struct fg139_route),"Route ABI");
'''
for i,field in enumerate(fields):unit+=f'_Static_assert(offsetof(gg_type_1,f{i})==offsetof(struct fg139_task,{field}),"{field} ABI");\n'
unit+='''void fg139_execute(const struct fg139_task*p,const void*x,int64_t nx,const void*y,int64_t ny,float*out,int64_t nout,const void*routes,int64_t nr){
 assert(p->kind>=0&&p->kind<5);
 gg_function_1((gg_type_1*)p,gg_view((void*)x,p->kind==0?nx:0),gg_view((void*)x,p->kind==1?nx:0),gg_view((void*)x,p->kind==2?nx:0),gg_view((void*)x,p->kind==3?nx:0),gg_view((void*)x,p->kind==4?nx:0),gg_view((void*)y,p->kind<3?ny:0),gg_view((void*)y,p->kind>=3?ny:0),gg_view(out,nout),gg_view((void*)iq2xxs_grid,256),gg_view((void*)iq2s_grid,1024),gg_view((void*)ksigns_iq2xs,128),gg_view((void*)kvalues_iq4nl,16),gg_view((void*)routes,nr),0);
}
void fg139_dot(int kind,int width,const void*x,const void*y,float*out){
 const int block=kind==0?64:kind<3?32:256;assert(width>=0&&width%block==0);
 struct fg139_task p={0};p.kind=kind;p.blocks=width/block;p.rows=1;p.cols=1;p.weight_stride=p.blocks;p.output_stride=1;
 fg139_execute(&p,x,p.blocks,y,width/(kind<3?32:256),out,1,NULL,0);
}
void fg139_dequantize(int kind,const void*x,float*out,int64_t width){
 const int block=kind==0?64:kind<3?32:256;assert(width>=0&&width%block==0);
 struct fg139_task p={0};p.mode=2;p.kind=kind;p.blocks=width/block;p.rows=1;p.cols=1;
 fg139_execute(&p,x,p.blocks,NULL,0,out,width,NULL,0);
}
#if FG139_MATH_TRACE
__attribute__((destructor))static void fg139_math_report(void){
 for(int m=0;m<4;++m)for(int k=0;k<5;++k){uint64_t c=atomic_load(&fg139_math_calls[m][k]);if(c)fprintf(stderr,"FG139_MATH mode=%d kind=%d calls=%llu rows=%llu dot_blocks=%llu\\n",m,k,(unsigned long long)c,(unsigned long long)atomic_load(&fg139_math_rows[m][k]),(unsigned long long)atomic_load(&fg139_math_blocks[m][k]));}
}
#endif
'''
(s/'matrix-unit.c').write_text(unit)
flags=['--param=max-inline-insns-auto=2000','--param=max-inline-insns-single=4000','--param=inline-unit-growth=500']
flags=['/usr/bin/gcc-13','-O3','-march=native','-fPIC','-fno-strict-aliasing','-ffp-contract=off','-I'+str(r/'source/ggml/src'),'-I'+str(r/'source/ggml/include')]
flags+=['--param=max-inline-insns-auto=2000','--param=max-inline-insns-single=4000','--param=inline-unit-growth=500']
compile=flags+['-DFG139_MATH_TRACE=1','-c',str(s/'matrix-unit.c'),'-o',str(v/'build/matrix.o')]
p=subprocess.run(compile,capture_output=True,text=True);(v/'build/matrix-compile.log').write_text(p.stdout+p.stderr);print('matrix compile',p.returncode,(p.stdout+p.stderr)[-1800:],flush=True);p.check_returncode()
link=['/usr/bin/gcc-13','-shared',str(v/'build/matrix.o'),'-lm','-o',str(v/'build/libgoose-moe-matrix.so')]
p=subprocess.run(link,capture_output=True,text=True);(v/'build/matrix-link.log').write_text(p.stdout+p.stderr);print('matrix link',p.returncode,(p.stdout+p.stderr)[-1000:],flush=True);p.check_returncode()
(v/'build/matrix-commands.json').write_text(json.dumps([compile,link],indent=2)+'\n')
qcompile=[x.replace(str(v/'build/matrix.o'),str(v/'build/matrix-qwen.o')) for x in compile]+['-DFG139_IQ4_TAIL_FMA=1']
p=subprocess.run(qcompile,capture_output=True,text=True);(v/'build/matrix-qwen-compile.log').write_text(p.stdout+p.stderr);print('qwen matrix compile',p.returncode,(p.stdout+p.stderr)[-1000:],flush=True);p.check_returncode()
qlink=[x.replace(str(v/'build/matrix.o'),str(v/'build/matrix-qwen.o')).replace(str(v/'build/libgoose-moe-matrix.so'),str(v/'build/libgoose-moe-matrix-qwen.so')) for x in link]
p=subprocess.run(qlink,capture_output=True,text=True);(v/'build/matrix-qwen-link.log').write_text(p.stdout+p.stderr);p.check_returncode()
(v/'build/matrix-qwen-commands.json').write_text(json.dumps([qcompile,qlink],indent=2)+'\n')
