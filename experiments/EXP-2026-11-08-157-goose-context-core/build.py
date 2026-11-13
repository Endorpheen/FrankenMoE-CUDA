from pathlib import Path
import hashlib,json,re,subprocess,os
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-157-goose-context-core';S=E/'source';prior=F/'experiments/EXP-2026-11-08-156-goose-tensor-constructors';P155=F/'experiments/EXP-2026-11-08-155-goose-tensor-geometry';B=F/'runtime/exp157/build';R=F/'runtime/exp157/run';B.mkdir(parents=True,exist_ok=True);R.mkdir(parents=True,exist_ok=True);C=F/'runtime/compiler/stage3-f849233b999c';assert hashlib.sha256(C.read_bytes()).hexdigest()=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a';rows=[]
def run(label,args,cwd=B,env=None):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=240);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,flush=True)
 if q.returncode:print(q.stderr[-6500:]);raise SystemExit(q.returncode)
 return q
baseline=Path(json.loads((prior/'source/manifest.json').read_text())['library']);include='/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/include'
run('metadata-probe-build',['/usr/bin/g++-13','-std=c++17','-O2','-I'+include,S/'probe.cpp','-ldl','-o',B/'metadata-probe'])
q=run('metadata-probe',[B/'metadata-probe',baseline],env=dict(os.environ,LD_LIBRARY_PATH=str(F/'runtime/exp156/run')));types=[];labels=[];counts=[]
for line in q.stdout.splitlines():
 v=line.split()
 if v[0]=='C':counts=list(map(int,v[1:]))
 if v[0]=='T':types.append(dict(index=int(v[1]),block=int(v[2]),interleave=int(v[3]),bytes=int(v[4]),quantized=int(v[5]),to_float=bool(int(v[6])),from_float_ref=bool(int(v[7])),name=None if v[8]=='-' else list(bytes.fromhex(v[8]))))
 if v[0]=='L':labels.append(dict(group=int(v[1]),index=int(v[2]),bytes=list(bytes.fromhex(v[3]))))
assert len(types)==counts[0];assert max([len(t['name'] or []) for t in types]+[len(l['bytes']) for l in labels])<128
native=(prior/'source/ggml-constructors.c').read_text();header=Path(include+'/ggml.h').read_text();clean=re.sub(r'/\*.*?\*/|//[^\n]*','',header,flags=re.S);values={name:int(n) for name,n in re.findall(r'\b(GGML_(?:TYPE|FTYPE)_\w+)\s*=\s*(-?\d+)',clean)};mapping=[(a,b) for a,b in re.findall(r'case\s+(GGML_FTYPE_\w+):\s*wtype\s*=\s*(GGML_TYPE_\w+)',native) if b!='GGML_TYPE_COUNT'];assert mapping
registry=['fn initialize_registry(){']
for t in types:
 i=t['index'];registry.append(f'type_begin({i});')
 for k,key in enumerate(['block','interleave','bytes','quantized']):registry.append(f'type_put({i},{k},{t[key]} as u64);')
 if t['name'] is None:registry.append(f'type_put({i},4,0 as u64);')
 else:
  for j,c in enumerate(t['name']+[0]):registry.append(f'type_byte({i},{j},{c} as u64);')
for l in labels:
 g=l['group'];i=l['index'];registry.append(f'label_begin({g},{i});')
 for j,c in enumerate(l['bytes']+[0]):registry.append(f'label_byte({g},{i},{j},{c} as u64);')
