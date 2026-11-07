from pathlib import Path
import json,subprocess,re
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/cpu-tiled-all';v=r/'variants/cpu-tiled-all';(v/'build').mkdir(parents=True,exist_ok=True);qsrc=Path('/home/end0/AI/LlamaServer/franken-qwen38-v31/source/ggml')
fields=re.findall(r'(\w+):i64',(s/'kernel.goose').read_text().split('struct Plan {',1)[1].split('}',1)[0]);(s/'fields.json').write_text(json.dumps(fields,indent=2)+'\n');header='#pragma once\n#include <stdint.h>\nstruct fg141_plan {'+','.join(['int64_t '+fields[0]]+fields[1:])+';};\nvoid fg141_entry(const struct fg141_plan*,const void*,int64_t,const void*,int64_t,const void*,int64_t,void*);\nvoid fg141_unpack(int,const void*,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,void*);\n';header+=Path('/tmp/fg141_extra_prototypes.h').read_text();(s/'bridge.h').write_text(header)
unit='''#define GGML_COMMON_DECL_C
#define GGML_COMMON_IMPL_C
#include "ggml-common.h"
#include "bridge.h"
#include <immintrin.h>
#include <stddef.h>
#include <stdio.h>
#include <stdatomic.h>
static float fg141_half(uint16_t x){return _cvtsh_ss(x);}
#ifndef FG141_TRACE
#define FG141_TRACE 0
#endif
#if FG141_TRACE
static atomic_uint_fast64_t events[8][13];
#endif
static void fg141_witness(int64_t stage,int64_t kind,int64_t units){
#if FG141_TRACE
 if(stage<0||stage>=8||kind<0||kind>=13||units<0)__builtin_trap();atomic_fetch_add_explicit(&events[stage][kind],units,memory_order_relaxed);
#else
 (void)stage;(void)kind;(void)units;
#endif
}
#define main fg141_unused_main
#include "../../generated/cpu-tiled-all.c"
#undef main
_Static_assert(sizeof(gg_type_1)==sizeof(struct fg141_plan),"Plan ABI");
_Static_assert(sizeof(gg_type_5)==292,"Q8_K ABI");
_Static_assert(sizeof(gg_type_13)==8,"Route ABI");
_Static_assert(sizeof(gg_type_17)==100352,"Weight tile ABI");
_Static_assert(sizeof(gg_type_24)==82944,"Activation tile ABI");
_Static_assert(sizeof(gg_type_16)==445440,"Workspace ABI");
'''
for i,n in enumerate(fields):unit+=f'_Static_assert(offsetof(gg_type_1,f{i})==offsetof(struct fg141_plan,{n}),"{n} ABI");\n'
code=(r/'generated/cpu-tiled-all.c').read_text();wbody=re.search(r'struct gg_type_16 \{(.*?)\};',code,re.S).group(1);wf=re.findall(r'\s( f\d+|f\d+);',wbody);wf=[x.strip() for x in wf];assert len(wf)==3;unit+=f'_Static_assert(offsetof(gg_type_16,{wf[1]})==100352&&offsetof(gg_type_16,{wf[2]})==183296,"Workspace fields ABI");\n'
layout={'q4_K':(144,{'d':0,'dmin':2,'scales':4,'qs':16}),'q5_K':(176,{'d':0,'dmin':2,'scales':4,'qh':16,'qs':48}),'q6_K':(210,{'ql':0,'qh':128,'scales':192,'d':208}),'q3_K':(110,{'hmask':0,'qs':32,'scales':96,'d':108}),'q2_K':(84,{'scales':0,'qs':16,'d':80,'dmin':82}),'iq4_xs':(136,{'d':0,'scales_h':2,'scales_l':4,'qs':8}),'iq2_xxs':(66,{'d':0,'qs':2}),'iq2_xs':(74,{'d':0,'qs':2,'scales':66}),'iq2_s':(82,{'d':0,'qs':2,'qh':66,'scales':74}),'iq3_xxs':(98,{'d':0,'qs':2}),'iq3_s':(110,{'d':0,'qs':2,'qh':66,'signs':74,'scales':106}),'iq1_s':(50,{'d':0,'qs':2,'qh':34}),'iq1_m':(56,{'qs':0,'qh':32,'scales':48})}
for name,(size,offsets) in layout.items():
 unit+=f'_Static_assert(sizeof(block_{name})=={size},"{name} size");\n'
 for field,offset in offsets.items():unit+=f'_Static_assert(offsetof(block_{name},{field})=={offset},"{name}.{field}");\n'
unit+='''void fg141_entry(const struct fg141_plan*p,const void*x,int64_t nx,const void*y,int64_t ny,const void*routes,int64_t nr,void*w){
 gg_function_1((gg_type_1*)p,gg_view((void*)x,nx),gg_view((void*)y,ny),gg_view((void*)routes,nr),(gg_type_16*)w,gg_view((void*)iq2xxs_grid,256),gg_view((void*)iq2xs_grid,512),gg_view((void*)iq2s_grid,1024),gg_view((void*)iq3xxs_grid,256),gg_view((void*)iq3s_grid,512),gg_view((void*)iq1s_grid,NGRID_IQ1S),gg_view((void*)ksigns_iq2xs,128),gg_view((void*)kvalues_iq4nl,16),0);
}
void fg141_unpack(int kind,const void*x,int64_t nx,int64_t stride,int64_t first,int64_t start,int64_t count,int64_t nk,void*w){struct fg141_plan p={0};p.kind=kind;p.stride=stride;p.first=first;p.start=start;p.count=count;p.nk=nk;fg141_entry(&p,x,nx,NULL,0,NULL,0,w);}
#if FG141_TRACE
__attribute__((destructor))static void fg141_report(void){for(int s=0;s<8;++s)for(int k=0;k<13;++k){uint64_t n=atomic_load(&events[s][k]);if(n)fprintf(stderr,"FG141_TILED stage=%d kind=%d units=%llu\\n",s,k,(unsigned long long)n);}}
#endif
''';unit+=Path('/tmp/fg141_extra_unit.c').read_text();unit=unit.replace('#define main fg141_unused_main',Path('/tmp/fg141_services_decl.c').read_text()+'\n#define main fg141_unused_main').replace('gg_view((void*)kvalues_iq4nl,16),0);','gg_view((void*)kvalues_iq4nl,16),gg_view(NULL,0),0);');(s/'unit.c').write_text(unit);(s/'block-layout.json').write_text(json.dumps(layout,indent=2)+'\n')
cmd=['/usr/bin/gcc-13','-O3','-march=native','-fPIC','-fno-strict-aliasing','-ffp-contract=off','-I'+str(qsrc/'src'),'-I'+str(qsrc/'include'),'-DFG141_TRACE=1','-DFG141_KERNEL_ONLY=1','-shared',str(s/'unit.c'),'-lm','-o',str(v/'build/libgoose-tiled-all.so')];p=subprocess.run(cmd,capture_output=True,text=True);(v/'build/unit-compile.log').write_text(p.stdout+p.stderr);(v/'build/unit-command.json').write_text(json.dumps(cmd,indent=2)+'\n');print('unit',p.returncode,p.stderr[-2200:],flush=True);p.check_returncode()
