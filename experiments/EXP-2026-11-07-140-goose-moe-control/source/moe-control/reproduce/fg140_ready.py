from pathlib import Path
import json,sys,urllib.request,urllib.error,time
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');mode=sys.argv[1];assert (r/'results/current-mode.txt').read_text()==mode;pid=int((r/'results/current-pid.txt').read_text());args=json.loads((r/'results'/f'{mode}-argv.json').read_text());env=json.loads((r/'results'/f'{mode}-environment.json').read_text());assert Path(f'/proc/{pid}/exe').resolve()==Path(args[0]).resolve();maps=sorted(set(line.split()[-1] for line in Path(f'/proc/{pid}/maps').read_text().splitlines() if 'libggml-cpu.so.' in line));assert maps==[str((Path(env['LD_LIBRARY_PATH'])/'libggml-cpu.so.0.27.0').resolve())],maps
deadline=time.monotonic()+45
while True:
 assert Path(f'/proc/{pid}/exe').resolve()==Path(args[0]).resolve()
 try:
  with urllib.request.urlopen('http://127.0.0.1:8081/health',timeout=3) as f:
   if json.load(f)['status']=='ok':break
 except urllib.error.HTTPError as e:
  if e.code!=503:raise
 if time.monotonic()>=deadline:raise TimeoutError('Own server still loading; inspect same process without restarting')
 time.sleep(0.5)
(r/'results'/f'{mode}-loaded-libraries.json').write_text(json.dumps(maps,indent=2)+'\n');print('Own experimental model ready',mode,pid)