registry.append('}\nfn ftype_map(x:i64)->i64{')
for a,b in mapping:registry.append(f'if x=={values[a]}{{return {values[b]};}}')
registry.append('fail(30);return 0;}');data='\n'.join(registry);(S/'registry-data.goose').write_text(data+'\n');policy=(S/'context-policy.goose').read_text();assert '// FG157_REGISTRY_DATA' in policy;(S/'context.goose').write_text(policy.replace('// FG157_REGISTRY_DATA',data));facts=dict(counts=counts,types=types,labels=labels,ftype_map=[dict(source=a,key=values[a],target=b,value=values[b]) for a,b in mapping],baseline_sha256=hashlib.sha256(baseline.read_bytes()).hexdigest());(S/'metadata-facts.json').write_text(json.dumps(facts,indent=2)+'\n')
run('context-core-selfhosted-emit',[C,'-o',S/'context-generated.c',S/'context.goose']);g=(S/'context-generated.c').read_text();main=g[g.rfind('static void gg_function_0('):];entry=re.search(r'(gg_function_\d+)\(t\d+, t\d+, base',main);assert entry,main[-1000:];entry='fg157_'+entry.group(1);(S/'context-qualified.c').write_text(re.sub(r'\b(gg_|ggf_|gs_)',lambda m:'fg157_'+m.group(1),g));h=(S/'core.h').read_text();h=re.sub(r'(?m)^#define FG157_ENTRY .*\n','',h);h=h.replace('#define main fg157_unused_generated_main','#define FG157_ENTRY '+entry+'\n#define main fg157_unused_generated_main');(S/'core.h').write_text(h)
s=native;anchor='void * ggml_aligned_malloc(';pos=s.index(anchor);s=s[:pos]+'#include "core-api.h"\n\n'+s[pos:];s=s.replace('static const struct ggml_type_traits type_traits[','static const struct ggml_type_traits fg157_native_traits[',1);anchor='const struct ggml_type_traits * ggml_get_type_traits(';pos=s.index(anchor);s=s[:pos]+'#define type_traits (fg157_enabled?fg157_goose_traits:fg157_native_traits)\n\n'+s[pos:]
macros=[]
for i,name in enumerate(['GGML_OP_NAME','GGML_OP_SYMBOL','GGML_UNARY_OP_NAME','GGML_GLU_OP_NAME']):
 new='fg157_native_'+name.lower();s=s.replace('static const char * '+name+'[','static const char * '+new+'[',1);macros.append('#define '+name+' (fg157_enabled?fg157_label_pointers['+str(i)+']:'+new+')')
anchor='#include "constructors.h"';pos=s.index(anchor)+len(anchor);s=s[:pos]+'\n'+'\n'.join(macros)+'\n#include "core.h"\n'+s[pos:];replacements={}
def inject(name,code):
 global s
 pat=r'(?m)^[A-Za-z_][\w ]*?\s+(?:\*\s*)?'+re.escape(name)+r'\s*\([^;{}]*\)\s*\{';m=re.search(pat,s);assert m,name;pos=m.end();s=s[:pos]+'\n    if(fg157_enabled){'+code+'}\n'+s[pos:];replacements[name]=code
