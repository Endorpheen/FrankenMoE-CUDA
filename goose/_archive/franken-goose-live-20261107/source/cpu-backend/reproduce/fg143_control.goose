extern "fg143_ctl_meta" fn meta(context:i64,node:i64,field:i64)->i64;
extern "fg143_ctl_extra_work" fn extra_work(context:i64,node:i64,threads:i64)->i64;
extern "fg143_ctl_tiled_work" fn tiled_work(context:i64,node:i64,tasks:i64)->i64;
extern "fg143_ctl_goose_work" fn goose_work(context:i64,node:i64,tasks:i64)->i64;
extern "fg143_ctl_extra_compute" fn extra_compute(context:i64)->bool;
extern "fg143_ctl_witness" fn witness(kind:i64);
fn add(a:i64,b:i64)->i64 {assert(a>=0&&b>=0&&a<=9223372036854775807-b);return a+b;}
fn mul(a:i64,b:i64)->i64 {assert(a>=0&&b>=0);if a==0||b==0 {return 0;}assert(a<=9223372036854775807/b);return a*b;}
fn maximum(a:i64,b:i64)->i64 {return if a>b {a}else {b};}
fn minimum(a:i64,b:i64)->i64 {return if a<b {a}else {b};}
fn align_size(a:i64,alignment:i64)->i64 {assert(alignment>0);return mul(add(a,alignment-1)/alignment,alignment);}
fn shape(c:i64,n:i64,s:i64,axis:i64)->i64 {assert(s>=0&&s<3&&axis>=0&&axis<4);return meta(c,n,20+4*s+axis);}
fn elements(c:i64,n:i64,s:i64)->i64 {var total:i64=1;for axis in 4 {total=mul(total,shape(c,n,s,axis));}return total;}
fn empty(c:i64,n:i64)->bool {for axis in 4 {if meta(c,n,10+axis)==0 {return true;}}return false;}
fn tasks(c:i64,n:i64,threads:i64)->i64 {
    assert(threads>0);if empty(c,n) {return 1;}
    let op=meta(c,n,0);let sub=meta(c,n,8);
    if op==OP_UNARY {
        if sub==UNARY_OP_GELU||sub==UNARY_OP_GELU_ERF||sub==UNARY_OP_GELU_QUICK||sub==UNARY_OP_SILU||sub==UNARY_OP_XIELU {return threads;}
        assert(sub>=0&&sub<meta(c,n,60));return 1;
    }
    if op==OP_GLU {assert(sub>=0&&sub<meta(c,n,59));return threads;}
    if op==OP_SOFT_MAX {return minimum(threads,mul(mul(shape(c,n,0,1),shape(c,n,0,2)),shape(c,n,0,3)));}
    if op==OP_RWKV_WKV6||op==OP_RWKV_WKV7||op==OP_GATED_LINEAR_ATTN {return minimum(threads,shape(c,n,1,1));}
    if op==OP_MAP_CUSTOM1||op==OP_MAP_CUSTOM2||op==OP_MAP_CUSTOM3||op==OP_CUSTOM {let requested=meta(c,n,7);let count=if requested== -1 {threads}else {minimum(requested,threads)};assert(count>0);return count;}
    if SINGLE_THREAD_OPS {return 1;}
    if PARALLEL_OPS {return threads;}
    assert(false);return 0;
}
fn row_size(c:i64,n:i64,count:i64)->i64 {let block_width=meta(c,n,42);assert(block_width>0&&count>=0&&count%block_width==0);return mul(count/block_width,meta(c,n,41));}
fn workspace(c:i64,n:i64,threads:i64,count:i64)->i64 {
    let extra=extra_work(c,n,threads);var cur:i64=0;
    if extra>=0 {cur=extra;}else {
        let op=meta(c,n,0);let kind=meta(c,n,1);let x=meta(c,n,2);let y=meta(c,n,3);let dstwidth=meta(c,n,10);let width=shape(c,n,0,0);
        if op==OP_CPY||op==OP_DUP {
            if meta(c,n,5)!=0||(x==1&&y==30)||(x==30&&y==1)||(x==0&&y==26)||(x==26&&y==0) {cur=mul(mul(4,dstwidth),count);}
        }else if op==OP_ADD||op==OP_ADD_ID||op==OP_ADD1 {
            if meta(c,n,6)!=0 {cur=mul(mul(4,width),count);}
        }else if op==OP_ACC {if meta(c,n,6)!=0 {cur=mul(mul(4,shape(c,n,1,0)),count);}}
        else if op==OP_COUNT_EQUAL {cur=mul(meta(c,n,43),count);}
        else if op==OP_MUL_MAT {
            if y!=meta(c,n,40) {cur=row_size(c,n,elements(c,n,1));}
            cur=add(align_size(cur,64),tiled_work(c,n,count));
        }else if op==OP_MUL_MAT_ID {
            if y!=meta(c,n,40) {cur=add(row_size(c,n,elements(c,n,1)),8);}
            let experts=shape(c,n,0,2);cur=add(cur,add(mul(experts,8),8));
            cur=add(cur,add(mul(mul(mul(experts,shape(c,n,2,0)),shape(c,n,2,1)),meta(c,n,54)),8));
            cur=add(cur,mul(meta(c,n,53),add(experts,1)));cur=add(align_size(cur,64),tiled_work(c,n,count));
        }else if op==OP_OUT_PROD {if meta(c,n,6)!=0||x==1 {cur=mul(mul(4,width),count);}}
        else if op==OP_SET_ROWS {if x==1&&kind!=1 {cur=mul(mul(4,width),count);}}
        else if op==OP_SOFT_MAX||op==OP_ROPE||op==OP_ROPE_BACK {cur=mul(mul(4,dstwidth),count);}
        else if op==OP_CONV_TRANSPOSE_1D {
            assert(shape(c,n,0,3)==1&&shape(c,n,1,2)==1&&shape(c,n,1,3)==1);
            assert((x==1||x==30||x==0)&&y==0);let bytes=if x==0 {4}else {2};
            cur=mul(bytes,add(mul(mul(width,shape(c,n,0,1)),shape(c,n,0,2)),mul(shape(c,n,1,0),shape(c,n,1,1))));
        }else if op==OP_CONV_2D||op==OP_CONV_3D {cur=meta(c,n,58);}
        else if op==OP_CONV_TRANSPOSE_2D {assert((x==0||x==1)&&y==0);cur=mul(meta(c,n,44),add(elements(c,n,0),elements(c,n,1)));}
        else if op==OP_TOP_K {cur=mul(mul(4,width),count);}
        else if op==OP_FLASH_ATTN_EXT {
            let heads=shape(c,n,0,2);let dk=shape(c,n,1,0);let dv=shape(c,n,2,0);let tq=meta(c,n,55);let tk=meta(c,n,56);
            let prefill=mul(mul(4,count),add(add(mul(tq,dk),mul(mul(2,tq),tk)),add(mul(tq,dv),add(mul(tk,dv),mul(tk,dk)))));
            let decode=mul(4,add(mul(mul(heads,count),add(2,dv)),mul(count,add(dk,mul(2,dv)))));cur=maximum(prefill,decode);
        }else if op==OP_FLASH_ATTN_BACK {
            if y==0||y==1||y==30 {let dimension=maximum(width,align_size(shape(c,n,1,1),meta(c,n,57)));cur=mul(mul(mul(16,dimension),count),1);}
        }else if op==OP_CROSS_ENTROPY_LOSS {cur=mul(meta(c,n,43),add(count,mul(width,count)));}
        else if op==OP_GATED_DELTA_NET {let width_v=shape(c,n,2,0);let per=add(width_v,if sub_snapshot(c,n)>1 {mul(width_v,width_v)}else {0});cur=mul(mul(per,4),count);}
        else if op==OP_LIGHTNING_INDEXER {cur=mul(mul(4,shape(c,n,1,0)),count);}
        else {assert(op!=OP_COUNT);}
    }
    return maximum(cur,goose_work(c,n,count));
}
fn sub_snapshot(c:i64,n:i64)->i64 {return meta(c,n,8);}
fn plan(c:i64,count:i64,requested:i64,out:i64[:]) {
    assert(count>=0&&out.len>=2);var threads=requested;
    if threads<=0 {let pool=meta(c,-1,50);threads=if pool>0 {pool}else {meta(c,-1,51)};}
    if meta(c,-1,52)!=0 {threads=1;}assert(threads>0);
    var max_tasks:i64=1;var work:i64=0;
    for node in count {let nt=tasks(c,node,threads);assert(nt>0);max_tasks=maximum(max_tasks,nt);work=maximum(work,workspace(c,node,threads,nt));}
    if work>0 {work=add(work,mul(meta(c,-1,53),threads));}
    out[0]=work;out[1]=minimum(max_tasks,threads);witness(0);
}
DISPATCH_DECLARATIONS
fn dispatch(c:i64) {
    let op=meta(c,0,0);if op==OP_NONE||empty(c,0) {return;}
    if extra_compute(c) {return;}
    DISPATCH_BODY
    witness(2);
}
extern "fg143_job_get" fn job_get(context:i64,field:i64)->i64;
extern "fg143_job_set" fn job_set(context:i64,field:i64,value:i64);
extern "fg143_job_create" fn job_create(context:i64,threads:i64);
extern "fg143_job_free" fn job_free(context:i64);
extern "fg143_job_team" fn job_team(context:i64,threads:i64);
extern "fg143_job_barrier" fn job_barrier();
extern "fg143_job_place" fn job_place(context:i64,thread:i64);
extern "fg143_job_compute" fn job_compute(context:i64,thread:i64);
extern "fg143_job_clear_affinity" fn job_clear_affinity();
fn job_worker(c:i64,single:bool) {
    let thread=if single {0}else {job_get(c,10)};
    let actual=if single {1}else {job_get(c,11)};
    assert(actual>0&&thread>=0&&thread<actual&&actual<=job_get(c,4));
    if thread==0 {job_set(c,5,actual);}
    if !single {job_barrier();}
    if !single {job_place(c,thread);}
    job_compute(c,thread);witness(4);
}
fn graph_job(c:i64,out:i64[:]) {
    assert(out.len>=1&&job_get(c,0)!=0);let requested=job_get(c,1);
    assert(requested>0&&job_get(c,2)>=0&&(job_get(c,2)==0||job_get(c,3)!=0));
    let created=job_get(c,6)==0;
    if created {job_create(c,requested);}else {
        job_set(c,0,0);job_set(c,1,0);job_set(c,2,0);job_set(c,3,-1);job_set(c,4,job_get(c,12));
    }
    assert(job_get(c,6)!=0&&requested<=job_get(c,4));
    if requested>1 {job_team(c,requested);}else {job_worker(c,true);}
    job_clear_affinity();let status=job_get(c,7);
    if created {job_free(c);}out[0]=status;witness(3);
}

fn entry(mode:i64,c:i64,count:i64,requested:i64,out:i64[:]) {
    if mode==0 {plan(c,count,requested,out);}
    else if mode==1 {assert(out.len>=1);out[0]=tasks(c,0,requested);witness(1);}
    else if mode==2 {dispatch(c);}
    else if mode==3 {graph_job(c,out);}
    else {assert(mode==4);job_worker(c,false);}
}
fn main(){var out:i64[2]=default<i64[2]>();entry(0,0,0,1,out);}
