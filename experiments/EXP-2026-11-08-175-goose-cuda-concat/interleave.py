from pathlib import Path
import os,json,time,subprocess,urllib.request,signal,sys,threading
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
L=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107/results')
stage=sys.argv[1]; mode=sys.argv[2]           # exec stage, goose|native
label=f'interleave-{stage}-{mode}'
R=A/f'runtime/{stage}/run'
B=A/f'runtime/exp175/results/{label}'; T=A/f'runtime/exp175/measured/{label}/run'
B.mkdir(parents=True,exist_ok=True);T.mkdir(parents=True,exist_ok=True)
if (B/'summary.json').exists():raise SystemExit('receipt exists')
for f in R.iterdir():
    if f.is_file() or f.is_symlink():
        q=T/f.name
        if q.is_symlink():q.unlink()
        elif q.exists():raise RuntimeError(str(q))
        q.symlink_to(f.resolve())
argv=json.loads((L/'plan-apex-native-argv.json').read_text());argv[0]=str(T/'llama-server')
req=(L/'plan-apex-native-32k128-request.json').read_bytes()
env=dict(os.environ)
for k in list(env):
    if k.startswith('GGML_GOOSE_'):env.pop(k)
saved=json.loads(Path('/home/end0/AI/LlamaServer/franken-goose-exp150-expert-registry-20261107/results/model/audit-exp150-omp-apex-32k128-environment.json').read_text())
env.update(saved);env['LD_PRELOAD']='';env['GGML_EXPERT_HOTSTORE']='0';env['LD_LIBRARY_PATH']=str(T)
if mode=='native':env['GGML_GOOSE_CUDA_CONCAT']='0'
log=(B/'server.log').open('w')
p=subprocess.Popen(argv,env=env,cwd=T,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
clock_samples=[];clock_errors=[]
stop=threading.Event()
def sample():
    while not stop.wait(0.5):
        try:
            q=subprocess.run(['nvidia-smi','--query-gpu=clocks.sm,power.draw,utilization.gpu','--format=csv,noheader,nounits'],capture_output=True,text=True,timeout=3)
            line=q.stdout.strip().splitlines()[0]
            a=[x.strip() for x in line.split(',')]
            clock_samples.append((float(a[0]),float(a[1]),float(a[2])))
        except Exception as e:
            clock_errors.append(repr(e))
th=threading.Thread(target=sample,daemon=True); th.start()
try:
    deadline=time.monotonic()+1200
    while True:
        if p.poll() is not None:raise RuntimeError('exited')
        try:
            with urllib.request.urlopen('http://127.0.0.1:8081/health',timeout=2) as r:
                if json.load(r).get('status')=='ok':break
        except Exception:pass
        if time.monotonic()>deadline:raise RuntimeError('startup timeout')
        time.sleep(1)
    load=os.getloadavg()
    reqq=urllib.request.Request('http://127.0.0.1:8081/completion',data=req,headers={'Content-Type':'application/json'})
    with urllib.request.urlopen(reqq,timeout=1800) as r:resp=json.load(r)
    clock_samples.clear()  # keep only the measurement window from here on is too late; keep all
    base=json.loads((L/'plan-apex-native-32k128-response.json').read_text())
    sm=[c[0] for c in clock_samples]; pw=[c[1] for c in clock_samples]; ut=[c[2] for c in clock_samples]
    s=dict(stage=stage,mode=mode,loadavg=load,timings=resp.get('timings'),
      tokens_equal=resp.get('tokens')==base.get('tokens'),
      content_equal=resp.get('content')==base.get('content'),
      probs_equal=resp.get('completion_probabilities')==base.get('completion_probabilities'),
      gpu_clock_sm_min=min(sm) if sm else None,gpu_clock_sm_max=max(sm) if sm else None,
      gpu_power_max=max(pw) if pw else None,gpu_util_max=max(ut) if ut else None,
      gpu_samples=len(sm),gpu_errors=clock_errors[:3])
    (B/'summary.json').write_text(json.dumps(s,indent=2)+'\n')
    print('IL_RESULT '+json.dumps(s),flush=True)
except Exception as e:
    (B/'failure.txt').write_text(repr(e));print('IL_FAILED',e,flush=True)
finally:
    stop.set();th.join(timeout=3)
    if p.poll() is None:
        os.killpg(p.pid,signal.SIGTERM)
        try:p.wait(timeout=15)
        except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait()
    log.close();print('IL_EXIT',p.returncode,flush=True)
