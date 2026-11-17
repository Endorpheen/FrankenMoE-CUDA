#!/usr/bin/env python3
"""Stage, check, generate and incrementally build an isolated Edge0 candidate."""
import argparse,hashlib,json,subprocess,sys,shlex,os,shutil
from pathlib import Path
R=Path(__file__).resolve().parents[2]
a=argparse.ArgumentParser();a.add_argument('--out',type=Path,required=True);a.add_argument('--step',choices=['stage','goose','configure','build'],required=True);a.add_argument('--attempt',type=int,default=1);a=a.parse_args()
o=a.out.resolve();assert o.is_relative_to(Path('/home/end0/AI/LlamaServer'))
o.mkdir(exist_ok=True,parents=True)
log=o/(a.step+('' if a.attempt==1 else '-'+str(a.attempt))+'.log');assert not log.exists(), 'use a new log/candidate instead of overwriting evidence'
(o/(a.step+'.pid')).write_text(str(os.getpid())+'\n')
with log.open('x') as f:
 def run(cmd):
  cmd=list(map(str,cmd));s='$ '+shlex.join(cmd);print(s,flush=True);f.write(s+'\n');f.flush()
  p=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
  for line in p.stdout: print(line,end='',flush=True);f.write(line);f.flush()
  rc=p.wait();f.write(f'exit_code={rc}\n');f.flush()
  if rc:raise SystemExit(rc)
 if a.step=='stage':
  assert not (o/'source').exists();shutil.copytree(R/'vendor/flint',o/'source')
  shutil.copytree(R/'integration/edge0',o/'integration')
  (o/'stage-inputs.json').write_text(json.dumps({str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*(R/'integration/edge0').glob('*'),R/'vendor/flint/src/models/qwen4exp.cpp',R/'vendor/flint/src/models/models.h'] if p.is_file()},indent=2)+'\n')
 elif a.step=='goose':
  pin=json.loads((R/'runtime/compiler/current.json').read_text());g=Path(pin['compiler']);assert hashlib.sha256(g.read_bytes()).hexdigest()==pin['sha256']
  modules=[R/'goose/cpu/goose-graph-builders.goose',R/'goose/cpu/goose-moe-router.goose',R/'goose/model/expert-lru.goose']+[R/f'experiments/EXP-2026-11-17-{v}/source/runtime.goose' for v in ['909-edge0-layout','910-edge0-ple-fp8','911-edge0-ple-provider','915-edge0-latent-expert']]+[R/'integration/edge0/runtime.goose']
  (o/'generated').mkdir(exist_ok=True)
  run([sys.executable,R/'tools/compose-goose.py',o/'generated/runtime.goose',*modules]);run([g,'check',o/'generated/runtime.goose'])
  run([g,'--standalone','-o',o/'generated/runtime.c','--header',o/'generated/runtime.h',o/'generated/runtime.goose'])
 elif a.step=='configure':
  run(['cmake','-S',o/'integration','-B',o/'build','-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_C_COMPILER=gcc-13','-DCMAKE_CXX_COMPILER=g++-13','-DCMAKE_CXX_FLAGS_RELEASE=-O2 -DNDEBUG',f'-DFLINT={o}/source',f'-DSUPPORT=/home/end0/AI/LlamaServer/franken-recovery-20261116/support',f'-DGENERATED={o}/generated',f'-DREPO={R}'])
 else:run(['cmake','--build',o/'build','-j','6'])
 f.write('RESULT PASS\n')
