extern "fg143_ex_meta" fn meta(context:i64,node:i64,field:i64)->i64;
extern "fg143_ex_abort_value" fn abort_value(context:i64)->i64;
extern "fg143_ex_compute" fn compute(context:i64,node:i64);
extern "fg143_ex_fused" fn fused(context:i64,node:i64,weight_slot:i64);
extern "fg143_ex_abort_requested" fn abort_requested(context:i64)->bool;
extern "fg143_ex_abort_at" fn abort_at(context:i64,node:i64);
extern "fg143_ex_barrier" fn barrier(context:i64);
extern "fg143_ex_witness" fn witness(steps:i64,fusions:i64);
fn no_compute(op:i64)->bool {return op==OP_NONE||op==OP_RESHAPE||op==OP_TRANSPOSE||op==OP_VIEW||op==OP_PERMUTE;}
fn fuse_slot(context:i64,node:i64,count:i64,compute_mask:i64,output_mask:i64)->i64 {
    if node+1>=count||meta(context,node,0)!=OP_RMS_NORM||meta(context,node+1,0)!=OP_MUL {return -1;}
    let flags=meta(context,node,1);let next_flags=meta(context,node+1,1);
    if (flags&compute_mask)==0||(next_flags&compute_mask)==0||(flags&output_mask)!=0||meta(context,node,5)!=0||meta(context,node,6)!=1 {return -1;}
    let id=meta(context,node,2);var weight_slot:i64=-1;
    if meta(context,node+1,3)==id {weight_slot=1;}else if meta(context,node+1,4)==id {weight_slot=0;}
    if weight_slot<0 {return -1;}
    for dim in 4 {if meta(context,node,10+dim)!=meta(context,node+1,10+dim) {return -1;}}
    if meta(context,node,8)!=0||meta(context,node+1,7)!=0||meta(context,node+1,8+weight_slot)!=0||meta(context,node+1,14+weight_slot)!=meta(context,node,10)||meta(context,node+1,16+weight_slot)!=4 {return -1;}
    return weight_slot;
}
fn execute(context:i64,count:i64,ith:i64,nth:i64,use_ref:i64,can_fuse:i64,compute_mask:i64,output_mask:i64)->i64 {
    assert(count>=0&&nth>0&&ith>=0&&ith<nth);var node:i64=0;var completed:i64=0;var fusions:i64=0;
    while node<count&&abort_value(context)!=node {
        let op=meta(context,node,0);let flags=meta(context,node,1);
        if no_compute(op)||(flags&compute_mask)==0 {node+=1;continue;}
        let slot=if use_ref==0&&can_fuse!=0 {fuse_slot(context,node,count,compute_mask,output_mask)} else {-1};
        if slot>=0 {fused(context,node,slot);node+=1;completed+=2;fusions+=1;}else {compute(context,node);completed+=1;}
        if ith==0&&abort_requested(context) {abort_at(context,node+1);}
        if node+1<count {barrier(context);}node+=1;
    }
    barrier(context);witness(completed,fusions);return completed;
}
fn main(){let result=execute(0,0,0,1,0,0,1,1);print(result);}
