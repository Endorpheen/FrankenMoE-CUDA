// argsort_moe.goose - Apex expert selection: bitonic argsort / top-k.
// Port of ggml-cuda/argsort.cu k_argsort_f32_i32 (bitonic network) and the
// bitonic branch of ggml-cuda/top-k.cu top_k_argsort_cuda.
//
// One Goose entry is one phase of one block.  The device wrapper in the
// adapter runs the unchanged native stage order (init, all (k,j) stages,
// copy) and calls this entry once per phase; host differential does the same
// through per-thread calls.  Shared indices are 32-bit, the launch block is
// min(ncols_pad, 1024), one block per row.
//
// Config indices are defined in fg153-moe-abi.h.
extern "fg153_moe_cfg_i64" fn ci(c:i64,k:i64)->i64;
extern "fg153_moe_load" fn ld(p:i64,i:i64)->f32;
extern "fg153_moe_store_i64" fn sti(p:i64,i:i64,v:i64);
extern "fg153_moe_shload" fn shld(i:i64)->i64;
extern "fg153_moe_shstore" fn shst(i:i64,v:i64);
extern "fg153_moe_tid" fn tid()->i64;
extern "fg153_moe_bdim" fn bdim()->i64;
extern "fg153_moe_bidx" fn bid(a:i64)->i64;
extern "fg153_moe_reject" fn reject(c:i64,code:i64)->i64;

fn bad(c:i64,code:i64)->i64 {
    let ignored=reject(c,code);
    return -1;
}
fn entry(c:i64)->i64 {
    let xp=ci(c,0);
    let dst=ci(c,1);
    let ncols=ci(c,2);
    let npad=ci(c,3);
    let outcols=ci(c,5);
    let order=ci(c,6);
    let phase=ci(c,7);
    let kind=ci(c,11);
    let dststride=ci(c,12);
    if xp==0||dst==0 {return bad(c,1);}
    if ncols<=0||npad<ncols||npad>12288 {return bad(c,2);}
    if outcols<1||outcols>ncols {return bad(c,3);}
    if dststride<outcols {return bad(c,4);}
    if order!=0&&order!=1 {return bad(c,5);}
    if kind!=0&&kind!=1 {return bad(c,6);}
    if kind==0&&outcols!=ncols {return bad(c,7);}
    let row=bid(0);
    if row<0 {return bad(c,8);}
    let t=tid();
    let bd=bdim();
    if bd<=0 {return bad(c,9);}
    if phase==0 {
        var col:i64=t;
        while col<npad {
            shst(col,col);
            col=col+bd;
        }
        return 0;
    }
    if phase==1 {
        let kk=ci(c,8);
        let jj=ci(c,9);
        if kk<2||jj<1||jj>=kk {return bad(c,10);}
        var col:i64=t;
        while col<npad {
            var ixj:i64=col;
            if (col&jj)==0 {ixj=col+jj;} else {ixj=col-jj;}
            if ixj>col {
                let a=shld(col);
                let b=shld(ixj);
                var swap:bool=false;
                if (col&kk)==0 {
                    if a>=ncols {
                        swap=true;
                    } else {
                        if b<ncols {
                            if order==0 {
                                if ld(xp,row*ncols+a)>ld(xp,row*ncols+b) {swap=true;}
                            } else {
                                if ld(xp,row*ncols+a)<ld(xp,row*ncols+b) {swap=true;}
                            }
                        }
                    }
                } else {
                    if b>=ncols {
                        swap=true;
                    } else {
                        if a<ncols {
                            if order==0 {
                                if ld(xp,row*ncols+a)<ld(xp,row*ncols+b) {swap=true;}
                            } else {
                                if ld(xp,row*ncols+a)>ld(xp,row*ncols+b) {swap=true;}
                            }
                        }
                    }
                }
                if swap {
                    shst(col,b);
                    shst(ixj,a);
                }
            }
            col=col+bd;
        }
        return 0;
    }
    if phase==2 {
        var col:i64=t;
        while col<outcols {
            let idx=shld(col);
            sti(dst,row*dststride+col,idx);
            col=col+bd;
        }
        return 0;
    }
    return bad(c,11);
}
fn main(){let r=entry(0);print(r);}
