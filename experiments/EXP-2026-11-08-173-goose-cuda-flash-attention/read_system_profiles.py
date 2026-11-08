from pathlib import Path
import subprocess,json,csv,hashlib
F=Path(__file__).resolve().parents[2];E=F/'experiments/EXP-2026-11-08-173-goose-cuda-flash-attention';P=F/'runtime/exp173/profiles';summaries={}
for label in ['inline-value','exp172-control']:
 args=['nsys','stats','--report','cuda_gpu_kern_sum','--format','csv','--output',str(P/label),str(P/(label+'.nsys-rep'))];q=subprocess.run(args,capture_output=True,text=True,timeout=300);(P/(label+'-stats.stdout')).write_text(q.stdout);(P/(label+'-stats.stderr')).write_text(q.stderr);print('stats',label,q.returncode,flush=True)
 if q.returncode:print(q.stderr[-2500:]);raise SystemExit(q.returncode)
 files=list(P.glob(label+'*cuda_gpu_kern_sum*.csv'));assert len(files)==1,files
 rows=list(csv.DictReader(files[0].open()));assert rows
 print('COLUMNS',list(rows[0]),flush=True)
 summaries[label]=rows
 for r in rows[:12]:print(label,{k:(v[:180]if k=='Name'else v)for k,v in r.items()},flush=True)
(E/'system-profile-kernel-summary.json').write_text(json.dumps(dict(kind='CUDA trace of actual whole Apex32K/128,including startup/warmup+decode;profile overhead excludes throughput acceptance;kernel totals not microbenchmark timing',reports={k:{'path':str(P/(k+'.nsys-rep')),'sha256':hashlib.sha256((P/(k+'.nsys-rep')).read_bytes()).hexdigest()}for k in summaries},kernels=summaries,compiler_changed=False,production_changed=False),indent=2)+'\n')
