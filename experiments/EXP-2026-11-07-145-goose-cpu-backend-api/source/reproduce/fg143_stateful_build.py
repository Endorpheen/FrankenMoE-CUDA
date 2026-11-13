from pathlib import Path
import re,json,hashlib,subprocess,shutil
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');parent=r/'source/cpu-backend';s=parent/'stateful';s.mkdir(exist_ok=True);b=r/'variants/cpu-backend/build';shutil.copy2('/tmp/fg143_stateful.goose',s/'stateful.goose');base=(parent/'tensor-base.goose').read_text();dot=(parent/'matrix.goose').read_text().split('fn matrix(')[0];scan=(parent/'recurrent.goose').read_text();scan=scan[:scan.index('fn ssm_conv(')]+scan[scan.index('fn softplus_state('):];scan=scan.replace('dst:f32[:]','dst:u8[:]');scan=scan.replace('assert(dst.len>=add_size(output,mul_size(mul_size(state_stride,seqs),p.snapshots)))','assert(dst.len>=mul_size(add_size(output,mul_size(mul_size(state_stride,seqs),p.snapshots)),4))');scan=scan.replace('else {dst[target]}','else {value_at(dst,target)}').replace('dst[target]=value;','set_value(dst,target,value);').replace('dst[lane+width*(head+heads*(token+tokens*seq))]=sum;','set_value(dst,lane+width*(head+heads*(token+tokens*seq)),sum);').replace('dst[output+(slot*seqs+seq)*state_stride+pos]=dst[output+seq*state_stride+pos];','set_value(dst,output+(slot*seqs+seq)*state_stride+pos,value_at(dst,output+seq*state_stride+pos));');(s/'scan-composition.goose').write_text(scan);text=base+'\n'+dot+'\n'+scan+'\n'+(s/'stateful.goose').read_text();mapping=json.loads((parent/'operation-map.json').read_text());text=re.sub(r'\bOP_[A-Z0-9_]+\b',lambda m:str(mapping[m.group(0)]),text);(s/'kernel.goose').write_text(text);producer=json.loads((parent/'producer.json').read_text());cmd=[producer['frozen'],'-o',str(r/'generated/cpu-stateful.c'),str(s/'kernel.goose')];p=subprocess.run(cmd,capture_output=True,text=True,timeout=90);(s/'generation.log').write_text(p.stdout+p.stderr);(s/'generation-command.json').write_text(json.dumps(cmd,indent=2)+'\n');print('CPU stateful module generation',p.returncode,(p.stdout+p.stderr)[-1400:],flush=True)
if p.returncode:raise SystemExit(p.returncode)
raw=(r/'generated/cpu-stateful.c').read_text();candidates=[]
for index,body in re.findall(r'struct (gg_type_\d+) \{(.*?)\n\};',raw,re.S):
 fields=re.findall(r'^\s+([^;]+?) (f\d+);',body,re.M)
 if len(fields)==7 and all(fields[i][0]=='int64_t' for i in range(2,6)):candidates.append((index,fields))
assert len(candidates)==1,candidates;plan,fields=candidates[0];tensor=fields[1][0]
header='''#pragma once
#include "../bridge.h"
struct fg143_state_plan {struct fg143_tensor source[7],dst;int64_t ith,nth,op,snapshots;float fp[4];};
#ifdef __cplusplus
extern "C" {
#endif
uint8_t fg143_state_compute(const struct fg143_state_plan*,const void*const*,void*);
#ifdef __cplusplus
}
#endif
''';(s/'bridge.h').write_text(header)
unit='''#include "bridge.h"
#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdatomic.h>
#include <stdio.h>
static uint32_t fg143_bits(float x){uint32_t u;memcpy(&u,&x,4);return u;}
static float fg143_from_bits(uint32_t u){float x;memcpy(&x,&u,4);return x;}
static float fg143_half(uint16_t u){return _cvtsh_ss(u);}
static uint16_t fg143_to_half(float x){return _cvtss_sh(x,_MM_FROUND_TO_NEAREST_INT);}
static atomic_uint_fast64_t stateful_calls[128];
static void fg143_witness(int64_t op,int64_t values){(void)values;if(op<0||op>=128)__builtin_trap();atomic_fetch_add_explicit(stateful_calls+op,1,memory_order_relaxed);}
#define main fg143_state_unused_main
#include "../../../generated/cpu-stateful.c"
#undef main
''';unit+=f'_Static_assert(sizeof({plan})==sizeof(struct fg143_state_plan),"StatePlan ABI");\n_Static_assert(sizeof({tensor})==sizeof(struct fg143_tensor),"Tensor ABI");\n'
for field,(_,cname) in zip(['source','dst','ith','nth','op','snapshots','fp'],fields):unit+=f'_Static_assert(offsetof({plan},{cname})==offsetof(struct fg143_state_plan,{field}),"{field} ABI");\n'
unit+=f'uint8_t fg143_state_compute(const struct fg143_state_plan*p,const void*const*inputs,void*dst){{return gg_function_1(({plan}*)p,'+','.join(f'gg_view((void*)inputs[{i}],p->source[{i}].bytes)' for i in range(7))+',gg_view(dst,p->dst.bytes),0);}\n';unit+='''__attribute__((destructor))static void fg143_state_report(void){for(int i=0;i<128;++i){uint64_t n=atomic_load(stateful_calls+i);if(n)fprintf(stderr,"FG143_STATE op=%d calls=%llu\\n",i,(unsigned long long)n);}}
''';(s/'unit.c').write_text(unit);cmd=['/usr/bin/gcc-13','-O3','-march=native','-fPIC','-fno-strict-aliasing','-ffp-contract=off','-c',str(s/'unit.c'),'-o',str(b/'stateful.o')];p=subprocess.run(cmd,capture_output=True,text=True);(b/'stateful-build.log').write_text(p.stdout+p.stderr);(b/'stateful-command.json').write_text(json.dumps(cmd,indent=2)+'\n');print('CPU stateful module build',p.returncode,(p.stdout+p.stderr)[-1400:],flush=True)
if p.returncode:raise SystemExit(p.returncode)
(s/'manifest.json').write_text(json.dumps({'status':'BUILT_ABI_CHECKED_FUNCTIONAL_AND_MODEL_GATE_PENDING','producer':producer,'goose_sha256':hashlib.sha256((s/'kernel.goose').read_bytes()).hexdigest(),'scope':'Complete portable GDN/KDA state/head/sequence update and snapshot packing plus SSM scan with sequence IDs. Reuses canonical tensor/vector helpers; no original arithmetic callback.','production_changed':False,'compiler_changed':False,'qwen_development':False},indent=2)+'\n')
