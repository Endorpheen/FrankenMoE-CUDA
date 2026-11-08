from pathlib import Path
import hashlib,json,re,subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-156-goose-tensor-constructors';S=E/'source';prior=F/'experiments/EXP-2026-11-08-155-goose-tensor-geometry';B=F/'runtime/exp156/build';R=F/'runtime/exp156/run';B.mkdir(parents=True,exist_ok=True);R.mkdir(parents=True,exist_ok=True);C=F/'runtime/compiler/stage3-f849233b999c';assert hashlib.sha256(C.read_bytes()).hexdigest()=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a';rows=[]
def run(label,args,cwd=B):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,capture_output=True,text=True,timeout=180);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,flush=True)
 if q.returncode:print(q.stderr[-6000:]);raise SystemExit(q.returncode)
run('constructors-selfhosted-emit',[C,'-o',S/'constructors-generated.c',S/'constructors.goose']);g=(S/'constructors-generated.c').read_text();body=g[g.rfind('static void gg_function_0('):];entry=re.search(r'(gg_function_\d+)\(t\d+, t\d+, base',body);assert entry,body[-2000:];entry='fg156_'+entry.group(1);qualified=re.sub(r'\b(gg_|ggf_|gs_)',lambda m:'fg156_'+m.group(1),g);(S/'constructors-qualified.c').write_text(qualified)
h=(S/'constructors.h').read_text();h=re.sub(r'(?m)^#define FG156_ENTRY .*\n','',h);h=h.replace('#define main fg156_unused_generated_main','#define FG156_ENTRY '+entry+'\n#define main fg156_unused_generated_main');(S/'constructors.h').write_text(h)
s=(prior/'source/ggml-geometry.c').read_text();anchor='#include "geometry.h"';pos=s.index(anchor)+len(anchor);s=s[:pos]+'\n#include "constructors.h"\n'+s[pos:];replacements={}
def inject(name,code):
 global s
 pattern=r'(?m)^(?:static )?(?:(?:struct\s+\w+\s*\*)|(?:void\s*\*)|void|size_t)\s*'+re.escape(name)+r'\s*\([^;{}]*\)\s*\{';m=re.search(pattern,s);assert m,name;pos=m.end();s=s[:pos]+'\n    if(fg156_enabled){'+code+'}\n'+s[pos:];replacements[name]=code
A=lambda x:'FG156_ARG('+x+')'
call=lambda mode,args:'FG156_CALL('+str(mode)+','+','.join(A(x) for x in args)+')'
ret=lambda mode,args:'return (void *)(intptr_t)'+call(mode,args)+';'
inject('ggml_new_object',ret(0,['ctx','type','size']))
inject('ggml_new_tensor_impl',ret(1,['ctx','type','n_dims','ne','view_src','view_offs']))
inject('ggml_dup_tensor',ret(2,['ctx','src']))
inject('ggml_view_tensor',ret(3,['ctx','src']))
inject('ggml_reshape',ret(4,['ctx','a','GGML_MAX_DIMS','b->ne']))
for n in range(1,5):
 dims='const int64_t fg_ne[]={'+','.join('ne'+str(i) for i in range(n))+'};'
 inject('ggml_reshape_'+str(n)+'d',dims+ret(4,['ctx','a',str(n),'fg_ne']))
inject('ggml_view_impl',ret(14,['ctx','a','n_dims','ne','offset']))
for n in range(1,5):
 dims='const int64_t fg_ne[]={'+','.join('ne'+str(i) for i in range(n))+'};'
 strides=['nb'+str(i) if i<n else '0' for i in range(1,4)]
 inject('ggml_view_'+str(n)+'d',dims+(ret(14,['ctx','a',str(n),'fg_ne','offset']) if n==1 else ret(5,['ctx','a',str(n),'fg_ne','offset',*strides])))
inject('ggml_permute',ret(6,['ctx','a','axis0','axis1','axis2','axis3']))
inject('ggml_transpose',ret(7,['ctx','a']))
inject('ggml_get_first_tensor',ret(8,['ctx']))
inject('ggml_get_next_tensor',ret(9,['ctx','tensor']))
inject('ggml_get_tensor',ret(10,['ctx','name']))
inject('ggml_new_buffer',ret(11,['ctx','nbytes']))
inject('ggml_get_max_tensor_size','return (size_t)'+call(12,['ctx'])+';')
inject('ggml_unravel_index',call(13,['tensor','i','i0','i1','i2','i3'])+';return;')
(S/'ggml-constructors.c').write_text(s)
recipes=json.loads((F/'runtime/exp155/build/commands.json').read_text());r=next(r for r in recipes if r['label']=='geometry-shared-core-build');args=r['argv'].copy();args[1:1]=['-I'+str(prior/'source')];obj=B/'ggml-constructors.o';old=args[args.index('-o')+1];args[args.index('-o')+1]=str(obj);args[args.index('-c')+1]=str(S/'ggml-constructors.c');run('constructors-shared-core-build',args,Path(r['cwd']))
r=next(r for r in recipes if r['label']=='geometry-shared-base-link');ref=r['argv'].copy();refout=B/'unchanged-exp155-reference.so';ref[ref.index('-o')+1]=str(refout);run('unchanged-exp155-link-integrity',ref,Path(r['cwd']));expected=Path(json.loads((prior/'source/manifest.json').read_text())['library']);assert hashlib.sha256(refout.read_bytes()).digest()==hashlib.sha256(expected.read_bytes()).digest()
args=r['argv'].copy();assert args.count(old)==1;args=[str(obj) if x==old else x for x in args];out=R/('libggml-base-constructors-'+hashlib.sha256(obj.read_bytes()).hexdigest()[:12]+'.so.0.27.0');assert not out.exists();args[args.index('-o')+1]=str(out);run('constructors-shared-base-link',args,Path(r['cwd']))
for f in (F/'runtime/exp155/run').iterdir():
 if f.is_file() or f.is_symlink():
  q=R/f.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise RuntimeError(str(q))
  q.symlink_to(out if f.name.startswith('libggml-base') else f.resolve())
(S/'manifest.json').write_text(json.dumps(dict(status='LINKED_PENDING_DIFFERENTIAL',compiler_sha256=hashlib.sha256(C.read_bytes()).hexdigest(),entry=entry,replaced_functions=list(replacements),inherited='Verified EXP155 geometry and original fast numerical kernels',library=str(out),sha256=hashlib.sha256(out.read_bytes()).hexdigest(),rollback='GGML_GOOSE_TENSOR_CONSTRUCTORS=0',production_changed=False),indent=2)+'\n');print('LINKED_SOLO_GOOSE_CONSTRUCTORS',out)
