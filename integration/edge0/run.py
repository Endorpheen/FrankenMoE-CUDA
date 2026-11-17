#!/usr/bin/env python3
"""One owned diagnostic CLI, bounded resources, complete logs and no server stop."""
from pathlib import Path
import argparse,subprocess,os,time,json,shlex,selectors,signal,fcntl,atexit
ap=argparse.ArgumentParser();ap.add_argument('--candidate',type=Path,required=True);ap.add_argument('--tag',required=True);ap.add_argument('--seconds',type=int,default=900);ap.add_argument('--ngl',type=int,default=48);ap.add_argument('--prompt',default='Ответь кратко по-русски: как называется столица Франции?');ap.add_argument('--tokens',type=int,default=64);ap.add_argument('--profile',action='store_true');a=ap.parse_args()
p=a.candidate.resolve();o=p/a.tag;o.mkdir(exist_ok=False)
lock=open('/tmp/apex-test.lock','a');fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
ports=subprocess.check_output(['ss','-ltnp','( sport = :9101 or sport = :8950 )'],text=True);(o/'ports-before.txt').write_text(ports)
assert len(ports.strip().splitlines())==1,'runtime port occupied; not touching its owner'
proc=subprocess.check_output(['ps','-C','llama-server','-o','pid=,comm='],text=True) if subprocess.run(['pgrep','-x','llama-server'],stdout=subprocess.DEVNULL).returncode==0 else ''
assert not proc.strip(),'another llama-server is running'
from jinja2 import Environment
pack=Path('/home/end0/AI/LocalAI/Models/Edge0-120B-q3k')
env=Environment();env.globals['raise_exception']=lambda s:(_ for _ in ()).throw(ValueError(s))
prompt=env.from_string((pack/'tokenizer/chat_template.jinja').read_text()).render(messages=[{'role':'user','content':a.prompt}],add_generation_prompt=True,enable_thinking=False)
(o/'prompt.txt').write_text(prompt)
command=[str(p/'build/edge0-cli'),str(pack),str(o/'prompt.txt'),str(a.tokens)]
runtime_env={**os.environ,'FRANKEN_EDGE0_NGL':str(a.ngl),'LD_LIBRARY_PATH':str(p/'build/llama')+':/home/end0/AI/LlamaServer/franken-recovery-20261116/support/bin'}
if a.profile:
 runtime_env['FRANKEN_EDGE0_PROFILE']='1'
 command=['nsys','profile','--capture-range=cudaProfilerApi','--capture-range-end=stop','--trace=cuda,nvtx,osrt','--sample=none','--cpuctxsw=none','--force-overwrite=false','--output='+str(o/'decode'),*command]
def descendants(pid):
 found=[];todo=[pid]
 while todo:
  n=todo.pop();found.append(n)
  try:todo.extend(map(int,Path(f'/proc/{n}/task/{n}/children').read_text().split()))
  except (FileNotFoundError,PermissionError,ProcessLookupError):pass
 return found
def target_pid(root):
 for pid in descendants(root):
  try:
   if Path(f'/proc/{pid}/comm').read_text().strip()=='edge0-cli':return pid
  except (FileNotFoundError,PermissionError,ProcessLookupError):pass
 for d in Path('/proc').iterdir():
  if not d.name.isdigit():continue
  try:
   if (d/'exe').resolve()==p/'build/edge0-cli' and str(o/'prompt.txt').encode() in (d/'cmdline').read_bytes().split(b'\0'):return int(d.name)
  except (FileNotFoundError,PermissionError):pass
 return root
def swapout():
 for s in Path('/proc/vmstat').read_text().splitlines():
  if s.startswith('pswpout '):return int(s.split()[1])
