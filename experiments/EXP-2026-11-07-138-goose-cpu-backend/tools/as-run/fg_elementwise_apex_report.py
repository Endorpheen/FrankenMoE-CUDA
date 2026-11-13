from pathlib import Path
import json,re
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
checks={}
for case in ['probe','2048']:
    names={m:f'element-fma-cpu-{m}-{case}' for m in ['off','on']}
    responses={m:json.loads((r/'results'/f'{n}-response.json').read_text()) for m,n in names.items()}
    payloads={m:json.loads((r/'results'/f'{n}-request.json').read_text()) for m,n in names.items()}
    same={k:responses['on'].get(k)==responses['off'].get(k) for k in ['content','tokens','completion_probabilities']}
    assert all(same.values()) and payloads['on']==payloads['off'],same
    old='moe-cpu-tiled-pure-off-'+case
    ref=json.loads((r/'results'/f'{old}-response.json').read_text())
    assert all(responses['on'].get(k)==ref.get(k) for k in same)
    checks[case]={'equal_pair_and_original_cpu_reference':same,'request_equal':True}
witness={}
for mode in ['off','on']:
    text=(r/'results'/f'element-fma-cpu-{mode}-server.log').read_text()
    total=int(re.search(r'FG_SELFHOST_ELEMENT total_values=(\d+)',text)[1])
    rows=[{'mode':int(a),'op':int(b),'calls':int(c),'values':int(d)} for a,b,c,d in re.findall(r'FG_SELFHOST_ELEMENT mode=(\d+) op=(\d+) calls=(\d+) values=(\d+)',text)]
    assert sum(x['values'] for x in rows)==total
    witness[mode]={'total_values':total,'operations':rows}
assert witness['off']['total_values']==0 and witness['on']['total_values']>0
report={'status':'PASS_REAL_APEX_FULL_BINARY_UNARY_WITH_OPEN_NAN_PAYLOAD_LIMIT','checks':checks,'witness':witness,'runs':{m:{k:v for k,v in json.loads((r/'results'/f'element-fma-cpu-{m}-2048-summary.json').read_text()).items() if k!='tokens'} for m in ['off','on']},
 'scope':'Entire binary-ops.cpp and unary-ops.cpp for all supported formats; prior Goose MoE and tiled paths kept enabled in both arms',
 'not_ported':'GELU/SiLU/GLU implementations elsewhere in ops.cpp, physical allocator/threadpool services and the rest of CPU backend',
 'trace_enabled':True,'preliminary':True,'production_accepted':False,'strict_nan_payload_gate':'Still fails at170 release values in independent edge corpus; ordinary values, infinities, signed zeros and actual Apex all match exactly',
 'producer_sha256':'81d6d066a675b282c72ee214d7154b709f3a3f581f72103a6b0790469f4f460f','sanitizer_report':'elementwise-fma-sanitizer-summary.json','pending':['Counter-free operation and model timing','Broader CPU backend migration','Qwen major-milestone test']}
(r/'results/elementwise-fma-apex-execution-proof.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'status':report['status'],'checks':checks,'witness':witness},indent=2))
