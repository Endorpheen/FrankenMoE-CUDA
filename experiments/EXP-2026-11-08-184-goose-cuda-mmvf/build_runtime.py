from pathlib import Path
import subprocess,json,re,shlex,hashlib
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E=F/'experiments/EXP-2026-11-08-184-goose-cuda-mmvf'
S=E/'source';B=F/'runtime/exp184/build';R=F/'runtime/exp184/run'
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
r=next(x for x in db if x['file'].endswith('/mmvf.cu'))
cmd=shlex.split(r['command'])
cmd.insert(1,'-I'+str(Path(r['file']).parent))
cmd.insert(1,'-I'+str(S))
cmd[cmd.index('-c')+1]=str(S/'cuda-runtime.cu')
cmd[cmd.index('-o')+1]=str(B/'cuda-mmvf.o')
run('cuda-mmvf-build',cmd,Path(r['directory']))
run('host-C-build',['gcc-13','-std=gnu11','-O3','-DNDEBUG','-fPIC','-fvisibility=hidden',
    '-Werror=implicit-int','-Werror=implicit-function-declaration','-c',S/'host-unit.c','-o',B/'host.o'])

# Reuse the recorded link line from EXP173, swapping concat's object for the
# private renamed original plus the two Goose objects.
# Base lineage is EXP174 (SET_ROWS Goose) so this candidate adds CONCAT without
# dropping the previous qualified component. The recorded EXP174 link line is the
# authoritative composition; only concat's object is replaced.
prev=json.loads((F/'runtime/exp183/build/commands.json').read_text())
prev=[x for x in prev if x['label']=='cuda-fwht-link'][0]
link=prev['argv'].copy();cwd=Path(prev['cwd'])
# quantize.cu is already owned by EXP178, which renamed quantize_row_q8_1_cuda and
# links its own object. This stage extends that same object with the two MMQ entry
# points, so the private copy is taken from EXP178's renamed object rather than
# from a fresh quantize.cu.o.
orig=next(x for x in link if x.endswith('/mmvf.cu.o'))
obj=Path(orig) if Path(orig).is_absolute() else cwd/orig
nm=run('native-mmvf-symbols',['nm','--defined-only',obj])
# Both entry points must be privatised: ggml_cuda_dup wraps ggml_cuda_cpy and
# carries its own call, so leaving it global would keep DUP on the native path
# while the Goose route claimed to cover it.
want={'ggml_cuda_mul_mat_vec_f':'fg184m_native_ggml_cuda_mul_mat_vec_f',
      }
found={}
for ln in nm.stdout.splitlines():
    ps=ln.split()
    if len(ps)<3:continue
    sym=ps[-1]
    # objcopy must target the real definition: the compiler also emits a
    # `.cold` companion with the same mangled prefix, and renaming that one leaves
    # the callable symbol global and the Goose entry undefined.
    if sym.endswith('.cold'):continue
    m=re.match(r'_Z(\d+)',sym)
    if not m:continue
    base=sym[m.end():m.end()+int(m[1])]
    if base in want:found[base]=sym
assert set(found)==set(want),found
renames=[]
for base,newname in want.items():
    old=found[base];m=re.match(r'_Z(\d+)',old)
    new='_Z'+str(len(newname))+newname+old[m.end()+int(m[1]):]
    renames.append((old,new))
args=['objcopy']
for old,new in renames:args+=['--redefine-sym',old+'='+new]
run('private-native-mmvf',args+[obj,B/'native-mmvf.o'])

vm=(F/'experiments/EXP-2026-11-08-183-goose-cuda-fwht/source/visibility.map').read_text()
vm=vm.replace('local:','fg184m_cuda_host_seen; fg184m_cuda_device_read; local: fg184h_*;')
vm=vm.replace(' *fg183f*;',' *fg183f*; *fg184m*; *fg184hs_*; *fg184md_*;')
(S/'visibility.map').write_text(vm)

link=[str(B/'native-mmvf.o') if x==orig else
      ('-Wl,--version-script='+str(S/'visibility.map')) if x.startswith('-Wl,--version-script=') else x
      for x in link]
link+=[str(B/'cuda-mmvf.o'),str(B/'host.o')]
key=hashlib.sha256((H(B/'cuda-mmvf.o')+H(B/'host.o')+H(B/'native-mmvf.o')+H(S/'visibility.map')).encode()).hexdigest()[:12]
out=R/('libggml-cuda-mmvf-'+key+'.so.0.27.0')
replay=B/'runtime-replay.so' if out.exists() else out
link[link.index('-o')+1]=str(replay)
run('cuda-mmvf-link',link,cwd)
if replay!=out:assert H(replay)==H(out),'immutable runtime replay differs'
for p in (F/'runtime/exp183/run').iterdir():
    if p.is_file() or p.is_symlink():
        q=R/p.name;target=out if p.name.startswith('libggml-cuda') else p.resolve()
        if q.is_symlink():
            if q.resolve()==target:continue
            q.unlink()
        elif q.exists():raise RuntimeError('refuse to overwrite '+str(q))
        q.symlink_to(target)
(E/'runtime-manifest.json').write_text(json.dumps(dict(
    status='LINKED_PENDING_HOST_GPU_APEX',library=str(out),sha256=H(out),
    base_library_sha256=H(F/'runtime/exp183/run/libggml-cuda.so'),
    original_object=str(obj),original_object_sha256=H(obj),
    private_symbol_rename=dict(renames),
    compiler=json.loads((F/'runtime/compiler/current.json').read_text()),
    production_changed=False,active_runtime_changed=False,
    numerical_original_callback_on_enabled_supported_path=False),indent=2)+'\n')
print('EXP175_LINKED',out)
