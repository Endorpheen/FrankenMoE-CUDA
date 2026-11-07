from pathlib import Path
import json,subprocess,hashlib,shutil
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
old=r/'source/elementwise'
s=r/'source/elementwise-fma'
s.mkdir(exist_ok=True)
code=(old/'kernel.goose').read_text()
code=code.replace('extern "fabsf"','extern "fmaf" fn fma32(a:f32,b:f32,c:f32)->f32;\nextern "fabsf"',1)
pos='p.params[1]*x*x+p.params[2]*x'
neg='(expm132(min32(x,p.params[3]))-x)*p.params[0]+p.params[2]*x'
assert code.count(pos)==5 and code.count(neg)==5
code=code.replace(pos,'fma32(p.params[1]*x,x,p.params[2]*x)').replace(neg,'fma32(expm132(min32(x,p.params[3]))-x,p.params[0],p.params[2]*x)')
(s/'kernel.goose').write_text(code)
shutil.copy2(old/'operations.json',s/'operations.json')
(s/'hypothesis.json').write_text(json.dumps({'scope':'Same full binary/unary modules; make XIELU final fused multiply-add explicit to preserve native rounding across release and sanitizer builds. Original implicit-FMA candidate retained.','evidence':'Native xielu object disassembly uses FMA for both positive and negative branches; sanitizer O1 implicit Goose expression differs at3900 output values.','producer_sha256':'81d6d066a675b282c72ee214d7154b709f3a3f581f72103a6b0790469f4f460f','compiler_change':False,'nan_payload_difference':'Separate unresolved issue; not fixed or concealed by this change'},indent=2)+'\n')
producer=r/'compiler/stage3-81d6d066a675'
assert hashlib.sha256(producer.read_bytes()).hexdigest()=='81d6d066a675b282c72ee214d7154b709f3a3f581f72103a6b0790469f4f460f'
out=r/'generated/elementwise-fma.c'
cmd=[str(producer),'-o',str(out),str(s/'kernel.goose')]
p=subprocess.run(cmd,cwd=r,capture_output=True,text=True)
(s/'generation.log').write_text(p.stdout+p.stderr)
(s/'generation-command.json').write_text(json.dumps(cmd,indent=2)+'\n')
print('generation',p.returncode,p.stdout[-1000:],p.stderr[-1000:],flush=True)
p.check_returncode()
for name in ['build','check','nan_diagnosis','sanitize']:
 text=Path('/tmp/fg_elementwise_'+name+'.py').read_text().replace('elementwise','elementwise-fma').replace('/tmp/fg_elementwise-fma_fixture.c','/tmp/fg_elementwise_fixture.c')
 Path('/tmp/fg_elementwise_fma_'+name+'.py').write_text(text)
