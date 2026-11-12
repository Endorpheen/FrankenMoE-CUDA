from pathlib import Path
import json,subprocess,hashlib,shutil
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/cpu-backend';v=r/'variants/cpu-backend';b=v/'build';b.mkdir(parents=True,exist_ok=True)
header='''#pragma once
#include <stdint.h>
struct fg143_tensor {int64_t ne[4],nb[4],kind,bytes;};
struct fg143_node {struct fg143_tensor x,y,z,d;int64_t op,ith,nth,ip[16];float fp[8];};
#ifdef __cplusplus
extern "C" {
#endif
void fg143_workspace(const struct fg143_node*,int64_t*);
uint8_t fg143_compute(const struct fg143_node*,const void*,const void*,const void*,void*,float*,int64_t,int64_t*,int64_t);
#ifdef __cplusplus
}
#endif
''';(s/'bridge.h').write_text(header)
unit='''#define GGML_COMMON_DECL_C
#define GGML_COMMON_IMPL_C
#include "ggml-common.h"
#include "bridge.h"
#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>
static uint32_t fg143_bits(float x){uint32_t v;memcpy(&v,&x,4);return v;}
static float fg143_from_bits(uint32_t v){float x;memcpy(&x,&v,4);return x;}
static float fg143_half(uint16_t v){return _cvtsh_ss(v);}
static float fg141_half(uint16_t v){return _cvtsh_ss(v);}
static float fg143_codec_from_bits(uint32_t v){float x;memcpy(&x,&v,4);return x;}
static void fg141_witness(int64_t s,int64_t k,int64_t n){(void)s;(void)k;(void)n;}
static uint16_t fg143_to_half(float v){return _cvtss_sh(v,_MM_FROUND_TO_NEAREST_INT);}
#ifndef FG143_TRACE
#define FG143_TRACE 1
#endif
#if FG143_TRACE
static atomic_uint_fast64_t fg143_calls[128],fg143_values[128];
#endif
static void fg143_witness(int64_t op,int64_t values){
#if FG143_TRACE
 if(op<0||op>=128||values<0)__builtin_trap();atomic_fetch_add_explicit(fg143_calls+op,1,memory_order_relaxed);atomic_fetch_add_explicit(fg143_values+op,values,memory_order_relaxed);
#else
 (void)op;(void)values;
#endif
}
#define main fg143_unused_main
#include "../../generated/cpu-backend.c"
#undef main
_Static_assert(sizeof(gg_type_1)==sizeof(struct fg143_node),"Node ABI");
_Static_assert(sizeof(gg_type_2)==sizeof(struct fg143_tensor),"Tensor ABI");
'''
for n,field in enumerate(['x','y','z','d','op','ith','nth','ip','fp'],4):unit+=f'_Static_assert(offsetof(gg_type_1,f{n})==offsetof(struct fg143_node,{field}),"{field} ABI");\n'
for n,field in enumerate(['ne','nb','kind','bytes']):unit+=f'_Static_assert(offsetof(gg_type_2,f{n})==offsetof(struct fg143_tensor,{field}),"{field} tensor ABI");\n'
unit+='''void fg143_workspace(const struct fg143_node*p,int64_t*out){struct fg143_node local=*p;local.ip[15]=1464816203;uint8_t ok=gg_function_1((gg_type_1*)&local,gg_view(0,0),gg_view(0,0),gg_view(0,0),gg_view(0,0),gg_view(0,0),gg_view(out,4),gg_view((void*)iq2xxs_grid,256),gg_view((void*)iq2xs_grid,512),gg_view((void*)iq2s_grid,1024),gg_view((void*)iq3xxs_grid,256),gg_view((void*)iq3s_grid,512),gg_view((void*)iq1s_grid,NGRID_IQ1S),gg_view((void*)ksigns_iq2xs,128),gg_view((void*)kvalues_iq4nl,16),0);if(!ok)__builtin_trap();}
uint8_t fg143_compute(const struct fg143_node*p,const void*x,const void*y,const void*z,void*dst,float*scratch,int64_t nf,int64_t*indices,int64_t ni){return gg_function_1((gg_type_1*)p,gg_view((void*)x,p->x.bytes),gg_view((void*)y,p->y.bytes),gg_view((void*)z,p->z.bytes),gg_view(dst,p->d.bytes),gg_view(scratch,nf),gg_view(indices,ni),gg_view((void*)iq2xxs_grid,256),gg_view((void*)iq2xs_grid,512),gg_view((void*)iq2s_grid,1024),gg_view((void*)iq3xxs_grid,256),gg_view((void*)iq3s_grid,512),gg_view((void*)iq1s_grid,NGRID_IQ1S),gg_view((void*)ksigns_iq2xs,128),gg_view((void*)kvalues_iq4nl,16),0);}
#if FG143_TRACE
__attribute__((destructor))static void fg143_report(void){for(int i=0;i<128;++i){uint64_t n=atomic_load(fg143_calls+i);if(n)fprintf(stderr,"FG143_CPU op=%d calls=%llu values=%llu\\n",i,(unsigned long long)n,(unsigned long long)atomic_load(fg143_values+i));}}
#endif
''';(s/'unit.c').write_text(unit);cmd=['/usr/bin/gcc-13','-I'+str(r/'source/ggml/src'),'-I'+str(r/'source/ggml/include'),'-O3','-march=native','-fPIC','-shared','-fno-strict-aliasing','-ffp-contract=off','-DFG143_TRACE=1',str(s/'unit.c'),'-lm','-o',str(b/'libgoose-cpu-backend.so')];(b/'unit-command.json').write_text(json.dumps(cmd,indent=2)+'\n');p=subprocess.run(cmd,capture_output=True,text=True);(b/'unit-build.log').write_text(p.stdout+p.stderr);print('B1 common CPU unit build',p.returncode,(p.stdout+p.stderr)[-1800:],flush=True)
if p.returncode:raise SystemExit(p.returncode)
(b/'manifest.json').write_text(json.dumps({'status':'BUILT_ABI_CHECKED_FUNCTIONAL_GATE_PENDING','library_sha256':hashlib.sha256((b/'libgoose-cpu-backend.so').read_bytes()).hexdigest(),'producer':json.loads((s/'producer.json').read_text()),'goose_sha256':hashlib.sha256((s/'kernel.goose').read_bytes()).hexdigest(),'working_runtime_changed':False,'runtime_integration':False,'model_loaded':False,'requirements_closed':0,'scope':'Common scalar/tensor/operator module only; missing operator/quant/thread/backend work remains in B1; no full CPU claim'},indent=2)+'\n')
