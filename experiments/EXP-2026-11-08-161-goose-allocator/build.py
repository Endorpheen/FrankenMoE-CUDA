from pathlib import Path
import json,re,hashlib,subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-161-goose-allocator';S=E/'source';P=F/'experiments/EXP-2026-11-08-160-goose-shared-fusion';B=F/'runtime/exp161/build';R=F/'runtime/exp161/run';B.mkdir(parents=True,exist_ok=True);R.mkdir(parents=True,exist_ok=True);C=F/'runtime/compiler/stage3-f849233b999c';H=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();assert H(C)=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a';rows=[]
def run(label,args,cwd=B):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,capture_output=True,text=True,timeout=240);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,flush=True)
 if q.returncode:print((q.stdout+q.stderr)[-5000:]);raise SystemExit(q.returncode)
run('allocator-selfhosted-emit',[C,'-o',S/'allocator-generated.c',S/'allocator.goose']);g=(S/'allocator-generated.c').read_text();main=g[g.rfind('static void gg_function_0('):];entry=re.search(r'(gg_function_\d+)\(t\d+, base',main);assert entry,main[-1200:];entry='fg161_'+entry[1];(S/'allocator-qualified.c').write_text(re.sub(r'\b(gg_|ggf_|gs_)',lambda m:'fg161_'+m[1],g));h=(S/'allocator.h').read_text();h=re.sub(r'(?m)^#define FG161_ENTRY .*$', '#define FG161_ENTRY '+entry,h);(S/'allocator.h').write_text(h)
native=(S/'native-allocator.c').read_text();s='#include "'+str(P/'source/transport.h')+'"\n'+native;anchor='// ops that return true';pos=s.index(anchor);s=s[:pos]+'#include "allocator-api.h"\n\n'+s[pos:];pos=s.index('ggml_gallocr_t ggml_gallocr_new_n(');s=s[:pos]+'#include "allocator.h"\n\n'+s[pos:]
for name,r in json.loads((S/'recipes.json').read_text()).items():
 pat=r'(?m)^[A-Za-z_][\w ]*?\s+(?:\*+\s*)?'+re.escape(name)+r'\s*\([^;{}]*\)\s*\{';m=re.search(pat,s);assert m,name;call='FG161_CALL('+str(r['mode'])+','+','.join('FG161_A('+x+')' for x in r['args'])+')';kind=r['kind'];pre=r['pre'] or ''
 if pre and kind.startswith('struct'):code=pre+call+';return fg_result;'
 elif kind=='void':code=pre+call+';return;'
 else:code=pre+'return '+('(void *)(intptr_t)' if kind=='ptr' else '('+kind+')')+call+';'
 s=s[:m.end()]+'\n    if(fg161_enabled){'+code+'}\n'+s[m.end():]
(S/'ggml-allocator.c').write_text(s)
p=Path('/home/end0/AI/LlamaServer/franken-goose-exp149-cpu-control-20261107/variants/cpu-backend/build/commands.json');r=next(x for x in json.loads(p.read_text()) if x['label']=='base-ggmlalloc');cmd=r['argv'].copy();old=cmd[cmd.index('-o')+1];obj=B/'allocator.o';cmd[cmd.index('-o')+1]=str(obj);cmd[cmd.index('-c')+1]=str(S/'ggml-allocator.c');run('allocator-core-build',cmd,Path(r['cwd']))
oldrows=json.loads((P/'build-commands.json').read_text());r=next(x for x in oldrows if x['label']=='shared-base-link');cmd=r['argv'].copy();ref=B/'unchanged-exp160.so';cmd[cmd.index('-o')+1]=str(ref);run('unchanged-exp160-link-integrity',cmd,Path(r['cwd']));baseline=(F/'runtime/exp160/run/libggml-base.so').resolve();assert H(ref)==H(baseline);assert cmd.count(old)==1;cmd=[str(obj) if x==old else x for x in cmd];out=R/('libggml-base-allocator-'+H(obj)[:12]+'.so.0.27.0');linkout=B/'relink.so' if out.exists() else out;cmd[cmd.index('-o')+1]=str(linkout);run('allocator-base-link',cmd,Path(r['cwd']));
if out!=linkout:assert H(out)==H(linkout)
for f in (F/'runtime/exp160/run').iterdir():
 if f.is_file() or f.is_symlink():
  q=R/f.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise RuntimeError(str(q))
  q.symlink_to(out if f.name.startswith('libggml-base') else f.resolve())
(S/'manifest.json').write_text(json.dumps(dict(status='LINKED_PENDING_CORRECTNESS',compiler_sha256=H(C),entry=entry,library=str(out),sha256=H(out),core_bodies=list(json.loads((S/'recipes.json').read_text())),rollback='GGML_GOOSE_ALLOCATOR=0 or previous immutable EXP160 runtime',qualified_scope='Release GGML allocator module;GGML_ALLOCATOR_DEBUG stays native;backend bulk-plan/provider internals remain separate',source_reproduction=json.loads((B/'source-reproduction.json').read_text()),production_changed=False),indent=2)+'\n');print('LINKED_GOOSE_ALLOCATOR',out)
