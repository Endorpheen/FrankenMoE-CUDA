'''Bounded deterministic Goose specializer/unroller for fattn iterate().'''
import re
INDEX=('kqc','kqold','kqmn','rsa','scl','sh2','b'); ITYPES={'i8','i16','i32','i64','u8','u16','u32','u64'}
TYPES=ITYPES|{'f32','bool','U2','U4','F4','F2'}
EXPECTED=[('k_h2','u64'),('v_h2','u64'),('mask_h','u64'),('stride_k','i64'),('stride_v','i64'),('stride_mask','i64'),('slope','f32'),('jt','i32'),('kb0','i32'),('n_queries','i64'),('ncols1','i32'),('qregs','u64'),('vkqregs','u64'),('kmax','u64'),('rowsums','u64'),('smem','u64'),('tx','i32'),('ty','i32'),('np','i32'),('last','bool')]
class TransformError(RuntimeError): pass
_TOK=re.compile(r'(?P<ws>\s+)|(?P<comment>//[^\n]*|/\*.*?\*/)|(?P<string>"(?:\\.|[^"\\])*")|(?P<ident>[A-Za-z_]\w*)|(?P<number>[0-9][0-9A-Za-z_.]*)|(?P<punct><<=|>>=|\+=|-=|\*=|/=|%=|&=|\|=|\^=|==|!=|<=|>=|&&|\|\||<<|>>|->|.)',re.S)
def lex(src):
    out=[]; i=0
    while i<len(src):
        m=_TOK.match(src,i)
        if not m: raise TransformError('lex failure')
        out.append((m.lastgroup,m.group())); i=m.end()
    return out
def sig(ts): return [(k,t) for k,t in ts if k not in ('ws','comment')]
def match(ts,i):
    op=ts[i][1]; cl={'(':')','[':']','{':'}'}[op]; d=0
    for j in range(i,len(ts)):
        if ts[j][0]=='punct':
            if ts[j][1]==op: d+=1
            elif ts[j][1]==cl:
                d-=1
                if d==0: return j
    raise TransformError('unbalanced '+op)
def typed(v,ty='i32'): return '(%d as %s)'%(v,ty)
def ns(ts,i):
    while i<len(ts) and ts[i][0] in ('ws','comment'): i+=1
    return i
def ev(ts,k):
    s=' '.join(t for _,t in sig(ts))
    s=re.sub(r'\bas\s+(?:i|u)(?:8|16|32|64)\b','',s)
    for name,val in k.items():
        if name in s: s=re.sub(r'\b'+re.escape(name)+r'\b',str(val),s)
    if re.search(r'[A-Za-z_]',s) or '**' in s or not re.fullmatch(r'[0-9\s()+\-*/%&|^]+',s): return None
    try: return int(eval(s.replace('/','//'),{'__builtins__':{}},{}))
    except Exception: return None
def cons_end(ts,i):
    key=ts[i][1]; j=i+1; par=0
    while j<len(ts):
        if ts[j]==('punct','('): par+=1
        elif ts[j]==('punct',')'): par-=1
        elif ts[j]==('punct','{') and par==0: break
        j+=1
    if j>=len(ts): raise TransformError('missing block')
    e=match(ts,j)+1
    if key=='if':
        while True:
            m=ns(ts,e)
            if m>=len(ts) or ts[m]!=('ident','else'): break
            m=ns(ts,m+1)
            if m<len(ts) and ts[m]==('ident','if'): e=cons_end(ts,m)
            elif m<len(ts) and ts[m]==('punct','{'): e=match(ts,m)+1
            else: raise TransformError('bad else')
    return e
def split_items(ts):
    it=[]; pend=[]; i=0; n=len(ts)
    while i<n:
        if ts[i][0] in ('ws','comment'): pend.append(ts[i]); i+=1; continue
        st=i
        if ts[i][0]=='ident' and ts[i][1] in ('while','if'): e=cons_end(ts,i)
        elif ts[i]==('punct','{'): e=match(ts,i)+1
        else:
            j=i; d=0
            while j<n:
                if ts[j][0]=='punct':
                    c=ts[j][1]
                    if c in '([{': d+=1
                    elif c in ')]}': d-=1
                    elif c==';' and d==0: j+=1; break
                j+=1
            if j==0 or ts[j-1]!=('punct',';'): raise TransformError('unterminated statement')
            e=j
        it.append(pend+ts[st:e]); pend=[]; i=e
    return it+([pend] if pend else [])
