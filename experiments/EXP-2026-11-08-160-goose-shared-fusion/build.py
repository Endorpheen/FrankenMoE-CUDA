from pathlib import Path
import json,re,hashlib,subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-160-goose-shared-fusion';S=E/'source';P=F/'experiments/EXP-2026-11-08-159-goose-graph-lifecycle';B=F/'runtime/exp160/build';R=F/'runtime/exp160/run';B.mkdir(parents=True,exist_ok=True);R.mkdir(parents=True,exist_ok=True);C=F/'runtime/compiler/stage3-f849233b999c';assert hashlib.sha256(C.read_bytes()).hexdigest()=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a';rows=[]
def run(label,args,cwd=B):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,capture_output=True,text=True,timeout=600);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,flush=True)
 if q.returncode:print((q.stdout+q.stderr)[-6000:]);raise SystemExit(q.returncode)
 return q
H=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
previous=json.loads((P/'build-commands.json').read_text());last={r['label']:r for r in previous};cpu150=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-expert-registry-20261107');cpu_cmds={r['label']:r for r in json.loads((cpu150/'variants/cpu-backend/build/commands.json').read_text())};cmd149={r['label']:r for r in json.loads(Path('/home/end0/AI/LlamaServer/franken-goose-exp149-cpu-control-20261107/variants/cpu-backend/build/commands.json').read_text())};cuda107=json.loads(Path('/home/end0/AI/LlamaServer/exp107-expert-overlap/build/commands.json').read_text());cuda_cwd=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda');pins={};refs={}
for kind,r,library in [('base',last['graph-base-link'],(F/'runtime/exp159/run/libggml-base.so').resolve()),('cpu',cpu_cmds['link-cpu'],(F/'runtime/exp159/run/libggml-cpu.so').resolve()),('cuda',dict(argv=cuda107['link-cuda'],cwd=str(cuda_cwd)),(F/'runtime/exp159/run/libggml-cuda.so').resolve())]:
 cmd=r['argv'].copy();out=B/('unchanged-'+kind+'.so');cmd[cmd.index('-o')+1]=str(out);cmd=[('-Wl,--dependency-file='+str(B/('reference-'+kind+'.d'))) if x.startswith('-Wl,--dependency-file=') else x for x in cmd];run('unchanged-'+kind+'-link-integrity',cmd,Path(r['cwd']));assert H(out)==H(library),(kind,H(out),H(library));pins[kind]=dict(path=str(library),sha256=H(library));refs[kind]=r
run('shared-selfhosted-emit',[C,'-o',S/'service-generated.c',S/'service.goose']);g=(S/'service-generated.c').read_text();main=g[g.rfind('static void gg_function_0('):];entry=re.search(r'(gg_function_\d+)\(t\d+, t\d+, base',main);assert entry,main[-1500:];entry='fg160_'+entry[1];(S/'service-qualified.c').write_text(re.sub(r'\b(gg_|ggf_|gs_)',lambda m:'fg160_'+m[1],g));h=(S/'service.h').read_text();h=re.sub(r'(?m)^#define FG160_ENTRY .*$', '#define FG160_ENTRY '+entry,h);(S/'service.h').write_text(h)
s=(P/'source/ggml-graph-lifecycle.c').read_text().replace('#include "ggml-impl-graph.h"','#include "core-impl.h"',1);pos=s.index('struct ggml_tensor * ggml_rope_set_offset(');s=s[:pos]+'#include "service.h"\n\n'+s[pos:]
for name,(mode,args,ty) in json.loads((S/'core-recipes.json').read_text()).items():
 pat=r'(?m)^[A-Za-z_][\w ]*?\s+(?:\*+\s*)?'+re.escape(name)+r'\s*\([^;{}]*\)\s*\{';m=re.search(pat,s);assert m,name;pre=''
 if name.startswith('ggml_pad_ext'):pre='const int32_t fg_pad[]={lp0,rp0,lp1,rp1,lp2,rp2,lp3,rp3};'
 call='fg160_packet('+str(mode)+','+','.join('FG160_A('+x+')' for x in args+['0']*(7-len(args)))+')';code=pre+'return '+('(void *)(intptr_t)' if ty=='ptr' else '('+ty+')')+call+';';s=s[:m.end()]+'\n    if(fg160_fusion_enabled){'+code+'}\n'+s[m.end():]
