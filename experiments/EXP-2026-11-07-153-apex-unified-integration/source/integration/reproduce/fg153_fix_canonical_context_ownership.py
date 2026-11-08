from pathlib import Path
import subprocess,json,os
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');S=A/'source/canonical-storage-composed';B=A/'build/canonical-storage-composed';records=json.loads((B/'commands.json').read_text())
p=S/'context/canonical-context.goose';t=p.read_text();t=t.replace('return 1129270833','return 1129601073').replace('return 1111638065','return 1111577649').replace('return 1112838705','return 1430410289')
t=t.replace('s(r,4,n_vocab);s(r,5,allow);','s(r,4,n_vocab);s(r,5,0);s(r,6,allow);').replace('s(b,7,g(ctx,5))','s(b,7,g(ctx,6))')
old='if mode==15 {return rfree(a);}'
new='''if mode==15 {
        ensure(rkind(a)==kind_ub());
        var k:i64=20;
        while k<=33 {
            let v=g(a,k);
            if v!=0 {let ignored=vfree(v);}
            k=add(k,1);
        }
        return rfree(a);
    }'''
assert old in t;t=t.replace(old,new);p.write_text(t)
def run(label,args):
 q=subprocess.run([str(x) for x in args],cwd=S,capture_output=True,text=True,timeout=150)
 records.append({'label':label,'argv':[str(x) for x in args],'cwd':str(S),'exit_code':q.returncode});(B/'commands.json').write_text(json.dumps(records,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,flush=True)
 if q.returncode:print((q.stdout+q.stderr)[-5500:]);raise SystemExit(q.returncode)
 return q
run('ownership-context-emit',[A/'compiler/stage3-f849233b999c','-o',S/'context/canonical-context.c',p])
row=next(r for r in records if r['label']=='compile-context-goose_unit');run('ownership-context-build',row['argv'])
row=next(r for r in records if r['label']=='archive');run('ownership-context-archive',row['argv'])
p=S/'composed-fixture.cpp';t=p.read_text();t=t.replace('CHECK(check_model_composed()==0,"actual canonical MODEL storage");','CHECK(check_model_composed()==0,"actual canonical MODEL storage");\n    CHECK(fgc_live_allocs()==0,"MODEL ownership closure");')
t=t.replace('fgc_batch_h cb = fgc_batch_new(ctx);','fgc_batch_h cb = fgc_batch_new(ctx);\n        CHECK(fgc_record_kind(ctx)==FGC_KIND_CTX && fgc_record_kind(cb)==FGC_KIND_BATCH,"published context kinds");\n        CHECK(fgc_record_get(ctx,FGC_CTX_ALLOW_MIXED)==0,"published mixed key");')
p.write_text(t)
row=next(r for r in records if r['label']=='fixture-build');run('ownership-fixture-build',row['argv'])
row=next(r for r in records if r['label']=='fixture-link');run('ownership-fixture-link',row['argv'])
q=run('ownership-composed-boundary',[B/'composed-fixture'])
summary={'status':'PASS_MODEL_BATCH_REAL_GOOSE_STORAGE','source':str(S),'archive':str(B/'libfgc-apex-storage.a'),'fixes':['Distinct i64 bridge exports preserve typed foundation ABI','UBATCH destructor frees all fourteen owned vectors','Goose kind IDs match published schema','ALLOW_MIXED stored under published key6'],'negative_result':'Initial combined corpus found leaked UBATCH vectors;fixed in Goose,not hidden in primitive','stdout':q.stdout,'foundation_ownership':'zero live allocations and bytes after full composed corpus','full_runtime_activated':False,'model_loads':0,'production_changed':False,'remaining':'Actual canonical public-runtime adapters and other schema integrations'}
(A/'results/canonical-storage-composed.json').write_text(json.dumps(summary,indent=2)+'\n')
print('PASS actual MODEL + batch/context + Goose foundation,zero live foundation allocations')
