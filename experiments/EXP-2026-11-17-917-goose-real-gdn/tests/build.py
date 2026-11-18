#!/usr/bin/env python3
"""Small selective real GDN ABI gates; no full runtime or model server."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

E=Path(__file__).resolve().parents[1]
ROOT=E.parents[1]
MODULES=[ROOT/"goose/cpu/goose-graph-builders.goose",ROOT/"goose/model/qgate-split.goose",
         ROOT/"goose/model/qwen35-attention.goose",ROOT/"goose/model/gdn-ops.goose",E/"source/runtime.goose"]


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()


def main():
    p=argparse.ArgumentParser()
    p.add_argument("--out",type=Path,required=True)
    p.add_argument("--support",type=Path,required=True)
    p.add_argument("--model-proof",type=Path,required=True)
    p.add_argument("--step",choices=["check","build","fixture","gate","contracts","mutations","regression","main-check"],required=True)
    p.add_argument("--fixture-name",default="gate")
    p.add_argument("--mutation",choices=['beta','gate-operator','conv-tail','config-scale','restore-state'])
    a=p.parse_args();out=a.out.resolve();support=a.support.resolve()
    if not a.fixture_name.replace('-','').isalnum():raise SystemExit('invalid fixture name')
    if not out.is_relative_to(Path('/home/end0/AI/LlamaServer')):raise SystemExit('artifacts must be inside LlamaServer')
    out.mkdir(parents=True,exist_ok=True);(out/(a.step+'.pid')).write_text(str(os.getpid())+'\n')
    pin=json.loads((ROOT/'runtime/compiler/current.json').read_text());goose=Path(pin['compiler']);assert sha(goose)==pin['sha256']
    binding=ROOT/'integration/goose-standalone/graph-bindings.c'
    key={str(p):sha(p) for p in [*MODULES,binding,support/'bin/libggml-base.so.0',support/'bin/libggml-cpu.so.0']};key['compiler']=pin['sha256']
    kp=out/'inputs.json'
    if kp.exists() and json.loads(kp.read_text())!=key:raise SystemExit('preserve changed candidate inputs in a new directory')
    kp.write_text(json.dumps(key,indent=2)+'\n')
    stem=a.step+('-'+a.fixture_name if a.step in ('fixture','gate') and a.fixture_name!='gate' else '')
    if a.step=='mutations' and a.mutation:stem+='-'+a.mutation
    with (out/(stem+'.log')).open('x') as log:
        def run(cmd,env=None,reject=False):
            cmd=list(map(str,cmd));controlled=[k+'='+env[k] for k in ('GGML_CPU_DISABLE_FUSION','GGML_GOOSE_GRAPH_BUILDERS','GGML_NO_BACKTRACE','LD_PRELOAD') if env and k in env]
            line='$ '+('env '+shlex.join(controlled)+' ' if controlled else '')+shlex.join(cmd)
            print(line,flush=True);log.write(line+'\n');log.flush()
            r=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,env=env,timeout=180)
            print(r.stdout,end='',flush=True);log.write(r.stdout+'\nexit_code='+str(r.returncode)+'\n');log.flush()
            if reject and (r.returncode not in (1,3) or not any(t in r.stdout for t in ['FAIL ','AssertionError'])):raise RuntimeError('mutation must fail a semantic assertion')
            if not reject and r.returncode:raise RuntimeError('command failed: '+str(r.returncode))
        include=ROOT/'vendor/flint/ggml/include';lib=out/'gdn.so'
        if a.step=='check':
            run([sys.executable,ROOT/'tools/compose-goose.py',out/'unit.goose',*MODULES]);run([goose,'check',out/'unit.goose'])
        elif a.step=='build':
            if lib.exists():raise RuntimeError('refusing to overwrite library')
            run([goose,'--standalone','-o',out/'runtime.c','--header',out/'runtime.h',out/'unit.goose'])
            run(['gcc-13','-O1','-g','-fPIC','-std=c11','-I',include,'-c',binding,'-o',out/'bindings.o'])
            run(['gcc-13','-O1','-g','-fPIC','-shared','-Wl,-z,defs',out/'runtime.c',out/'bindings.o',support/'bin/libggml-base.so.0','-Wl,-rpath,'+str(support/'bin'),'-lm','-o',lib])
        elif a.step=='fixture':
            if (out/a.fixture_name).exists():raise RuntimeError('refusing to overwrite fixture')
            run(['g++-13','-std=c++17','-O1','-g','-I',include,'-I',out,E/'tests/gate.cpp',lib,support/'bin/libggml-cpu.so.0',support/'bin/libggml-base.so.0','-lcrypto','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(support/'bin'),'-o',out/a.fixture_name])
            (out/(a.fixture_name+'-source.json')).write_text(json.dumps({'source_sha256':sha(E/'tests/gate.cpp')},indent=2)+'\n')
        elif a.step=='contracts':
            run([sys.executable,E/'tests/contracts.py',lib])
        elif a.step=='regression':
            previous=Path('/home/end0/AI/LlamaServer/exp912-goose-typed-graph-20261117')
            for record in json.loads((previous/'support-inputs.json').read_text()):
                if sha(Path(record['path']))!=record['sha256']:raise RuntimeError('native regression input changed')
            if (out/'constructor.so').exists():raise RuntimeError('refusing to overwrite regression artifact')
            run(['gcc-13','-O1','-g','-fPIC','-c',out/'runtime.c','-o',out/'runtime.o'])
            command=next(shlex.split(line[2:]) for line in (previous/'build.log').read_text().splitlines() if line.startswith('$ ') and str(previous/'candidate.so')+' ' in line)
            replacements={str(previous/'candidate.so'):str(out/'constructor.so'),str(previous/'candidate/runtime.o'):str(out/'runtime.o'),str(previous/'bindings.o'):str(out/'bindings.o')}
            command=[replacements.get(word,word) if not word.startswith('-Wl,--dependency-file=') else '-Wl,--dependency-file='+str(out/'constructor.d') for word in command]
            run(command)
            for route in (0,1):run([previous/'constructor-corpus',support/'bin/libggml-base.so.0',out/'constructor.so',route],{**os.environ,'GGML_GOOSE_GRAPH_BUILDERS':'1','GGML_NO_BACKTRACE':'1'})
            run([previous/'constructor-corpus',support/'bin/libggml-base.so.0',out/'constructor.so',1],{**os.environ,'GGML_GOOSE_GRAPH_BUILDERS':'0','GGML_NO_BACKTRACE':'1'})
        elif a.step=='main-check':
            block=(ROOT/'tools/build-main-runtime.sh').read_text().split('MODULES=(',1)[1].split('\n)',1)[0]
            modules=[ROOT/name for name in shlex.split(block,comments=True)]
            run([sys.executable,ROOT/'tools/compose-goose.py',out/'main.goose',*modules]);run([goose,'check',out/'main.goose'])
        elif a.step=='mutations':
            text=(out/'unit.goose').read_text()
            changes=[('beta','nodes[7] = qg_unary(c, nodes[6], 7, work);','nodes[7] = qg_unary(c, nodes[6], 10, work);','graph'),
                     ('gate-operator','nodes[12] = qa_binary(c, nodes[11], weights[6], 7, work);\n    nodes[13] = qa_reshape',
                      'nodes[12] = qa_binary(c, nodes[11], weights[6], 2, work);\n    nodes[13] = qa_reshape','graph'),
                     ('conv-tail','let off_rows: i64 = total_rows - keep;','let off_rows: i64 = total_rows - keep - 1;','graph'),
                     ('config-scale','qg_positive_bits(1.0 / qg_sqrt(head as f32))','qg_positive_bits(1.0 / (head as f32))','contracts'),
                     ('restore-state','state[i] = data[nc + i];','state[i] = data[i];','contracts')]
            for name,old,new,kind in changes:
                if a.mutation and name!=a.mutation:continue
                if text.count(old)!=1:raise RuntimeError('mutation anchor: '+name)
                directory=out/name;directory.mkdir(exist_ok=False);source=directory/'unit.goose';source.write_text(text.replace(old,new))
                run([goose,'check',source]);run([goose,'--standalone','-o',directory/'runtime.c','--header',directory/'runtime.h',source])
                mutant=directory/'gdn.so'
                run(['gcc-13','-O0','-g','-fPIC','-shared','-Wl,-z,defs',directory/'runtime.c',out/'bindings.o',support/'bin/libggml-base.so.0','-Wl,-rpath,'+str(support/'bin'),'-lm','-o',mutant])
                if kind=='contracts':run([sys.executable,E/'tests/contracts.py',mutant],reject=True)
                else:
                    proof=json.loads(a.model_proof.read_text())
                    run([out/a.fixture_name,proof['model']],{**os.environ,'GGML_CPU_DISABLE_FUSION':'0','LD_PRELOAD':str(mutant)},reject=True)
                log.write('KILLED '+name+'\n');print('KILLED',name,flush=True)
        else:
            proof=json.loads(a.model_proof.read_text());model=Path(proof['model']);before=model.stat()
            assert proof['result']=='PASS' and proof['sha256']=='c3b92d94cd03a7730ad45cc45de8c753e369e0228b2f5d83d819ce2ea6ab3355'
            assert (before.st_size,before.st_mtime_ns,before.st_ino)==(proof['bytes'],proof['mtime_ns'],proof['inode'])
            log.write('reused_model_proof_sha256='+sha(a.model_proof)+'\n')
            run(['/usr/bin/time','-f','gate_maxrss_KiB=%M',out/a.fixture_name,model],{**os.environ,'GGML_CPU_DISABLE_FUSION':'0'})
            after=model.stat();assert (before.st_size,before.st_mtime_ns,before.st_ino)==(after.st_size,after.st_mtime_ns,after.st_ino)
        log.write('RESULT PASS\n')


if __name__=='__main__':main()
