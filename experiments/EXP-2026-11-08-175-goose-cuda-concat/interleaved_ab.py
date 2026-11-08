"""Interleaved A/B comparison of two candidate DSO lineages under identical
conditions.

Order is alternated (old, new, new, old) so a slowly drifting machine cannot
favour one arm. Each arm runs the whole fixed-profile request from a fresh
server and process cache. GPU clock/power/utilization are sampled during the
measured window only, because idle clocks say nothing about inference.
"""
from pathlib import Path
import os,json,time,subprocess,urllib.request,signal,sys,threading
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
L=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107/results')
ORDER=sys.argv[1:]                      # e.g. exp174 exp175 exp175 exp174
assert ORDER and all(x in ('exp174','exp175') for x in ORDER),ORDER
OUT=A/'runtime/exp175/results/interleaved'
OUT.mkdir(parents=True,exist_ok=True)
req=(L/'plan-apex-native-32k128-request.json').read_bytes()
base=json.loads((L/'plan-apex-native-32k128-response.json').read_text())
results=[]

def one(stage,idx):
    R=A/f'runtime/{stage}/run'
    T=A/f'runtime/exp175/measured/interleaved-{idx}-{stage}/run'
    B=OUT/f'{idx}-{stage}'
    B.mkdir(parents=True,exist_ok=True);T.mkdir(parents=True,exist_ok=True)
    for f in R.iterdir():
        if f.is_file() or f.is_symlink():
            q=T/f.name
            if q.is_symlink():q.unlink()
            elif q.exists():raise RuntimeError(str(q))
            q.symlink_to(f.resolve())
    argv=json.loads((L/'plan-apex-native-argv.json').read_text());argv[0]=str(T/'llama-server')
    env=dict(os.environ)
    for k in list(env):
        if k.startswith('GGML_GOOSE_'):env.pop(k)
    saved=json.loads(Path('/home/end0/AI/LlamaServer/franken-goose-exp150-expert-registry-20261107/results/model/audit-exp150-omp-apex-32k128-environment.json').read_text())
    env.update(saved);env['LD_PRELOAD']='';env['GGML_EXPERT_HOTSTORE']='0';env['LD_LIBRARY_PATH']=str(T)
    log=(B/'server.log').open('w')
    p=subprocess.Popen(argv,env=env,cwd=T,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
    clocks=[];stop=threading.Event()
    def sample():
        while not stop.wait(0.25):
            try:
                q=subprocess.run(['nvidia-smi','--query-gpu=clocks.sm,power.draw,utilization.gpu',
                                  '--format=csv,noheader,nounits'],capture_output=True,text=True,timeout=3)
                a=[x.strip() for x in q.stdout.strip().splitlines()[0].split(',')]
                clocks.append((float(a[0]),float(a[1]),float(a[2]),time.time()))
            except Exception: pass
    th=threading.Thread(target=sample,daemon=True);th.start()
    rec=dict(stage=stage,index=idx)
    try:
        dl=time.monotonic()+1200
        while True:
            if p.poll() is not None:raise RuntimeError('exited')
            try:
                with urllib.request.urlopen('http://127.0.0.1:8081/health',timeout=2) as r:
                    if json.load(r).get('status')=='ok':break
            except Exception:pass
            if time.monotonic()>dl:raise RuntimeError('startup timeout')
            time.sleep(1)
        rec['loadavg_before']=os.getloadavg()
        clocks.clear()
        t0=time.time()
        rq=urllib.request.Request('http://127.0.0.1:8081/completion',data=req,
                                  headers={'Content-Type':'application/json'})
        with urllib.request.urlopen(rq,timeout=1800) as r:resp=json.load(r)
        t1=time.time()
        rec['loadavg_after']=os.getloadavg()
        rec['timings']=resp.get('timings')
        rec['tokens_equal']=resp.get('tokens')==base.get('tokens')
        rec['content_equal']=resp.get('content')==base.get('content')
        rec['probs_equal']=resp.get('completion_probabilities')==base.get('completion_probabilities')
        win=[c for c in clocks if t0<=c[3]<=t1]
        if win:
            rec['gpu_sm_min']=min(c[0] for c in win);rec['gpu_sm_max']=max(c[0] for c in win)
            rec['gpu_power_max_w']=max(c[1] for c in win);rec['gpu_util_max']=max(c[2] for c in win)
            rec['gpu_samples']=len(win)
    except Exception as e:
        rec['error']=repr(e)
    finally:
        stop.set();th.join(timeout=3)
        if p.poll() is None:
            os.killpg(p.pid,signal.SIGTERM)
            try:p.wait(timeout=15)
            except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait()
        log.close();rec['exit']=p.returncode
    (B/'receipt.json').write_text(json.dumps(rec,indent=2)+'\n')
    print('ARM '+json.dumps({k:rec.get(k) for k in
        ['stage','index','tokens_equal','content_equal','probs_equal','gpu_sm_max','gpu_power_max_w']}),flush=True)
    return rec

for i,st in enumerate(ORDER):
    results.append(one(st,i))
(OUT/'interleaved.json').write_text(json.dumps(results,indent=2)+'\n')
print('INTERLEAVED_DONE',len(results),flush=True)