start_swap=swapout();start=time.monotonic();samples=[];reason=None
with (o/'log.txt').open('x') as log,(o/'answer.bin').open('xb') as answer:
 header='$ env FRANKEN_EDGE0_NGL='+str(a.ngl)+' LD_LIBRARY_PATH='+runtime_env['LD_LIBRARY_PATH']+' '+shlex.join(command)
 print(header,flush=True);log.write(header+'\n');log.flush()
 child=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,start_new_session=True,env=runtime_env)
 def cleanup_owned():
  if child.poll() is not None:return
  try:os.killpg(child.pid,signal.SIGTERM)
  except ProcessLookupError:return
  try:child.wait(timeout=5)
  except subprocess.TimeoutExpired:
   try:os.killpg(child.pid,signal.SIGKILL)
   except ProcessLookupError:pass
   child.wait()
 atexit.register(cleanup_owned)
 (o/'pid').write_text(str(child.pid)+'\n');sel=selectors.DefaultSelector();sel.register(child.stdout,selectors.EVENT_READ,'out');sel.register(child.stderr,selectors.EVENT_READ,'err');last=0
 while sel.get_map() or child.poll() is None:
  elapsed=time.monotonic()-start
  if elapsed-last>=1 and child.poll() is None:
   last=elapsed;target=target_pid(child.pid);sample={'s':elapsed,'pid':target,'rss_KiB':0,'swap_KiB':0,'gpu_MiB':None}
   try:
    status=Path(f'/proc/{target}/status').read_text()
    for row in status.splitlines():
     if row.startswith('VmRSS:'):sample['rss_KiB']=int(row.split()[1])
     if row.startswith('VmSwap:'):sample['swap_KiB']=int(row.split()[1])
    sample['io']={k:int(v) for k,v in (x.split(':',1) for x in Path(f'/proc/{target}/io').read_text().splitlines())}
   except (FileNotFoundError,PermissionError,ProcessLookupError):sample['proc_sample_incomplete']=True
   q=subprocess.run(['nvidia-smi','--query-compute-apps=pid,used_memory','--format=csv,noheader,nounits'],capture_output=True,text=True,timeout=5)
   for row in q.stdout.splitlines():
    fields=row.split(',')
    if fields[0].strip()==str(target):sample['gpu_MiB']=int(fields[1].strip())
   samples.append(sample)
   with (o/'samples.jsonl').open('a') as saved:saved.write(json.dumps(sample)+'\n')
   if sample['rss_KiB']>12*1024*1024:reason='RSS exceeded 12 GiB'
   if sample['swap_KiB']>0:reason='owned process swapped'
   if elapsed>a.seconds:reason='wall limit'
   if reason:
    os.killpg(child.pid,signal.SIGTERM)
  for key,_ in sel.select(timeout=.25):
   data=os.read(key.fileobj.fileno(),65536)
   if not data:sel.unregister(key.fileobj);continue
   if key.data=='out':answer.write(data);answer.flush()
   txt=data.decode('utf8','replace');log.write(txt);log.flush();print(txt,end='',flush=True) if key.data=='out' or any(mark in txt for mark in ['EDGE0_','Edge0 FFN','Edge0 pack','failed','error','ASSERT']) else None
 rc=child.wait();log.write(f'\nexit_code={rc}\nstop_reason={reason}\n');log.flush()
result={'command':command,'ngl':a.ngl,'seconds':time.monotonic()-start,'exit_code':rc,'stop_reason':reason,'samples':samples,'pswpout_start':start_swap,'pswpout_end':swapout(),'maxrss_KiB':max([x['rss_KiB'] for x in samples],default=0),'max_gpu_MiB':max([x['gpu_MiB'] or 0 for x in samples],default=0),'max_swap_KiB':max([x['swap_KiB'] for x in samples],default=0)}
(o/'resources.json').write_text(json.dumps(result,indent=2)+'\n');print('DIAGNOSTIC_RESULT',json.dumps({k:v for k,v in result.items() if k!='samples'}),flush=True)
raise SystemExit(rc if rc>=0 else 128-rc)
