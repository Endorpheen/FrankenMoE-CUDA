from pathlib import Path
import json,time,urllib.request
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');pid=int((r/'results/current-pid.txt').read_text());mode=(r/'results/current-mode.txt').read_text();assert mode.startswith(('rms','q8k','moe','element'))
maps=Path(f'/proc/{pid}/maps').read_text()
if mode.startswith('element'):
 variant=('elementwise-vector-trace' if '-trace' in mode else 'elementwise-vector') if '-vector' in mode else 'elementwise-fma' if '-fma' in mode else 'elementwise'
 if '-perf' in mode:variant+='-perf'
 assert str(r/f'variants/{variant}/run/libggml-cpu.so.0.27.0') in maps
elif 'cpu-tiled' in mode and 'pure' in mode:
 variant=('cpu-moe-tiled-register-perf' if '-perf' in mode else 'cpu-moe-tiled-register') if 'register' in mode else 'cpu-moe-tiled-perf' if 'tiled-perf' in mode else 'cpu-moe-tiled'
 assert str(r/f'variants/{variant}/run/libggml-cpu.so.0.27.0') in maps
 assert 'libfranken-goose-rms-block' not in maps
else:
 lib=r/'build'/('libfranken-goose-rms-block-trace.so' if mode.startswith('rms-trace') else 'libfranken-goose-rms-block.so');assert str(lib) in maps
for _ in range(40):
 try:
  with urllib.request.urlopen('http://127.0.0.1:8081/health',timeout=1) as a:assert json.load(a)['status']=='ok'
  print('Apex ready',mode);break
 except Exception:time.sleep(0.5)
else:raise RuntimeError('Apex startup timeout')
