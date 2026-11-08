from pathlib import Path
import os,json,time,subprocess,urllib.request,urllib.error,threading,hashlib,signal,traceback
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
L=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107/results')
R=A/'runtime/exp175/run'
mode=os.environ.get('FG175_MEASURE_MODE','goose')
assert mode in ['goose','native-rollback'],mode
label='apex-32k128-concat-pair-'+mode
B=A/'runtime/exp175/results'/label
T=A/'runtime/exp175/measured'/label/'run'
B.mkdir(parents=True,exist_ok=True);T.mkdir(parents=True,exist_ok=True)
if (B/'process.json').exists():raise SystemExit('Existing receipt;inspect before rerunning')
for f in R.iterdir():
    if f.is_file() or f.is_symlink():
        q=T/f.name
        if q.is_symlink():q.unlink()
        elif q.exists():raise RuntimeError(str(q))
        q.symlink_to(f.resolve())
libs={n:{'path':str((T/n).resolve()),'sha256':hashlib.sha256((T/n).resolve().read_bytes()).hexdigest()}
      for n in ['libllama.so','libggml.so','libggml-base.so','libggml-cpu.so','libggml-cuda.so']}
(B/'libraries.json').write_text(json.dumps(libs,indent=2)+'\n')
argv=json.loads((L/'plan-apex-native-argv.json').read_text());argv[0]=str(T/'llama-server')
(B/'argv.json').write_text(json.dumps(argv,indent=2)+'\n')
req=(L/'plan-apex-native-32k128-request.json').read_bytes();(B/'request.json').write_bytes(req)
for source in ['/proc/net/tcp','/proc/net/tcp6']:
    for line in Path(source).read_text().splitlines()[1:]:
        fields=line.split()
        if fields[1].endswith(':1F91') and fields[3]=='0A':raise SystemExit('Port 8081 occupied;no other process stopped')
env=dict(os.environ)
for k in list(env):
    if k.startswith('GGML_GOOSE_'):env.pop(k)
saved=json.loads(Path('/home/end0/AI/LlamaServer/franken-goose-exp150-expert-registry-20261107/results/model/audit-exp150-omp-apex-32k128-environment.json').read_text())
env.update(saved)
removed=[p for p in env.get('LD_PRELOAD','').split() if Path(p).name in
         ['libfranken-goose-silu.so','libfranken-goose-scale.so','libfranken-goose-rms-block.so']]
assert len(removed)==3
preserved=[p for p in env['LD_PRELOAD'].split() if p not in removed];assert not preserved
env['LD_PRELOAD']=' '.join(preserved)
env['GGML_EXPERT_HOTSTORE']='0';env['LD_LIBRARY_PATH']=str(T)
env.pop('GGML_GOOSE_CUDA_CONCAT',None)
if mode=='native-rollback':env['GGML_GOOSE_CUDA_CONCAT']='0'
(B/'environment.json').write_text(json.dumps({k:v for k,v in env.items()
    if k.startswith(('GGML_GOOSE_','GGML_EXPERT_')) or k=='LD_LIBRARY_PATH'},indent=2)+'\n')
(B/'control-mode.json').write_text(json.dumps(dict(mode=mode,
    same_cuda_library_sha256=libs['libggml-cuda.so']['sha256'],
    pair='EXP175-CONCAT-only;all other EXP174 modules identical'),indent=2)+'\n')
