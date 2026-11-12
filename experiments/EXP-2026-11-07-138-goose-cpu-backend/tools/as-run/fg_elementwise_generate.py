from pathlib import Path
import json,subprocess,hashlib,re,shutil
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
s=r/'source/elementwise'
s.mkdir(exist_ok=True)
producer=r/'compiler/stage3-81d6d066a675'
sha=hashlib.sha256(producer.read_bytes()).hexdigest()
assert sha=='81d6d066a675b282c72ee214d7154b709f3a3f581f72103a6b0790469f4f460f'
(s/'hypothesis.json').write_text(json.dumps({'scope':'Replace entire binary-ops.cpp and unary-ops.cpp implementation for all existing supported types with Goose arithmetic, conversions, broadcast/row addressing and worker ranges. Keep native objects for rollback only.','producer_sha256':sha,'production_change':False,'performance':'Unmeasured; exact correctness and real Apex gates before claims'},indent=2)+'\n')
head='''struct Shape { ne:i64[4], stride:i64[4], kind:i64 }
struct Plan { a:Shape, b:Shape, d:Shape, ith:i64, nth:i64, mode:i64, op:i64, params:f32[4] }
extern "fg_el_bits" fn bits(x:f32)->u32;
extern "fg_el_from_bits" fn from_bits(x:u32)->f32;
extern "fg_el_half" fn half(x:u16)->f32;
extern "fg_el_to_half" fn to_half(x:f32)->u16;
extern "fg_element_witness" fn witness(mode:i64,op:i64,values:i64);
extern "fabsf" fn abs32(x:f32)->f32;
extern "expf" fn exp32(x:f32)->f32;
extern "expm1f" fn expm132(x:f32)->f32;
extern "logf" fn log32(x:f32)->f32;
extern "tanhf" fn tanh32(x:f32)->f32;
extern "sqrtf" fn sqrt32(x:f32)->f32;
extern "sinf" fn sin32(x:f32)->f32;
extern "cosf" fn cos32(x:f32)->f32;
extern "floorf" fn floor32(x:f32)->f32;
extern "ceilf" fn ceil32(x:f32)->f32;
extern "roundf" fn round32(x:f32)->f32;
extern "truncf" fn trunc32(x:f32)->f32;
extern "fminf" fn min32(a:f32,b:f32)->f32;
extern "fmaxf" fn max32(a:f32,b:f32)->f32;
fn bf16(x:u16)->f32 {return from_bits((x as u32)<<16);}
fn to_bf16(x:f32)->u16 {
 let u=bits(x);
 if (u&2147483647)>2139095040 {return ((u>>16)|64) as! u16;}
 return ((u+(32767+((u>>16)&1)))>>16) as! u16;
}
fn minimum(a:i64,b:i64)->i64 {return if a<b {a} else {b};}
'''
unary=[('abs','abs32(x)'),('sgn','if x>0.0 {1.0} else if x<0.0 {-1.0} else {0.0}'),('neg','-x'),('step','if x>0.0 {1.0} else {0.0}'),('tanh','tanh32(x)'),('elu','if x>0.0 {x} else {expm132(x)}'),('relu','if x>0.0 {x} else {0.0}'),('sigmoid','1.0/(1.0+exp32(-x))'),('hardsigmoid','min32(1.0,max32(0.0,(x+3.0)/6.0))'),('exp','exp32(x)'),('hardswish','x*min32(1.0,max32(0.0,(x+3.0)/6.0))'),('sqr','x*x'),('sqrt','sqrt32(x)'),('sin','sin32(x)'),('cos','cos32(x)'),('log','log32(x)'),('expm1','exp32(x)-1.0'),('softplus','if x>20.0 {x} else {log32(1.0+exp32(x))}'),('floor','floor32(x)'),('ceil','ceil32(x)'),('round','round32(x)'),('trunc','trunc32(x)'),('xielu','if x>0.0 {p.params[1]*x*x+p.params[2]*x} else {(expm132(min32(x,p.params[3]))-x)*p.params[0]+p.params[2]*x}')]
types=[('f32','a32','d32',''),('f16','a16','d16','half'),('bf16','a16','d16','bf16')]
formats=[(0,0),(1,1),(2,2),(2,0),(1,0)]
bformats=[(0,0,0),(1,1,1),(2,2,2),(2,0,2),(2,0,0),(1,0,1),(1,0,0)]
def load(kind,which,index):
 arr=which+('32' if kind==0 else '16')
 expr=f'{arr}[{index}]'
 return expr if kind==0 else f'{"half" if kind==1 else "bf16"}({expr})'
def store(kind,index,expr):
 arr='d32' if kind==0 else 'd16'
 value=expr if kind==0 else f'{"to_half" if kind==1 else "to_bf16"}({expr})'
 return f'{arr}[{index}]={value};'
