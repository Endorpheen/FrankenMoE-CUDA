from pathlib import Path
import json,os,subprocess,sys,math,struct
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');a=Path('/home/end0/AI/LlamaServer/franken-goose-exp149-cpu-control-20261107')
sanitize='--sanitize' in sys.argv;threaded='--pthread' in sys.argv;v=a/'variants'/('pthread-sanitized' if threaded and sanitize else 'pthread' if threaded else 'sanitized' if sanitize else 'cpu-backend');b=v/'build';run=v/'run';out=a/'results'/('pthread-sanitized' if threaded and sanitize else 'pthread' if threaded else 'sanitized' if sanitize else 'release');out.mkdir(exist_ok=True)
def mapped(arg):
 for old,new in [(r/'source/cpu-backend',a/'source/cpu-backend'),(r/'generated',a/'generated'),(r/'variants/cpu-backend/build',b),(r/'variants/cpu-backend/run',run)]:arg=arg.replace(str(old),str(new))
 return arg
records=[]
def execute(label,argv,env=None):
 p=subprocess.run(argv,capture_output=True,text=True,env=env,timeout=180);(out/(label+'.log')).write_text(p.stdout+p.stderr);records.append({'label':label,'argv':argv,'exit_code':p.returncode});print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-2000:]);raise SystemExit(p.returncode)
 return p
def environment(bundle,control='1',api='1'):
 env=dict(os.environ);env.pop('LD_PRELOAD',None)
 for key in list(env):
  if key.startswith('GGML_GOOSE_'):env.pop(key)
 env.update(LD_LIBRARY_PATH=str(bundle),GGML_GOOSE_ALLOC='0',GGML_GOOSE_COALESCE='0',GGML_GOOSE_LIFETIME='0',GGML_GOOSE_GRAPH_PLAN='0',GGML_GOOSE_CPU_BACKEND='1',GGML_GOOSE_CPU_EXECUTOR='1',GGML_GOOSE_CPU_CONTROL=control,GGML_GOOSE_CPU_BACKEND_API=api,GGML_EXPERT_HOTSTORE='0',ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 return env
checks={}
for name,recipefile in [('composite',r/'variants/cpu-backend/build/composite-command.json'),('apex-contract',r/'source/cpu-backend/apex-contract-command.json'),('planning',r/'variants/cpu-backend/build/control-planner-command.json'),('backend-buffer',r/'variants/cpu-backend/build/backend-buffer-fixture-command.json')]:
 if '--buffer-only' in sys.argv and name!='backend-buffer':continue
 recipe=json.loads(recipefile.read_text());argv=[mapped(x) for x in recipe]
 if sanitize:argv=[('-O1' if x in ['-O2','-O3'] else x) for x in argv]+['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g']
 execute('compile-'+name,argv);binary=argv[argv.index('-o')+1]
 arms=[('native',r/'run','1','0'),('candidate',run,'1','1')]
 if name=='planning':arms=[('native',r/'variants/cpu-backend-control-reference/run','0','1'),('candidate',run,'1','1'),('rollback',run,'0','1')]
 if name=='backend-buffer':arms=[('native',r/'variants/cpu-backend-api-reference/run','1','0'),('candidate',run,'1','1'),('rollback',run,'1','0')]
 data={};meta={}
 for arm,bundle,control,api in arms:
  target=out/(name+'-'+arm+'.bin');args=[binary]
  if name=='apex-contract':args.append(str(r/'variants/cpu-backend/build/apex-embedding-selected-rows.local.bin'))
  p=execute(name+'-'+arm,args+[str(target)],environment(bundle,control,api));data[arm]=target.read_bytes();meta[arm]=[json.loads(x) for x in p.stdout.splitlines()]
 if threaded and name=='backend-buffer':
  normalized=bytearray();offset=0;newmeta=[]
  for row in meta['native']:
   row=dict(row);part=data['native'][offset:offset+row['bytes']];offset+=row['bytes']
   if row['name']=='feature-list':
    fields=part.split(b'\0');assert fields[-1]==b'';fields=fields[:-1];assert len(fields)%2==0
    pairs=list(zip(fields[::2],fields[1::2]));assert (b'OPENMP',b'1') in pairs
    part=b''.join(k+b'\0'+v+b'\0' for k,v in pairs if k!=b'OPENMP');row['bytes']=len(part)
   normalized.extend(part);newmeta.append(row)
  assert offset==len(data['native']);data['native']=bytes(normalized);meta['native']=newmeta
 assert all(meta[x]==meta['native'] for x in meta)
 if name in ['planning','backend-buffer']:
  assert all(data[x]==data['native'] for x in data);checks[name]={'cases':len(meta['candidate']),'exact':True,'rollback':True}
 else:
  offset=0;exact=0;violations=0
  for row in meta['candidate']:
   n=row['bytes'];x=data['native'][offset:offset+n];y=data['candidate'][offset:offset+n];exact+=x==y
   if row.get('type',0)==0:
    for xx,yy in zip(struct.unpack('<'+str(n//4)+'f',x),struct.unpack('<'+str(n//4)+'f',y)):
     if xx==yy or math.isnan(xx) and math.isnan(yy):continue
     if not(math.isfinite(xx) and math.isfinite(yy)) or abs(xx-yy)>2e-5+2e-5*abs(xx):violations+=1
   elif x!=y:violations+=1
   offset+=n
  assert offset==len(data['native'])==len(data['candidate']) and violations==0
  checks[name]={'cases':len(meta['candidate']),'byte_exact':exact,'violations':violations}
report={'status':'PASS_EXP149_SCOPED_SANITIZED' if sanitize else 'PASS_EXP149_FUNCTIONAL_AND_ROLLBACK','checks':checks,'model_loaded':False,'scope':'Rebuilt base/CPU units and fixtures instrumented; inherited objects/vendor libraries not fully instrumented' if sanitize else 'Native comparison with fixed EXP146 fixture and shared B1 corpora'}
(out/'summary.json').write_text(json.dumps(report,indent=2)+'\n');(out/'commands.json').write_text(json.dumps(records,indent=2)+'\n');print(json.dumps(report,indent=2))
