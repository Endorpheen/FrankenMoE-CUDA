#!/usr/bin/env python3
"""Host-owned, fail-closed candidate controller. Never promotes or pushes code."""
import argparse, contextlib, fcntl, hashlib, importlib.util, json, os, re, resource, shutil, stat, subprocess, sys, tarfile, tempfile, time, uuid
from pathlib import Path
sys.dont_write_bytecode=True

DEFAULT_ROOT=Path('/home/end0/AI/LlamaServer/franken-goose-deepseek-autonomy-20261108')
REPO=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
UPSTREAM=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
EXP='experiments/EXP-2026-11-08-174-goose-cuda-set-rows'
FACTORY=Path('/home/end0/.codex/skills/commandcode-subagents/scripts/factory.py')
def H(p):
 h=hashlib.sha256()
 with Path(p).open('rb')as f:
  while True:
   chunk=f.read(8*1024**2)
   if not chunk:break
   h.update(chunk)
 return h.hexdigest()

def save(p,d):
 p=Path(p);p.parent.mkdir(parents=True,exist_ok=True);tmp=p.with_name(p.name+'.tmp-'+uuid.uuid4().hex);tmp.write_text(json.dumps(d,indent=2)+'\n');os.replace(tmp,p)
def load(p):return json.loads(Path(p).read_text())
def env():return {'PATH':'/usr/bin:/bin','HOME':'/tmp','LANG':'C.UTF-8','GIT_CONFIG_GLOBAL':'/dev/null','GIT_CONFIG_NOSYSTEM':'1','GIT_TERMINAL_PROMPT':'0'}
def git(root,*args):
 return subprocess.check_output(['git','-c','core.hooksPath=/dev/null','-c','core.fsmonitor=false',*args],cwd=root,env=env(),text=True).strip()
def copy(src,dst):
 src=Path(src);dst=Path(dst);dst.parent.mkdir(parents=True,exist_ok=True)
 if dst.exists():
  if H(src)!=H(dst):raise RuntimeError('Conflicting frozen dependency '+str(dst))
  return
 subprocess.run(['cp','--reflink=auto','--',str(src.resolve()),str(dst)],check=True)
def safe_rel(s):
 p=Path(s)
 if not s or p.is_absolute() or '..' in p.parts or any(x.startswith('.') for x in p.parts):raise ValueError('Unsafe candidate path')
 if len(s)>512:raise ValueError('Path too long')
 return p
def owned(s):
 p=safe_rel(s)
 return bool(re.fullmatch(r'experiments/(?:EXP-[A-Za-z0-9-]+|autonomous-[A-Za-z0-9-]+)/source/[A-Za-z0-9_-]+\.goose',str(p)))
def read_regular(p,limit):
 fd=os.open(p,os.O_RDONLY|os.O_NOFOLLOW)
 try:
  st=os.fstat(fd)
  if not stat.S_ISREG(st.st_mode) or st.st_nlink!=1 or st.st_size>limit:raise ValueError('Not an independent bounded regular file')
  out=b''
  while len(out)<=limit:
   x=os.read(fd,min(65536,limit+1-len(out)))
   if not x:break
   out+=x
  if len(out)>limit:raise ValueError('Input too large')
  return out
 finally:os.close(fd)
def event(root,kind,**data):
 p=root/'audit.jsonl';prev='0'*64
 if p.exists():
  with p.open('rb') as f:
   for line in f:
    old=json.loads(line);sig=old.pop('hash');assert old['previous']==prev and hashlib.sha256(json.dumps(old,sort_keys=True).encode()).hexdigest()==sig,'Audit integrity failure';prev=sig
 d=dict(time=time.time(),kind=kind,previous=prev,**data);d['hash']=hashlib.sha256(json.dumps(d,sort_keys=True).encode()).hexdigest()
 with p.open('a')as f:f.write(json.dumps(d,sort_keys=True)+'\n');f.flush();os.fsync(f.fileno())
def manifest(root):
 out={}
 for p in (root/'trusted').rglob('*'):
  if '__pycache__' in p.parts or p.suffix=='.pyc':continue
  if p.is_symlink():raise RuntimeError('Trusted symlink forbidden '+str(p))
  if p.is_file():out[str(p.relative_to(root))]=H(p)
 save(root/'sealed.json',out)
