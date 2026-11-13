// ssm_conv.goose - Apex CUDA ssm_conv path.
// Port of ggml-cuda/ssm-conv.cu: short rolling-register kernel and the
// long-token split kernel with shared-memory staging.  Each Goose entry is
// one channel (threadIdx.x) of one batch/group/chunk block.  The long path
// stages the same row layout as the native shared tile and keeps the
// split_n_t chunk sequence, so token order and bounds are unchanged.
// Device/thread primitives are mechanical externs supplied by the adapter.
//
// Config indices are defined in fg153-gdn-abi.h.
extern "fg153_conv_cfg_i64" fn ci(c:i64,k:i64)->i64;
extern "fg153_conv_load" fn ld(p:i64,i:i64)->f32;
extern "fg153_conv_store" fn st(p:i64,i:i64,v:f32);
extern "fg153_conv_shload" fn shld(i:i64)->f32;
extern "fg153_conv_shstore" fn shst(i:i64,v:f32);
extern "fg153_conv_sync" fn sync();
extern "fg153_conv_tid" fn tid()->i64;
extern "fg153_conv_bidx" fn bid(axis:i64)->i64;
extern "fg153_conv_reject" fn reject(c:i64,code:i64)->i64;
extern "expf" fn exp32(x:f32)->f32;

fn bad(c:i64,code:i64)->i64 {
    let ignored=reject(c,code);
    return -1;
}
fn silu(x:f32)->f32 {return x/((1 as f32)+exp32(-x));}
fn supported(nc:i64)->bool {
    return nc==3||nc==4||nc==5||nc==7||nc==9||nc==15;
}
fn short_path(c:i64)->i64 {
    let nc=ci(c,4);
    let nr=ci(c,5);
    let nt=ci(c,6);
    let nth=ci(c,17);
    let wp=ci(c,1);
    let xp=ci(c,0);
    let yp=ci(c,3);
    let bp=ci(c,2);
    let xsc=ci(c,8);
    let wsc=ci(c,10);
    let ysc=ci(c,11);
    let yst=ci(c,12);
    let silu_on=ci(c,14);
    let seq=bid(0);
    let xbase=seq*ci(c,9);
    let ybase=seq*ci(c,13);
    let ch=bid(1)*nth+tid();
    if ch<0||ch>=nr {return 0;}
    var w:f32[15]=default<f32[15]>();
    var xb:f32[15]=default<f32[15]>();
    var j:i64=0;
    while j<nc {
        w[j]=ld(wp,ch*wsc+j);
        j=j+1;
    }
    let b=if bp!=0 {ld(bp,ch)} else {0 as f32};
    var i:i64=0;
    while i<nt {
        if i==0 {
            j=0;
            while j<nc {
                xb[j]=ld(xp,xbase+ch*xsc+j);
                j=j+1;
            }
        } else {
            xb[(i-1)%nc]=ld(xp,xbase+ch*xsc+i+nc-1);
        }
        var sum:f32=0 as f32;
        j=0;
        while j<nc {
            sum=sum+xb[(i+j)%nc]*w[j];
            j=j+1;
        }
        sum=sum+b;
        let y=if silu_on!=0 {silu(sum)} else {sum};
        st(yp,ybase+ch*ysc+i*yst,y);
        i=i+1;
    }
    return 0;
}
fn long_path(c:i64)->i64 {
    let nc=ci(c,4);
    let nr=ci(c,5);
    let nt=ci(c,6);
    let nth=ci(c,17);
    let split=ci(c,15);
    let wp=ci(c,1);
    let xp=ci(c,0);
    let yp=ci(c,3);
    let bp=ci(c,2);
    let xsc=ci(c,8);
    let wsc=ci(c,10);
    let ysc=ci(c,11);
    let yst=ci(c,12);
    let silu_on=ci(c,14);
    let seq=bid(0);
    let xbase=seq*ci(c,9);
    let ybase=seq*ci(c,13);
    let ch=bid(1)*nth+tid();
    if ch<0||ch>=nr {return 0;}
    let bz=bid(2);
    let chunk=bz*split;
    var local=split;
    let rem=nt-chunk;
    if rem<local {local=rem;}
    if local<=0 {return 0;}
    let ncols=nc-1+split;
    var j:i64=0;
    // Stage exactly one channel row of the native shared tile.
    while j<ncols {
        shst(tid()*ncols+j,ld(xp,xbase+ch*xsc+chunk+j));
        j=j+1;
    }
    sync();
    var w:f32[15]=default<f32[15]>();
    j=0;
    while j<nc {
        w[j]=ld(wp,ch*wsc+j);
        j=j+1;
    }
    let b=if bp!=0 {ld(bp,ch)} else {0 as f32};
    var i:i64=0;
    while i<local {
        var sum:f32=0 as f32;
        j=0;
        while j<nc {
            sum=sum+shld(tid()*ncols+i+j)*w[j];
            j=j+1;
        }
        sum=sum+b;
        let y=if silu_on!=0 {silu(sum)} else {sum};
        st(yp,ybase+ch*ysc+(chunk+i)*yst,y);
        i=i+1;
    }
    return 0;
}
fn entry(c:i64)->i64 {
    let nc=ci(c,4);
    let nr=ci(c,5);
    let nt=ci(c,6);
    let ns=ci(c,7);
    let mode=ci(c,16);
    let split=ci(c,15);
    let nth=ci(c,17);
    let cap=ci(c,18);
    if !supported(nc) {return bad(c,1);}
    if nr<=0 {return bad(c,2);}
    if nt<0 {return bad(c,3);}
    if ns<=0 {return bad(c,4);}
    if nth<=0||nth>128 {return bad(c,5);}
    if ci(c,0)==0||ci(c,1)==0||ci(c,3)==0 {return bad(c,6);}
    if ci(c,14)!=0&&ci(c,14)!=1 {return bad(c,7);}
    if mode==0 {
        if nr%nth!=0 {return bad(c,8);}
        return short_path(c);
    }
    if mode==1 {
        if split<=0||split>64 {return bad(c,9);}
        if nth*(nc-1+split)>cap {return bad(c,10);}
        return long_path(c);
    }
    return bad(c,11);
}
fn main(){let r=entry(0);print(r);}
