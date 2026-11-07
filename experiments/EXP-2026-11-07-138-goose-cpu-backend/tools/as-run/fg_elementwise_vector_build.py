from pathlib import Path
import json,subprocess,shutil
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
s=r/'source/elementwise-vector'
base=r/'variants/cpu-moe-tiled-register-perf'
v=r/'variants/elementwise-vector'
(v/'build').mkdir(parents=True,exist_ok=True)
(v/'run').mkdir(exist_ok=True)
ops=json.loads((s/'operations.json').read_text())
header='''#pragma once
#include <stdint.h>
struct fg_el_shape {int64_t ne[4],stride[4],kind;};
struct fg_el_plan {struct fg_el_shape a,b,d;int64_t ith,nth,mode,op;float params[4];};
#ifdef __cplusplus
extern "C" {
#endif
void fg_element_compute(const struct fg_el_plan*,const void*,int64_t,const void*,int64_t,void*,int64_t);
#ifdef __cplusplus
}
#endif
'''
(s/'bridge.h').write_text(header)
unit='''#include "bridge.h"
#include "ggml-cpu/simd-mappings.h"
#include <stddef.h>
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>
#ifndef FG_ELEMENT_TRACE
#define FG_ELEMENT_TRACE 0
#endif
static uint32_t fg_el_bits(float x){uint32_t u;memcpy(&u,&x,4);return u;}
static float fg_el_from_bits(uint32_t u){float x;memcpy(&x,&u,4);return x;}
static float fg_el_half(uint16_t x){return GGML_CPU_FP16_TO_FP32(x);}
static uint16_t fg_el_to_half(float x){return GGML_CPU_FP32_TO_FP16(x);}
#if FG_ELEMENT_TRACE
static atomic_uint_fast64_t fg_el_calls[2][23],fg_el_values[2][23];
#endif
static void fg_element_witness(int64_t mode,int64_t op,int64_t values){
#if FG_ELEMENT_TRACE
 if(mode<0||mode>1||op<0||op>=23)abort();
 atomic_fetch_add_explicit(&fg_el_calls[mode][op],1,memory_order_relaxed);
 atomic_fetch_add_explicit(&fg_el_values[mode][op],values,memory_order_relaxed);
#else
 (void)mode;(void)op;(void)values;
#endif
}
#define main fg_element_unused_main
#include "../../generated/elementwise-vector.c"
#undef main
_Static_assert(sizeof(gg_type_1)==sizeof(struct fg_el_plan),"Plan ABI");
_Static_assert(sizeof(gg_type_2)==sizeof(struct fg_el_shape),"Shape ABI");
_Static_assert(offsetof(gg_type_1,f3)==offsetof(struct fg_el_plan,a),"a ABI");
_Static_assert(offsetof(gg_type_1,f4)==offsetof(struct fg_el_plan,b),"b ABI");
_Static_assert(offsetof(gg_type_1,f5)==offsetof(struct fg_el_plan,d),"d ABI");
_Static_assert(offsetof(gg_type_1,f6)==offsetof(struct fg_el_plan,ith),"ith ABI");
_Static_assert(offsetof(gg_type_1,f7)==offsetof(struct fg_el_plan,nth),"nth ABI");
_Static_assert(offsetof(gg_type_1,f8)==offsetof(struct fg_el_plan,mode),"mode ABI");
_Static_assert(offsetof(gg_type_1,f9)==offsetof(struct fg_el_plan,op),"op ABI");
_Static_assert(offsetof(gg_type_1,f10)==offsetof(struct fg_el_plan,params),"params ABI");
void fg_element_compute(const struct fg_el_plan*p,const void*a,int64_t na,const void*b,int64_t nb,void*d,int64_t nd){
 gg_function_1((gg_type_1*)p,gg_view((void*)a,p->a.kind==0?na:0),gg_view((void*)a,p->a.kind!=0?na:0),gg_view((void*)b,p->b.kind==0?nb:0),gg_view((void*)b,p->b.kind!=0?nb:0),gg_view(d,p->d.kind==0?nd:0),gg_view(d,p->d.kind!=0?nd:0),gg_view((void*)a,p->a.kind==0?na/8:0),gg_view((void*)b,p->b.kind==0?nb/8:0),gg_view(d,p->d.kind==0?nd/8:0),0);
}
#if FG_ELEMENT_TRACE
__attribute__((destructor))static void fg_element_report(void){
 uint64_t total=0;
 for(int mode=0;mode<2;++mode)for(int op=0;op<23;++op){uint64_t c=atomic_load(&fg_el_calls[mode][op]),n=atomic_load(&fg_el_values[mode][op]);total+=n;if(c)fprintf(stderr,"FG_SELFHOST_ELEMENT mode=%d op=%d calls=%llu values=%llu\\n",mode,op,(unsigned long long)c,(unsigned long long)n);}
 fprintf(stderr,"FG_SELFHOST_ELEMENT total_values=%llu\\n",(unsigned long long)total);
}
#endif
'''
(s/'unit.c').write_text(unit)
bridge='''#include "bridge.h"
#include "ggml-cpu/binary-ops.h"
#include "ggml-cpu/unary-ops.h"
#include <cstdlib>
#include <cstring>
static bool fg_el_enabled=true;
__attribute__((constructor))static void fg_el_config(void){const char*v=getenv("GGML_GOOSE_ELEMENTWISE");fg_el_enabled=!v||strcmp(v,"0");}
typedef void(*native_op)(const ggml_compute_params*,ggml_tensor*);
'''
mapping={'add':'add_non_quantized'}
for mode,names in [('u',ops['unary']),('b',ops['binary'])]:
 for name in names:bridge+=f'extern "C" void fg_el_native_{mode}_{name}(const ggml_compute_params*,ggml_tensor*);\n'
 bridge+=f'static native_op fg_el_native_{mode}[]='+'{'+','.join('fg_el_native_'+mode+'_'+name for name in names)+'};\n'
