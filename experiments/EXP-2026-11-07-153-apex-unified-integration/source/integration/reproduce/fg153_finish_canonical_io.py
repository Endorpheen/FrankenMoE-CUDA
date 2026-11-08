from pathlib import Path
import json,subprocess
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');S=A/'source/canonical-io';B=A/'build/canonical-io';records=json.loads((B/'commands.json').read_text())
p=S/'canonical-io.goose';t=p.read_text();old='let ctx=raw(21,cntp*ovh,0,0,0); let org=vn(8); var k:i64=0;';new='''var read_count:i64=0;var ri:i64=0;
  while ri<n {if raw(12,rg(vat(l,ri),0),0,0,0)==buft {read_count=read_count+1;}ri=ri+1;}
  let ctx=raw(21,read_count*ovh,0,0,0); let org=vn(8); var k:i64=0;''';assert old in t;t=t.replace(old,new);p.write_text(t)
def run(label,args):
 q=subprocess.run([str(x) for x in args],cwd=S,capture_output=True,text=True,timeout=150);records.append({'label':label,'argv':[str(x) for x in args],'cwd':str(S),'exit_code':q.returncode});(B/'commands.json').write_text(json.dumps(records,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,flush=True)
 if q.returncode:print((q.stdout+q.stderr)[-5000:]);raise SystemExit(q.returncode)
 return q
run('io-fixed-emit',[A/'compiler/stage3-f849233b999c','-o',S/'generated.c',p])
run('io-fixed-unit',next(r['argv'] for r in records if r['label']=='io-build-unit'))
run('io-fixed-link',next(r['argv'] for r in records if r['label']=='io-real-link'))
p=run('io-fixed-real-device-boundary',[B/'real-fixture'])
summary={'status':'PASS_CANONICAL_IO_REAL_GGML_AND_GOOSE_STORAGE','source':str(S),'stdout':p.stdout,'fixes':['writer queued entry records freed after flush','reader temporary buft vector freed','typed i64 foundation ABI bridges','cached buffer read from group field instead of group record cast','old buffer freed on replacement','pending versus committed geometry distinct','read arena sized from actual read chunks instead of previous writer count','real ggml tensor prototypes;no phantom overload','no shared generated stack reset'],'model_loads':0,'production_changed':False,'remaining':'normal context/shared runtime canonical IO adapters and complete failure/OOM closure'};(A/'results/canonical-io-real.json').write_text(json.dumps(summary,indent=2)+'\n')
print('PASS canonical IO actual backend repeated/split/changed geometry;zero owned foundation allocations')
