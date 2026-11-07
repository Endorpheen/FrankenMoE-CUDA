from pathlib import Path
import json,subprocess,hashlib,shutil,re
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
old=r/'source/elementwise-fma'
s=r/'source/elementwise-vector'
s.mkdir(exist_ok=True)
code=(old/'kernel.goose').read_text()
code=code.replace('d16:u16[:]', 'd16:u16[:],a8:const f32[8][:],b8:const f32[8][:],d8:f32[8][:]')
oldcall='p,a32,a16,b32,b16,d32,d16,a0,b0,d0'
newcall='p,a32,a16,b32,b16,d32,d16,a8,b8,d8,a0,b0,d0'
assert code.count(oldcall)==27
code=code.replace(oldcall,newcall)
vector=[]
for name,op in [('add','+'),('sub','-'),('mul','*'),('div','/')]:
 f=f'''fn row_v_{name}(p:const Plan&,a32:const f32[:],a8:const f32[8][:],b32:const f32[:],b8:const f32[8][:],d8:f32[8][:],a0:i64,b0:i64,d0:i64) {{
  if p.b.ne[0]==1 {{
    let scalar=b32[b0];
    for block in p.a.ne[0]/8 {{
      let x=a8[a0/8+block];var out:f32[8]=default<f32[8]>();
      for lane in 8 {{out[lane]=x[lane]{op}scalar;}}
      d8[d0/8+block]=out;
    }}
  }} else {{
    let width=p.b.ne[0]/8;
    for repeat in p.a.ne[0]/p.b.ne[0] {{
      let first_a=a0/8+repeat*width;let first_d=d0/8+repeat*width;
      for block in width {{
        let x=a8[first_a+block];let y=b8[b0/8+block];var out:f32[8]=default<f32[8]>();
        for lane in 8 {{out[lane]=x[lane]{op}y[lane];}}
        d8[first_d+block]=out;
      }}
    }}
  }}
}}
'''
 vector.append(f)
code=code.replace('fn execute(',''.join(vector)+'fn execute(',1)
needle='if p.mode==0 {\nif p.op==0'
assert code.count(needle)==1
dispatch='''if p.mode==1&&p.a.kind==0&&p.b.kind==0&&p.d.kind==0&&p.a.ne[0]%8==0&&a0%8==0&&d0%8==0&&(p.b.ne[0]==1||(p.b.stride[0]==1&&p.b.ne[0]%8==0&&b0%8==0)) {
'''
for i,name in enumerate(['add','sub','mul','div']):
 dispatch+=('if ' if i==0 else 'else if ')+f'p.op=={i} '+'{'+f'row_v_{name}(p,a32,a8,b32,b8,d8,a0,b0,d0);'+'}\n'
dispatch+='else {assert(false);}\n} else '
code=code.replace(needle,dispatch+needle,1)
oldmain='fn main() {let p:Plan=default<Plan>();var f:f32[1]=default<f32[1]>();var h:u16[1]=default<u16[1]>();execute(p,f,h,f,h,f,h);}'
newmain='fn main() {let p:Plan=default<Plan>();var f:f32[1]=default<f32[1]>();var h:u16[1]=default<u16[1]>();var v:f32[8][1]=default<f32[8][1]>();execute(p,f,h,f,h,f,h,v,v,v);}'
assert oldmain in code
code=code.replace(oldmain,newmain)
if (s/'generation.log').exists() and 'expected identifier' in (s/'generation.log').read_text() and not (s/'kernel-v1-rejected-keyword.goose').exists():
 shutil.copy2(s/'kernel.goose',s/'kernel-v1-rejected-keyword.goose')
 shutil.copy2(s/'generation.log',s/'generation-v1-rejected-keyword.log')
code=re.sub(r'\bblock\b','chunk',code)
(s/'kernel.goose').write_text(code)
shutil.copy2(old/'operations.json',s/'operations.json')
(s/'hypothesis.json').write_text(json.dumps({'scope':'Full binary/unary modules plus checked F32x8 binary path for aligned logical row/block offsets and contiguous/broadcast inputs. Arbitrary layouts and F16/BF16 remain in Goose scalar path, not native fallback.','producer_sha256':'81d6d066a675b282c72ee214d7154b709f3a3f581f72103a6b0790469f4f460f','compiler_change':False,'evidence':'Scalar counter-free large F32 ADD42.6us versus native4.2us; DIV46.05us versus native6.2us. All ordinary values exact; NaN payload limitation retained.'},indent=2)+'\n')
producer=r/'compiler/stage3-81d6d066a675'
assert hashlib.sha256(producer.read_bytes()).hexdigest()=='81d6d066a675b282c72ee214d7154b709f3a3f581f72103a6b0790469f4f460f'
out=r/'generated/elementwise-vector.c'
cmd=[str(producer),'-o',str(out),str(s/'kernel.goose')]
p=subprocess.run(cmd,cwd=r,capture_output=True,text=True)
(s/'generation.log').write_text(p.stdout+p.stderr)
(s/'generation-command.json').write_text(json.dumps(cmd,indent=2)+'\n')
print('generation',p.returncode,p.stdout[-1000:],p.stderr[-1000:],flush=True)
p.check_returncode()
for name in ['check','nan_diagnosis','sanitize']:
 text=Path('/tmp/fg_elementwise_fma_'+name+'.py').read_text().replace('elementwise-fma','elementwise-vector')
 Path('/tmp/fg_elementwise_vector_'+name+'.py').write_text(text)
build=Path('/tmp/fg_elementwise_fma_build.py').read_text().replace('elementwise-fma','elementwise-vector')
needle='gg_view(d,p->d.kind!=0?nd:0),0);'
assert build.count(needle)==1
build=build.replace(needle,'gg_view(d,p->d.kind!=0?nd:0),gg_view((void*)a,p->a.kind==0?na/8:0),gg_view((void*)b,p->b.kind==0?nb/8:0),gg_view(d,p->d.kind==0?nd/8:0),0);')
Path('/tmp/fg_elementwise_vector_build.py').write_text(build)
text=Path('/tmp/fg_elementwise_perf.py').read_text().replace('elementwise-fma','elementwise-vector')
Path('/tmp/fg_elementwise_vector_perf.py').write_text(text)
