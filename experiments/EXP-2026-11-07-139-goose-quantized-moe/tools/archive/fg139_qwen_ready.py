from pathlib import Path
import json,sys,urllib.request
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');mode=sys.argv[1];assert (r/'results/current-mode.txt').read_text()==mode;pid=int((r/'results/current-pid.txt').read_text());env=json.loads((r/'results'/f'{mode}-environment.json').read_text());argv=json.loads((r/'results'/f'{mode}-argv.json').read_text());assert Path(f'/proc/{pid}/exe').resolve()==Path(argv[0]).resolve();expected=env['LD_LIBRARY_PATH'];paths=sorted(set(line.split()[-1] for line in Path(f'/proc/{pid}/maps').read_text().splitlines() if 'libggml-cpu.so.' in line or 'libggml-base.so.' in line));assert len(paths)==2 and all(p.startswith(expected+'/') for p in paths),paths
with urllib.request.urlopen('http://127.0.0.1:8081/health',timeout=3) as f:assert json.load(f)['status']=='ok'
(r/'results'/f'{mode}-loaded-libraries.json').write_text(json.dumps(paths,indent=2)+'\n');print('Own Qwen ready; exact CPU/base maps verified',mode,pid)
