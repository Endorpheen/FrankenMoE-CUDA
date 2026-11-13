from pathlib import Path
import json,re,hashlib,subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-164-goose-scheduler';S=E/'source';P=E.parent/'EXP-2026-11-08-163-goose-stream-control';B=F/'runtime/exp164/build';R=F/'runtime/exp164/run';B.mkdir(parents=True,exist_ok=True);R.mkdir(parents=True,exist_ok=True);C=F/'runtime/compiler/stage3-f849233b999c';H=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();assert H(C)=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a';rows=[]
def run(label,args,cwd=B):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,capture_output=True,text=True,timeout=300);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,flush=True)
 if q.returncode:print((q.stdout+q.stderr)[-6000:]);raise SystemExit(q.returncode)
run('scheduler-selfhosted-emit',[C,'-o',S/'scheduler-generated.c',S/'scheduler.goose']);g=(S/'scheduler-generated.c').read_text();main=g[g.rfind('static void gg_function_0('):];entry=re.search(r'(gg_function_\d+)\(t\d+, base',main);assert entry,main[-1000:];entry='fg164_'+entry[1];(S/'scheduler-qualified.c').write_text(re.sub(r'\b(gg_|ggf_|gs_)',lambda m:'fg164_'+m[1],g));(S/'scheduler-unit.c').write_text('#include "scheduler-ffi.h"\n#define main fg164_unused_generated_main\n#include "scheduler-qualified.c"\n#undef main\nuint64_t fg164_goose_entry(int64_t p){return '+entry+'(p,1);}\n')
s=(S/'native-backend.cpp').read_text();pos=s.index('// creates a copy of the tensor with the same memory layout');s=s[:pos]+'#include "scheduler-api.h"\n\n'+s[pos:];needle='    int debug_realloc;\n};';assert needle in s;extra='''    // Private allocation bounds/transient owners;the public scheduler type stays opaque.
    size_t fg164_ids_capacity;
    void *fg164_active_deps;
    void *fg164_candidates;
    size_t fg164_candidate_count,fg164_candidate_capacity;
    size_t fg164_pipeline_sizes[GGML_SCHED_MAX_BACKENDS];
    bool fg164_pipeline_ready[GGML_SCHED_MAX_BACKENDS];
    size_t fg164_pipeline_slots[GGML_SCHED_MAX_BACKENDS];
    size_t fg164_query_size;
''';s=s.replace(needle,'    int debug_realloc;\n'+extra+'};',1);pos=s.index('#define hash_id(tensor)');s=s[:pos]+'#include "scheduler-bridge.h"\n\n'+s[pos:]
for name,r in json.loads((S/'recipes.json').read_text()).items():
 pat=r'(?m)^[A-Za-z_][\w ]*?\s+(?:\*+\s*)?'+re.escape(name)+r'\s*\([^;{}]*\)\s*\{';m=re.search(pat,s);assert m,name;call='fg164_call('+str(r['mode'])+','+','.join('FG164_A('+x+')' for x in r['args'])+')';ty=r['kind'];code=call+';return;' if ty=='void' else 'return ('+ty+')'+('(uintptr_t)' if '*' in ty or ty.endswith('_t') and ty!='size_t' else '')+call+';';s=s[:m.end()]+'\n    if(fg164_enabled){'+code+'}\n'+s[m.end():]
(S/'backend-scheduler.cpp').write_text(s)
prev=json.loads((P/'build-commands.json').read_text());r=next(x for x in prev if x['label']=='stream-backend-build');cmd=r['argv'].copy();old=cmd[cmd.index('-o')+1];obj=B/'backend-scheduler.o';original=Path(cmd[cmd.index('-c')+1]);cmd.insert(1,'-I'+str(original.parent));cmd[cmd.index('-c')+1]=str(S/'backend-scheduler.cpp');cmd[cmd.index('-o')+1]=str(obj);run('scheduler-backend-build',cmd,Path(r['cwd']));goose=B/'scheduler-goose.o';run('scheduler-goose-C-build',['/usr/bin/gcc-13','-std=gnu11','-O3','-DNDEBUG','-fPIC','-fvisibility=hidden','-Werror=implicit-int','-Werror=implicit-function-declaration','-c',S/'scheduler-unit.c','-o',goose]);r=next(x for x in prev if x['label']=='stream-base-link');cmd=r['argv'].copy();ref=B/'unchanged-exp163.so';cmd[cmd.index('-o')+1]=str(ref);run('unchanged-exp163-link-integrity',cmd,Path(r['cwd']));baseline=(F/'runtime/exp163/run/libggml-base.so').resolve();assert H(ref)==H(baseline);assert cmd.count(old)==1;cmd=[str(obj) if x==old else x for x in cmd];cmd.append(str(goose));out=R/('libggml-base-scheduler-'+hashlib.sha256((H(obj)+H(goose)).encode()).hexdigest()[:12]+'.so.0.27.0');linkout=B/'relink.so' if out.exists() else out;cmd[cmd.index('-o')+1]=str(linkout);run('scheduler-base-link',cmd,Path(r['cwd']));
if out!=linkout:assert H(out)==H(linkout)
for f in (F/'runtime/exp163/run').iterdir():
 if f.is_file() or f.is_symlink():
  q=R/f.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise RuntimeError(str(q))
  q.symlink_to(out if f.name.startswith('libggml-base') else f.resolve())
(S/'manifest.json').write_text(json.dumps(dict(status='LINKED_PENDING_CORRECTNESS',compiler_sha256=H(C),entry=entry,library=str(out),sha256=H(out),bodies=list(json.loads((S/'recipes.json').read_text())),rollback='GGML_GOOSE_SCHEDULER=0 or previous immutable EXP163 runtime',public_abi='Opaque pointer unchanged;private capacity/transient-owner fields appended only in this control TU',provider_boundaries='Existing Goose core/allocator/interfaces reused;actual CPU/CUDA callbacks/kernels and global dynamic registry separate',production_changed=False),indent=2)+'\n');print('LINKED_GOOSE_SCHEDULER',out)
