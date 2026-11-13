struct Row { i1:i32, i2:i32 }
struct Plan {
    context:i64, ith:i64, nth:i64, experts:i64, ids_per_token:i64, tokens:i64,
    need_quant:i64, block_size:i64, block_bytes:i64,
    ne00:i64, ne01:i64, ne10:i64, ne11:i64, ne12:i64, ne13:i64,
    nb01:i64, nb02:i64, nb10:i64, nb11:i64, nb12:i64, nb13:i64,
    dst_nb1:i64, dst_nb2:i64, row_size:i64, source_contiguous:i64,
    x_base:i64, y_base:i64, w_base:i64, dst_base:i64,
    disable_chunking:i64, weight_row_bytes:i64
}
extern "fg_moe_begin" fn begin(context:i64);
extern "fg_moe_quant" fn quantize(context:i64,src:i64,dst:i64,n:i64);
extern "fg_moe_read_id" fn read_id(context:i64,token:i64,slot:i64)->i32;
extern "fg_moe_reset" fn reset(context:i64,expert:i64,value:i32);
extern "fg_moe_barrier" fn barrier(context:i64);
extern "fg_moe_try_tiled" fn try_tiled(context:i64,expert:i64,count:i64)->bool;
extern "fg_moe_matrix" fn matrix(context:i64,weights:i64,activation:i64,out:i64,width:i64,rows:i64);
extern "fg_moe_claim" fn claim(context:i64,expert:i64)->i32;
fn minimum(a:i64,b:i64)->i64 {return if a<b {a} else {b};}
fn goose_cpu_moe(p:const Plan&,counts:i64[:],rows:Row[:]) {
    assert(p.nth>0&&p.ith>=0&&p.ith<p.nth&&counts.len>=p.experts);
    assert(rows.len>=p.experts*p.ids_per_token*p.tokens);
    begin(p.context);
    if p.need_quant!=0 {
        let nw1=p.row_size;
        let nw2=nw1*p.ne11;
        let nw3=nw2*p.ne12;
        let first=(p.ith*p.ne10/p.block_size)/p.nth;
        let last=((p.ith+1)*p.ne10/p.block_size)/p.nth;
        for sample in p.ne13 {for token in p.ne12 {for slot in p.ne11 {
            let src=p.y_base+sample*p.nb13+token*p.nb12+slot*p.nb11+first*p.block_size*p.nb10;
            let dst=p.w_base+sample*nw3+token*nw2+slot*nw1+first*p.block_bytes;
            quantize(p.context,src,dst,(last-first)*p.block_size);
        }}}
    }
    if p.ith==0 {
        for expert in p.experts {counts[expert]=0;}
        for token in p.tokens {for slot in p.ids_per_token {
            let expert=read_id(p.context,token,slot) as i64;
            assert(expert>=0&&expert<p.experts);
            let index=expert*p.ids_per_token*p.tokens+counts[expert];
            rows[index]=Row{slot as! i32,token as! i32};
            counts[expert]+=1;
        }}
    }
    var expert=p.ith;
    while expert<p.experts {reset(p.context,expert,p.nth as! i32);expert+=p.nth;}
    barrier(p.context);
    expert=0;
    while expert<p.experts {
        let n1=counts[expert];
        if n1>0 {
            if !try_tiled(p.context,expert,n1) {
                let nr0=p.ne01;
                var chunk:i64=16;
                if nr0==1||n1==1 {chunk=64;}
                var nc0=(nr0+chunk-1)/chunk;
                var nc1=(n1+chunk-1)/chunk;
                if nc0*nc1<p.nth*4||p.disable_chunking!=0 {
                    nc0=if nr0>n1 {p.nth} else {1};
                    nc1=if nr0>n1 {1} else {p.nth};
                }
                let dr0=(nr0+nc0-1)/nc0;
                let dr1=(n1+nc1-1)/nc1;
                var current=p.ith;
                while current<nc0*nc1 {
                    let r0=dr0*(current%nc0);
                    let r1=dr1*(current/nc0);
                    let end0=minimum(r0+dr0,nr0);
                    let end1=minimum(r1+dr1,n1);
                    var block1=r1;
                    while block1<end1 {
                        var block0=r0;
                        while block0<end0 {
                            var row=block1;
                            while row<minimum(block1+16,end1) {
                                let mapping=rows[expert*p.ids_per_token*p.tokens+row];
                                let slot=mapping.i1 as i64;
                                let token=mapping.i2 as i64;
                                let input_slot=slot%p.ne11;
                                let y=if p.source_contiguous!=0||p.need_quant!=0 {
                                    p.w_base+(input_slot+token*p.ne11)*p.row_size
                                } else {p.w_base+input_slot*p.nb11+token*p.nb12};
                                let out=p.dst_base+slot*p.dst_nb1+token*p.dst_nb2+block0*4;
                                let count=minimum(block0+16,end0)-block0;
                                let x=p.x_base+expert*p.nb02+block0*p.nb01;
                                if p.nb01==p.weight_row_bytes {
                                    matrix(p.context,x,y,out,p.ne00,count);
                                } else {
                                    for k in count {matrix(p.context,x+k*p.nb01,y,out+k*4,p.ne00,1);}
                                }
                                row+=1;
                            }
                            block0+=16;
                        }
                        block1+=16;
                    }
                    if p.nth>=nc0*nc1 {break;}
                    current=claim(p.context,expert) as i64;
                }
            }
        }
        expert+=1;
    }
}
fn main() {var p:Plan=default<Plan>();p.nth=1;var counts:i64[1]=default<i64[1]>();var rows:Row[1]=default<Row[1]>();goose_cpu_moe(p,counts,rows);print(counts[0]);}
