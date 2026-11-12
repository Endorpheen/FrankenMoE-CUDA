from pathlib import Path
import importlib.util,re,json,hashlib
F=Path(__file__).resolve().parents[2];E=F/'experiments/EXP-2026-11-08-173-goose-cuda-flash-attention';S=E/'source';spec=importlib.util.spec_from_file_location('lexical',E/'specialize_iter.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
lex=m.lex;sig=m.sig;match=m.match;ns=m.ns;join=lambda t:''.join(v for _,v in t)
it=(S/'mma-iter-device.goose').read_text();pr=(S/'mma-process-device.goose').read_text();stats={}
def parts(src,name):
 ts=lex(src);fi,ni,j,pc,k,bc=m.find(ts,name);return ts[:fi],ts[fi:k],ts[k+1:bc],ts[bc+1:]
def args(ts):
 out=[];st=0;depth=0
 for i,(k,t)in enumerate(ts):
  if k=='punct':
   if t in '([{':depth+=1
   elif t in ')]}':depth-=1
   elif t==','and depth==0:out.append(ts[st:i]);st=i+1
 out.append(ts[st:]);return out

def field(ptr,index,env,write=False,bits=False):
 n=m.ev(index,env)
 if ptr in ['qregs','vkqregs']:
  assert n is not None and 0<=n<64,(ptr,join(index),env)
  return ('qr.q'if ptr=='qregs'else'acc.v')+str(n)
 assert ptr in ['kmax','rowsums'];base='acc.k'if ptr=='kmax'else'acc.s'
 if n is not None:assert n in [0,1];return base+str(n)
 assert not write and re.sub(r'\bas\s+(?:u|i)(?:32|64)\b|[\s()]','',join(index))=='tx%2',('unexpected dynamic private state index',ptr,join(index))
 return '(if tx%2==0{'+base+'0}else{'+base+'1})'

def pointer(a):
 ss=sig(a)
 while ss and ss[0]==('punct','(')and match(ss,0)==len(ss)-1:ss=ss[1:-1]
 if not ss or ss[0][0]!='ident'or ss[0][1]not in ['qregs','vkqregs','kmax','rowsums']:return None
 ptr=ss[0][1]
 if len(ss)==1:return ptr,[('number','0')]
 assert ss[1]==('punct','+');return ptr,ss[2:]

def walk(ts,env,nc):
 out=[];i=0
 while i<len(ts):
  k,t=ts[i]
  if k=='ident'and t in ['let','var']:
   j=i;depth=0
   while j<len(ts):
    if ts[j][0]=='punct':
     u=ts[j][1]
     if u in '([{':depth+=1
     elif u in ')]}':depth-=1
     elif u==';'and depth==0:break
    j+=1
   assert j<len(ts);stmt=ts[i:j+1];ss=sig(stmt);assert ss[1][0]=='ident';name=ss[1][1];eq=next((a for a,x in enumerate(ss)if x==('punct','=')),None)
   val=m.ev(ss[eq+1:-1],env)if eq is not None else None
   # Recurse only over the initializer; the declaration prefix is unchanged.
   if eq is not None:
    equal=next(a for a,x in enumerate(stmt)if x==('punct','='));out+=stmt[:equal+1]+walk(stmt[equal+1:],dict(env),nc)
   else:out+=stmt
   env[name]=val;i=j+1;continue
  if (k,t)==('punct','{'):
   e=match(ts,i);out.append(ts[i]);out+=walk(ts[i+1:e],dict(env),nc);out.append(ts[e]);i=e+1;continue
  if k=='ident'and t in ['ldu','stu','gload','gstore','ldu32','stu32','iterate']:
   j=ns(ts,i+1)
   if j<len(ts)and ts[j]==('punct','('):
    e=match(ts,j);aa=args(ts[j+1:e]);rewritten=[join(walk(a,dict(env),nc))for a in aa];replace=None
    if t=='iterate':
     assert len(aa)==20;new=rewritten[:10]+['qr','acc']+[rewritten[x]for x in [15,16,17,19]];replace='acc=iterate_value'+str(nc)+'('+','.join(new)+')'
    elif t in ['ldu','stu']:
     p=pointer(aa[0])
     if p:
      buf,offset=p;n=m.ev(offset,env);assert n is not None and n%4==0,(buf,join(offset));idx=[('number',str(n//4))];f=field(buf,idx,env,t=='stu')
      if t=='ldu':replace=f if buf in ['qregs','vkqregs']else'bits('+f+')'
      else:replace=f+'='+('frombits('+rewritten[1]+')'if buf in ['kmax','rowsums']else rewritten[1])
    else:
     ss=sig(aa[0]);buf=ss[0][1]if len(ss)==1 else ''
     if buf in ['qregs','vkqregs','kmax','rowsums']:
      write=t in ['gstore','stu32'];f=field(buf,aa[1],env,write);replace=f+'='+rewritten[2]if write else f
    if replace is not None:
     stats[t]=stats.get(t,0)+1;out.append(('raw',replace));i=e+1;continue
  out.append(ts[i]);i+=1
 return out

basehead=join(parts(it,'iterate_nc4_np2')[0]);phead=join(parts(pr,'process_tile_apex_nc4_np2')[0]);phead=re.sub(r'struct U4\s*\{[^}]*\}','',phead)
phead=re.sub(r'extern "fg175m_iter" fn iterate\([^;]+;','',phead)
# Both pure source prefixes contain the original helpers and raw-effect declarations only.
assert 'fn iterate('not in basehead and'fn process_tile('not in phead
qtype='struct QueryRegs {'+','.join('q'+str(i)+':u32'for i in range(64))+'}\n'
atype='struct Accumulator {'+','.join(['v'+str(i)+':u32'for i in range(64)]+['k0:f32','k1:f32','s0:f32','s1:f32'])+'}\n'
chunks=[basehead,phead,qtype,atype]
for nc,npv in [(4,2),(8,1)]:
 _,_,ib,_=parts(it,'iterate_nc'+str(nc)+'_np'+str(npv));ib=walk(ib,{},nc)
 signature='fn iterate_value'+str(nc)+'(k_h2:u64,v_h2:u64,mask_h:u64,stride_k:i64,stride_v:i64,stride_mask:i64,slope:f32,jt:i32,kb0:i32,n_queries:i64,qr:QueryRegs,initial:Accumulator,smem:u64,tx:i32,ty:i32,last:bool)->Accumulator'
 chunks.append(signature+'{var acc:Accumulator=initial;\n'+join(ib)+'\nreturn acc;}\n')
 _,ph,pb,_=parts(pr,'process_tile_apex_nc'+str(nc)+'_np'+str(npv));pbody=join(walk(pb,{},nc));init='var qr:QueryRegs=QueryRegs{'+','.join('0 as u32'for _ in range(64))+'};\nvar acc:Accumulator=Accumulator{'+','.join(['0 as u32']*64+['0.0 as f32']*4)+'};\n'
 chunks.append(join(ph)+'{'+init+pbody+'}\n')
# Keep the established internal process ABI. Legacy raw-state pointers are never used by the new value path.
_,head,_,_=parts(pr,'process_tile');head=join(head);ps=head[head.index('(')+1:head.rfind(')')];names=[x.split(':')[0].strip()for x in ps.split(',')];assert len(names)==34
body='if ncols1==4&&np==2{process_tile_apex_nc4_np2('+','.join(names)+');}else{if ncols1==8&&np==1{process_tile_apex_nc8_np1('+','.join(names)+');}}'
chunks.append(head+'{'+body+'}\n');main=pr[pr.rfind('fn main()'):];chunks.append(main)
src='\n'.join(chunks)
seen_externs={}
def unique_extern(mm):
 name=mm[2];signature=([x.split(':')[1].strip()for x in mm[3].split(',')if x.strip()],mm[4]or'void')
 if name in seen_externs:
  assert name in ['barrier','expf','shflx','load_kv','load_mask','hlo','hhi','ldm4']and signature==seen_externs[name],('nonidentical duplicate effect',name)
  return ''
 seen_externs[name]=signature;return mm[0]
src=re.sub(r'extern "([^"]+)" fn (\w+)\(([^;]*)\)(?:->(\w+))?;',unique_extern,src)
# Adapt raw ISA POD values to this module's actual generated POD fields; no numerical work moves to C++.
raw=[]
for mm in re.finditer(r'extern "(fg176i_[^"]+|fg175p_[^"]+)" fn (\w+)\(([^;]*)\)(?:->(\w+))?;',src):
 raw.append(dict(original=mm[1],symbol='fg175c_'+mm[1],goose_name=mm[2],params=mm[3],result=mm[4]or'void'))
assert len({x['original']for x in raw})==len(raw)
for r in raw:src=src.replace('extern "'+r['original']+'"','extern "'+r['symbol']+'"')
assert not re.search(r'\b(?:ldu|stu|gload|gstore|ldu32|stu32)\(\s*(?:qregs|vkqregs|kmax|rowsums)\b',src)
(S/'value-prefill-device.goose').write_text('\n'.join(x.rstrip()for x in src.splitlines())+'\n')
(E/'value-prefill-pending.json').write_text(json.dumps(dict(status='SOURCE_ONLY_PENDING_SELFHOSTED_ABI_GPU_AND_APEX',hypothesis='Replace all address-taken per-thread Q/V/max/sum storage with actual scalar-field Goose value structs;keep math,source ordering,barriers,and numerical partition.',query_bytes=256,accumulator_bytes=272,private_state_raw_load_store_calls_remaining=0,rewritten_effects=stats,raw_adapters=raw,compiler_changed=False,production_changed=False,active_runtime='EXP172'),indent=2)+'\n');print('VALUE_PREFILL_GOOSE_READY',len(src),'bytes',stats)
