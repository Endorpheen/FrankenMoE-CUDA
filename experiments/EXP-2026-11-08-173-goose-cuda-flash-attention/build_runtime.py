from pathlib import Path
import json,re,shlex,subprocess,hashlib,os
F=Path(__file__).resolve().parents[2];E=F/'experiments/EXP-2026-11-08-173-goose-cuda-flash-attention';S=E/'source';B=F/'runtime/exp173/build';R=F/'runtime/exp173/run';R.mkdir(exist_ok=True);H=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();rows=[]
assert H(F/'runtime/compiler/stage3-f849233b999c')=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a';assert all(H(p)==h for p,h in json.loads((E/'external-header-pins.json').read_text()).items())
def run(label,args,cwd=B):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,capture_output=True,text=True,timeout=900);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'runtime-commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,flush=True)
 if q.returncode:print((q.stdout+q.stderr)[-10000:]);raise SystemExit(q.returncode)
 return q
run('verify-selfhosted-compute-source',['python3',E/'verify_generated.py'])
D=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda');db=json.loads((D/'compile_commands.json').read_text());r=next(x for x in db if x['file'].endswith('/fattn-vec-instance-q4_0-q4_0.cu'));cmd=shlex.split(r['command']);cmd.insert(1,'-I'+str(Path(r['file']).parent.parent));cmd.insert(1,'-I'+str(S));cmd[cmd.index('-c')+1]=str(S/'cuda-runtime.cu');cmd[cmd.index('-o')+1]=str(B/'cuda-runtime.o');run('cuda-fa-runtime-build',cmd,Path(r['directory']));run('fa-host-C-build',['/usr/bin/gcc-13','-std=gnu11','-O3','-DNDEBUG','-fPIC','-fvisibility=hidden','-Werror=implicit-int','-Werror=implicit-function-declaration','-c',S/'host-unit.c','-o',B/'host.o'])
prev=json.loads((F/'runtime/exp172/combined-build/commands.json').read_text());lr=next(x for x in prev if x['label']=='cuda-gdn-link');link=lr['argv'].copy();orig=next(x for x in link if x.endswith('/fattn.cu.o'));obj=Path(lr['cwd'])/orig if not Path(orig).is_absolute()else Path(orig);nm=run('native-fa-symbols',['nm','--defined-only',obj]);mapping={};names={'ggml_cuda_flash_attn_ext','ggml_cuda_flash_attn_ext_supported','ggml_cuda_flash_attn_ext_get_alloc_size'}
for line in nm.stdout.splitlines():
 parts=line.split()
 if len(parts)<3:continue
 symbol=parts[-1];m=re.match(r'_Z(\d+)',symbol)
 if not m:continue
 n=int(m[1]);name=symbol[m.end():m.end()+n]
 if name in names:new='fg175_native_'+name;mapping[symbol]='_Z'+str(len(new))+new+symbol[m.end()+n:]
assert len(mapping)==3;rename=['objcopy']
for a,b in mapping.items():rename+=['--redefine-sym',a+'='+b]
rename+=[str(obj),str(B/'native-fattn-host.o')];run('private-original-fa-host',rename)
ref=link.copy();ref[ref.index('-o')+1]=str(B/'unchanged-exp172.so');ref=[('-Wl,--dependency-file='+str(B/'unchanged-fa.d'))if x.startswith('-Wl,--dependency-file=')else x for x in ref];run('unchanged-exp172-link-integrity',ref,Path(lr['cwd']));assert H(B/'unchanged-exp172.so')==H(F/'runtime/exp172/run/libggml-cuda.so')
vm=(F/'experiments/EXP-2026-11-08-172-goose-cuda-gdn/source/visibility.map').read_text().replace('local:','fg175fa_cuda_host_seen; fg175fa_cuda_device_read; local:').replace(' };',' *fg175_native*; *fg175v_*; *fg175m_*; *fg175f_*; *fg175q_*; fg175*; fg176i_*; fg168f_*; };');(S/'visibility.map').write_text(vm)
link=[str(B/'native-fattn-host.o')if x==orig else x for x in link];link=[('-Wl,--version-script='+str(S/'visibility.map'))if x.startswith('-Wl,--version-script=')else x for x in link];link+=[str(B/'cuda-runtime.o'),str(B/'host.o')];out=R/('libggml-cuda-fa-'+hashlib.sha256((H(B/'cuda-runtime.o')+H(B/'host.o')+H(B/'native-fattn-host.o')+H(S/'visibility.map')).encode()).hexdigest()[:12]+'.so.0.27.0');linkout=B/'runtime-relink.so'if out.exists()else out;link[link.index('-o')+1]=str(linkout);run('cuda-fa-runtime-link',link,Path(lr['cwd']));
if linkout!=out:assert H(linkout)==H(out),'Immutable runtime replay differs'
for p in (F/'runtime/exp172/run').iterdir():
 if p.is_file()or p.is_symlink():
  q=R/p.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise RuntimeError(str(q))
  q.symlink_to(out if p.name.startswith('libggml-cuda')else p.resolve())
manifest=dict(status='LINKED_PENDING_JOINT_FA_CORRECTNESS_AND_APEX',library=str(out),sha256=H(out),previous_sha256=H(F/'runtime/exp172/run/libggml-cuda.so'),original_fattn_object=str(obj),original_fattn_object_sha256=H(obj),private_original_fattn_object_sha256=H(B/'native-fattn-host.o'),symbol_renames=mapping,numerical_original_callbacks_on_active_path=False,compiler_sha256=H(F/'runtime/compiler/stage3-f849233b999c'),production_changed=False,active_runtime_changed=False);(E/'runtime-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print('LINKED_PENDING_FA_GATE',out)
