from pathlib import Path
import os,signal,sys
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
mode=(r/'results/current-mode.txt').read_text()
assert mode==sys.argv[1],mode
pid=int((r/'results/current-pid.txt').read_text())
assert Path(f'/proc/{pid}/exe').resolve()==(r/'run/llama-server').resolve()
os.kill(pid,signal.SIGINT)
print('Stopped own experimental Apex server',pid,mode)
