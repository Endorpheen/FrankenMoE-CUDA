from pathlib import Path
import re,subprocess,hashlib,json
F=Path(__file__).resolve().parents[2];E=F/'experiments/EXP-2026-11-08-173-goose-cuda-flash-attention';S=E/'source'
base=subprocess.check_output(['git','-C',str(F),'show','c46b72b:experiments/EXP-2026-11-08-173-goose-cuda-flash-attention/source/vector-device.goose'],text=True)
assert hashlib.sha256(base.encode()).hexdigest()=='059a9b36b129fbb4f8d46147f265601e0f2b1664e8e053957bba3acd6ca706e0'
def endbrace(s,begin):
 depth=1;i=begin+1
 while depth:
  if s.startswith('//',i):
   i=s.index('\n',i);continue
  if s.startswith('/*',i):
   i=s.index('*/',i+2)+2;continue
  if s[i]=='"':
   i+=1
   while s[i]!='"':i+=2 if s[i]=='\\' else 1
  else:depth+=(s[i]=='{')-(s[i]=='}')
  i+=1
 return i
stats=[]
def unroll(s,cols):
 pattern=r'while\s+(j|h|i0|iv|kk|w)\s*<\s*(ncols|2|4|64|128|256)\s*\{'
 while True:
  matches=list(re.finditer(pattern,s))
  if not matches:return s
  m=matches[-1];v,b=m[1],m[2];limit=cols if b=='ncols' else int(b);end=endbrace(s,m.end()-1);body=s[m.end():end-1]
  step=re.search(r'\b'+v+r'\s*\+=\s*(\d+)\s*;\s*$',body)
  assert step,(v,b,body[-120:]);inc=int(step[1]);body=body[:step.start()]
  assert not re.search(r'\b'+v+r'\s*(?:[+*/-]?=(?!=)|\+\+)',body),(v,body)
  replacement='\n'.join('if true {\nlet '+v+':i32='+str(x)+';\n'+body+'\n}'for x in range(0,limit,inc))+'\n'+v+'=('+str(limit)+' as i32);\n'
  s=s[:m.start()]+replacement+s[end:];stats.append(dict(variable=v,bound=b,step=inc,copies=len(range(0,limit,inc))))
m=re.search(r'fn fattn_vec\(',base);start=m.start();brace=base.index('{',m.end());end=endbrace(base,brace);signature=base[start:brace];body=base[brace+1:end-1]
# The sink and metadata lanes must not force private arrays back into addressable local memory.
a=body.index('  let js=ty;');b=body.index('\n }\n j=0;',a)
sink=body[a:b];inner=sink[sink.index('{')+1:sink.rfind('}')]
assert inner.count('js')==10,inner.count('js')
ma=body.index(' if gy!=1 && tid<ncols');metadata=body[ma:];assert metadata.rstrip().endswith('}');metainner=metadata[metadata.index('{')+1:metadata.rfind('}')]
versions=[]
params=re.findall(r'\b(\w+)\s*:\s*\w+',signature);assert len(params)==46,len(params)
for n in (1,2):
 x=body[:a]+''.join('  if ty==('+str(j)+' as i32) {\n'+'let js:i32='+str(j)+';\n'+inner+'\n  }'for j in range(n))+body[b:ma]
 x+=''.join('\n if gy!=1 && tid==('+str(j)+' as i32) && ('+str(n)+'==1 || ic0+('+str(j)+' as i32)<ne01_z) {\n'+'let tid:i32='+str(j)+';\n'+metainner+'\n }'for j in range(n))
 # j<jc is uniform across the block. Keep its conditional barriers in the same order.
 jm=re.search(r'while j<jc\s*\{',x);je=endbrace(x,jm.end()-1);jb=x[jm.end():je-1];assert re.search(r'j\+=1;\s*$',jb);jb=re.sub(r'j\+=1;\s*$','',jb)
 x=x[:jm.start()]+''.join('\n if ('+str(j)+' as i32)<jc {\n'+'let j:i32='+str(j)+';\n'+jb+'\n }'for j in range(n))+'\n j=('+str(n)+' as i32);'+x[je:]
 x=unroll(x,n)
 x='\n let ncols:i32='+str(n)+';\n'+x
 versions.append(signature.replace('fattn_vec(','fattn_vec_'+str(n)+'(').replace('ncols:i32','unused_ncols:i32')+'{'+x+'}\n')
dispatch=signature+'{\n if ncols==1 {fattn_vec_1('+','.join(params)+');} else {fattn_vec_2('+','.join(params)+');}\n}\n'
result=base[:start]+''.join(versions)+dispatch+base[end:]
# Unroll the two-term Q4 dot without changing accumulation order.
hs=result.index('fn vec_dot_kq4(');hb=result.index('{',hs);he=endbrace(result,hb);result=result[:hb+1]+unroll(result[hb+1:he-1],0)+result[he-1:]
result='\n'.join(line.rstrip()for line in result.splitlines())+'\n'
(S/'vector-device.goose').write_text(result)
(E/'vector-specialization-pending.json').write_text(json.dumps(dict(status='SOURCE_ONLY_PENDING_SELFHOSTED_AND_GPU_CHECK',base_commit='c46b72b',base_source_sha256=hashlib.sha256(base.encode()).hexdigest(),source_sha256=hashlib.sha256(result.encode()).hexdigest(),specializations=[1,2],unrolled=stats,numerical_order_changed=False,production_changed=False),indent=2)+'\n')
print('Goose vector source specialized',len(base),'->',len(result),'bytes;',len(stats),'small loops unrolled')
