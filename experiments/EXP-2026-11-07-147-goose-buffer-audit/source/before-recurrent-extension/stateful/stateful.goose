struct StatePlan { source:Tensor[7], dst:Tensor, ith:i64, nth:i64, op:i64, snapshots:i64, fp:f32[4] }
fn value_at(a:const u8[:],index:i64)->f32 {return from_bits(read32(a,mul_size(index,4)));}
fn set_value(a:u8[:],index:i64,v:f32){write32(a,mul_size(index,4),bits(v));}
fn gdn(p:const StatePlan&,q:const u8[:],k:const u8[:],v:const u8[:],g:const u8[:],beta:const u8[:],initial:const u8[:],dst:u8[:])->bool {
    let width=p.source[2].ne[0];let heads=p.source[2].ne[1];let tokens=p.source[2].ne[2];let seqs=p.source[2].ne[3];let snapshots=p.snapshots;
    assert(width>0&&heads>0&&tokens>=0&&seqs>0&&snapshots>=1&&p.nth>0&&p.ith>=0&&p.ith<p.nth);assert(p.dst.kind==0);for source in 6 {assert(p.source[source].kind==0);}
    assert(p.source[0].ne[0]==width&&p.source[1].ne[0]==width&&p.source[0].ne[1]>0&&p.source[1].ne[1]>0&&heads%p.source[0].ne[1]==0&&heads%p.source[1].ne[1]==0&&p.source[0].ne[3]>0&&p.source[1].ne[3]>0&&seqs%p.source[0].ne[3]==0&&seqs%p.source[1].ne[3]==0);
    assert((p.source[3].ne[0]==1||p.source[3].ne[0]==width)&&p.source[4].ne[0]==1);assert(p.source[5].ne[0]==width&&p.source[5].ne[1]==width&&p.source[5].ne[2]==heads&&p.source[5].ne[3]>=seqs);
    let square=mul_size(width,width);let scores=mul_size(mul_size(mul_size(width,heads),tokens),seqs);let snapshot_values=mul_size(mul_size(square,heads),seqs);assert(mul_size(add_size(scores,mul_size(snapshot_values,snapshots)),4)<=dst.len);
    let scale=(1 as f32)/sqrt32(width as f32);var task=p.ith;
    while task<mul_size(heads,seqs) {
        let head=task%heads;let seq=task/heads;let qhead=head%p.source[0].ne[1];let khead=head%p.source[1].ne[1];let qseq=seq/(seqs/p.source[0].ne[3]);let kseq=seq/(seqs/p.source[1].ne[3]);let statebase=scores+task*square;
        if tokens>0||snapshots==1 {for j in width {for i in width {let at=i*p.source[5].nb[0]+j*p.source[5].nb[1]+head*p.source[5].nb[2]+seq*p.source[5].nb[3];set_value(dst,statebase+j*width+i,load_at(p.source[5],initial,at));}}}
        for token in tokens {
            let qb=qhead*p.source[0].nb[1]+token*p.source[0].nb[2]+qseq*p.source[0].nb[3];let kb=khead*p.source[1].nb[1]+token*p.source[1].nb[2]+kseq*p.source[1].nb[3];let vb=head*p.source[2].nb[1]+token*p.source[2].nb[2]+seq*p.source[2].nb[3];let gb=head*p.source[3].nb[1]+token*p.source[3].nb[2]+seq*p.source[3].nb[3];let bb=head*p.source[4].nb[1]+token*p.source[4].nb[2]+seq*p.source[4].nb[3];let rate=load_at(p.source[4],beta,bb);
            for j in width {for i in width {let decay=exp32(load_at(p.source[3],g,gb+(if p.source[3].ne[0]==1 {0} else {i*p.source[3].nb[0]})));let at=statebase+j*width+i;set_value(dst,at,value_at(dst,at)*decay);}}
            for j in width {
                let predicted=vector_dot(p.dst,dst,(statebase+j*width)*4,4,p.source[1],k,kb,p.source[1].nb[0],width);let delta=(load_at(p.source[2],v,vb+j*p.source[2].nb[0])-predicted)*rate;
                for i in width {let at=statebase+j*width+i;let key=load_at(p.source[1],k,kb+i*p.source[1].nb[0]);set_value(dst,at,fma32(key,delta,value_at(dst,at)));}
            }
            for j in width {let result=vector_dot(p.dst,dst,(statebase+j*width)*4,4,p.source[0],q,qb,p.source[0].nb[0],width);set_value(dst,(seq*tokens*heads+token*heads+head)*width+j,result*scale);}
            let slot=tokens-1-token;
            if snapshots>1&&slot>0&&slot<snapshots {for i in square {set_value(dst,scores+slot*snapshot_values+task*square+i,value_at(dst,statebase+i));}}
        }
        task+=p.nth;
    }
    witness(OP_GATED_DELTA_NET,scores);return true;
}
fn state_entry(p:const StatePlan&,a0:const u8[:],a1:const u8[:],a2:const u8[:],a3:const u8[:],a4:const u8[:],a5:const u8[:],a6:const u8[:],dst:u8[:])->bool {
    validate(p.dst,dst.len);validate(p.source[0],a0.len);validate(p.source[1],a1.len);validate(p.source[2],a2.len);validate(p.source[3],a3.len);validate(p.source[4],a4.len);validate(p.source[5],a5.len);
    if p.op==OP_GATED_DELTA_NET {return gdn(p,a0,a1,a2,a3,a4,a5,dst);}
    if p.op==OP_SSM_SCAN {validate(p.source[6],a6.len);var scan:ScanPlan=default<ScanPlan>();scan.source=p.source;scan.ith=p.ith;scan.nth=p.nth;scan.snapshots=p.snapshots;assert(p.source[3].ne[0]==1||p.snapshots==1);return scan_entry(scan,a0,a1,a2,a3,a4,a5,a6,dst);}
    return false;
}
fn main(){var p:StatePlan=default<StatePlan>();let a:u8[1]=default<u8[1]>();var d:u8[1]=default<u8[1]>();let ok=state_entry(p,a,a,a,a,a,a,a,d);print(ok);}
