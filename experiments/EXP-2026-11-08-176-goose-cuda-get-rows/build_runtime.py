from pathlib import Path
import subprocess,json,re,shlex,hashlib
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E=F/'experiments/EXP-2026-11-08-176-goose-cuda-get-rows'
S=E/'source';B=F/'runtime/exp176/build';R=F/'runtime/exp176/run'
B.mkdir(parents=True,exist_ok=True)
H=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
rows=[]
def run(label,args,cwd=B):
    q=subprocess.run(list(map(str,args)),cwd=cwd,capture_output=True,text=True,timeout=1800)
    rows.append(dict(label=label,argv=[str(x) for x in args],cwd=str(cwd),exit_code=q.returncode))
    (B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n')
    (B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr)
    print(label,q.returncode,flush=True)
    if q.returncode:
        print((q.stdout+q.stderr)[-10000:]);raise SystemExit(q.returncode)
    return q

D=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda')
db=json.loads((D/'compile_commands.json').read_text())
r=next(x for x in db if x['file'].endswith('/getrows.cu'))
cmd=shlex.split(r['command'])
cmd.insert(1,'-I'+str(Path(r['file']).parent))
cmd.insert(1,'-I'+str(S))
cmd[cmd.index('-c')+1]=str(S/'cuda-runtime.cu')
cmd[cmd.index('-o')+1]=str(B/'cuda-getrows.o')
run('cuda-getrows-build',cmd,Path(r['directory']))
run('host-C-build',['gcc-13','-std=gnu11','-O3','-DNDEBUG','-fPIC','-fvisibility=hidden',
    '-Werror=implicit-int','-Werror=implicit-function-declaration','-c',S/'host-unit.c','-o',B/'host.o'])

# Reuse the recorded link line from EXP173, swapping concat's object for the
# private renamed original plus the two Goose objects.
# Base lineage is EXP174 (SET_ROWS Goose) so this candidate adds CONCAT without
# dropping the previous qualified component. The recorded EXP174 link line is the
# authoritative composition; only concat's object is replaced.
prev=json.loads((F/'runtime/exp175/build/commands.json').read_text())
prev=[x for x in prev if x['label']=='cuda-concat-link'][0]
link=prev['argv'].copy();cwd=Path(prev['cwd'])
orig=next(x for x in link if x.endswith('/getrows.cu.o'))
obj=Path(orig) if Path(orig).is_absolute() else cwd/orig
nm=run('native-getrows-symbols',['nm','--defined-only',obj])
syms=[]
for ln in nm.stdout.splitlines():
    ps=ln.split()
    if len(ps)<3:continue
    sym=ps[-1];m=re.match(r'_Z(\d+)',sym)
    if m and sym[m.end():m.end()+int(m[1])]=='ggml_cuda_op_get_rows':syms.append(sym)
assert len(syms)==1,syms
old=syms[0];m=re.match(r'_Z(\d+)',old)
newname='fg176g_native_ggml_cuda_op_get_rows'
new='_Z'+str(len(newname))+newname+old[m.end()+int(m[1]):]
run('private-native-getrows',['objcopy','--redefine-sym',old+'='+new,obj,B/'native-getrows.o'])

vm=(F/'experiments/EXP-2026-11-08-175-goose-cuda-concat/source/visibility.map').read_text()
vm=vm.replace('local:','fg176g_cuda_host_seen; fg176g_cuda_device_read; local: fg176h_*;')
vm=vm.replace(' *fg175c*;',' *fg175c*; *fg176g*; *fg176gs_*; *fg176gd_*;')
(S/'visibility.map').write_text(vm)

link=[str(B/'native-getrows.o') if x==orig else
      ('-Wl,--version-script='+str(S/'visibility.map')) if x.startswith('-Wl,--version-script=') else x
      for x in link]
link+=[str(B/'cuda-getrows.o'),str(B/'host.o')]
key=hashlib.sha256((H(B/'cuda-getrows.o')+H(B/'host.o')+H(B/'native-getrows.o')+H(S/'visibility.map')).encode()).hexdigest()[:12]
out=R/('libggml-cuda-getrows-'+key+'.so.0.27.0')
replay=B/'runtime-replay.so' if out.exists() else out
link[link.index('-o')+1]=str(replay)
run('cuda-getrows-link',link,cwd)
if replay!=out:assert H(replay)==H(out),'immutable runtime replay differs'
for p in (F/'runtime/exp175/run').iterdir():
    if p.is_file() or p.is_symlink():
        q=R/p.name;target=out if p.name.startswith('libggml-cuda') else p.resolve()
        if q.is_symlink():
            if q.resolve()==target:continue
            q.unlink()
        elif q.exists():raise RuntimeError('refuse to overwrite '+str(q))
        q.symlink_to(target)
(E/'runtime-manifest.json').write_text(json.dumps(dict(
    status='LINKED_PENDING_HOST_GPU_APEX',library=str(out),sha256=H(out),
    base_library_sha256=H(F/'runtime/exp175/run/libggml-cuda.so'),
    original_object=str(obj),original_object_sha256=H(obj),
    private_symbol_rename={old:new},
    compiler=json.loads((F/'runtime/compiler/current.json').read_text()),
    production_changed=False,active_runtime_changed=False,
    numerical_original_callback_on_enabled_supported_path=False),indent=2)+'\n')
print('EXP175_LINKED',out)
