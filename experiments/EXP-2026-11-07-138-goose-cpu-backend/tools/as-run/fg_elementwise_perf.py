from pathlib import Path
import json,subprocess,os,statistics,struct,collections,hashlib
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
s=r/'source/elementwise-fma'
source=r/'variants/elementwise-fma'
v=r/'variants/elementwise-fma-perf'
(v/'build').mkdir(parents=True,exist_ok=True)
(v/'run').mkdir(exist_ok=True)
cmds=json.loads((source/'build/commands.json').read_text())
unit=[x.replace(str(source/'build'),str(v/'build')) for x in cmds[3] if x!='-DFG_ELEMENT_TRACE=1']
p=subprocess.run(unit,capture_output=True,text=True)
(v/'build/unit-compile.log').write_text(p.stdout+p.stderr)
print('unit',p.returncode,(p.stdout+p.stderr)[-500:],flush=True);p.check_returncode()
link=[x.replace(str(source/'run'),str(v/'run')).replace(str(source/'build/goose-element.o'),str(v/'build/goose-element.o')) for x in cmds[-1]]
p=subprocess.run(link,capture_output=True,text=True)
(v/'build/link.log').write_text(p.stdout+p.stderr)
p.check_returncode()
for path in (source/'run').iterdir():
 if path.name.startswith('libggml-cpu.so'):continue
 target=v/'run'/path.name
 if not target.exists() and not target.is_symlink():target.symlink_to(path)
for name in ['libggml-cpu.so','libggml-cpu.so.0']:
 p=v/'run'/name
 if not p.exists() and not p.is_symlink():p.symlink_to('libggml-cpu.so.0.27.0')
nm=subprocess.check_output(['nm','--defined-only',str(v/'run/libggml-cpu.so.0.27.0')],text=True)
assert all(x not in nm for x in ['fg_el_calls','fg_el_values','fg_element_report'])
fixture=(s/'fixture.c').read_text()
fixture=fixture.replace('int width=widths[profile%4],pad=profile%2,strided=mode&&profile%4==3;', 'int width=profile>=8?4096:widths[profile%4],pad=profile%2,strided=mode&&profile%4==3;')
fixture=fixture.replace('int inplace=(ak==dk&&profile%4==2);','int inplace=0;')
fixture=fixture.replace('fill(a,profile>=4,0)','fill(a,profile>=4&&profile<8,0)').replace('fill(b,profile>=4,1)','fill(b,profile>=4&&profile<8,1)')
needle='enum ggml_status status=ggml_graph_compute(g,&plan);if(status!=GGML_STATUS_SUCCESS)abort();'
assert fixture.count(needle)==1
timing='''
 size_t expected_bytes=ggml_nbytes(out);unsigned char*expected=malloc(expected_bytes);if(!expected)abort();memcpy(expected,out->data,expected_bytes);
 for(int warm=0;warm<2;++warm)if(ggml_graph_compute(g,&plan)!=GGML_STATUS_SUCCESS)abort();
 for(int repeat=0;repeat<5;++repeat){int64_t begin=ggml_time_us();for(int pass=0;pass<20;++pass)if(ggml_graph_compute(g,&plan)!=GGML_STATUS_SUCCESS)abort();double us=(ggml_time_us()-begin)/20.0;if(memcmp(expected,out->data,expected_bytes))abort();printf("{\\"kind\\":\\"operation\\",\\"case\\":%llu,\\"mode\\":%d,\\"op\\":%d,\\"format\\":%d,\\"profile\\":%d,\\"width\\":%d,\\"threads\\":%d,\\"repeat\\":%d,\\"microseconds\\":%.9g}\\n",(unsigned long long)cases,mode,op,format,profile,width,threads_list[thread_index],repeat,us);}
 free(expected);
'''
fixture=fixture.replace(needle,needle+timing)
needle='fclose(f);for(int i=0;i<3;++i)'
assert fixture.count(needle)==1
extra='for(int mode=0;mode<2;++mode)for(int op=0;op<(mode?4:23);++op)for(int profile=8;profile<10;++profile)run(f,mode,op,0,profile);\n '
fixture=fixture.replace(needle,extra+needle)
(s/'operation-benchmark.c').write_text(fixture)
cmd=[x.replace(str(s/'fixture.c'),str(s/'operation-benchmark.c')).replace(str(source/'run'),str(v/'run')).replace(str(source/'build/elementwise-fma-fixture'),str(v/'build/elementwise-operation-benchmark')) for x in json.loads((source/'build/fixture-compile.json').read_text())]
p=subprocess.run(cmd,capture_output=True,text=True)
(v/'build/fixture-compile.log').write_text(p.stdout+p.stderr)
print('benchmark compile',p.returncode,(p.stdout+p.stderr)[-1000:],flush=True);p.check_returncode()
(v/'build/commands.json').write_text(json.dumps([unit,link,cmd],indent=2)+'\n')
runs={}
for mode in ['off','on']:
 env=dict(os.environ);env.pop('LD_PRELOAD',None)
 env.update(LD_LIBRARY_PATH=str(v/'run'),GGML_GOOSE_ELEMENTWISE='0' if mode=='off' else '1',GGML_GOOSE_ALLOC='0',GGML_GOOSE_GRAPH_PLAN='0',GGML_GOOSE_COALESCE='0',GGML_GOOSE_LIFETIME='0')
 p=subprocess.run([str(v/'build/elementwise-operation-benchmark'),str(r/'results'/f'elementwise-fma-operation-{mode}.bin')],env=env,capture_output=True,text=True,timeout=180)
 (r/'results'/f'elementwise-fma-operation-{mode}.jsonl').write_text(p.stdout)
 (r/'results'/f'elementwise-fma-operation-{mode}.stderr').write_text(p.stderr)
 print(mode,p.returncode,p.stdout.splitlines()[-1:] if p.stdout else '',p.stderr[-500:],flush=True);p.check_returncode()
 runs[mode]=[json.loads(x) for x in p.stdout.splitlines()]
