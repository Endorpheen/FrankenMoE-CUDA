from pathlib import Path
import os,json,time,subprocess,urllib.request,urllib.error,threading,hashlib,signal,traceback
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');L=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107/results');R=A/('runtime/'+os.environ.get('FG_STAGE','exp174')+'/run');mode=os.environ.get('FG174_MEASURE_MODE','goose');assert mode in ['goose','native-rollback'];label='apex-pinned'+os.environ.get('FG_SUFFIX','')+'-'+mode;B=A/'runtime/exp196/results'/label;T=A/'runtime/exp196/measured'/label/'run';B.mkdir(parents=True,exist_ok=True);T.mkdir(parents=True,exist_ok=True)
if (B/'process.json').exists():raise SystemExit('Existing diagnostic receipt;inspect before rerunning')
for f in R.iterdir():
 if f.is_file() or f.is_symlink():
  q=T/f.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise RuntimeError(str(q))
  q.symlink_to(f.resolve())
libs={n:{'path':str((T/n).resolve()),'sha256':hashlib.sha256((T/n).resolve().read_bytes()).hexdigest()} for n in ['libllama.so','libggml.so','libggml-base.so','libggml-cpu.so','libggml-cuda.so']};(B/'libraries.json').write_text(json.dumps(libs,indent=2)+'\n')
argv=json.loads((L/'plan-apex-native-argv.json').read_text());argv[0]=str(T/'llama-server');(B/'argv.json').write_text(json.dumps(argv,indent=2)+'\n');req=(L/'plan-apex-native-32k128-request.json').read_bytes();(B/'request.json').write_bytes(req)
for source in ['/proc/net/tcp','/proc/net/tcp6']:
 for line in Path(source).read_text().splitlines()[1:]:
  fields=line.split()
  if fields[1].endswith(':1F91') and fields[3]=='0A':raise SystemExit('Port8081 already occupied;no other process stopped')
env=dict(os.environ)
for k in list(env):
 if k.startswith('GGML_GOOSE_'):env.pop(k)
saved_env=json.loads(Path('/home/end0/AI/LlamaServer/franken-goose-exp150-expert-registry-20261107/results/model/audit-exp150-omp-apex-32k128-environment.json').read_text());env.update(saved_env)
removed=[p for p in env.get('LD_PRELOAD','').split() if Path(p).name in ['libfranken-goose-silu.so','libfranken-goose-scale.so','libfranken-goose-rms-block.so']]
assert len(removed)==3
preserved=[p for p in env['LD_PRELOAD'].split() if p not in removed];assert not preserved
env['LD_PRELOAD']=' '.join(preserved);(B/'preload-transition.json').write_text(json.dumps(dict(superseded=removed,preserved=preserved,reason='SiLU/scale inherited from EXP166;RMS inherited from EXP168 in the CUDA DSO;all superseded preloads removed only here'),indent=2)+'\n');env['GGML_EXPERT_HOTSTORE']='0';env['LD_LIBRARY_PATH']=str(T)
(B/'environment.json').write_text(json.dumps({k:v for k,v in env.items() if k.startswith(('GGML_GOOSE_','GGML_EXPERT_')) or k=='LD_LIBRARY_PATH'},indent=2)+'\n')
env.pop('GGML_GOOSE_CUDA_FA',None)
if mode=='native-rollback':env['GGML_GOOSE_CUDA_PINNED']='0'
env.pop('GGML_GOOSE_CUDA_SET_ROWS',None)
(B/'environment.json').write_text(json.dumps({k:v for k,v in env.items() if k.startswith(('GGML_GOOSE_','GGML_EXPERT_')) or k=='LD_LIBRARY_PATH'},indent=2)+'\n')
(B/'setrows-control-mode.json').write_text(json.dumps(dict(mode=mode,same_cuda_library_sha256=libs['libggml-cuda.so']['sha256'],pair='stage='+os.environ.get('FG_STAGE','exp176')+' same DSO switch: mul_mat_vec_q Q4_K only'),indent=2)+'\n')
log=(B/'server.log').open('w');p=subprocess.Popen(argv,env=env,cwd=T,stdout=log,stderr=subprocess.STDOUT,start_new_session=True);receipt=dict(pid=p.pid,started=time.time(),phase='LOADING',model='Apex-35B-V1.0-Q4_K_M',input_tokens=32768,output_tokens=128,preliminary=True,not_acceptance='EXP174 SET_ROWS F32/Q4_0 I32/I64 arithmetic and checked host planning in Goose;all other EXP173 components unchanged;actual SET_ROWS witness required;full migration incomplete',production_changed=False);(B/'process.json').write_text(json.dumps(receipt,indent=2)+'\n');print('APEX_DIAGNOSTIC_STARTED pid='+str(p.pid),flush=True)
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
    board=subprocess.run(['nvidia-smi','--query-gpu=clocks.sm,clocks.mem,pstate,power.draw,temperature.gpu,utilization.gpu','--format=csv,noheader,nounits'],capture_output=True,text=True,timeout=4);d['board_telemetry']=board.stdout.strip()
    q=subprocess.run(['nvidia-smi','--query-compute-apps=pid,used_gpu_memory','--format=csv,noheader,nounits'],capture_output=True,text=True,timeout=4)
    for line in q.stdout.splitlines():
     a=line.split(',')
     if len(a)==2 and a[0].strip()==str(p.pid):d['gpu_mib']=int(a[1].strip())
   samples.append(d)
  except (OSError,ValueError,subprocess.TimeoutExpired):pass
