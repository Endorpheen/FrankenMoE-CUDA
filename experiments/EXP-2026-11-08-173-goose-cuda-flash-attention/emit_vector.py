from pathlib import Path
import json,re,shlex,subprocess,hashlib,os
from generate_cuda_headers import materialize
F=Path(__file__).resolve().parents[2];E=F/'experiments/EXP-2026-11-08-173-goose-cuda-flash-attention';S=E/'source';B=F/'runtime/exp173/build';R=F/'runtime/exp173/run';B.mkdir(parents=True,exist_ok=True);C=Path(os.environ.get('EXP173_GOOSE', F/'runtime/compiler/stage3-f849233b999c'));H=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();assert H(C)=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a';rows=[]
assert all(__import__('hashlib').sha256(Path(p).read_bytes()).hexdigest()==h for p,h in json.loads((E/'external-header-pins.json').read_text()).items()),'Pinned external header changed; review first'

def run(label,args,cwd=B):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,capture_output=True,text=True,timeout=900);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,flush=True)
 if q.returncode:print((q.stdout+q.stderr)[-9000:]);raise SystemExit(q.returncode)
 return q
def qualify(g,p):return re.sub(r'\b(gg_|ggf_|gs_)',lambda m:p+m[1],g)
prefix='fg175vd_';run('emit-vector',[C,'-o',S/'vector-device-generated.c',S/'vector-device.goose']);g=(S/'vector-device-generated.c').read_text();main=g[g.rfind('static void gg_function_0('):];calls=[n for n in re.findall(r'\b(gg_function_\d+)\(',main)if n!='gg_function_0'];assert len(calls)==2;entries=[prefix+n for n in calls]
tm=re.search(r'(?m)^typedef struct[^\n]*gg_type_\d+',g);assert tm;start=g.rfind('\n\n',0,tm.start());types=g[start:g.index('static void gg_quote(')];named=[]
for m in re.finditer(r'struct (gg_type_\d+) \{\n([^}]+)\};',g):
 fields=re.findall(r'\bfloat (f\d+);',m[2])
 if len(fields)==4:named.append((m[1],fields))
assert len(named)==1
(S/'vector-types-generated.cuh').write_text('#pragma once\n#include <stdint.h>\n'+qualify('typedef struct {void*data;int64_t len;} gg_slice;\n'+types,prefix)+'using fg175v_V4 = '+prefix+named[0][0]+';\n')
materialize(S, B/'generated-cuda-headers', ('vector',))
