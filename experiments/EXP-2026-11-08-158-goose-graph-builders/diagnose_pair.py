from pathlib import Path
import subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-158-goose-graph-builders';base=(E/'run_apex.py').read_text()
for kind,runtime in [('control-exp157','exp157'),('candidate-exp158','exp158')]:
 s=base.replace("R=A/'runtime/exp158/run'",f"R=A/'runtime/{runtime}/run'").replace("B=A/'runtime/exp158/results/apex-32k128'",f"B=A/'runtime/exp158/results/pair-{kind}'").replace("T=A/'runtime/exp158/measured/run'",f"T=A/'runtime/exp158/measured/pair-{kind}'");p=F/f'runtime/exp158/run-pair-{kind}.py';p.write_text(s);q=subprocess.run(['/usr/bin/python3',str(p)]);assert q.returncode==0
 import json
 d=json.loads((F/f'runtime/exp158/results/pair-{kind}/process.json').read_text());assert d['phase']=='COMPLETE_DIAGNOSTIC' and d['exit_code']==0
