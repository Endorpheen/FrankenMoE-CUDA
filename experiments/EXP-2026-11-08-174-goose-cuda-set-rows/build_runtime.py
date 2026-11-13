from pathlib import Path
import subprocess,json,re,shlex,hashlib
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-174-goose-cuda-set-rows';S=E/'source';B=F/'runtime/exp174/build';R=F/'runtime/exp174/run';H=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();rows=[]
assert all(H(p)==h for p,h in json.loads((E/'external-header-pins.json').read_text()).items())
def run(label,args,cwd=B):
 q=subprocess.run(list(map(str,args)),cwd=cwd,capture_output=True,text=True,timeout=900);rows.append(dict(label=label,argv=list(map(str,args)),cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,flush=True)
 if q.returncode:print((q.stdout+q.stderr)[-8000:]);raise SystemExit(q.returncode)
 return q
D=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda');db=json.loads((D/'compile_commands.json').read_text());r=next(x for x in db if x['file'].endswith('/set-rows.cu'));cmd=shlex.split(r['command']);cmd.insert(1,'-I'+str(Path(r['file']).parent));cmd.insert(1,'-I'+str(S));cmd[cmd.index('-c')+1]=str(S/'cuda-runtime.cu');cmd[cmd.index('-o')+1]=str(B/'cuda-runtime.o');run('cuda-setrows-build',cmd,Path(r['directory']));run('host-C-build',['gcc-13','-std=gnu11','-O3','-DNDEBUG','-fPIC','-fvisibility=hidden','-Werror=implicit-int','-Werror=implicit-function-declaration','-c',S/'host-unit.c','-o',B/'host.o'])
prev=json.loads((F/'experiments/EXP-2026-11-08-173-goose-cuda-flash-attention/private-abi-link-fix.json').read_text());link=prev['link_argv'].copy();cwd=Path(prev['link_cwd']);orig=next(x for x in link if x.endswith('/set-rows.cu.o'));obj=Path(orig)if Path(orig).is_absolute()else cwd/orig
nm=run('native-setrows-symbols',['nm','--defined-only',obj]);symbols=[]
for ln in nm.stdout.splitlines():
 ps=ln.split()
 if len(ps)<3:continue
 sym=ps[-1];m=re.match(r'_Z(\d+)',sym)
 if m and sym[m.end():m.end()+int(m[1])]=='ggml_cuda_op_set_rows':symbols.append(sym)
assert len(symbols)==1;old=symbols[0];m=re.match(r'_Z(\d+)',old);newname='fg176r_native_ggml_cuda_op_set_rows';new='_Z'+str(len(newname))+newname+old[m.end()+int(m[1]):];run('private-native-setrows',['objcopy','--redefine-sym',old+'='+new,obj,B/'native-set-rows.o'])
ref=link.copy();ref[ref.index('-o')+1]=str(B/'unchanged-exp173.so');run('unchanged-exp173-link-integrity',ref,cwd);assert H(B/'unchanged-exp173.so')==H(F/'runtime/exp173/run/libggml-cuda.so')
vm=(F/'experiments/EXP-2026-11-08-173-goose-cuda-flash-attention/source/visibility.map').read_text();vm=vm.replace('local:','fg176r_cuda_host_seen; fg176r_cuda_device_read; local:').replace(' *fg175*;',' *fg176r*; *fg175s*; *fg176rs_*; *fg176rd_*; *fg175*;');(S/'visibility.map').write_text(vm)
link=[str(B/'native-set-rows.o')if x==orig else ('-Wl,--version-script='+str(S/'visibility.map'))if x.startswith('-Wl,--version-script=')else x for x in link];link+=[str(B/'cuda-runtime.o'),str(B/'host.o')];out=R/('libggml-cuda-setrows-'+hashlib.sha256((H(B/'cuda-runtime.o')+H(B/'host.o')+H(B/'native-set-rows.o')+H(S/'visibility.map')).encode()).hexdigest()[:12]+'.so.0.27.0');replay=B/'runtime-replay.so' if out.exists() else out;link[link.index('-o')+1]=str(replay);run('cuda-setrows-link',link,cwd)
if replay!=out:assert H(replay)==H(out),'Immutable runtime replay differs'
for p in(F/'runtime/exp173/run').iterdir():
 if p.is_file()or p.is_symlink():
  q=R/p.name;target=out if p.name.startswith('libggml-cuda')else p.resolve()
  if q.is_symlink():
   if q.resolve()==target:continue
   q.unlink()
  elif q.exists():raise RuntimeError('Refuse to overwrite regular runtime artifact '+str(q))
  q.symlink_to(target)
(E/'runtime-manifest.json').write_text(json.dumps(dict(status='LINKED_PENDING_HOST_GPU_APEX',library=str(out),sha256=H(out),base_library_sha256=H(F/'runtime/exp173/run/libggml-cuda.so'),original_object=str(obj),original_object_sha256=H(obj),private_symbol_rename={old:new},compiler=json.loads((E/'source-checkpoint.json').read_text())['compiler'],production_changed=False,active_runtime_changed=False,numerical_original_callback_on_enabled_supported_path=False),indent=2)+'\n');print('EXP174_LINKED',out)
