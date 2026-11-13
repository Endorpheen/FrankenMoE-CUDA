from pathlib import Path
import json,re,shlex,subprocess,hashlib
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-165-goose-registry-loader';S=E/'source';B=F/'runtime/exp165/build';R=F/'runtime/exp165/run';B.mkdir(parents=True,exist_ok=True);R.mkdir(parents=True,exist_ok=True);C=F/'runtime/compiler/stage3-f849233b999c';H=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();assert H(C)=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a';rows=[]
def run(label,args,cwd=B):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,capture_output=True,text=True,timeout=300);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,flush=True)
 if q.returncode:print((q.stdout+q.stderr)[-7000:]);raise SystemExit(q.returncode)
 return q
run('registry-selfhosted-emit',[C,'-o',S/'registry-generated.c',S/'registry.goose']);g=(S/'registry-generated.c').read_text();main=g[g.rfind('static void gg_function_0('):];m=re.search(r'(gg_function_\d+)\(t\d+, base',main);assert m;entry='fg165_'+m[1];(S/'registry-qualified.c').write_text(re.sub(r'\b(gg_|ggf_|gs_)',lambda m:'fg165_'+m[1],g));(S/'registry-unit.c').write_text('#include "registry-ffi.h"\n#define main fg165_unused_main\n#include "registry-qualified.c"\n#undef main\nuint64_t fg165_goose_entry(int64_t p){return '+entry+'(p,1);}\n')
s=(S/'native-ggml-backend-reg.cpp').read_text();at=s.index('// Backend registry');# Platform headers and backend includes must precede the bridge.
at=s.index('namespace fs = std::filesystem;');s=s[:at]+'#if defined(__linux__)\n#include "registry-bridge.h"\n#endif\n\n'+s[at:]
for name,v in json.loads((S/'recipes.json').read_text()).items():
 p=r'(?m)^[A-Za-z_][\w ]*?\s+(?:\*+\s*)?'+re.escape(name)+r'\s*\([^;{}]*\)\s*\{';m=re.search(p,s);assert m,name;call='fg165_call('+str(v['mode'])+''.join(',FG165_A('+x+')' for x in v['args'])+')';kind=v['kind'];body=call+';return;' if kind=='void' else 'return ('+kind+')'+('(uintptr_t)' if kind.endswith('_t') and kind!='size_t' else '')+call+';';s=s[:m.end()]+'\n#if defined(__linux__)\n    if(fg165_enabled()){'+body+'}\n#endif\n'+s[m.end():]
(S/'backend-registry.cpp').write_text(s)
D=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda');db=json.loads((D/'compile_commands.json').read_text());record=next(r for r in db if r['file'].endswith('/ggml-backend-reg.cpp'));cmd=shlex.split(record['command']);obj=B/'registry.o';cmd.insert(1,'-I'+str(Path(record['file']).parent));cmd[cmd.index('-c')+1]=str(S/'backend-registry.cpp');cmd[cmd.index('-o')+1]=str(obj);run('registry-backend-build',cmd,Path(record['directory']));goose=B/'registry-goose.o';run('registry-goose-C-build',['/usr/bin/gcc-13','-std=gnu11','-O3','-DNDEBUG','-fPIC','-fvisibility=hidden','-Werror=implicit-int','-Werror=implicit-function-declaration','-c',S/'registry-unit.c','-o',goose])
objects=[D/'ggml/src/CMakeFiles/ggml.dir/ggml-backend-dl.cpp.o',D/'ggml/src/CMakeFiles/ggml.dir/ggml-backend-reg.cpp.o'];ref=B/'unchanged-exp164.so';cmd=['/usr/bin/g++-13','-O3','-DNDEBUG','-shared','-Wl,-soname,libggml.so.0','-o',ref]+objects+['-Wl,-rpath,'+str(D/'bin')+':','-ldl','bin/libggml-cpu.so.0.27.0','bin/libggml-cuda.so.0.27.0','bin/libggml-base.so.0.27.0'];run('unchanged-exp164-registry-link-integrity',cmd,D);old=(F/'runtime/exp164/run/libggml.so').resolve();assert H(ref)==H(old),(H(ref),H(old))
cmd=[str(obj) if str(x)==str(objects[1]) else str(x) for x in cmd];cmd +=[str(goose),'-Wl,-z,defs','-Wl,-Bsymbolic'];out=R/('libggml-registry-'+hashlib.sha256((H(obj)+H(goose)).encode()).hexdigest()[:12]+'.so.0.27.0');linkout=B/'relink.so' if out.exists() else out;cmd[cmd.index('-o')+1]=str(linkout);run('registry-link',cmd,D)
if out!=linkout:assert H(out)==H(linkout)
for p in (F/'runtime/exp164/run').iterdir():
 if p.is_file() or p.is_symlink():
  q=R/p.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise RuntimeError(str(q))
  q.symlink_to(out if re.fullmatch(r'libggml\.so(?:\..*)?',p.name) else p.resolve())
(S/'manifest.json').write_text(json.dumps(dict(status='LINKED_PENDING_CORRECTNESS',compiler_sha256=H(C),entry=entry,library=str(out),sha256=H(out),entrypoints=list(json.loads((S/'recipes.json').read_text())),unchanged_sha256=H(ref),rollback='GGML_GOOSE_REGISTRY=0 or immutable EXP164 runtime',scope='Actual Linux registry/loader/state/path/score/init selection;non-Linux native fallback retained',production_changed=False),indent=2)+'\n');print('LINKED_GOOSE_REGISTRY',out)