log=(B/'server.log').open('w')
p=subprocess.Popen(argv,env=env,cwd=T,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
receipt=dict(pid=p.pid,started=time.time(),phase='LOADING',model='Apex-35B-V1.0-Q4_K_M',
             input_tokens=32768,output_tokens=128,preliminary=True,
             not_acceptance='EXP175 CONCAT arithmetic and host planning in Goose;all other EXP174 components unchanged;actual CONCAT witness required;full migration incomplete',
             production_changed=False)
(B/'process.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('APEX_STARTED pid='+str(p.pid),flush=True)
stop=threading.Event();samples=[]
def monitor():
    while not stop.wait(1):
        try:
            d={}
            for line in Path('/proc/'+str(p.pid)+'/status').read_text().splitlines():
                k,_,v=line.partition(':')
                if k in ['VmRSS','VmHWM','VmSwap','RssAnon','RssFile']:d[k]=int(v.split()[0])
            d['at']=time.time();d['loadavg']=os.getloadavg()[0]
            if len(samples)%5==0:
                board=subprocess.run(['nvidia-smi','--query-gpu=clocks.sm,clocks.mem,pstate,power.draw,temperature.gpu,utilization.gpu','--format=csv,noheader,nounits'],capture_output=True,text=True,timeout=4)
                d['board_telemetry']=board.stdout.strip()
                q=subprocess.run(['nvidia-smi','--query-compute-apps=pid,used_gpu_memory','--format=csv,noheader,nounits'],capture_output=True,text=True,timeout=4)
                for line in q.stdout.splitlines():
                    a=line.split(',')
                    if len(a)==2 and a[0].strip()==str(p.pid):d['gpu_mib']=int(a[1].strip())
            samples.append(d)
        except (OSError,ValueError,subprocess.TimeoutExpired):pass
thread=threading.Thread(target=monitor,daemon=True);thread.start()
def savephase(ph):
    receipt['phase']=ph;receipt['updated']=time.time()
    (B/'process.json').write_text(json.dumps(receipt,indent=2)+'\n');print('APEX_PHASE '+ph,flush=True)
try:
    deadline=time.monotonic()+1200
    while True:
        if p.poll() is not None:raise RuntimeError('Server exited during startup,code='+str(p.returncode))
        try:
            with urllib.request.urlopen('http://127.0.0.1:8081/health',timeout=2) as r:
                if json.load(r).get('status')=='ok':break
        except (urllib.error.URLError,TimeoutError,json.JSONDecodeError):pass
        if time.monotonic()>deadline:raise RuntimeError('Startup exceeded 20min')
        time.sleep(1)
    savephase('READY')
    maps=Path('/proc/'+str(p.pid)+'/maps').read_text()
    (B/'loaded-libraries.txt').write_text('\n'.join(sorted(set(l.split()[-1] for l in maps.splitlines()
        if 'libggml' in l or 'libllama' in l)))+'\n')
    for n,item in libs.items():
        if item['path'] not in maps:raise RuntimeError('Expected library not mapped: '+n)
    savephase('PREFILL_DECODE');start=time.monotonic()
    request=urllib.request.Request('http://127.0.0.1:8081/completion',data=req,
                                   headers={'Content-Type':'application/json'})
    with urllib.request.urlopen(request,timeout=1800) as r:response=json.load(r)
    elapsed=time.monotonic()-start
    (B/'response.json').write_text(json.dumps(response,ensure_ascii=False)+'\n')
    baseline=json.loads((L/'plan-apex-native-32k128-response.json').read_text())
    tok=response.get('tokens',[]);ref=baseline.get('tokens',[])
    first=next((i for i,(a,b) in enumerate(zip(tok,ref)) if a!=b),None)
    summary=dict(completion_probabilities_equal=response.get('completion_probabilities')==baseline.get('completion_probabilities'),
        status='RESPONSE_RECEIVED',wall_seconds=elapsed,timings=response.get('timings'),
        tokens_equal=tok==ref,token_count=len(tok),first_different_token_index=first,
        content_equal=response.get('content')==baseline.get('content'),
        content_sha256=hashlib.sha256(response.get('content','').encode()).hexdigest(),
        reference='Saved wholly-C++ correctness oracle;performance paired only for CONCAT in same DSO',
        preliminary=True,source_closure_complete=False,
        concurrent_load='No build or helper task started during measurement;other host activity uncontrolled',
        production_changed=False)
    (B/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('APEX_RESPONSE '+json.dumps(summary),flush=True);savephase('COMPLETE_DIAGNOSTIC')
except Exception as e:
    receipt['error']=str(e);(B/'failure.txt').write_text(traceback.format_exc())
    savephase('FAILED');print('APEX_FAILED '+str(e),flush=True)
finally:
    stop.set();thread.join(timeout=6)
    (B/'memory.json').write_text(json.dumps(samples,indent=2)+'\n')
    if p.poll() is None:
        os.killpg(p.pid,signal.SIGTERM)
        try:p.wait(timeout=15)
        except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait()
    log.close();receipt['exit_code']=p.returncode;receipt['finished']=time.time()
    (B/'process.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('APEX_FINISHED '+str(p.returncode),flush=True)