def verify(root):
 for rel,sha in load(root/'sealed.json').items():
  p=root/rel
  if p.is_symlink() or not p.is_file() or H(p)!=sha:raise RuntimeError('Sealed input changed: '+rel)
 event(root,'INTEGRITY_PASS',manifest_sha256=H(root/'sealed.json'))
def limits():
 resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_FSIZE,(1024**3,1024**3));resource.setrlimit(resource.RLIMIT_NOFILE,(512,512))
def bwrap(root,work,*,build=False,gpu=False,network=False):
 # Private mount/PID namespaces prevent writes/signals to the host even for shell tools.
 cmd=['bwrap','--die-with-parent','--new-session','--unshare-all']
 if network:cmd+=['--share-net']
 cmd+=['--ro-bind','/usr','/usr','--ro-bind','/lib','/lib','--ro-bind','/lib64','/lib64','--symlink','usr/bin','/bin','--symlink','usr/sbin','/sbin','--proc','/proc','--dev','/dev','--tmpfs','/tmp','--tmpfs','/session','--dir','/session/.commandcode','--clearenv','--setenv','PATH','/usr/bin:/bin','--setenv','HOME','/session','--setenv','LC_ALL','C.UTF-8','--setenv','ASAN_OPTIONS','detect_leaks=1:halt_on_error=1','--setenv','LSAN_OPTIONS','exitcode=87','--setenv','UBSAN_OPTIONS','halt_on_error=1']
 for x in ['/etc/ld.so.cache','/etc/resolv.conf','/etc/hosts','/etc/nsswitch.conf','/etc/ssl','/opt/cuda']:
  if Path(x).exists():cmd+=['--ro-bind',x,x]
 cmd+=['--ro-bind',str(root/'trusted'),'/guard','--ro-bind',str(root/'results'),'/results','--bind',str(work),'/work','--ro-bind',str(root/'trusted/empty'),'/work/.git','--chdir','/work']
 if build:
  # Trusted dependencies are copied,not shared writable hardlinks to the working project.
  cmd+=['--ro-bind',str(root/'trusted/upstream'),str(UPSTREAM),'--bind',str(work),str(REPO)]
  for x in (root/'trusted/mounts').iterdir() if (root/'trusted/mounts').exists() else []:
   meta=load(x/'mount.json');cmd+=['--ro-bind',str(x/'data'),meta['target']]
 if gpu:
  model=load(root/'trusted/model.json');cmd+=['--ro-bind',model['path'],model['path']]
  for p in Path('/dev').glob('nvidia*'):
   if p.is_char_device():cmd+=['--dev-bind',str(p),str(p)]
 return cmd

def run_gate(root,stage,label,argv,*,gpu=False,timeout=300,extra_env=None):
 log=root/'results'/stage.name;log.mkdir(parents=True,exist_ok=True)
 cmd=bwrap(root,stage,build=True,gpu=gpu)
 for k,v in (extra_env or {}).items():cmd+=['--setenv',k,str(v)]
 cmd+=['--',*map(str,argv)];out=log/(label+'.stdout');err=log/(label+'.stderr')
 with out.open('wb')as so,err.open('wb')as se:
  try:q=subprocess.run(cmd,stdout=so,stderr=se,timeout=timeout,env=env(),preexec_fn=limits);rc=q.returncode
  except subprocess.TimeoutExpired:rc=124
 row=dict(label=label,argv=list(map(str,argv)),exit_code=rc,stdout_sha256=H(out),stderr_sha256=H(err));event(root,'GATE',stage=stage.name,**row)
 if rc:raise RuntimeError('Gate failed: '+label+'; exit='+str(rc))
 return row