thread=threading.Thread(target=monitor,daemon=True);thread.start()
# CPU telemetry of the server process itself, sampled once per second while it
# runs. Thread CPU time is read from /proc/<pid>/task/<tid>/stat so contention on
# the expert threads is visible without touching the server.
cpu_samples=[]
def cpu_monitor():
 prev={}
 while not stop.wait(1):
  try:
   now={}
   for d in Path('/proc/'+str(p.pid)+'/task').iterdir():
    try:
     f=Path(str(d)+'/stat').read_text().rsplit(')',1)[1].split()
     utime,stime=int(f[11]),int(f[12])
     now[d.name]=utime+stime
    except (OSError,IndexError,ValueError): continue
   total=sum(now.values())
   tick=os.sysconf('SC_CLK_TCK')
   if prev:
    used=sum(max(0,now[k]-prev.get(k,0)) for k in now)/tick
    freqs={}
    for c in sorted(os.sched_getaffinity(p.pid)):
     try: freqs[str(c)]=int(Path(f'/sys/devices/system/cpu/cpu{c}/cpufreq/scaling_cur_freq').read_text())//1000
     except OSError: pass
    cpu_samples.append(dict(at=time.time(),cores_used=round(used,3),
      threads=len(now),loadavg=os.getloadavg()[0],
      mhz_min=min(freqs.values()) if freqs else None,
      mhz_max=max(freqs.values()) if freqs else None,
      mhz={k:v for k,v in freqs.items()}))
   prev=now
  except (OSError,ValueError): pass
cpu_thread=threading.Thread(target=cpu_monitor,daemon=True);cpu_thread.start()
def savephase(phase):
 receipt['phase']=phase;receipt['updated']=time.time();(B/'process.json').write_text(json.dumps(receipt,indent=2)+'\n');print('APEX_PHASE '+phase,flush=True)