def decl_all(ts): return {s[i+1][1] for s in (sig(ts),) for i in range(len(s)-1) if s[i] in (('ident','let'),('ident','var')) and s[i+1][0]=='ident'}
def decl_top(items): return {s[1][1] for x in items for s in (sig(x),) if len(s)>1 and s[0] in (('ident','let'),('ident','var')) and s[1][0]=='ident'}
def var_types(ts): return {s[i+1][1]:s[i+3][1] for s in (sig(ts),) for i in range(len(s)-3) if s[i]==('ident','var') and s[i+1][0]=='ident' and s[i+2]==('punct',':') and s[i+3][0]=='ident'}
def assigned(ts):
    s=sig(ts)
    return {s[i+1][1] for i in range(len(s)-1) if s[i]==('ident','var') and s[i+1][0]=='ident'}|{s[i][1] for i in range(len(s)-1) if s[i][0]=='ident' and s[i][1] not in TYPES and s[i+1][0]=='punct' and s[i+1][1] in ('=','+=','-=','*=','/=','%=','++','--')}
def fold(ts,k):
    out=[]; i=0
    while i<len(ts):
        t=ts[i]
        if t[0]=='ident' and t[1] in INDEX:
            j=ns(ts,i+1)
            if j<len(ts) and ts[j]==('punct','['):
                c=match(ts,j); v=ev(ts[j+1:c],k)
                if v is not None:
                    out.append(t); out.extend(ts[i+1:j]); out.append(('punct','[')); out.append(('raw',typed(v))); out.append(('punct',']')); i=c+1; continue
        out.append(t); i+=1
    return out
def subst(ts,name,repl): return [('raw',repl) if k=='ident' and t==name else (k,t) for k,t in ts]
def rename(ts,m): return [(k,m.get(t,t)) if k=='ident' else (k,t) for k,t in ts]
def note(item,k):
    s=sig(item)
    if not s: return
    if s[0] in (('ident','let'),('ident','var')):
        if len(s)<2 or s[1][0]!='ident': return
        name=s[1][1]; ei=None; d=0
        for i in range(2,len(s)):
            if s[i][0]=='punct':
                if s[i][1] in '([{': d+=1
                elif s[i][1] in ')]}': d-=1
                elif s[i][1]=='=' and d==0: ei=i; break
        if ei is None: k[name]=None
        else:
            v=ev(s[ei+1:-1] if s[-1]==('punct',';') else s[ei+1:],k); k[name]=v if isinstance(v,int) else None
        return
    if len(s)>1 and s[0][0]=='ident' and s[1][0]=='punct' and s[1][1] in ('=','+=','-=','*=','/=','%=','++','--'):
        if s[1][1]=='=':
            v=ev(s[2:-1] if s[-1]==('punct',';') else s[2:],k); k[s[0][1]]=v if isinstance(v,int) else None
        else: k[s[0][1]]=None
def compound(item,k,vt,ctr):
    out=[]; i=0
    while i<len(item):
        if item[i]==('punct','{'):
            c=match(item,i); out.append(item[i]); out.extend(block(item[i+1:c],dict(k),vt,ctr)); out.append(item[c]); i=c+1
        else:
            j=i
            while j<len(item) and item[j]!=('punct','{'): j+=1
            out.extend(fold(item[i:j],k)); i=j
    for n in assigned(item): k[n]=None
    return out
def plain(item,wi,j,c,body,trail,k,vt,ctr):
    out=list(item[:wi])+item[wi:j+1]+block(body,dict(k),vt,ctr)+[item[c]]+list(trail)
    for n in assigned(body): k[n]=None
    return out