bridge+='''static fg_el_shape shape(const ggml_tensor*t){
 fg_el_shape s={};
 const size_t size=ggml_type_size(t->type);
 switch(t->type){case GGML_TYPE_F32:s.kind=0;break;case GGML_TYPE_F16:s.kind=1;break;case GGML_TYPE_BF16:s.kind=2;break;default:GGML_ABORT("unsupported elementwise-vector type");}
 for(int i=0;i<4;++i){GGML_ASSERT(t->nb[i]%size==0);s.ne[i]=t->ne[i];s.stride[i]=t->nb[i]/size;}
 return s;
}
static void apply(int mode,int op,const ggml_compute_params*params,ggml_tensor*dst){
 if(!fg_el_enabled){(mode?fg_el_native_b[op]:fg_el_native_u[op])(params,dst);return;}
 const ggml_tensor*a=dst->src[0],*b=mode?dst->src[1]:nullptr;
 GGML_ASSERT(ggml_are_same_shape(a,dst));
 GGML_ASSERT(a->nb[0]==ggml_type_size(a->type)&&dst->nb[0]==ggml_type_size(dst->type));
 if(mode)GGML_ASSERT(ggml_can_repeat(b,a));
 else GGML_ASSERT((op==22?(ggml_is_contiguous_1(a)&&ggml_is_contiguous_1(dst)):(ggml_is_contiguous_rows(a)&&ggml_is_contiguous_rows(dst))));
 fg_el_plan p={};p.a=shape(a);p.d=shape(dst);if(b)p.b=shape(b);p.ith=params->ith;p.nth=params->nth;p.mode=mode;p.op=op;
 if(!mode&&op==22)for(int i=0;i<4;++i)p.params[i]=ggml_get_op_params_f32(dst,i+1);
 fg_element_compute(&p,a->data,ggml_nbytes(a)/ggml_type_size(a->type),b?b->data:nullptr,b?ggml_nbytes(b)/ggml_type_size(b->type):0,dst->data,ggml_nbytes(dst)/ggml_type_size(dst->type));
}
'''
for mode,names in [(0,ops['unary']),(1,ops['binary'])]:
 for i,name in enumerate(names):
  exported=mapping.get(name,name) if mode else name
  bridge+=f'extern "C" void ggml_compute_forward_{exported}(const ggml_compute_params*p,ggml_tensor*d)'+'{'+f'apply({mode},{i},p,d);'+'}\n'
(s/'bridge.cpp').write_text(bridge)
original_core=json.loads((r/'variants/cpu-moe-tiled/build/commands.json').read_text())[0]
flags=[x for x in original_core[1:] if x.startswith(('-D','-I')) and x!='-DFG_MOE_TRACE=1']
flags+=['-O3','-DNDEBUG','-fPIC','-march=native','-fopenmp']
commands=[]
def run(name,cmd):
 p=subprocess.run(cmd,capture_output=True,text=True)
 (v/'build'/f'{name}.log').write_text(p.stdout+p.stderr)
 print(name,p.returncode,(p.stdout+p.stderr)[-1800:],flush=True)
 p.check_returncode();commands.append(cmd)
for mode,module,names in [('u','unary-ops',ops['unary']),('b','binary-ops',ops['binary'])]:
 defs=[f'-Dggml_compute_forward_{mapping.get(n,n) if mode=="b" else n}=fg_el_native_{mode}_{n}' for n in names]
 run('native-'+module,['/usr/bin/g++-13',*flags,'-std=gnu++17',*defs,'-c',str(r/'source/ggml/src/ggml-cpu'/f'{module}.cpp'),'-o',str(v/'build'/f'native-{module}.o')])
run('bridge',['/usr/bin/g++-13',*flags,'-std=gnu++17','-c',str(s/'bridge.cpp'),'-o',str(v/'build/bridge.o')])
run('unit',['/usr/bin/gcc-13',*flags,'-std=gnu11','-fno-strict-aliasing','-DFG_ELEMENT_TRACE=1','-c',str(s/'unit.c'),'-o',str(v/'build/goose-element.o')])
link=json.loads((base/'build/commands.json').read_text())[-1]
link=[x.replace(str(base/'run'),str(v/'run')) for x in link if not x.endswith(('/binary-ops.cpp.o','/unary-ops.cpp.o'))]
link+=[str(v/'build'/n) for n in ['native-unary-ops.o','native-binary-ops.o','bridge.o','goose-element.o']]
run('link',link)
for path in (base/'run').iterdir():
 if path.name.startswith('libggml-cpu.so'):continue
 target=v/'run'/path.name
 if not target.exists() and not target.is_symlink():target.symlink_to(path)
for name in ['libggml-cpu.so','libggml-cpu.so.0']:
 p=v/'run'/name
 if not p.exists() and not p.is_symlink():p.symlink_to('libggml-cpu.so.0.27.0')
(v/'build/commands.json').write_text(json.dumps(commands,indent=2)+'\n')
print('Isolated binary/unary backend replacement linked')
