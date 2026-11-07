from pathlib import Path
import re,json,subprocess,hashlib,shutil
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/moe-quantized'
parts=[]
for name in ['common','low','iq','dequant','entry']:
    src=Path('/tmp')/f'fg139_matrix_{name}.goose'
    dst=s/f'matrix-{name}.goose';shutil.copy2(src,dst)
    parts.append(dst.read_text())
code=''.join(parts)
code=code.replace('fn word32(bytes:u8[64],','fn word32(bytes:const u8[64]&,')
code=re.sub(r'(?<![\w.])\d+\.\d+(?!\w)',lambda m:'('+m[0]+' as f32)',code)
if (s/'matrix-generation.log').exists() and (s/'matrix-generation.log').read_text().strip() and (s/'matrix.goose').exists():
    n=1
    while (s/f'matrix-rejected-{n}.goose').exists():n+=1
    shutil.copy2(s/'matrix.goose',s/f'matrix-rejected-{n}.goose');shutil.copy2(s/'matrix-generation.log',s/f'matrix-rejected-{n}.log')
(s/'matrix.goose').write_text(code)
shutil.copy2(r/'source/cpu-moe/q4/packed_simd.goose',s/'packed_simd.goose')
producer=r/'compiler/stage3-7ab1a1ca1cdb'
assert hashlib.sha256(producer.read_bytes()).hexdigest()=='7ab1a1ca1cdb82bd45c447cafc9a8143cbcb23dab29bae3e813f66069c5d0846'
cmd=[str(producer),'-o',str(r/'generated/moe-matrix.c'),str(s/'matrix.goose')]
p=subprocess.run(cmd,cwd=r,capture_output=True,text=True)
(s/'matrix-generation.log').write_text(p.stdout+p.stderr);(s/'matrix-generation-command.json').write_text(json.dumps(cmd,indent=2)+'\n')
print('matrix generation',p.returncode,p.stdout[-1500:],p.stderr[-2500:],flush=True);p.check_returncode()
print('Goose lines',len(code.splitlines()),'generated C bytes',(r/'generated/moe-matrix.c').stat().st_size)