def prepare(root,resume=False):
 if root.exists() and (not resume or (root/'sealed.json').exists()or(root/'state.json').exists()):raise RuntimeError('Refuse to overwrite an existing safety bundle')
 root.mkdir(parents=True,mode=0o700,exist_ok=resume)
 for x in ['trusted/reference','trusted/upstream','trusted/mounts','trusted/empty','workspace','results','stages','checkpoints','restorations']:(root/x).mkdir(parents=True,exist_ok=resume)
 commit=git(REPO,'rev-parse','HEAD');branch=git(REPO,'branch','--show-current');ref=root/'trusted/reference'
 names=git(REPO,'ls-files','-z').split('\0')
 excluded=[]
 for rel in names:
  if not rel:continue
  p=Path(rel)
  if p.name in ['AGENTS.md','CLAUDE.md','COMPILER_FEEDBACK.md'] or any(x.startswith('.codex') or x in ['.agents','.git'] for x in p.parts):excluded.append(rel);continue
  src=REPO/p
  if src.is_symlink():raise RuntimeError('Review tracked symlink before handoff '+rel)
  copy(src,ref/p)
 # Freeze the exact build dependencies referenced by the qualified linker and compiler.
 link=load(REPO/'experiments/EXP-2026-11-08-173-goose-cuda-flash-attention/private-abi-link-fix.json');cwd=Path(link['link_cwd']);deps=[]
 for i,a in enumerate(link['link_argv']):
  if i and link['link_argv'][i-1]=='-o':continue
  if a.startswith('-'):continue
  p=Path(a) if Path(a).is_absolute() else cwd/a
  if p.is_file() and (p.suffix in ['.o','.a']or '.so'in p.name):deps.append(p)
 for p in deps:
  if p.is_relative_to(REPO):copy(p,ref/p.relative_to(REPO))
  elif p.is_relative_to(UPSTREAM):copy(p,root/'trusted/upstream'/p.relative_to(UPSTREAM))
  elif not str(p).startswith(('/usr/','/opt/')):raise RuntimeError('Unreviewed external dependency '+str(p))
 # Curated upstream source snapshot;never copy credentials,Git metadata or all build outputs.
 for base in ['ggml','include','common','src']:
  for p in (UPSTREAM/base).rglob('*'):
   if p.is_file() and not p.is_symlink() and '.git'not in p.parts:copy(p,root/'trusted/upstream'/p.relative_to(UPSTREAM))
 copy(UPSTREAM/'build-cuda/compile_commands.json',root/'trusted/upstream/build-cuda/compile_commands.json')
 for p in (REPO/'runtime/exp173/run').iterdir():
  if p.is_file():copy(p,ref/'runtime/exp173/run'/p.name)
 compiler=load(REPO/'runtime/compiler/current.json');copy(compiler['compiler'],ref/Path(compiler['compiler']).relative_to(REPO));copy(REPO/'runtime/compiler/current.json',ref/'runtime/compiler/current.json')
 # Runtime runner dependencies contain only approved profile/oracle files,not whole log directories.
 mounts={
 '/home/end0/AI/LlamaServer/franken-goose-live-20261107/results':['plan-apex-native-argv.json','plan-apex-native-32k128-request.json','plan-apex-native-32k128-response.json'],
 '/home/end0/AI/LlamaServer/franken-goose-exp150-expert-registry-20261107/results/model':['audit-exp150-omp-apex-32k128-environment.json']}
 for n,(target,files) in enumerate(mounts.items()):
  d=root/'trusted/mounts'/str(n);(d/'data').mkdir(parents=True,exist_ok=resume)
  save(d/'mount.json',{'target':target})
  for f in files:copy(Path(target)/f,d/'data'/f)
 # The model is an external readonly mount,never sent to an agent or copied into Git.
 model=Path('/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf')
 save(root/'trusted/model.json',{'path':str(model),'sha256':H(model),'external_readonly':True})
 for n in ['guard.py','selftest.py','SYSTEM_INSTRUCTIONS.md','START_HERE.md','request.py']:copy(Path(__file__).parent/n,root/'trusted'/n)
 # All editable work is an independent copy;there is no shared writable Git object database.
 shutil.copytree(ref,root/'workspace',dirs_exist_ok=True)
 history=root/'checkpoints/history.git';subprocess.run(['git','init','--bare','--initial-branch=codex/deepseek-autonomous-20261108',str(history)],stdout=subprocess.DEVNULL,check=True,env=env())
 save(root/'trusted/policy.json',{'max_failures':3,'performance_tolerance_percent':2,'max_source_bytes':2*1024**2,'max_patch_bytes':16*1024**2,'max_files':64,'production_promotion':False,'push':False,'automatic_trust':False,'recipe':'exp174','scope':[EXP+'/source/host.goose',EXP+'/source/device.goose'],'unsupported_scope':'DRAFT_ONLY_MISSING_TRUSTED_RECIPE','same_dso_pair_required':True})
 save(root/'trusted/baseline.json',{'repo':str(REPO),'commit':commit,'branch':branch,'compiler':compiler,'active_runtime':load(REPO/'runtime/active.json'),'exp174':load(REPO/EXP/'apex-summary.json'),'production_commit':git(Path('/home/end0/AI/FrankenMoE-CUDA'),'rev-parse','HEAD'),'excluded_private_instruction_paths':excluded,'source_snapshot':'working tracked files at pinned commit;hashes sealed separately','model_path':str(model)})
 # Repository state is held exclusively by the host controller,not the agent.
 save(root/'state.json',{'last_candidate':None,'last_local_pass':None,'last_integrated_candidate':None,'failures':0,'halted':False,'production_changed':False})
 checkpoint(root,root/'workspace','BASELINE','Frozen source/binary/compiler/oracle checkpoint',initial=True)
 manifest(root);event(root,'PREPARED',commit=commit,branch=branch)
 print('PREPARED '+str(root))

