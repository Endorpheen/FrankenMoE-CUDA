from pathlib import Path
import subprocess,re,json,hashlib
F=Path(__file__).resolve().parents[2];E=F/'experiments/EXP-2026-11-08-173-goose-cuda-flash-attention';R=F/'runtime/exp173/run';B=F/'runtime/exp173/build/owned-ptx';B.mkdir(exist_ok=True);resources=json.loads((E/'owned-occupancy-source-check.json').read_text());names=[x['symbol']for x in resources['kernels']];objects={'paired_value_inline':R/'libggml-cuda-fa-a20b3ad06955.so.0.27.0','owned_occupancy':R/'libggml-cuda.so'};found={}
for label,obj in objects.items():
 q=subprocess.run(['/usr/local/cuda/bin/cuobjdump','--function',','.join(names),'--dump-ptx',str(obj)],capture_output=True,text=True,check=True);(B/(label+'.ptx')).write_text(q.stdout);kernels={}
 for name in names:
  m=re.search(r'(?:\.visible\s+)?\.entry\s+'+re.escape(name)+r'\s*\(',q.stdout);assert m,name;a=q.stdout.index('{',m.end());end=a+1;depth=1
  while depth:depth+=(q.stdout[end]=='{')-(q.stdout[end]=='}');end+=1
  text=q.stdout[m.start():end];text=re.sub(r'\$L__BB\d+_',r'$L__BB_',text);text=re.sub(r'(?m)^\s*\.loc[^\n]*\n','',text);kernels[name]=text
 found[label]=kernels
rows=[]
for name in names:
 a,b=[found[k][name]for k in objects];rows.append(dict(symbol=name,normalized_ptx_equal=a==b,before_sha256=hashlib.sha256(a.encode()).hexdigest(),after_sha256=hashlib.sha256(b.encode()).hexdigest()));print(name.split('EEv')[0],'normalized actual PTX equal',a==b)
(E/'owned-occupancy-ptx-equivalence.json').write_text(json.dumps(dict(status='ALL_TARGET_KERNEL_PTX_IDENTICAL'if all(x['normalized_ptx_equal']for x in rows)else'DIFFERENCES_REQUIRE_REVIEW',normalization='Only compiler basic-block ordinal prefixes and optional source .loc lines removed;math/control/memory instructions and operands unchanged',kernels=rows),indent=2)+'\n')
