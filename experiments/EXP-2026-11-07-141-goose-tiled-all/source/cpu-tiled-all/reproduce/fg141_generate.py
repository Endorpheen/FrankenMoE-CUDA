from pathlib import Path
import json,hashlib,shutil,subprocess
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/cpu-tiled-all';parts=[]
for name in ['common','unpack','micro','driver','entry']:
 src=Path('/tmp')/f'fg141_{name}.goose'
 if not src.exists():continue
 text=src.read_text().replace('WeightTile&=&w.a','WeightTile& = &w.a');(s/f'{name}.goose').write_text(text);parts.append(text)
code='\n'.join(parts);(s/'kernel.goose').write_text(code);shutil.copy2(r/'source/cpu-moe-tiled-register/packed_simd.goose',s/'packed_simd.goose');producer=r/'compiler/stage3-0424b7bce8dd';assert hashlib.sha256(producer.read_bytes()).hexdigest()=='0424b7bce8ddfbf308d2208612cef01de1780ada26337ff0f556a6786f9ea364';cmd=[str(producer),'-o',str(r/'generated/cpu-tiled-all.c'),str(s/'kernel.goose')];p=subprocess.run(cmd,capture_output=True,text=True,cwd=r);(s/'generation.log').write_text(p.stdout+p.stderr);(s/'generation-command.json').write_text(json.dumps(cmd,indent=2)+'\n');print('generation',p.returncode,p.stdout[-500:],p.stderr[-2500:],flush=True);p.check_returncode();print('Goose lines',len(code.splitlines()),'emitted C bytes',(r/'generated/cpu-tiled-all.c').stat().st_size)