def while_item(item,k,vt,ctr):
    wi=ns(item,0); j=wi+1; par=0
    while j<len(item):
        if item[j]==('punct','('): par+=1
        elif item[j]==('punct',')'): par-=1
        elif item[j]==('punct','{') and par==0: break
        j+=1
    if j>=len(item): raise TransformError('while missing block')
    c=match(item,j); cond=item[wi+1:j]; body=item[j+1:c]; trail=item[c+1:]
    vi=ns(cond,0); oi=ns(cond,vi+1)
    var=cond[vi][1] if vi<len(cond) and cond[vi][0]=='ident' else None
    op=cond[oi][1] if oi<len(cond) and cond[oi] in (('punct','<'),('punct','<=')) else None
    items=split_items(body); si=-1
    for x in range(len(items)-1,-1,-1):
        if sig(items[x]): si=x; break
    step=None
    if var is not None and op is not None and si>=0:
        s=sig(items[si])
        if len(s)>=4 and s[0]==('ident',var) and s[1]==('punct','+=') and s[-1]==('punct',';'): step=ev(s[2:-1],k)
    ok=step is not None and step>0
    if ok:
        bi=items[:si]+items[si+1:]; bt=[x for y in bi for x in y]
        ok=not any(x[0]=='ident' and x[1] in ('break','continue') for x in sig(body)) and var not in assigned(bt)
    else: bi=[]; bt=[]
    start=k.get(var) if ok else None
    bound=ev(cond[oi+1:],k) if ok and oi<len(cond) else None
    ty=vt.get(var) if ok else None
    ok=ok and isinstance(start,int) and isinstance(bound,int) and ty in ITYPES
    if ok:
        top=decl_top(bi); alln=decl_all(bt); ok=not (top&(alln-top)) and var not in alln and not (top&(set(vt)-alln))
    n=0 if not ok else ((bound-start+step-1)//step if op=='<' else (bound-start)//step+1)
    if not ok or n>4096: return plain(item,wi,j,c,body,trail,k,vt,ctr)
    out=list(item[:wi])
    for it in range(n):
        ctr[0]+=1; mp={x:x+'__u%d'%ctr[0] for x in top}
        for a,b in mp.items():
            if a in vt: vt[b]=vt[a]
        e2=dict(k);e2[var]=start+it*step
        out.append(('raw','if true {\nlet '+var+':'+ty+'='+str(start+it*step)+';\n'))
        out.extend(block(rename(bt,mp),e2,vt,ctr))
        out.append(('raw','\n}\n'))
    out.append(('raw',var+'='+typed(start+n*step,ty)+';\n')); out.extend(trail); k[var]=None
    for x in assigned(bt): k[x]=None
    return out
def block(ts,k,vt,ctr):
    out=[]
    for it in split_items(ts):
        s=sig(it)
        if not s: out.extend(it)
        elif s[0]==('ident','while'): out.extend(while_item(it,k,vt,ctr))
        elif s[0]==('ident','if') or s[0]==('punct','{'): out.extend(compound(it,k,vt,ctr))
        else: out.extend(fold(it,k)); note(it,k)
    return out
def validate(ts):
    if any(k=='ident' and t=='while' for k,t in ts): raise TransformError('unbounded while remains')
    for i,(k,t) in enumerate(ts):
        if k!='ident' or t not in INDEX: continue
        j=ns(ts,i+1)
        if j<len(ts) and ts[j]==('punct','['):
            c=match(ts,j)
            if ev(ts[j+1:c],{}) is None: raise TransformError('dynamic private index '+t)
def find(ts,name):
    hs=[]
    for i in range(len(ts)):
        if ts[i]!=('ident','fn'): continue
        j=ns(ts,i+1)
        if j<len(ts) and ts[j]==('ident',name): hs.append((i,j))
    if len(hs)!=1: raise TransformError('expected exactly one fn '+name)
    fi,ni=hs[0]; j=ns(ts,ni+1)
    if j>=len(ts) or ts[j]!=('punct','('): raise TransformError('missing params')
    pc=match(ts,j); k=ns(ts,pc+1)
    if k>=len(ts) or ts[k]!=('punct','{'): raise TransformError('missing body')
    return fi,ni,j,pc,k,match(ts,k)
def split_params(ts):
    ps=[]; cur=[]; d=0
    for x in ts:
        if x[0]=='punct':
            c=x[1]
            if c in '([{': d+=1
            elif c in ')]}': d-=1
            elif c==',' and d==0: ps.append(cur); cur=[]; continue
        cur.append(x)
    ps.append(cur)
    return [''.join(t for _,t in p).strip() for p in ps if sig(p)]
def pname(d):
    m=re.match(r'[A-Za-z_]\w*',d); return m.group() if m else None
def ptype(d):
    m=re.search(r':\s*([A-Za-z_]\w*)',d); return m.group(1) if m else None
def transform(source):
    ts=lex(source); fi,ni,j,pc,k,bc=find(ts,'iterate')
    ps=split_params(ts[j+1:pc])
    if [(pname(d),ptype(d)) for d in ps]!=EXPECTED: raise TransformError('unexpected iterate params')
    body=ts[k+1:bc]; vt=var_types(body); names=[pname(d) for d in ps]
    keep=', '.join(d for d in ps if pname(d) not in ('ncols1','np'))
    sargs=', '.join(x for x in names if x not in ('ncols1','np')); args=', '.join(names)
    def build(nm,nc,npv):
        o=block(body,{'ncols1':nc,'np':npv},dict(vt),[0])
        validate(o)
        return 'fn %s(%s) {let ncols1:i32=%d;let np:i32=%d;\n%s}'%(nm,keep,nc,npv,''.join(t for _,t in o))
    g=list(ts[fi:bc+1]); g[ni-fi]=('ident','iterate_generic')
    disp=('\nfn iterate(%s){\nif ncols1==(4 as i32)&&np==(2 as i32){iterate_nc4_np2(%s);return;}\nif ncols1==(8 as i32)&&np==(1 as i32){iterate_nc8_np1(%s);return;}\niterate_generic(%s);\n}\n')%(', '.join(ps),sargs,sargs,args)
    return ''.join(t for _,t in ts[:fi])+build('iterate_nc4_np2',4,2)+'\n'+build('iterate_nc8_np1',8,1)+'\n'+''.join(t for _,t in g)+disp+''.join(t for _,t in ts[bc+1:])