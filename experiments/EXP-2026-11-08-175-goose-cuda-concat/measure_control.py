from pathlib import Path
import os,json,time,subprocess,urllib.request,urllib.error,signal,traceback,sys
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
L=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107/results')
stage=sys.argv[1]                      # e.g. exp174 or exp175
assert stage in ('exp174','exp175')
label='control-'+stage
R=A/f'runtime/{stage}/run'
B=A/f'runtime/exp175/results/{label}'
T=A/f'runtime/exp175/measured/{label}/run'
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
env.update(saved)
env['LD_PRELOAD']='';env['GGML_EXPERT_HOTSTORE']='0';env['LD_LIBRARY_PATH']=str(T)
(B/'environment.json').write_text(json.dumps({k:v for k,v in env.items()
  if k.startswith(('GGML_GOOSE_','GGML_EXPERT_')) or k=='LD_LIBRARY_PATH'},indent=2)+'\n')
log=(B/'server.log').open('w')
p=subprocess.Popen(argv,env=env,cwd=T,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
print('CONTROL_STARTED',stage,p.pid,flush=True)
try:
    deadline=time.monotonic()+1200
    while True:
        if p.poll() is not None:raise RuntimeError('exited '+str(p.returncode))
        try:
            with urllib.request.urlopen('http://127.0.0.1:8081/health',timeout=2) as r:
                if json.load(r).get('status')=='ok':break
        except Exception:pass
        if time.monotonic()>deadline:raise RuntimeError('startup timeout')
        time.sleep(1)
    load0=os.getloadavg()
    reqq=urllib.request.Request('http://127.0.0.1:8081/completion',data=req,headers={'Content-Type':'application/json'})
    with urllib.request.urlopen(reqq,timeout=1800) as r:resp=json.load(r)
    load1=os.getloadavg()
    base=json.loads((L/'plan-apex-native-32k128-response.json').read_text())
    s=dict(stage=stage,loadavg_before=load0,loadavg_after=load1,
      timings=resp.get('timings'),tokens_equal=resp.get('tokens')==base.get('tokens'),
      content_equal=resp.get('content')==base.get('content'),
      probs_equal=resp.get('completion_probabilities')==base.get('completion_probabilities'))
    (B/'summary.json').write_text(json.dumps(s,indent=2)+'\n')
    print('CONTROL_RESULT '+json.dumps(s),flush=True)
except Exception as e:
    (B/'failure.txt').write_text(traceback.format_exc());print('CONTROL_FAILED',e,flush=True)
finally:
    if p.poll() is None:
        os.killpg(p.pid,signal.SIGTERM)
        try:p.wait(timeout=15)
        except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait()
    log.close();print('CONTROL_EXIT',p.returncode,flush=True)