def checkpoint(root,stage,status,message,initial=False):
 history=root/'checkpoints/history.git';index=root/'checkpoints/index'
 e=env();e.update(GIT_INDEX_FILE=str(index),GIT_AUTHOR_NAME='DeepSeek candidate controller',GIT_AUTHOR_EMAIL='candidate@localhost',GIT_COMMITTER_NAME='DeepSeek candidate controller',GIT_COMMITTER_EMAIL='candidate@localhost')
 base=['git','-c','core.hooksPath=/dev/null','--git-dir='+str(history),'--work-tree='+str(stage)]
 files=[]
 for p in stage.rglob('*.goose'):
  rel=str(p.relative_to(stage))
  if owned(rel) and p.is_file() and not p.is_symlink():files.append(rel)
 if not files:raise RuntimeError('No checkpointable Goose source')
 subprocess.run(base+['read-tree','--empty'],env=e,check=True)
 subprocess.run(base+['add','--',*files],env=e,check=True,stdout=subprocess.DEVNULL)
 tree=subprocess.check_output(base+['write-tree'],env=e,text=True).strip();state=load(root/'state.json');parent=state.get('last_candidate');args=base+['commit-tree',tree,'-m',status+': '+message[:500]]
 if parent:args+=['-p',parent]
 oid=subprocess.check_output(args,env=e,text=True).strip();subprocess.run(base+['update-ref','refs/heads/codex/deepseek-autonomous-20261108',oid],env=e,check=True)
 state['last_candidate']=oid
 if status=='LOCAL_PASS':state['last_local_pass']=oid
 if status=='INTEGRATION_PASS_CANDIDATE':state['last_integrated_candidate']=oid
 save(root/'state.json',state);event(root,'CHECKPOINT',commit=oid,status=status,message=message[:500]);return oid

def snapshot(root,req):
 if shutil.disk_usage(root).free<8*1024**3:raise RuntimeError('Disk pressure:less than8GiB free;do not admit another candidate')
 # A request supplies intent only:never compiler flags,programs,logs or claimed measurements.
 if set(req)-{'id','message','files','integration'}:raise ValueError('Unknown request fields;candidate cannot supply commands or test outcomes')
 files=req.get('files');policy=load(root/'trusted/policy.json')
 if not isinstance(files,list) or not 1<=len(files)<=policy['max_files'] or len(set(files))!=len(files):raise ValueError('Invalid file list')
 stage=root/'stages'/('candidate-'+uuid.uuid4().hex);shutil.copytree(root/'trusted/reference',stage)
 previous=load(root/'state.json').get('last_local_pass')
 if previous:
  import io
  blob=subprocess.check_output(['git','--git-dir='+str(root/'checkpoints/history.git'),'archive',previous],env=env())
  with tarfile.open(fileobj=io.BytesIO(blob))as tf:tf.extractall(stage,filter='data')
 total=0
 for rel in files:
  if not isinstance(rel,str) or not owned(rel):raise ValueError('Only scoped Goose source can enter a checkpoint')
  p=root/'workspace'/safe_rel(rel)
  for parent in p.parents:
   if parent==root/'workspace':break
   if parent.is_symlink():raise ValueError('Symlink ancestor')
  data=read_regular(p,policy['max_source_bytes']);total+=len(data)
  if total>policy['max_patch_bytes']:raise ValueError('Patch too large')
  dest=stage/rel;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
 return stage,files

