#!/usr/bin/env python3
"""Selective reader builds/gates. No runtime rebuild or model inference."""
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
MODULES=[ROOT/'goose/misc/gguf-headers.goose',ROOT/'goose/model/gguf-tensors.goose',E/'source/reader.goose']
MODEL_MODULES=[ROOT/n for n in ['goose/cpu/goose-graph-builders.goose','goose/model/qgate-split.goose','goose/model/qwen35-attention.goose',
    'goose/model/gdn-ops.goose','goose/model/qwen35-gdn.goose','goose/model/qwen35-kv.goose','goose/model/gguf-tensors.goose','goose/model/qwen35-trunk.goose','goose/misc/gguf-headers.goose']]+[E/'source/reader.goose',E/'source/ownership.goose',E/'source/model.goose']


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()


def main():
    p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True)
    p.add_argument('--step',choices=['check','build','synthetic','synthetic-extra','apex','edge-skeleton','edge-ple','model-check','model-build','model-fixture','model-gate'],required=True)
    p.add_argument('--support',type=Path,required=True);a=p.parse_args();out=a.out.resolve()
    if not out.is_relative_to(Path('/home/end0/AI/LlamaServer')):raise SystemExit('outputs only in LlamaServer')
    out.mkdir(parents=True,exist_ok=True);(out/(a.step+'.pid')).write_text(str(os.getpid())+'\n')
    pin=json.loads((ROOT/'runtime/compiler/current.json').read_text());goose=Path(pin['compiler']);assert sha(goose)==pin['sha256']
    modules=MODEL_MODULES if a.step.startswith('model-') else MODULES
    key={str(p):sha(p) for p in modules};key['compiler']=pin['sha256'];kp=out/'inputs.json'
    if a.step.startswith('model-'):
        for p in [ROOT/'integration/goose-standalone/graph-bindings.c',a.support/'bin/libggml-base.so.0',a.support/'bin/libggml-cpu.so.0']:key[str(p)]=sha(p)
    if kp.exists() and json.loads(kp.read_text())!=key:raise SystemExit('changed source inputs: preserve old artifacts and use a new output')
    kp.write_text(json.dumps(key,indent=2)+'\n')
    with (out/(a.step+'.log')).open('x') as log:
        def run(cmd):
            cmd=list(map(str,cmd));line='$ '+shlex.join(cmd);print(line,flush=True);log.write(line+'\n');log.flush()
            r=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180)
            log.write(r.stdout+'\nexit_code='+str(r.returncode)+'\n');log.flush();print(r.stdout[-7000:],end='',flush=True)
            if r.returncode:raise RuntimeError('command failed: '+str(r.returncode))
        if a.step in ('check','model-check'):
            run([sys.executable,ROOT/'tools/compose-goose.py',out/'unit.goose',*modules]);run([goose,'check',out/'unit.goose'])
        elif a.step=='build':
            if (out/'reader.so').exists():raise RuntimeError('refusing to overwrite library')
            run([goose,'--standalone','-o',out/'runtime.c','--header',out/'runtime.h',out/'unit.goose'])
            run(['gcc-13','-O1','-g','-fPIC','-shared','-Wl,-z,defs',out/'runtime.c','-lm','-o',out/'reader.so'])
            run(['ldd',out/'reader.so']);run(['nm','-D','--undefined-only',out/'reader.so'])
        elif a.step=='model-build':
            if (out/'model.so').exists():raise RuntimeError('refusing to overwrite model library')
            run([goose,'--standalone','-o',out/'runtime.c','--header',out/'runtime.h',out/'unit.goose'])
            run(['gcc-13','-O1','-g','-fPIC','-std=c11','-I',ROOT/'vendor/flint/ggml/include','-c',ROOT/'integration/goose-standalone/graph-bindings.c','-o',out/'bindings.o'])
            # Isolated gate links the exact existing host byte-store bodies.
            # Production composition already links their shared host adapter.
            import re
            host=(ROOT/'integration/m1/host/goose_host.c').read_text()
            parts=[re.search(pattern,host,re.S).group(0) for pattern in [r'static void host_st_u8\([^}]+\}',r'void qz_st_u8\([^}]+\}']]
            primitive=out/'existing-byte-store.c';primitive.write_text('#include <stdint.h>\n#include <string.h>\n'+'\n'.join(parts)+'\n')
            run(['gcc-13','-O1','-g','-fPIC','-std=c11','-c',primitive,'-o',out/'existing-byte-store.o'])
            run(['gcc-13','-O1','-g','-fPIC','-shared','-Wl,-z,defs',out/'runtime.c',out/'bindings.o',out/'existing-byte-store.o',a.support/'bin/libggml-base.so.0','-Wl,-rpath,'+str(a.support/'bin'),'-lm','-o',out/'model.so'])
        elif a.step=='model-fixture':
            if (out/'loader-gate').exists():raise RuntimeError('refusing to overwrite fixture')
            run([sys.executable,E/'tests/make_fixture.py',out,'loader-gate'])
            run(['g++-13','-std=c++17','-O1','-g','-I',ROOT/'vendor/flint/ggml/include','-I',out,
                 '-DGDN_ORACLE_HEADER="'+str(out/'loader-gate-gdn-oracle.hpp')+'"',out/'loader-gate.cpp',out/'model.so',
                 a.support/'bin/libggml-cpu.so.0',a.support/'bin/libggml-base.so.0','-lcrypto','-lm','-Wl,--export-dynamic',
                 '-Wl,-rpath,'+str(a.support/'bin'),'-o',out/'loader-gate'])
        elif a.step=='model-gate':
            proof_path=Path('/home/end0/AI/LlamaServer/exp913-goose-real-attention-20261117/model-proof.json');proof=json.loads(proof_path.read_text());model=Path(proof['model']);before=model.stat()
            assert proof['result']=='PASS' and proof['sha256']=='c3b92d94cd03a7730ad45cc45de8c753e369e0228b2f5d83d819ce2ea6ab3355'
            assert (before.st_size,before.st_mtime_ns,before.st_ino)==(proof['bytes'],proof['mtime_ns'],proof['inode'])
            log.write('reused_model_proof_sha256='+sha(proof_path)+'\n')
            cmd=['/usr/bin/time','-f','fixture_maxrss_KiB=%M',str(out/'loader-gate'),str(model),'trunk']
            line='$ env GGML_CPU_DISABLE_FUSION=0 '+shlex.join(cmd);log.write(line+'\n');log.flush();print(line,flush=True)
            def descendants(pid):
                result=[pid]
                try:children=(Path('/proc')/str(pid)/'task'/str(pid)/'children').read_text().split()
                except OSError:children=[]
                for child in children:result+=descendants(int(child))
                return result
            def memory(pid):
                try:text=(Path('/proc')/str(pid)/'status').read_text()
                except OSError:return (0,0)
                fields={line.split(':')[0]:int(line.split()[1]) for line in text.splitlines() if line.startswith(('VmRSS:','VmSwap:'))}
                return fields.get('VmRSS',0),fields.get('VmSwap',0)
            peak=swap=0;start=time.monotonic();deadline=start+180
            with subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT,env={**os.environ,'GGML_CPU_DISABLE_FUSION':'0'},start_new_session=True) as process:
                (out/'model-child.pid').write_text(str(process.pid)+'\n')
                while process.poll() is None:
                    values=[memory(pid) for pid in descendants(process.pid)];peak=max(peak,sum(v[0] for v in values));swap=max(swap,sum(v[1] for v in values))
                    if peak>4*1024*1024 or time.monotonic()>deadline:
                        os.killpg(process.pid,signal.SIGTERM)
                        try:process.wait(timeout=5)
                        except subprocess.TimeoutExpired:os.killpg(process.pid,signal.SIGKILL);process.wait()
                        log.write(f'BOUND_STOP rss_KiB={peak} swap_KiB={swap} elapsed={time.monotonic()-start} exit={process.returncode}\n');raise RuntimeError('owned model exceeded RSS/time bound')
                    try:process.wait(timeout=.25)
                    except subprocess.TimeoutExpired:pass
                code=process.returncode
            log.write(f'\nexit_code={code}\nmonitored_process_tree_rss_KiB={peak}\nmonitored_process_tree_swap_KiB={swap}\n');log.flush()
            print((out/'model-gate.log').read_text()[-6000:])
            if code:raise RuntimeError('model loader/numerical gate failed: '+str(code))
            after=model.stat();assert (before.st_size,before.st_mtime_ns,before.st_ino)==(after.st_size,after.st_mtime_ns,after.st_ino)
        else:
            cmd=[sys.executable,E/'tests/gate.py',out/'reader.so','--native',a.support/'bin/libggml-base.so.0','--case',a.step]
            run(cmd)
        log.write('RESULT PASS\n')


if __name__=='__main__':main()
