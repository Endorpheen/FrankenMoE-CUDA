#!/usr/bin/env python3
"""Relink an isolated variant, reusing immutable accepted substrate objects."""
from pathlib import Path
import argparse,subprocess,shlex,hashlib,json,os
r=Path(__file__).resolve().parents[2]
ap=argparse.ArgumentParser();ap.add_argument('--base',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);ap.add_argument('--reuse-unit',type=Path);ap.add_argument('--goose',action='store_true');ap.add_argument('--reuse-goose',type=Path);a=ap.parse_args();b=a.base.resolve();o=a.out.resolve();assert o.is_relative_to(Path('/home/end0/AI/LlamaServer')) and not o.exists();o.mkdir(parents=True);(o/'build/llama').mkdir(parents=True)
commands=subprocess.check_output(['ninja','-t','commands'],cwd=b/'build',text=True).splitlines()
for name in ['pack.cpp','pack.h','cli.cpp','runtime.goose']: (o/name).write_bytes((r/'integration/edge0'/name).read_bytes())
unit=b/'build/llama/CMakeFiles/llama.dir/Unity/unity_10_cxx.cxx';text=unit.read_text();old=str(b/'integration/pack.cpp');assert text.count(old)==1;(o/'unit.cpp').write_text(text.replace(old,str(o/'pack.cpp')))
inputs={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [unit,o/'pack.cpp',o/'pack.h',o/'cli.cpp',b/'generated/runtime.h']};(o/'inputs.json').write_text(json.dumps(inputs,indent=2)+'\n');(o/'build.pid').write_text(str(os.getpid())+'\n')
with (o/'build.log').open('x') as log:
 def run(cmd):
  cmd=list(map(str,cmd));label='$ '+shlex.join(cmd);print(label,flush=True);log.write(label+'\n');log.flush();p=subprocess.run(cmd,cwd=b/'build',stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True);print(p.stdout,flush=True);log.write(p.stdout+f'\nexit_code={p.returncode}\n');log.flush()
  if p.returncode:raise SystemExit(p.returncode)
 if a.reuse_goose:
  import shutil
  assert not a.goose
  shutil.copytree(a.reuse_goose/'generated',o/'generated',ignore=shutil.ignore_patterns('runtime.c'))
  inputs[str(a.reuse_goose/'goose.o')]=hashlib.sha256((a.reuse_goose/'goose.o').read_bytes()).hexdigest()
 if a.goose:
  pin=json.loads((r/'runtime/compiler/current.json').read_text());g=Path(pin['compiler']);assert hashlib.sha256(g.read_bytes()).hexdigest()==pin['sha256'];(o/'generated').mkdir()
  run(['python3',r/'tools/compose-goose.py',o/'generated/runtime.goose',b/'generated/runtime.goose',r/'goose/model/expert-lru.goose'])
  run([g,'check',o/'generated/runtime.goose']);run([g,'--standalone','-o',o/'generated/runtime.c','--header',o/'generated/runtime.h',o/'generated/runtime.goose'])
  run(['gcc-13','-O2','-fPIC','-c',o/'generated/runtime.c','-o',o/'goose.o'])
 for key,source,out in [('unity_10_cxx' ,o/'unit.cpp',o/'unit.o'),('cli.cpp',o/'cli.cpp',o/'cli.o')]:
  if key=='unity_10_cxx' and a.reuse_unit:
   assert (a.reuse_unit.parent/'pack.cpp').read_bytes()==(o/'pack.cpp').read_bytes(), 'cannot reuse changed pack unit'
   inputs[str(a.reuse_unit)]=hashlib.sha256(a.reuse_unit.read_bytes()).hexdigest();continue
  line=next(x for x in commands if ' -c ' in x and key in x);cmd=shlex.split(line)
  for flag in ['-MD']:
   if flag in cmd:cmd.remove(flag)
  for flag in ['-MT','-MF']:
   if flag in cmd:i=cmd.index(flag);del cmd[i:i+2]
  cmd[cmd.index('-o')+1]=str(out);cmd[cmd.index('-c')+1]=str(source);cmd.insert(1,'-I'+str(o));
  if a.goose or a.reuse_goose:cmd.insert(1,'-I'+str(o/'generated'))
  run(cmd)
 line=next(x for x in commands if '-Wl,-soname,libllama.so.0' in x);cmd=shlex.split(line.split('&&')[1].strip());cmd=[x for x in cmd if not x.startswith('-Wl,--dependency-file=')]
 cmd[cmd.index('-o')+1]=str(o/'build/llama/libllama.so.0');
 if a.goose or a.reuse_goose:
  old=[x for x in cmd if x.endswith('/generated/runtime.c.o')];assert len(old)==1;cmd[cmd.index(old[0])]=str(a.reuse_goose/'goose.o' if a.reuse_goose else o/'goose.o')
 old_obj='llama/CMakeFiles/llama.dir/Unity/unity_10_cxx.cxx.o';assert old_obj in cmd;cmd[cmd.index(old_obj)]=str(a.reuse_unit if a.reuse_unit else o/'unit.o');run(cmd)
 run(['g++-13',o/'cli.o',o/'build/llama/libllama.so.0','/home/end0/AI/LlamaServer/franken-recovery-20261116/support/bin/libggml-base.so.0','/home/end0/AI/LlamaServer/franken-recovery-20261116/support/bin/libggml.so.0','-Wl,-rpath,'+str(o/'build/llama'),'-ldl','-o',o/'build/edge0-cli'])
 (o/'inputs.json').write_text(json.dumps(inputs,indent=2)+'\n')
 log.write('RESULT PASS: isolated changed native unit + CLI; unchanged objects reused\n')