def classify_pair(ab,ba,tolerance):
 # Opposite orders must be present,correct,and from exactly the same CUDA DSO.
 allrows=list(ab.values())+list(ba.values())
 if any(not r.get('correct')for r in allrows):return 'REJECT_CORRECTNESS'
 if len({r.get('library_sha256')for r in allrows})!=1 or not all(r.get('library_sha256')for r in allrows):return 'INCONCLUSIVE_BUILD_MISMATCH'
 deltas={m:[100*(p['goose'][m]/p['native'][m]-1)for p in [ab,ba]]for m in ['prefill','decode']}
 if any(all(x < -tolerance for x in xs)for xs in deltas.values()):return 'REJECT_CONFIRMED_REGRESSION'
 if any(min(xs)<-tolerance for xs in deltas.values()):return 'INCONCLUSIVE_ORDER_OR_LOAD'
 return 'PAIR_WITHIN_TOLERANCE_CANDIDATE'


def verify_response(response,reference):
 return (len(response.get('tokens',[]))==128 and response.get('tokens')==reference.get('tokens') and response.get('content')==reference.get('content') and response.get('completion_probabilities')==reference.get('completion_probabilities') and bool(response.get('completion_probabilities')))

def recipe_exp174(root,stage,integration):
 S=stage/EXP/'source';frozen=root/'trusted/reference'/EXP/'source'
 # Fixed FFI/ISA contracts:expanding authority or calling other native algorithms needs Codex review.
 for name in ['host.goose','device.goose']:
  declared=lambda p:re.findall(r'extern\s+"[^"]+"\s+fn\s+[^;]+;',p.read_text())
  if declared(S/name)!=declared(frozen/name):raise RuntimeError('FFI authority/ABI changed;requires Codex review')
 dest=stage/'runtime/exp174';dest.mkdir(parents=True,exist_ok=True)
 (dest/'build').mkdir(exist_ok=True);(dest/'run').mkdir(exist_ok=True)
 for script in ['emit_sources.py','emit_bridge.py','verify_generated.py','build_runtime.py','test_local.py']:
  run_gate(root,stage,script,['python3',str(REPO/EXP/script)],gpu=script=='test_local.py',timeout=900)
 # Existing host guards plus actual sanitizer instrumentation for changed generated host C.
 run_gate(root,stage,'host-asan-build',['gcc-13','-std=gnu11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-c',str(REPO/EXP/'source/host-unit.c'),'-o','/work/runtime/exp174/build/host-asan.o'])
 run_gate(root,stage,'host-asan-link',['g++-13','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(REPO/EXP/'tests/host-fixture.cpp'),'/work/runtime/exp174/build/host-asan.o','-o','/work/runtime/exp174/build/host-asan'])
 run_gate(root,stage,'host-asan',['/work/runtime/exp174/build/host-asan'])
 cs='/usr/bin/compute-sanitizer'
 for tool in ['memcheck','initcheck']:
  run_gate(root,stage,'cuda-'+tool,[cs,'--tool',tool,'--error-exitcode','89','/work/runtime/exp174/build/gpu-fixture'],gpu=True,timeout=300,extra_env={'LD_LIBRARY_PATH':str(REPO/'runtime/exp174/run')})
 if not integration:return 'LOCAL_PASS'
 # Full inference is reserved for explicit significant-integration requests.
 model=load(root/'trusted/model.json');assert H(model['path'])==model['sha256'],'Model changed'
 script=stage/EXP/'run_apex_pair.py';text=script.read_text().replace("label='apex-32k128-setrows-pair-'+mode;", "label='apex-32k128-setrows-pair-'+mode+os.environ.get('FG174_PAIR_SUFFIX','');")
 script.write_text(text)
 pairs=[]
 for order,suffix in [(['native-rollback','goose'],''),(['goose','native-rollback'],'-reverse')]:
  pair={}
  for mode in order:
   run_gate(root,stage,'apex'+suffix+'-'+mode,['python3',str(REPO/EXP/'run_apex_pair.py')],gpu=True,timeout=1800,extra_env={'FG174_MEASURE_MODE':mode,'FG174_PAIR_SUFFIX':suffix,'FG_GUARD_MODEL':model['path']})
   b=stage/'runtime/exp174/results'/('apex-32k128-setrows-pair-'+mode+suffix);s=load(b/'summary.json');p=load(b/'process.json');libs=load(b/'libraries.json');log=(b/'server.log').read_text();mem=load(b/'memory.json')
   response=load(b/'response.json');reference=load(root/'trusted/mounts/0/data/plan-apex-native-32k128-response.json')
   correct=verify_response(response,reference) and p['exit_code']==0 and s['tokens_equal'] and s['token_count']==128 and s['completion_probabilities_equal'] and s['content_equal'] and 'graphs reused =        127'in log
   if mode=='goose':correct=correct and 'FG176R_CUDA_SET_ROWS host=0x8,cuda_status=0,paths=0x8,invalid_index_threads=0'in log
   if not correct:raise RuntimeError('Real Apex correctness/liveness/graph gate failed')
   heaps=[l for l in log.splitlines()if l.startswith('FG')and'live_'in l]
   if len(heaps)!=7 or any(not re.search(r'live_\w+=0(?:,|$)',l)for l in heaps):raise RuntimeError('Missing/nonzero live allocation witnesses')
   if not mem or max(x.get('gpu_mib',0)for x in mem)<=0 or max(x.get('VmHWM',0)for x in mem)<=0 or max(x.get('VmSwap',0)for x in mem)>0:raise RuntimeError('Missing RAM telemetry or swap in use')
   pair['native'if mode=='native-rollback'else'goose']={'correct':correct,'library_sha256':libs['libggml-cuda.so']['sha256'],'prefill':s['timings']['prompt_per_second'],'decode':s['timings']['predicted_per_second'],'ram_mib':max(x.get('VmHWM',0)for x in mem)/1024,'vram_mib':max(x.get('gpu_mib',0)for x in mem)}
  if pair['goose']['ram_mib']>pair['native']['ram_mib']+512 or pair['goose']['vram_mib']>pair['native']['vram_mib']+128:raise RuntimeError('RAM/VRAM growth exceeds sealed headroom')
  pairs.append(pair);tol=load(root/'trusted/policy.json')['performance_tolerance_percent']
  if len(pairs)==1 and all(100*(pair['goose'][m]/pair['native'][m]-1)>=-tol for m in ['prefill','decode']):break
 if len(pairs)==2:
  verdict=classify_pair(*pairs,tol)
  if verdict!='PAIR_WITHIN_TOLERANCE_CANDIDATE':raise RuntimeError(verdict)
 save(root/'results'/stage.name/'apex-pairs.json',pairs)
 return 'INTEGRATION_PASS_CANDIDATE'

def process(root,p):
 verify(root);state=load(root/'state.json')
 if state['halted']:raise RuntimeError('HALTED after repeated failures;human restart required')
 req=json.loads(read_regular(p,65536));rid=req.get('id','')
 if not isinstance(rid,str) or not re.fullmatch(r'[a-zA-Z0-9_-]{1,80}',rid):raise ValueError('Invalid request ID')
 receipt=root/'results'/(rid+'.json')
 if receipt.exists():raise ValueError('Request already processed;no replay')
 stage=None
 try:
  stage,files=snapshot(root,req);policy=load(root/'trusted/policy.json')
  if not set(files)<=set(policy['scope']):
   (stage/'runtime/autonomy-types').mkdir(parents=True,exist_ok=True)
   compiler=load(root/'trusted/baseline.json')['compiler']['compiler']
   for i,rel in enumerate(files):run_gate(root,stage,'new-module-typecheck-'+str(i),[compiler,'-o','/work/runtime/autonomy-types/'+str(i)+'.c','/work/'+rel])
   oid=checkpoint(root,stage,'DRAFT_ONLY','Uncovered new subsystem;trusted integration/safety recipe missing')
   result=dict(status='DRAFT_ONLY_MISSING_TRUSTED_RECIPE',commit=oid,accepted=False,reason='New subsystem requires independent recipe/review;no self-certification or progression')
  else:
   verdict=recipe_exp174(root,stage,req.get('integration')is True);oid=checkpoint(root,stage,verdict,str(req.get('message','Candidate')));result=dict(status=verdict,commit=oid,accepted=False,reason='Candidate evidence only;Codex acceptance remains mandatory')
  state=load(root/'state.json');state['failures']=0;save(root/'state.json',state)
 except Exception as e:
  state=load(root/'state.json');state['failures']+=1;state['halted']=state['failures']>=load(root/'trusted/policy.json')['max_failures'];save(root/'state.json',state)
  oid=checkpoint(root,stage,'FAILED_DRAFT',str(e)) if stage else None
  result=dict(status='REJECTED',commit=oid,accepted=False,reason=str(e),halted=state['halted'])
 save(receipt,result);event(root,'RESULT',request=rid,**result);return result

def supervise(root,once):
 with (root/'controller.lock').open('w')as lock:
  fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
  while True:
   requests=root/'workspace/.requests';requests.mkdir(exist_ok=True)
   pending=sorted(requests.glob('*.json'))[:100]
   for p in pending:
    if (root/'results'/(p.stem+'.json')).exists():continue
    try:print(json.dumps(process(root,p)),flush=True)
    except Exception as e:event(root,'REQUEST_REFUSED',name=p.name,reason=str(e));save(root/'results'/(p.stem+'.json'),{'status':'REFUSED','reason':str(e),'accepted':False})
   if once or load(root/'state.json')['halted']:return
   time.sleep(2)

def restore(root,oid):
 if not re.fullmatch('[0-9a-f]{40}',oid):raise ValueError('Full checkpoint hash required')
 dest=root/'restorations'/(oid+'-'+uuid.uuid4().hex[:8]);shutil.copytree(root/'trusted/reference',dest)
 blob=subprocess.check_output(['git','--git-dir='+str(root/'checkpoints/history.git'),'archive',oid],env=env())
 p=dest/'snapshot.tar';p.write_bytes(blob)
 with tarfile.open(p)as tf:tf.extractall(dest,filter='data')
 p.unlink();event(root,'RESTORED_NEW_DIRECTORY',commit=oid,path=str(dest));print(dest)

def agent(root,extra):
 verify(root);s=load(root/'state.json')
 if s['halted']:raise RuntimeError('Controller halted;do not restart autonomously')
 spec=importlib.util.spec_from_file_location('ccfactory',FACTORY);mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod);cli=mod.cli_find();key=mod.auth_key()
 cmd=bwrap(root,root/'workspace',network=True)
 for a in cli:
  p=Path(a)
  if p.is_absolute() and not str(p).startswith('/usr/'):
   # Readonly installed CLI/Node tree,no host home or account directory.
   base=next((x for x in p.parents if x.name.startswith('v22.')),p.parent)
   if str(base)not in cmd:cmd+=['--ro-bind',str(base),str(base)]
 r,w=os.pipe();os.write(w,json.dumps({'apiKey':key}).encode());os.close(w);cmd+=['--file',str(r),'/session/.commandcode/auth.json','--',*cli,*extra]
 print('Starting isolated Command Code;host projects/Git/checkpoints readonly or absent. No autonomous process was started by prepare.',flush=True)
 try:return subprocess.call(cmd,env=env(),pass_fds=(r,),preexec_fn=limits)
 finally:os.close(r)

def main():
 p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=DEFAULT_ROOT);sub=p.add_subparsers(dest='action',required=True)
 for a in ['verify','status','selftest']:sub.add_parser(a)
 q=sub.add_parser('prepare');q.add_argument('--resume',action='store_true')
 q=sub.add_parser('supervise');q.add_argument('--once',action='store_true')
 q=sub.add_parser('restore');q.add_argument('commit')
 q=sub.add_parser('agent');q.add_argument('cli_args',nargs=argparse.REMAINDER)
 a=p.parse_args();root=a.root.resolve()
 if a.action=='prepare':prepare(root,a.resume)
 elif a.action=='verify':verify(root);print('SEALED_INPUTS_OK')
 elif a.action=='status':print(json.dumps(load(root/'state.json'),indent=2))
 elif a.action=='supervise':supervise(root,a.once)
 elif a.action=='restore':restore(root,a.commit)
 elif a.action=='selftest':subprocess.run([sys.executable,str(Path(__file__).with_name('selftest.py')),'--root',str(root)],check=True)
 elif a.action=='agent':raise SystemExit(agent(root,a.cli_args[1:]if a.cli_args[:1]==['--']else a.cli_args))
if __name__=='__main__':main()