metas={mode:[x for x in rows if x.get('kind')!='operation' and 'case' in x] for mode,rows in runs.items()}
assert metas['on']==metas['off']
comparison=[]
for case in metas['off']:
 med={mode:statistics.median(x['microseconds'] for x in rows if x.get('kind')=='operation' and x['case']==case['case']) for mode,rows in runs.items()}
 comparison.append({**case,'native_us':med['off'],'goose_us':med['on'],'goose_time_change_percent':100*(med['on']/med['off']-1) if med['off'] else None})
u_formats=[(0,0),(1,1),(2,2),(2,0),(1,0)];b_formats=[(0,0,0),(1,1,1),(2,2,2),(2,0,2),(2,0,0),(1,0,1),(1,0,0)]
a=(r/'results/elementwise-fma-operation-off.bin').read_bytes();b=(r/'results/elementwise-fma-operation-on.bin').read_bytes();assert len(a)==len(b)
def isnan(bits,kind):return (bits&0x7f800000==0x7f800000 and bits&0x7fffff!=0) if kind==0 else (bits&0x7c00==0x7c00 and bits&0x3ff!=0) if kind==1 else (bits&0x7f80==0x7f80 and bits&0x7f!=0)
offset=0;nan=0;other=[]
for meta in metas['off']:
 n=struct.unpack_from('<Q',a,offset)[0];assert struct.unpack_from('<Q',b,offset)[0]==n
 kind=(b_formats if meta['mode'] else u_formats)[meta['format']][-1];size=4 if kind==0 else 2;fmt='<I' if kind==0 else '<H'
 for i in range(0,n,size):
  x=struct.unpack_from(fmt,a,offset+8+i)[0];y=struct.unpack_from(fmt,b,offset+8+i)[0]
  if x==y:continue
  if isnan(x,kind) and isnan(y,kind):nan+=1
  else:other.append({'case':meta,'byte':i,'native':hex(x),'goose':hex(y)})
 offset+=8+n
report={'status':'MEASURED_COUNTER_FREE_MODULE_OPERATIONS','cases':len(comparison),'runs':{m:rows[-1] for m,rows in runs.items()},'counter_free':True,'repeated_outputs_stable':True,'comparison':comparison,'nan_payload_variations':nan,'other_mismatches':len(other),'mismatch_examples':other[:8],'method':'Actual GGML graph execution, persistent poll=0 threadpools; input/plan/allocation outside timing; no inplace mutation during repeated measurements; two warmups then five samples of twenty computes; exact output stability checked after each sample. All1144 functional profiles plus54 F32 width4096 profiles.','preliminary':True,'limit':'Uncontrolled competing load; isolated operation speeds are not model prefill/decode; strict NaN-payload equality unqualified','binary_sha256':hashlib.sha256((v/'run/libggml-cpu.so.0.27.0').read_bytes()).hexdigest()}
(r/'results/elementwise-fma-operation-summary.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'status':report['status'],'cases':len(comparison),'nan_payload_variations':nan,'other_mismatches':len(other),'large_f32_examples':[x for x in comparison if x['width']==4096 and x['profile']==9 and (x['mode']==1 or x['op'] in [7,17])]},indent=2))
assert not other
