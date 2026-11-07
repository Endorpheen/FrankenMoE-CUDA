from pathlib import Path
import json,subprocess
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
s=r/'source/moe-quantized';v=r/'variants/moe-quantized';(v/'build').mkdir(parents=True,exist_ok=True)
unit='''#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include "ggml-cpu/simd-mappings.h"
extern void quantize_row_q8_0(const float*,void*,int64_t);
extern void quantize_row_q8_K(const float*,void*,int64_t);
#ifndef FG139_QUANT_TRACE
#define FG139_QUANT_TRACE 0
#endif
static int fg139_quant_enabled=1;
__attribute__((constructor))static void fg139_quant_config(void){const char*v=getenv("GGML_GOOSE_MOE_QUANT");fg139_quant_enabled=!v||strcmp(v,"0");}
static uint32_t fg_q8k_float_bits(float x){uint32_t u;memcpy(&u,&x,4);return u;}
static float fg139_float_from_bits(uint32_t u){float x;memcpy(&x,&u,4);return x;}
static uint16_t fg139_float_half(float x){return GGML_CPU_FP32_TO_FP16(x);}
#if FG139_QUANT_TRACE
static atomic_uint_fast64_t fg139_quant_calls[2],fg139_quant_blocks[2];
#endif
static void fg139_quant_witness(int64_t kind,int64_t blocks){
#if FG139_QUANT_TRACE
 assert(kind==0||kind==1);
 atomic_fetch_add_explicit(&fg139_quant_calls[kind],1,memory_order_relaxed);
 atomic_fetch_add_explicit(&fg139_quant_blocks[kind],blocks,memory_order_relaxed);
#else
 (void)kind;(void)blocks;
#endif
}
#define main fg139_activation_unused_main
#include "../../generated/moe-activation.c"
#undef main
_Static_assert(sizeof(gg_type_1)==128,"F32x32 block ABI");
_Static_assert(sizeof(gg_type_5)==1024,"F32x256 block ABI");
_Static_assert(sizeof(gg_type_8)==34,"Q8_0 block ABI");
_Static_assert(offsetof(gg_type_8,f3)==2,"Q8_0 bytes ABI");
_Static_assert(sizeof(gg_type_13)==292,"Q8_K block ABI");
_Static_assert(offsetof(gg_type_13,f5)==4&&offsetof(gg_type_13,f6)==260,"Q8_K fields ABI");
void fg139_quantize(int kind,const float*x,void*y,int64_t n){
 assert(kind==0||kind==1);assert(n>=0&&n%(kind?256:32)==0);
 if(!fg139_quant_enabled){if(kind)quantize_row_q8_K(x,y,n);else quantize_row_q8_0(x,y,n);return;}
 gg_function_1(kind,gg_view((void*)x,kind?0:n/32),gg_view((void*)x,kind?n/256:0),gg_view(y,kind?0:n/32),gg_view(y,kind?n/256:0),0);
}
void fg_selfhost_quant_q8k(const float*x,void*y,int64_t n){fg139_quantize(1,x,y,n);}
void fg139_quantize_q8_0(const float*x,void*y,int64_t n){fg139_quantize(0,x,y,n);}
#if FG139_QUANT_TRACE
__attribute__((destructor))static void fg139_quant_report(void){for(int i=0;i<2;++i)fprintf(stderr,"FG139_ACT kind=%d calls=%llu blocks=%llu enabled=%d\\n",i,(unsigned long long)atomic_load(&fg139_quant_calls[i]),(unsigned long long)atomic_load(&fg139_quant_blocks[i]),fg139_quant_enabled);}
#endif
'''
(s/'activation-unit.c').write_text(unit)
flags=['/usr/bin/gcc-13','-O3','-march=native','-fPIC','-fno-strict-aliasing','-ffp-contract=off','-I'+str(r/'source/ggml/src'),'-I'+str(r/'source/ggml/src/ggml-cpu'),'-I'+str(r/'source/ggml/include')]
compile=flags+['-DFG139_QUANT_TRACE=1','-c',str(s/'activation-unit.c'),'-o',str(v/'build/activation.o')]
p=subprocess.run(compile,capture_output=True,text=True)
(v/'build/activation-compile.log').write_text(p.stdout+p.stderr)
print('compile',p.returncode,(p.stdout+p.stderr)[-1500:],flush=True);p.check_returncode()
link=['/usr/bin/gcc-13','-shared',str(v/'build/activation.o'),'-L'+str(r/'run'),'-lggml-cpu','-lggml-base','-lm','-Wl,-rpath,'+str(r/'run'),'-o',str(v/'build/libgoose-moe-activation.so')]
p=subprocess.run(link,capture_output=True,text=True)
(v/'build/activation-link.log').write_text(p.stdout+p.stderr)
print('link',p.returncode,(p.stdout+p.stderr)[-1000:],flush=True);p.check_returncode()
(v/'build/activation-commands.json').write_text(json.dumps([compile,link],indent=2)+'\n')
