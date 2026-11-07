from pathlib import Path
import json,subprocess
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/moe-control';v=r/'variants/moe-control';(v/'build').mkdir(parents=True,exist_ok=True);fields=json.loads((s/'fields.json').read_text());code='''#include "bridge.h"
#include <stddef.h>
#include <string.h>
#ifdef FG140_NO_WITNESS
#define fg140_witness(c,k,v) ((void)(c),(void)(k),(void)(v))
#endif
#define main fg140_unused_main
#include "../../generated/moe-control.c"
#undef main
_Static_assert(sizeof(gg_type_1)==sizeof(struct fg140_plan),"Plan ABI");
_Static_assert(sizeof(gg_type_4)==sizeof(struct fg140_row),"Row ABI");
'''
for i,n in enumerate(fields):code+=f'_Static_assert(offsetof(gg_type_1,f{i})==offsetof(struct fg140_plan,{n}),"{n} ABI");\n'
code+='''void fg140_entry(struct fg140_plan*p,int64_t phase,int64_t*counts,int64_t nc,struct fg140_row*rows,int64_t nr,const int32_t*ids,int64_t ni,const int32_t*mask,int64_t nm,const int32_t*lut,int64_t nl,uint8_t*dst,int64_t nd){
 gg_function_1((gg_type_1*)p,phase,gg_view(counts,nc),gg_view(rows,nr),gg_view((void*)ids,ni),gg_view((void*)mask,nm),gg_view((void*)lut,nl),gg_view(dst,nd),0);
}
#ifdef FG140_TEST
struct fg140_callbacks {
 void(*quantize)(int64_t,int64_t,int64_t,int64_t);
 void(*reset)(int64_t,int64_t,int32_t);
 void(*barrier)(int64_t);
 void(*ready)(int64_t,int64_t);
 uint8_t(*tiled)(int64_t,int64_t,int64_t);
 void(*chunk)(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
 void(*matrix)(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
 int32_t(*claim)(int64_t,int64_t);
 void(*stats)(int64_t,int64_t);
 void(*witness)(int64_t,int64_t,int64_t);
};
static struct fg140_callbacks callbacks;
void fg140_set_callbacks(const void*p){memcpy(&callbacks,p,sizeof(callbacks));}
void fg140_quantize(int64_t c,int64_t a,int64_t b,int64_t n){callbacks.quantize(c,a,b,n);}
void fg140_reset(int64_t c,int64_t e,int32_t n){callbacks.reset(c,e,n);}
void fg140_barrier(int64_t c){callbacks.barrier(c);}
void fg140_ready(int64_t c,int64_t e){callbacks.ready(c,e);}
uint8_t fg140_tiled(int64_t c,int64_t e,int64_t n){return callbacks.tiled(c,e,n);}
void fg140_chunk(int64_t c,int64_t e,int64_t a,int64_t b,int64_t x,int64_t y,int64_t f){callbacks.chunk(c,e,a,b,x,y,f);}
void fg140_matrix(int64_t c,int64_t w,int64_t y,int64_t o,int64_t n,int64_t r){callbacks.matrix(c,w,y,o,n,r);}
int32_t fg140_claim(int64_t c,int64_t e){return callbacks.claim(c,e);}
void fg140_iq_stats(int64_t c,int64_t i){callbacks.stats(c,i);}
void fg140_witness(int64_t c,int64_t k,int64_t v){callbacks.witness(c,k,v);}
#endif
''';(s/'unit.c').write_text(code)
cmd=['/usr/bin/gcc-13','-O3','-march=native','-fPIC','-fno-strict-aliasing','-DFG140_TEST=1','-shared',str(s/'unit.c'),'-lm','-o',str(v/'build/libgoose-moe-control-test.so')];p=subprocess.run(cmd,capture_output=True,text=True);(v/'build/compile.log').write_text(p.stdout+p.stderr);(v/'build/compile-command.json').write_text(json.dumps(cmd,indent=2)+'\n');print('compile',p.returncode,p.stdout[-500:],p.stderr[-1800:],flush=True);p.check_returncode()
