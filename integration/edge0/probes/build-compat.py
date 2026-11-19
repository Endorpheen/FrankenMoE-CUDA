#!/usr/bin/env python3
"""Reuse EXP251's mechanical C-to-device extraction; never touch old outputs."""
from pathlib import Path
import subprocess,json,hashlib,re,shlex,os
R=Path(__file__).resolve().parents[3];O=Path('/home/end0/AI/LlamaServer/edge0-compat-probe-922b');O.mkdir(exist_ok=True);pin=json.loads((R/'runtime/compiler/current.json').read_text());G=Path(pin['compiler']);assert hashlib.sha256(G.read_bytes()).hexdigest()==pin['sha256']
(O/'build.pid').write_text(str(os.getpid())+'\n')
with (O/'build.log').open('a') as log:
 def run(cmd):
  cmd=list(map(str,cmd));s='$ '+shlex.join(cmd);print(s,flush=True);log.write(s+'\n');log.flush();p=subprocess.run(cmd,capture_output=True,text=True);print(p.stdout+p.stderr,flush=True);log.write(p.stdout+p.stderr+f'\nexit_code={p.returncode}\n');log.flush();assert p.returncode==0
 run(['python3',R/'tools/compose-goose.py',O/'device.goose',R/'goose/cuda/goose-cuda-dequant-q3k/device.goose',R/'integration/edge0/probes/compat-dot.goose'])
 with (O/'device.goose').open('a') as source: source.write('\nfn main() {}\n')
 run([G,'check',O/'device.goose']);run([G,'--standalone','-o',O/'device.c',O/'device.goose'])
 g=(O/'device.c').read_text();funcs=[]
 pattern=r'(?m)^(?:static )?(?:int32_t|int64_t|uint32_t|uint64_t|double|float|void|uint8_t|bool)\s+(gg_function_\d+|gg_div|gg_mod|gg_udiv|gg_umod|gg_double_bits|e0c_\w+)\([^;{}]*\)\s*\{'
 for m in re.finditer(pattern,g):
  at=m.end();depth=1
  while depth:depth+=(g[at]=='{')-(g[at]=='}');at+=1
  if m.group(1).endswith('_g'): funcs.append(g[m.start():at])
 assert any('e0c_q3_part_g(' in f for f in funcs)
 funcs.insert(0,'static void gg_fail(const char * p){(void)p;asm("trap;");}')
 declarations=[x[:x.index('{')].rstrip()+';' for x in funcs];text='\n'.join(declarations+funcs)
 text=re.sub(r'(?m)^(?:static )?((?:int32_t|int64_t|uint32_t|uint64_t|double|float|void|uint8_t|bool)\s+)','static __device__ __forceinline__ '+r'\1',text)
 assert not any(s in text for s in ['gg_allocate(','gg_get_stack(','gg_append(']),'allocator in device closure'
 numeric=g[g.index('#define GS_DIVU(T,'):g.index('#endif  /* GS_DEBUG */')+len('#endif  /* GS_DEBUG */')]
 numeric+=''
 pre='#define GS_DEBUG 0\n#define GS_RANGE(v, lo, hi, t, f, l) (v)\n#define GS_UNREACHABLE(f,l) __builtin_unreachable()\nstatic __device__ const char gs_file0[] = \"Goose compatible dot\";\nstatic __device__ void gs_divfail(const char*,int){asm(\"trap;\");}\nstatic __device__ void gs_divovf(const char*,int){asm(\"trap;\");}\n'
 (O/'device.cuh').write_text(pre+numeric+'\n'+text+'\n');(O/'emission.json').write_text(json.dumps({'compiler':pin,'function_count':len(funcs),'sources':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [R/'goose/cuda/goose-cuda-dequant-q3k/device.goose',R/'integration/edge0/probes/compat-dot.goose']},'device_header_sha256':hashlib.sha256((O/'device.cuh').read_bytes()).hexdigest()},indent=2)+'\n')
 print('device closure functions',len(funcs),flush=True)
