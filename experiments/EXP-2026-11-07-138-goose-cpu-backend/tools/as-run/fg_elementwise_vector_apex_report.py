from pathlib import Path
import json,re
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
def read(name):return json.loads((r/'results'/name).read_text())
def equal(a,b):
 x,y=read(a+'-response.json'),read(b+'-response.json')
 assert read(a+'-request.json')==read(b+'-request.json')
 same={k:x.get(k)==y.get(k) for k in ['content','tokens','completion_probabilities']}
 assert all(same.values()),same
 return same
probe=equal('moe-cpu-tiled-pure-off-probe','element-vector-trace-cpu-on-probe')
names={m:f'element-vector-perf-cpu-{m}-2048' for m in ['off','on']}
same=equal(names['off'],names['on'])
equal('moe-cpu-tiled-pure-off-2048',names['on'])
assert read('element-vector-perf-cpu-off-argv.json')==read('element-vector-perf-cpu-on-argv.json')
envs={m:read(f'element-vector-perf-cpu-{m}-environment.json') for m in ['off','on']}
for key in set(envs['on'])|set(envs['off']):
 if key not in ['pid','mode','GGML_GOOSE_ELEMENTWISE']:assert envs['on'].get(key)==envs['off'].get(key),key
assert envs['on']['GGML_GOOSE_ELEMENTWISE']=='1' and envs['off']['GGML_GOOSE_ELEMENTWISE']=='0'
log=(r/'results/element-vector-trace-cpu-on-server.log').read_text()
m=re.search(r'FG_SELFHOST_ELEMENT_VECTOR calls=(\d+) values=(\d+)',log)
assert m and int(m[1])>0 and int(m[2])>0
vector={'calls':int(m[1]),'values':int(m[2])}
total=int(re.search(r'FG_SELFHOST_ELEMENT total_values=(\d+)',log)[1])
runs={m:{k:v for k,v in read(n+'-summary.json').items() if k!='tokens'} for m,n in names.items()}
changes={k:100*(runs['on']['timings'][k]/runs['off']['timings'][k]-1) for k in ['prompt_per_second','predicted_per_second']}
report={'status':'REAL_APEX_PASS_EXACT; PERFORMANCE_PRELIMINARY; NAN_PAYLOAD_QUALIFICATION_OPEN','probe_equal_original_cpu_reference':probe,'2048x16_equal_original_and_control':same,'argv_equal':True,'environment_only_feature_flag_differs':True,'runs':runs,'throughput_change_percent':changes,'counter_free_timing':True,'trace_witness_separate_run':{'module_total_values':total,'vector':vector},'scope':'Whole original binary-ops.cpp/unary-ops.cpp supported computations; checked scalar and F32x8 Goose paths; half/libm primitives and C++ tensor metadata bridge remain native','not_full_cpu_backend':True,'sanitizer_report':'elementwise-vector-sanitizer-summary.json','operator_report':'elementwise-vector-operation-summary.json','exact_nan_limitation':'170 release and156 sanitized edge-corpus outputs have different NaN payloads; zero non-NaN mismatches. Strict byte-equality verifier remains failed rather than being relaxed.','performance_accepted':False,'production_changed':False,'compiler_changed':False,'qwen_new_module_test':'Pending major milestone; Apex is primary','producer_sha256':'81d6d066a675b282c72ee214d7154b709f3a3f581f72103a6b0790469f4f460f'}
(r/'results/elementwise-vector-apex-stage-summary.json').write_text(json.dumps(report,indent=2)+'\n')
plan=r/'migration-plan.json'
p=json.loads(plan.read_text())
p['stages'][0]['verified']='Q8_K, Q4_K/Q6_K MoE controller/matrix and complete raw tiled-prefill in Goose; real Apex exact. Entire binary/unary module computations now Goose with F32x8 path, Apex exact, scoped sanitizers; NaN payload compatibility and performance acceptance open. Other quants/repack and native services remain.'
p['current_checkpoint']={'last_report':'results/elementwise-vector-apex-stage-summary.json','next':'Preserve/publish scoped experimental checkpoint, extend remaining CPU MoE/CPU backend modules; Qwen at substantive major milestone. No compiler edits or production promotion.','open':['NaN payload contract','Prefill/decode performance qualification','Other quantized MoE/repack and native backend services','Full GGML CPU/CUDA/scheduler/application stages']}
plan.write_text(json.dumps(p,indent=2)+'\n')
print(json.dumps({'equal':same,'vector_witness':vector,'throughput_change_percent':changes},indent=2))
