import json,shlex,subprocess,sys,re
from pathlib import Path
CU='/usr/local/cuda-12.6'
SRC=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments/EXP-2026-11-08-180-goose-cuda-mmq-q8-1/source')
d=json.load(open('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/compile_commands.json'))
e=[x for x in d if x['file'].endswith('mmq.cu')] or [x for x in d if x['file'].endswith('acc.cu')]
e=e[0]; a=shlex.split(e['command'])
# keep flags, drop the original source/output and any multi-arch gencode
out=[]; i=0
while i<len(a):
    x=a[i]
    if x=='--generate-code' or x.startswith('--generate-code'):
        i+=1; continue
    if x in ('-c','-o'): i+=2; continue
    if x.endswith('.cu') or x.endswith('.o'): i+=1; continue
    out.append(x); i+=1
for tag in ('old','new'):
    D=Path(f'/tmp/b180-{tag}')
    cmd=out+['-I/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml/src/ggml-cuda','-I'+str(SRC),'-arch=sm_89','-cubin','-o',str(D/'out.cubin'),str(D/'cuda-runtime.cu')]
    r=subprocess.run(cmd,cwd=e['directory'],capture_output=True,text=True,timeout=1800)
    if r.returncode: print(tag,'COMPILE FAIL'); print('\n'.join(r.stderr.splitlines()[:4])); continue
    subprocess.run([f'{CU}/bin/cuobjdump','-sass',str(D/'out.cubin')],stdout=open(D/'out.sass','w'),stderr=subprocess.DEVNULL)
    t=(D/'out.sass').read_text()
    tot=len(re.findall(r'^\s+/\*[0-9a-f]{4}\*/',t,re.M))
    imad=len(re.findall(r'\bIMAD',t))
    bra=len(re.findall(r'\bBRA',t))
    lds=len(re.findall(r'\bLDS\b',t))
    sts=len(re.findall(r'\bSTS\b',t))
    print(f'{tag}: total={tot} IMAD={imad} BRA={bra} LDS={lds} STS={sts}')