(S/'ggml-shared.c').write_text(s)
compile_recipes=[('shared-core',last['graph-core-build'],S/'ggml-shared.c',0),('shared-scheduler',cmd149['backend-base'],None,3),('shared-meta',cmd149['base-backendmeta'],None,4),('shared-cpu',cpu_cmds['core'],None,1),('shared-cuda',dict(argv=cuda107['cuda'],cwd=str(cuda_cwd)),None,2)];objects={};original_objects={};source_pins={}
for label,r,source,role in compile_recipes:
 cmd=r['argv'].copy();oldsource=Path(cmd[cmd.index('-c')+1]);oldobj=cmd[cmd.index('-o')+1];original_objects[label]=oldobj;source_pins[label]=dict(original=str(oldsource),sha256=H(oldsource))
 if source is None:
  source=S/(label+oldsource.suffix);source.write_text('#define FG160_CALLER '+str(role)+'\n#include "'+str(S/'transport.h')+'"\n'+oldsource.read_text())
 cmd.insert(1,'-I'+str(oldsource.parent));cmd[cmd.index('-c')+1]=str(source);obj=B/(label+'.o');cmd[cmd.index('-o')+1]=str(obj)
 # Core retains its earlier module include chain;the previous source folder owns graph.h.
 if role==0:cmd.insert(1,'-I'+str(P/'source'))
 receipt=json.loads((B/'caller-reuse.json').read_text()) if (B/'caller-reuse.json').exists() else {}
 if label in receipt:
  old=receipt[label];assert cmd==old['argv'] and str(r['cwd'])==old['cwd'];assert H(obj)==old['sha256'];assert all(H(path)==sha for path,sha in old['inputs'].items());rows.append(dict(label=label+'-build',argv=cmd,cwd=str(r['cwd']),exit_code=0,reused_verified_object=True,object_sha256=H(obj)));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');print(label+'-reuse',0,flush=True)
 else:run(label+'-build',cmd,Path(r['cwd']))
 objects[label]=obj
replacements={original_objects[k]:str(v) for k,v in objects.items()};outputs={}
for kind,labels in [('base',['shared-core','shared-scheduler','shared-meta']),('cpu',['shared-cpu']),('cuda',['shared-cuda'])]:
 r=refs[kind];cmd=r['argv'].copy();selected={original_objects[k]:str(objects[k]) for k in labels}
 for old in selected:assert cmd.count(old)==1,(kind,old)
 cmd=[selected.get(x,x) for x in cmd];cmd=[('-Wl,--dependency-file='+str(B/(kind+'.d'))) if x.startswith('-Wl,--dependency-file=') else x for x in cmd]
 if kind!='base':cmd=[str(outputs['base']) if Path(x).name.startswith('libggml-base.so') else x for x in cmd]
 out=R/('libggml-'+kind+'-shared-'+hashlib.sha256(''.join(H(objects[k]) for k in labels).encode()).hexdigest()[:12]+'.so.0.27.0');linkout=B/('relink-'+kind+'.so') if out.exists() else out;cmd[cmd.index('-o')+1]=str(linkout);cmd+=['-Wl,-z,defs'];run('shared-'+kind+'-link',cmd,Path(r['cwd']));
 if linkout!=out:assert H(linkout)==H(out),(kind,'existing immutable artifact differs')
 outputs[kind]=out
for f in (F/'runtime/exp159/run').iterdir():
 if f.is_file() or f.is_symlink():
  q=R/f.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise RuntimeError(str(q))
  target=next((outputs[k] for k in ['base','cpu','cuda'] if f.name.startswith('libggml-'+k)),f.resolve());q.symlink_to(target)
(S/'manifest.json').write_text(json.dumps(dict(status='LINKED_PENDING_CORRECTNESS',compiler_sha256=H(C),entry=entry,libraries={k:dict(path=str(v),sha256=H(v)) for k,v in outputs.items()},baseline_libraries=pins,source_pins=source_pins,header_fusion=list(json.loads((S/'header-recipes.json').read_text())),core_helpers=list(json.loads((S/'core-recipes.json').read_text())),roles=['core','cpu','cuda','scheduler','meta'],rollback=['GGML_GOOSE_FUSION=0','GGML_GOOSE_GRAPH_LIFECYCLE=0','previous EXP159 runtime'],production_changed=False),indent=2)+'\n');print('LINKED_SHARED_GOOSE_FUSION',json.dumps({k:str(v) for k,v in outputs.items()}))
