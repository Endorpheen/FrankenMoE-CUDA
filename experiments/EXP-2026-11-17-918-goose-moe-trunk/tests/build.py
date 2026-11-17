#!/usr/bin/env python3
"""Selective real MoE/trunk gates with immutable artifacts and bounded logs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import signal
import subprocess
import sys
import time

E=Path(__file__).resolve().parents[1];ROOT=E.parents[1]
MODULES=[ROOT/n for n in ['goose/cpu/goose-graph-builders.goose','goose/model/qgate-split.goose',
         'goose/model/qwen35-attention.goose','goose/model/gdn-ops.goose','goose/model/qwen35-gdn.goose',
         'goose/model/qwen35-kv.goose','goose/model/gguf-tensors.goose']]+[E/'source/runtime.goose']


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);ap.add_argument('--support',type=Path,required=True)
    ap.add_argument('--model-proof',type=Path,required=True);ap.add_argument('--step',choices=['check','build','fixture','ffn','trunk','bind','mutations','mutation-gate','main-check'],required=True)
    ap.add_argument('--mutation',choices=['binding','router','shared','trunk-residual','trunk-headnorm','trunk-convroot'])
    ap.add_argument('--fixture-name',default='gate');a=ap.parse_args();out=a.out.resolve();support=a.support.resolve()
    if not out.is_relative_to(Path('/home/end0/AI/LlamaServer')) or not a.fixture_name.replace('-','').isalnum():raise SystemExit('invalid output location/name')
    out.mkdir(parents=True,exist_ok=True);(out/(a.step+'.pid')).write_text(str(os.getpid())+'\n')
    pin=json.loads((ROOT/'runtime/compiler/current.json').read_text());goose=Path(pin['compiler']);assert sha(goose)==pin['sha256']
    binding=ROOT/'integration/goose-standalone/graph-bindings.c'
    key={str(p):sha(p) for p in [*MODULES,binding,support/'bin/libggml-base.so.0',support/'bin/libggml-cpu.so.0']};key['compiler']=pin['sha256']
    kp=out/'inputs.json'
    if kp.exists() and json.loads(kp.read_text())!=key:raise SystemExit('preserve old artifacts; changed modules/backends require a new directory')
    kp.write_text(json.dumps(key,indent=2)+'\n')
    stem=a.step+('-'+a.fixture_name if a.step in ('fixture','ffn','trunk','bind') and a.fixture_name!='gate' else '')
    if a.step in ('mutations','mutation-gate'):
        if not a.mutation:raise SystemExit('--mutation required for selective negative gates')
        stem+='-'+a.mutation
    with (out/(stem+'.log')).open('x') as log:
        def run(cmd,env=None,bounded=False,reject=False):
            cmd=list(map(str,cmd));settings=[k+'='+env[k] for k in ('GGML_CPU_DISABLE_FUSION','LD_PRELOAD') if env and k in env]
            line='$ '+('env '+shlex.join(settings)+' ' if settings else '')+shlex.join(cmd)
            print(line,flush=True);log.write(line+'\n');log.flush()
            if bounded:
                def descendants(pid):
                    result=[pid]
                    try:children=(Path('/proc')/str(pid)/'task'/str(pid)/'children').read_text().split()
                    except OSError:children=[]
                    for child in children:result+=descendants(int(child))
                    return result
                def memory(pid):
                    try:text=(Path('/proc')/str(pid)/'status').read_text()
                    except OSError:return (0,0)
                    values={line.split(':')[0]:int(line.split()[1]) for line in text.splitlines() if line.startswith(('VmRSS:','VmSwap:'))}
                    return values.get('VmRSS',0),values.get('VmSwap',0)
                peak=swap=0;deadline=time.monotonic()+180;start=log.tell()
                with subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT,env=env,start_new_session=True) as process:
                    (out/(stem+'-child.pid')).write_text(str(process.pid)+'\n')
                    while process.poll() is None:
                        samples=[memory(pid) for pid in descendants(process.pid)];peak=max(peak,sum(v[0] for v in samples));swap=max(swap,sum(v[1] for v in samples))
                        if peak>4*1024*1024 or time.monotonic()>deadline:
                            os.killpg(process.pid,signal.SIGTERM)
                            try:process.wait(timeout=5)
                            except subprocess.TimeoutExpired:os.killpg(process.pid,signal.SIGKILL);process.wait()
                            log.write(f'RESOURCE_OR_TIME_BOUND_STOP peak_rss_KiB={peak} swap_KiB={swap} deadline_exceeded={time.monotonic()>deadline} exit_code={process.returncode}\n');raise RuntimeError('owned trunk process exceeded 4GiB RSS or 180s')
                        try:process.wait(timeout=.25)
                        except subprocess.TimeoutExpired:pass
                    code=process.returncode
                log.flush();output=(out/(stem+'.log')).read_bytes()[start:].decode(errors='replace')
                log.write(f'\nexit_code={code}\nmonitored_peak_process_tree_rss_KiB={peak}\nmonitored_max_process_tree_swap_KiB={swap}\n')
                print(output[-5000:],flush=True)
            else:
                r=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,env=env,timeout=180);code=r.returncode;output=r.stdout
                print(output,end='',flush=True);log.write(output+'\nexit_code='+str(code)+'\n')
            log.flush()
            if reject and (code!=3 or 'FAIL ' not in output):raise RuntimeError('mutation must fail a semantic assertion, not crash')
            if not reject and code:raise RuntimeError('command failed: '+str(code))
        include=ROOT/'vendor/flint/ggml/include';lib=out/'model.so'
        if a.step=='check':
            run([sys.executable,ROOT/'tools/compose-goose.py',out/'unit.goose',*MODULES]);run([goose,'check',out/'unit.goose'])
        elif a.step=='build':
            if lib.exists():raise RuntimeError('refusing to overwrite library')
            run([goose,'--standalone','-o',out/'runtime.c','--header',out/'runtime.h',out/'unit.goose'])
            run(['gcc-13','-O1','-g','-fPIC','-std=c11','-I',include,'-c',binding,'-o',out/'bindings.o'])
            run(['gcc-13','-O1','-g','-fPIC','-shared','-Wl,-z,defs',out/'runtime.c',out/'bindings.o',support/'bin/libggml-base.so.0','-Wl,-rpath,'+str(support/'bin'),'-lm','-o',lib])
        elif a.step=='fixture':
            if (out/a.fixture_name).exists():raise RuntimeError('refusing to overwrite fixture')
            gdnp=E.parent/'EXP-2026-11-17-917-goose-real-gdn/tests/gate.cpp'
            text=gdnp.read_text().split('struct GData {',1)[0]
            text=text.replace('"../../EXP-2026-11-17-913-goose-real-attention/tests/gate.cpp"','"'+str(E.parent/'EXP-2026-11-17-913-goose-real-attention/tests/gate.cpp')+'"')
            header=out/(a.fixture_name+'-gdn-oracle.hpp');header.write_text(text)
            run(['g++-13','-std=c++17','-O1','-g','-I',include,'-I',out,'-DGDN_ORACLE_HEADER="'+str(header)+'"',E/'tests/gate.cpp',lib,support/'bin/libggml-cpu.so.0',support/'bin/libggml-base.so.0','-lcrypto','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(support/'bin'),'-o',out/a.fixture_name])
            (out/(a.fixture_name+'-sources.json')).write_text(json.dumps({str(p):sha(p) for p in [E/'tests/gate.cpp',gdnp,header]},indent=2)+'\n')
        elif a.step=='main-check':
            block=(ROOT/'tools/build-main-runtime.sh').read_text().split('MODULES=(',1)[1].split('\n)',1)[0]
            run([sys.executable,ROOT/'tools/compose-goose.py',out/'main.goose',*[ROOT/n for n in shlex.split(block,comments=True)]])
            run([goose,'check',out/'main.goose'])
        elif a.step in ('mutations','mutation-gate'):
            text=(out/'unit.goose').read_text()
            changes={
                'binding':('qa_set(tensor, 4, 0, mapping + (offset as u64));','qa_set(tensor, 4, 0, mapping + (offset as u64) + 8);','ffn'),
                'router':('nodes[9] = qa_binary(c, nodes[6], nodes[8], 8, work);','nodes[9] = qa_binary(c, nodes[6], nodes[8], 7, work);','ffn'),
                'shared':('qa_begin(work, 3, c, nodes[base + 5], 0, 0, 7);','qa_begin(work, 3, c, nodes[base + 5], 0, 0, 10);','ffn'),
                'trunk-residual':('current = qa_binary(c, ffn, residual, 2, work);','current = ffn;','trunk'),
                'trunk-headnorm':('frames[last + 1] = qa_binary(c, frames[last], head_norm, 7, work);','frames[last + 1] = frames[last];','trunk'),
                'trunk-convroot':('roots[root_count + 1] = branch_nodes[16];','roots[root_count + 1] = branch_nodes[14];','trunk')}
            old,new,kind=changes[a.mutation]
            if text.count(old)!=1:raise RuntimeError('mutation anchor drift')
            directory=out/('mutant-'+a.mutation);source=directory/'unit.goose';mutant=directory/'model.so'
            if a.step=='mutation-gate':
                if not mutant.exists() or source.read_text()!=text.replace(old,new):raise RuntimeError('changed/missing mutant inputs; do not silently reuse')
                log.write('reused_mutant_sha256='+sha(mutant)+'\n')
            else:
                directory.mkdir(exist_ok=False);source.write_text(text.replace(old,new))
                run([goose,'check',source]);run([goose,'--standalone','-o',directory/'runtime.c','--header',directory/'runtime.h',source])
                run(['gcc-13','-O0','-g','-fPIC','-shared','-Wl,-z,defs',directory/'runtime.c',out/'bindings.o',support/'bin/libggml-base.so.0','-Wl,-rpath,'+str(support/'bin'),'-lm','-o',mutant])
            proof=json.loads(a.model_proof.read_text())
            run([out/a.fixture_name,proof['model'],kind],{**os.environ,'GGML_CPU_DISABLE_FUSION':'0','LD_PRELOAD':str(mutant)},bounded=kind=='trunk',reject=True)
            log.write('KILLED '+a.mutation+'\n');print('KILLED',a.mutation,flush=True)
        else:
            proof=json.loads(a.model_proof.read_text());model=Path(proof['model']);before=model.stat()
            assert proof['result']=='PASS' and proof['sha256']=='c3b92d94cd03a7730ad45cc45de8c753e369e0228b2f5d83d819ce2ea6ab3355'
            assert (before.st_size,before.st_mtime_ns,before.st_ino)==(proof['bytes'],proof['mtime_ns'],proof['inode'])
            log.write('reused_model_proof_sha256='+sha(a.model_proof)+'\n')
            command=['/usr/bin/time','-f','fixture_maxrss_KiB=%M',str(out/a.fixture_name),str(model),a.step]
            run(command,{**os.environ,'GGML_CPU_DISABLE_FUSION':'0'},bounded=a.step=='trunk')
            after=model.stat();assert (before.st_size,before.st_mtime_ns,before.st_ino)==(after.st_size,after.st_mtime_ns,after.st_ino)
        log.write('RESULT PASS\n')


if __name__=='__main__':main()