try:
 deadline=time.monotonic()+1200
 while True:
  if p.poll() is not None:raise RuntimeError('Server exited during startup,code='+str(p.returncode))
  try:
   with urllib.request.urlopen('http://127.0.0.1:8081/health',timeout=2) as r:
    health=json.load(r)
   if health.get('status')=='ok':break
  except (urllib.error.URLError,TimeoutError,json.JSONDecodeError):pass
  if time.monotonic()>deadline:raise RuntimeError('Startup exceeded20min;diagnostic stopped,inspect live logs')
  time.sleep(1)
 savephase('READY')
 maps=Path('/proc/'+str(p.pid)+'/maps').read_text();(B/'loaded-libraries.txt').write_text('\n'.join(sorted(set(line.split()[-1] for line in maps.splitlines() if 'libggml' in line or 'libllama' in line)))+'\n')
 # Owner-requested evidence: exact argv, full environment, resolved library
 # hashes (not just symlink paths) and the thread affinity actually applied.
 (B/'argv-actual.json').write_text(json.dumps(list(map(str,argv)),indent=2)+'\n')
 (B/'env-actual.json').write_text(json.dumps(dict(env),indent=2,sort_keys=True)+'\n')
 opened={}
 for line in maps.splitlines():
  parts=line.split()
  if len(parts)>=6 and ('libggml' in parts[-1] or 'libllama' in parts[-1]):
   opened[parts[-1]]=hashlib.sha256(Path(parts[-1]).read_bytes()).hexdigest()
 (B/'loaded-library-hashes.json').write_text(json.dumps(opened,indent=2,sort_keys=True)+'\n')
 tasks={}
 for d in Path('/proc/'+str(p.pid)+'/task').iterdir():
  try:
   aff=Path(str(d)+'/status').read_text()
   for ln in aff.splitlines():
    if ln.startswith('Cpus_allowed_list:'):tasks[d.name]=ln.split(':',1)[1].strip()
  except OSError: pass
 (B/'thread-affinity.json').write_text(json.dumps(tasks,indent=2,sort_keys=True)+'\n')
 (B/'cpu-topology.json').write_text(json.dumps(dict(cpus=len(os.sched_getaffinity(0)),
   physical_cores=subprocess.run(['bash','-lc','lscpu -p=core,socket | grep -v ^# | sort -u | wc -l'],capture_output=True,text=True).stdout.strip()),indent=2)+'\n')
 for n,item in libs.items():
  if item['path'] not in maps:raise RuntimeError('Expected current library not mapped: '+n)
 savephase('PREFILL_DECODE');start=time.monotonic();request=urllib.request.Request('http://127.0.0.1:8081/completion',data=req,headers={'Content-Type':'application/json'})
 with urllib.request.urlopen(request,timeout=1800) as r:response=json.load(r)
 elapsed=time.monotonic()-start;(B/'response.json').write_text(json.dumps(response,ensure_ascii=False)+'\n');baseline=json.loads((L/'plan-apex-native-32k128-response.json').read_text());tok=response.get('tokens',[]);ref=baseline.get('tokens',[]);first=next((i for i,(a,b) in enumerate(zip(tok,ref)) if a!=b),None)
 summary=dict(completion_probabilities_equal=response.get('completion_probabilities')==baseline.get('completion_probabilities'),status='RESPONSE_RECEIVED',wall_seconds=elapsed,timings=response.get('timings'),tokens_equal=tok==ref,token_count=len(tok),first_different_token_index=first,content_equal=response.get('content')==baseline.get('content'),content_sha256=hashlib.sha256(response.get('content','').encode()).hexdigest(),reference='Saved wholly-C++ correctness oracle;performance paired only for SET_ROWS in same DSO',preliminary=True,source_closure_complete=False,concurrent_load='No build or helper task started during measurement;other host activity uncontrolled;telemetry saved',production_changed=False)
 (B/'summary.json').write_text(json.dumps(summary,indent=2)+'\n');print('APEX_DIAGNOSTIC_RESPONSE '+json.dumps(summary),flush=True);savephase('COMPLETE_DIAGNOSTIC')
except Exception as e:
 receipt['error']=str(e);(B/'failure.txt').write_text(traceback.format_exc());savephase('FAILED');print('APEX_DIAGNOSTIC_FAILED '+str(e),flush=True)
finally:
 stop.set();thread.join(timeout=6);cpu_thread.join(timeout=3);(B/'cpu-telemetry.json').write_text(json.dumps(cpu_samples,indent=2)+'\n');(B/'memory.json').write_text(json.dumps(samples,indent=2)+'\n')
 if p.poll() is None:
  os.killpg(p.pid,signal.SIGTERM)
  try:p.wait(timeout=15)
  except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait()
 log.close();receipt['exit_code']=p.returncode;receipt['finished']=time.time();(B/'process.json').write_text(json.dumps(receipt,indent=2)+'\n')
 print('APEX_PROCESS_FINISHED '+str(p.returncode),flush=True)
