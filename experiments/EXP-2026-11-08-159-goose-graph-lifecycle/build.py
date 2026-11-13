from pathlib import Path
import json,re,hashlib,subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-159-goose-graph-lifecycle';P=F/'experiments/EXP-2026-11-08-158-goose-graph-builders';S=E/'source';B=F/'runtime/exp159/build';R=F/'runtime/exp159/run';B.mkdir(parents=True,exist_ok=True);R.mkdir(parents=True,exist_ok=True);C=F/'runtime/compiler/stage3-f849233b999c';assert hashlib.sha256(C.read_bytes()).hexdigest()=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a';rows=[]
def run(label,args,cwd=B):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,capture_output=True,text=True,timeout=240);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,flush=True)
 if q.returncode:print((q.stdout+q.stderr)[-6000:]);raise SystemExit(q.returncode)
run('graph-selfhosted-emit',[C,'-o',S/'graph-generated.c',S/'graph.goose']);g=(S/'graph-generated.c').read_text();main=g[g.rfind('static void gg_function_0('):];entry=re.search(r'(gg_function_\d+)\(t\d+, base',main);assert entry,main[-1500:];entry='fg159_'+entry[1];(S/'graph-qualified.c').write_text(re.sub(r'\b(gg_|ggf_|gs_)',lambda m:'fg159_'+m[1],g));h=(S/'graph.h').read_text();h=re.sub(r'(?m)^#define FG159_ENTRY .*$', '#define FG159_ENTRY '+entry,h);(S/'graph.h').write_text(h)
s=(P/'source/ggml-graph-builders.c').read_text().replace('#include "ggml-impl.h"','#include "ggml-impl-graph.h"',1);anchor='struct ggml_hash_set ggml_hash_set_new(';pos=s.index(anchor);s=s[:pos]+'#include "graph.h"\n\n'+s[pos:];recipes=json.loads((S/'recipes.json').read_text())
for name,r in recipes.items():
 pat=r'(?m)^[A-Za-z_][\w ]*?\s+(?:\*+\s*)?'+re.escape(name)+r'\s*\([^;{}]*\)\s*\{';m=re.search(pat,s);assert m,name;call='FG159_CALL('+str(r['mode'])+','+','.join('FG159_ARG('+x+')' for x in r['args'])+')';kind=r['kind'];code=(r['local'] or '')
 if r['local']:code+=call+';return fg_result;'
 elif kind=='void':code+=call+';return;'
 else:code+='return '+('(void *)(intptr_t)' if kind=='ptr' else '('+kind+')')+call+';'
 s=s[:m.end()]+'\n    if(fg159_enabled){'+code+'}\n'+s[m.end():]
(S/'ggml-graph-lifecycle.c').write_text(s)
previous=json.loads((P/'build-commands.json').read_text());r=next(x for x in previous if x['label']=='builders-core-build');cmd=r['argv'].copy();old=cmd[cmd.index('-o')+1];obj=B/'ggml-graph.o';cmd.insert(1,'-I'+str(P/'source'));cmd[cmd.index('-c')+1]=str(S/'ggml-graph-lifecycle.c');cmd[cmd.index('-o')+1]=str(obj);run('graph-core-build',cmd,Path(r['cwd']));r=next(x for x in previous if x['label']=='builders-base-link');cmd=r['argv'].copy();ref=B/'unchanged-exp158.so';cmd[cmd.index('-o')+1]=str(ref);run('unchanged-exp158-link-integrity',cmd,Path(r['cwd']));baseline=Path(json.loads((P/'source/manifest.json').read_text())['library']);assert hashlib.sha256(ref.read_bytes()).digest()==hashlib.sha256(baseline.read_bytes()).digest();cmd=[str(obj) if x==old else x for x in cmd];assert str(obj) in cmd;out=R/('libggml-base-graph-'+hashlib.sha256(obj.read_bytes()).hexdigest()[:12]+'.so.0.27.0');assert not out.exists();cmd[cmd.index('-o')+1]=str(out);run('graph-base-link',cmd,Path(r['cwd']))
for f in (F/'runtime/exp158/run').iterdir():
 if f.is_file() or f.is_symlink():
  p=R/f.name
  if p.is_symlink():p.unlink()
  elif p.exists():raise RuntimeError(str(p))
  p.symlink_to(out if f.name.startswith('libggml-base') else f.resolve())
(S/'manifest.json').write_text(json.dumps(dict(status='LINKED_PENDING_CORRECTNESS',compiler_sha256=hashlib.sha256(C.read_bytes()).hexdigest(),entry=entry,library=str(out),sha256=hashlib.sha256(out.read_bytes()).hexdigest(),rollback='GGML_GOOSE_GRAPH_LIFECYCLE=0',core_bodies=list(recipes),header_bodies=list(json.loads((S/'header-recipes.json').read_text())),scope='Core DSO inference graph/hash/traversal/lifecycle;other TU inlines,autodiff/training/export remain native or deferred',production_changed=False),indent=2)+'\n');print('LINKED_GRAPH_LIFECYCLE',out)