body=[]
args='p:const Plan&,a32:const f32[:],a16:const u16[:],b32:const f32[:],b16:const u16[:],d32:f32[:],d16:u16[:]'
rowargs=args+',a0:i64,b0:i64,d0:i64'
for i,(name,expr) in enumerate(unary):
 code=f'fn row_u_{name}({rowargs}) {{\n'
 for j,(ak,dk) in enumerate(formats):
  cond=f'p.a.kind=={ak}&&p.d.kind=={dk}'
  code+=('if ' if j==0 else 'else if ')+cond+' {\n'
  code+='for lane in p.a.ne[0] {'+f'let x={load(ak,"a","a0+lane")};let value:f32={expr};'+store(dk,'d0+lane','value')+'}\n}\n'
 code+='else {assert(false);}\n}\n'
 body.append(code)
for i,(name,op) in enumerate([('add','+'),('sub','-'),('mul','*'),('div','/')]):
 code=f'fn row_b_{name}({rowargs}) {{\n'
 for j,(ak,bk,dk) in enumerate(bformats):
  code+=('if ' if j==0 else 'else if ')+f'p.a.kind=={ak}&&p.b.kind=={bk}&&p.d.kind=={dk}'+' {\n'
  code+='for repeat in p.a.ne[0]/p.b.ne[0] {for lane in p.b.ne[0] {'
  code+=f'let at=a0+repeat*p.b.ne[0]+lane;let bt=b0+lane*p.b.stride[0];let dt=d0+repeat*p.b.ne[0]+lane;let value:f32={load(ak,"a","at")}{op}{load(bk,"b","bt")};'+store(dk,'dt','value')+'}}\n}\n'
 code+='else {assert(false);}\n}\n'
 body.append(code)
entry=f'''fn execute({args}) {{
 assert(p.nth>0&&p.ith>=0&&p.ith<p.nth);
 let nr=p.a.ne[1]*p.a.ne[2]*p.a.ne[3];let step=(nr+p.nth-1)/p.nth;
 let first=step*p.ith;let last=minimum(first+step,nr);var done:i64=0;
 var row=first;
 while row<last {{
  let z=row/(p.a.ne[2]*p.a.ne[1]);let y=(row-z*p.a.ne[2]*p.a.ne[1])/p.a.ne[1];let x=row-z*p.a.ne[2]*p.a.ne[1]-y*p.a.ne[1];
  let a0=z*p.a.stride[3]+y*p.a.stride[2]+x*p.a.stride[1];
  let d0=z*p.d.stride[3]+y*p.d.stride[2]+x*p.d.stride[1];
  var b0:i64=0;
  if p.mode==1 {{b0=(z%p.b.ne[3])*p.b.stride[3]+(y%p.b.ne[2])*p.b.stride[2]+(x%p.b.ne[1])*p.b.stride[1];}}
'''
call='p,a32,a16,b32,b16,d32,d16,a0,b0,d0'
entry+='if p.mode==0 {\n'
for i,(name,_) in enumerate(unary):entry+=('if ' if i==0 else 'else if ')+f'p.op=={i} '+'{'+f'row_u_{name}({call});'+'}\n'
entry+='else {assert(false);}\n} else {\n'
for i,name in enumerate(['add','sub','mul','div']):entry+=('if ' if i==0 else 'else if ')+f'p.op=={i} '+'{'+f'row_b_{name}({call});'+'}\n'
entry+='else {assert(false);}\n}\n'
entry+='done+=p.a.ne[0];row+=1;\n}\nif done>0 {witness(p.mode,p.op,done);}\n}\n'
main='fn main() {let p:Plan=default<Plan>();var f:f32[1]=default<f32[1]>();var h:u16[1]=default<u16[1]>();execute(p,f,h,f,h,f,h);}\n'
source=head+''.join(body)+entry+main
if (s/'generation.log').exists() and 'incompatible types' in (s/'generation.log').read_text() and not (s/'kernel-v1-rejected-literal.goose').exists():
 shutil.copy2(s/'kernel.goose',s/'kernel-v1-rejected-literal.goose')
 shutil.copy2(s/'generation.log',s/'generation-v1-rejected-literal.log')
source=re.sub(r'(?<![\w.])\d+\.\d+(?!\w)',lambda m:'('+m[0]+' as f32)',source)
(s/'kernel.goose').write_text(source)
(s/'operations.json').write_text(json.dumps({'unary':[n for n,_ in unary],'binary':['add','sub','mul','div'],'unary_formats':formats,'binary_formats':bformats},indent=2)+'\n')
out=r/'generated/elementwise.c'
cmd=[str(producer),'-o',str(out),str(s/'kernel.goose')]
p=subprocess.run(cmd,cwd=r,capture_output=True,text=True)
(s/'generation.log').write_text(p.stdout+p.stderr)
(s/'generation-command.json').write_text(json.dumps(cmd,indent=2)+'\n')
print('generation',p.returncode,p.stdout[-2000:],p.stderr[-2000:],flush=True)
p.check_returncode()
print(json.dumps({'unary_operations':len(unary),'binary_operations':4,'goose_lines':len(source.splitlines()),'generated_bytes':out.stat().st_size,'producer':sha},indent=2))