A=lambda x:'FG157_ARG('+x+')';call=lambda mode,args:'FG157_CALL('+str(mode)+','+','.join(A(x) for x in args)+')';retptr=lambda mode,args:'return (void *)(intptr_t)'+call(mode,args)+';';void=lambda mode,args:call(mode,args)+';return;'
inject('ggml_init',retptr(0,['params.mem_size','params.mem_buffer','params.no_alloc']));inject('ggml_reset',void(1,['ctx']));inject('ggml_free',void(2,['ctx']));inject('ggml_used_mem','return (size_t)'+call(3,['ctx'])+';');inject('ggml_get_no_alloc','return '+call(4,['ctx'])+'!=0;');inject('ggml_set_no_alloc',void(5,['ctx','no_alloc']));inject('ggml_get_mem_buffer',retptr(6,['ctx']));inject('ggml_get_mem_size','return (size_t)'+call(7,['ctx'])+';');inject('ggml_aligned_malloc',retptr(8,['size']));inject('ggml_aligned_free',void(9,['ptr']));inject('ggml_malloc',retptr(10,['size']));inject('ggml_calloc',retptr(11,['num','size']))
for name,k,kind in [('ggml_blck_size',0,'int64_t'),('ggml_type_size',2,'size_t'),('ggml_is_quantized',3,'bool')]:inject(name,'return ('+kind+')'+call(12,['type',str(k)])+';')
inject('ggml_get_type_traits',retptr(12,['type','5']));inject('ggml_type_name',retptr(12,['type','4']));inject('ggml_type_sizef','return fg157_decode_f64('+call(18,['type'])+');')
for name,mode,arg in [('ggml_op_name',13,'op'),('ggml_op_symbol',14,'op'),('ggml_unary_op_name',15,'op'),('ggml_glu_op_name',16,'op'),('ggml_op_desc',17,'t'),('ggml_status_to_string',27,'status')]:inject(name,retptr(mode,[arg]))
inject('ggml_ftype_to_ggml_type','return (enum ggml_type)'+call(19,['ftype'])+';')
for which,name in enumerate(['ggml_set_input','ggml_set_output','ggml_set_param','ggml_set_loss']):inject(name,void(20,['tensor',str(which)]))
for name,k,kind,arg in [('ggml_get_data',0,'ptr','tensor'),('ggml_get_data_f32',1,'ptr','tensor'),('ggml_get_name',2,'ptr','tensor'),('ggml_get_unary_op',3,'enum ggml_unary_op','tensor'),('ggml_get_glu_op',4,'enum ggml_glu_op','tensor'),('ggml_is_view',5,'bool','t')]:inject(name,retptr(21,[arg,str(k)]) if kind=='ptr' else 'return ('+kind+')'+call(21,[arg,str(k)])+';')
inject('ggml_set_name',retptr(22,['tensor','name']));inject('ggml_tensor_overhead','return (size_t)'+call(24,['0'])+';');(S/'ggml-context-core.c').write_text(s)
recipes=json.loads((F/'runtime/exp156/build/commands.json').read_text());r=next(r for r in recipes if r['label']=='constructors-shared-core-build');args=r['argv'].copy();args[1:1]=['-I'+str(prior/'source')];old=args[args.index('-o')+1];obj=B/'ggml-context-core.o';args[args.index('-o')+1]=str(obj);args[args.index('-c')+1]=str(S/'ggml-context-core.c');run('context-core-build',args,Path(r['cwd']));r=next(r for r in recipes if r['label']=='constructors-shared-base-link');reference=r['argv'].copy();refout=B/'unchanged-exp156-reference.so';reference[reference.index('-o')+1]=str(refout);run('unchanged-exp156-link-integrity',reference,Path(r['cwd']));assert hashlib.sha256(refout.read_bytes()).digest()==hashlib.sha256(baseline.read_bytes()).digest();args=r['argv'].copy();assert args.count(old)==1;args=[str(obj) if x==old else x for x in args];out=R/('libggml-base-context-core-'+hashlib.sha256(obj.read_bytes()).hexdigest()[:12]+'.so.0.27.0');assert not out.exists();args[args.index('-o')+1]=str(out);run('context-core-link',args,Path(r['cwd']))
for f in (F/'runtime/exp156/run').iterdir():
 if f.is_file() or f.is_symlink():
  p=R/f.name
  if p.is_symlink():p.unlink()
  elif p.exists():raise RuntimeError(str(p))
  p.symlink_to(out if f.name.startswith('libggml-base') else f.resolve())
(S/'manifest.json').write_text(json.dumps(dict(status='LINKED_PENDING_CORRECTNESS',compiler_sha256=hashlib.sha256(C.read_bytes()).hexdigest(),entry=entry,replaced_functions=list(replacements),active_registry='Initialized by Goose from metadata-facts.json;native declarations only rollback/numerical callback addresses',library=str(out),sha256=hashlib.sha256(out.read_bytes()).hexdigest(),rollback='GGML_GOOSE_CONTEXT_CORE=0',qualified_host='Linux POSIX,x86_64;other allocation adapters native/unqualified',production_changed=False),indent=2)+'\n');print('LINKED_CONTEXT_LIFECYCLE_AND_METADATA',out)
