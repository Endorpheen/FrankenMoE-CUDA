from pathlib import Path
import re,json
p=Path('/home/end0/AI/LlamaServer/franken-goose-exp149-cpu-control-20261107');s=p/'source/cpu-backend/control';text=(s/'kernel.goose').read_text();leaves=re.findall(r'extern "(fg143_ctl_leaf_\w+)" fn (\w+)\(([^)]*)\)',text)
ops=json.loads((p/'source/cpu-backend/operation-map.json').read_text());opnames={v:k for k,v in ops.items() if k.startswith('OP_') and k!='OP_COUNT'};assert len(opnames)==101
native_path=p/'source/ggml/src/ggml-cpu/ggml-cpu.c'
native=native_path.read_text();m=re.search(r'(?m)^static void ggml_compute_forward\(struct ggml_compute_params \* params, struct ggml_tensor \* tensor\) \{',native);assert m
at=m.end();level=1;stop=at
while level:
 if native[stop]=='{':level+=1
 elif native[stop]=='}':level-=1
 stop+=1
body=native[at:stop-1];native_targets={}
for op,fragment in re.findall(r'case GGML_(OP_\w+):([\s\S]*?)(?=case GGML_OP_|$)',body):
 if op=='OP_COUNT':continue
 calls=re.findall(r'ggml_compute_forward_(\w+)\(',fragment);assert len(calls)<=1,(op,calls)
 native_targets[ops[op]]=calls[0] if calls else None
assert len(native_targets)==101
bridges=(s/'services.c').read_text();bridge_targets={}
for symbol,fragment in re.findall(r'void (fg143_ctl_leaf_\w+)\([^\n]+?\)\{([^\n]+)',bridges):
 calls=re.findall(r'ggml_compute_forward_(\w+)\(',fragment);assert len(calls)==1,(symbol,calls);bridge_targets[symbol]=calls[0]
assert len(bridge_targets)==96
# Expected opcode routing comes from the unchanged native switch,not Goose implementation.
ordered=sorted({v for v in native_targets.values() if v is not None});native_ids={name:i for i,name in enumerate(ordered)}
flash=ops['OP_FLASH_ATTN_BACK'];mapping=native_targets
source='''#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void fg143_cpu_control(int64_t,int64_t,int64_t,int64_t,int64_t*);
static int op,empty,takeover,selected,mask,called;
int64_t fg143_ctl_meta(int64_t c,int64_t n,int64_t field){(void)c;(void)n;if(field==0)return op;if(field>=10&&field<14)return empty?0:1;if(field==8)return mask;return 0;}
int64_t fg143_ctl_extra_work(int64_t c,int64_t n,int64_t t){(void)c;(void)n;(void)t;return -1;}
int64_t fg143_ctl_goose_work(int64_t c,int64_t n,int64_t t){(void)c;(void)n;(void)t;return 0;}
uint8_t fg143_ctl_extra_compute(int64_t c){(void)c;return takeover;}
int64_t fg143_job_get(int64_t c,int64_t f){(void)c;(void)f;abort();}
void fg143_job_set(int64_t c,int64_t f,int64_t v){(void)c;(void)f;(void)v;abort();}
void fg143_job_create(int64_t c,int64_t t){(void)c;(void)t;abort();}
void fg143_job_free(int64_t c){(void)c;abort();}
void fg143_job_team(int64_t c,int64_t t){(void)c;(void)t;abort();}
void fg143_job_barrier(void){abort();}
void fg143_job_place(int64_t c,int64_t t){(void)c;(void)t;abort();}
void fg143_job_compute(int64_t c,int64_t t){(void)c;(void)t;abort();}
void fg143_job_clear_affinity(void){abort();}
void fg149_job_kickoff(int64_t c,int64_t t){(void)c;(void)t;abort();}
'''
index={}
for i,(symbol,leaf,args) in enumerate(leaves):
 i=native_ids[bridge_targets[symbol]];index[leaf]=i
 if leaf=='leaf_flash_attn_back':source+=f'void {symbol}(int64_t c,int64_t m){{(void)c;if(m!=mask)abort();selected={i};++called;}}\n'
 else:source+=f'void {symbol}(int64_t c){{(void)c;selected={i};++called;}}\n'
expected=[native_ids[mapping[k]] if mapping[k] is not None else -1 for k in range(101)]
assert sum(x>=0 for x in expected)==96,(len(mapping),expected)
source+='static const int expected[]={'+','.join(map(str,expected))+'};\n'
source+='''int main(void){int cases=0;
 for(op=0;op<101;++op)for(empty=0;empty<2;++empty)for(takeover=0;takeover<2;++takeover){
  mask=op==FLASH?1:0;selected=-1;called=0;fg143_cpu_control(2,0,0,0,NULL);
  int want=empty||takeover?-1:expected[op];if(selected!=want||called!=(want>=0)){fprintf(stderr,"bad dispatch op=%d empty=%d extra=%d selected=%d want=%d\\n",op,empty,takeover,selected,want);abort();}++cases;
 }
 printf("{\\"status\\":\\"PASS_ALL_101_DISPATCH_ENTRIES\\",\\"entries\\":101,\\"leaf_callbacks\\":96,\\"cases\\":%d,\\"scope\\":\\"Actual Goose routing with stub computational callbacks; numerical correctness is the separate real-GGML corpus\\"}\\n",cases);return 0;}
'''.replace('FLASH',str(flash)).replace('\\"','\"')
# Keep C string escaping intact for JSON output.
source=source.replace('printf("{"status":"PASS_ALL_101_DISPATCH_ENTRIES","entries":101,"leaf_callbacks":96,"cases":%d,"scope":"Actual Goose routing with stub computational callbacks; numerical correctness is the separate real-GGML corpus"}\\n",cases);', 'printf("{\\"status\\":\\"PASS_ALL_101_DISPATCH_ENTRIES\\",\\"entries\\":101,\\"leaf_callbacks\\":96,\\"cases\\":%d}\\n",cases);')
(s/'dispatch-fixture.c').write_text(source)
(p/'results/dispatch-map.json').write_text(json.dumps({'expected':{opnames[k]:mapping.get(k) or 'no-op' for k in opnames},'leaves':len(leaves),'scope':'Independent unchanged-native-switch oracle versus actual Goose routing and bridge targets;leaf math is separate actual GGML corpus','native_source':str(native_path)},indent=2)+'\n')
print('All101operation entries and96leaf bindings mapped')
