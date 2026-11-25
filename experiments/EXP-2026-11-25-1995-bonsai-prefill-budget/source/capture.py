"""Capture one requested Bonsai phase trace and sample its board/process VRAM."""
import json
import os
from pathlib import Path
import subprocess
import sys
import time

repo=Path(__file__).resolve().parents[3]
output,base,profile,model=map(Path,sys.argv[1:5])
n=int(sys.argv[5])
assert n in (4096,32768)
output.mkdir()
ref=repo/'experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json'
request=json.loads(ref.read_text())
env=dict(PATH='/usr/bin:/bin',HOME=os.environ['HOME'],LANG='C',CUDA_VISIBLE_DEVICES='0',BONSAI_UBATCH='1024',
         BONSAI_FLASH_F32='1',BONSAI_FLASH_REUSE='1',BONSAI_FLASH_ORDERED_MERGE='1',
         BONSAI_FLASH_COMPACT='1',BONSAI_FLASH_ASYNC='1')
nsys=os.environ.get('NSYS','/usr/local/bin/nsys')
prefix=[nsys,'profile','--trace=cuda,nvtx,osrt','--sample=none','--cpuctxsw=none',
        '--osrt-threshold=1000','--force-overwrite=false','-o',str(output/'trace'),'taskset','0f0f',
        str(profile),str(model),str(base/'pq2-mmq-wide.cubin'),str(base/'frozen-goose.ptx'),
        '36864','16','--ignore-eos']
(output/'profile-settings.json').write_text(json.dumps(dict(command=prefix,env=env,
    prompt_tokens=n,output_tokens=16),indent=2)+'\n')


def swap():
    return int(next(s.split()[1] for s in Path('/proc/vmstat').read_text().splitlines() if s.startswith('pswpout ')))


start=time.monotonic()
before=swap()
with (output/'board.csv').open('w') as g,(output/'process-vram.csv').open('w') as v,(output/'trace.log').open('w') as log:
    board_fields='timestamp,memory.used,clocks.current.sm,clocks.current.memory,pcie.link.gen.current,pcie.link.width.current,power.draw,temperature.gpu,utilization.gpu'
    board=subprocess.Popen(['nvidia-smi','--query-gpu='+board_fields,'--format=csv,noheader,nounits','-lms','250'],stdout=g)
    memory=subprocess.Popen(['nvidia-smi','--query-compute-apps=pid,used_gpu_memory','--format=csv,noheader,nounits','-lms','250'],stdout=v)
    try:
        subprocess.run([*prefix,*map(str,request['prompt'][:n])],env=env,stdout=log,stderr=subprocess.STDOUT,
                       timeout=180,check=True)
    finally:
        for monitor in (board,memory):
            monitor.terminate()
            monitor.wait(timeout=5)
(output/'resources.json').write_text(json.dumps(dict(wall_with_init_s=time.monotonic()-start,
        pswpout_before=before,pswpout_after=swap()),indent=2)+'\n')
subprocess.run([nsys,'export','--type','sqlite','-o',str(output/'trace.sqlite'),str(output/'trace.nsys-rep')],
               check=True,timeout=45)
