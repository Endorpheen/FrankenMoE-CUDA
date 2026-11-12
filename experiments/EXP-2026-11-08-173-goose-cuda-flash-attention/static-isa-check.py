from pathlib import Path
import subprocess,re,json,hashlib
F=Path(__file__).resolve().parents[2];E=F/'experiments/EXP-2026-11-08-173-goose-cuda-flash-attention';B=F/'runtime/exp173/build';O=B/'codegen-audit';cu='/usr/local/cuda/bin/cuobjdump';obj=B/'cuda-runtime.o';rows=json.loads((O/'selected-resources.json').read_text());selected=[x for x in rows if x['label']=='goose_after_static_index_fix'];names=','.join(x['mangled']for x in selected)
for mode,suffix in [('--dump-sass','sass'),('--dump-ptx','ptx')]:
 q=subprocess.run([cu,'--gpu-architecture','sm_89','--function',names,mode,str(obj)],capture_output=True,text=True,check=True);(O/('selected-current.'+suffix)).write_text(q.stdout)
sass=(O/'selected-current.sass').read_text();details=[]
for row in selected:
 name=row['mangled'];m=re.search(r'Function\s*:\s*'+re.escape(name)+r'\s*\n',sass);assert m,name
 tail=sass[m.end():];stop=re.search(r'\n\s*Function\s*:',tail);body=tail[:stop.start()]if stop else tail
 opcodes=re.findall(r'/\*[0-9a-fA-F]+\*/\s+@?!?P?\d*\s*([A-Z][A-Z0-9.]*)',body)
 # Count static instruction sites, not dynamic operations or a performance measurement.
 instructions=[]
 for line in body.splitlines():
  mm=re.search(r'/\*[0-9a-fA-F]+\*/\s+(?:@!?P\d+\s+)?([A-Z][A-Z0-9.]*)',line)
  if mm:instructions.append(mm[1])
 item=dict(row,static_instruction_sites=len(instructions),static_local_load_sites=sum(x.startswith('LDL')for x in instructions),static_local_store_sites=sum(x.startswith('STL')for x in instructions));details.append(item)
 print(item['demangled'].split('(')[0],item['resources'],'static local LD/ST',item['static_local_load_sites'],item['static_local_store_sites'],flush=True)
P=O/'ptx-assembly';P.mkdir(exist_ok=True)
q=subprocess.run([cu,'--extract-ptx','all',str(obj)],cwd=P,capture_output=True,text=True,check=True);(P/'extraction.stdout').write_text(q.stdout)
ptxs=list(P.glob('*.ptx'));assert len(ptxs)==1,ptxs
args=['/usr/local/cuda/bin/ptxas','-arch=sm_89','-O3','-v','--warn-on-spills','--warn-on-local-memory-usage',str(ptxs[0]),'-o',str(P/'diagnostic.cubin')];q=subprocess.run(args,cwd=P,capture_output=True,text=True);(P/'ptxas.stdout').write_text(q.stdout);(P/'ptxas.stderr').write_text(q.stderr);assert q.returncode==0,q.stderr
res=subprocess.run([cu,'--dump-resource-usage',str(P/'diagnostic.cubin')],capture_output=True,text=True,check=True).stdout;(P/'reassembled-resources.txt').write_text(res)
for x in details:
 m=re.search(r'Function\s+'+re.escape(x['mangled'])+r':\s*\n([^\n]+)',res);assert m and m[1].strip()==x['resources'],x['demangled']+' reassembly resource mismatch'
 # ptxas reports the spill traffic separately from the stack frame bytes.
 pattern=r"Function properties for "+re.escape(x['mangled'])+r'\s*\n\s*(\d+) bytes stack frame, (\d+) bytes spill stores, (\d+) bytes spill loads'
 m=re.search(pattern,q.stderr);assert m,x['mangled'];x['ptxas_stack_frame_bytes']=int(m[1]);x['ptxas_spill_store_bytes']=int(m[2]);x['ptxas_spill_load_bytes']=int(m[3]);print('PTXAS',x['demangled'].split('(')[0],m.groups(),flush=True)
report=dict(status='STATIC_ISA_AND_PTXAS_VERIFIED',compiler_sha256=hashlib.sha256((F/'runtime/compiler/stage3-f849233b999c').read_bytes()).hexdigest(),object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest(),source='Actual embedded PTX reassembled with ptxas O3 sm89;resource summaries equal existing actual cubin for every selected kernel',counts='SASS counts are static instruction sites,not runtime traffic or timing. Stack bytes are not spill bytes.',ptxas_argv=args,kernels=details,production_changed=False,compiler_changed=False)
(E/'codegen-static-report.json').write_text(json.dumps(report,indent=2)+'\n')
