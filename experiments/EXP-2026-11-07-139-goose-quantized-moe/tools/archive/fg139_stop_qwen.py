from pathlib import Path
import os,signal,sys,json
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');mode=(r/'results/current-mode.txt').read_text();assert mode==sys.argv[1] and mode.startswith('qwen139-')
pid=int((r/'results/current-pid.txt').read_text());argv=json.loads((r/'results'/f'{mode}-argv.json').read_text());assert Path(f'/proc/{pid}/exe').resolve()==Path(argv[0]).resolve()==(r/'variants/qwen839a-shared/build/bin/llama-server').resolve()
os.kill(pid,signal.SIGINT);print('Stopped own experimental Qwen server',pid,mode)
