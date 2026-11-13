// gdn_recurrent.goose - Apex CUDA gated-delta-net recurrence.
// Port of the scalar recurrence from ggml-cuda/gated_delta_net.cu
// (gated_delta_net_cuda, KDA and non-KDA, keep_rs_t snapshot path).
// One Goose entry is evaluated per state column; all state rows for that
// column live in a fixed register file and are written back in place.
// Device/thread primitives are mechanical externs supplied by the adapter.
//
// Config indices are defined in fg153-gdn-abi.h.
extern "fg153_gdn_cfg_i64" fn ci(c:i64,k:i64)->i64;
extern "fg153_gdn_cfg_f32" fn cf(c:i64,k:i64)->f32;
extern "fg153_gdn_load" fn ld(p:i64,i:i64)->f32;
extern "fg153_gdn_store" fn st(p:i64,i:i64,v:f32);
extern "fg153_gdn_tid" fn tid()->i64;
extern "fg153_gdn_bidx" fn bid(axis:i64)->i64;
extern "fg153_gdn_reject" fn reject(c:i64,code:i64)->i64;
extern "expf" fn exp32(x:f32)->f32;

fn bad(c:i64,code:i64)->i64 {
    let ignored=reject(c,code);
    return -1;
}
// Binary XOR tree matching warp_reduce_sum<width> (shfl_xor) order.
fn reduce(part:f32[8],width:i64)->f32 {
    var off:i64=width/2;
    while off>0 {
        var i:i64=0;
        while i<width {
            if (i&off)==0 {
                let a=part[i];
                let b=part[i+off];
                let v=a+b;
                part[i]=v;
                part[i+off]=v;
            }
            i=i+1;
        }
        off=off/2;
    }
    return part[0];
}
fn entry(c:i64)->i64 {
    let Sv=ci(c,28);
    let H=ci(c,8);
    let nt=ci(c,9);
    let ns=ci(c,10);
    let neq=ci(c,20);
    let rq3=ci(c,21);
    let K=ci(c,23);
    let kda=ci(c,24);
    let keep=ci(c,25);
    let cta=ci(c,26);
    let stride=ci(c,22);
    if !(Sv==16||Sv==32||Sv==64||Sv==128) {return bad(c,1);}
    if H<=0 {return bad(c,2);}
    if nt<0 {return bad(c,3);}
    if ns<=0 {return bad(c,4);}
    if neq<=0 {return bad(c,5);}
    if rq3<=0 {return bad(c,6);}
    if K<1 {return bad(c,7);}
    if cta<=0||cta>128 {return bad(c,8);}
    if ci(c,0)==0||ci(c,1)==0||ci(c,2)==0||ci(c,3)==0||ci(c,4)==0 {return bad(c,9);}
    if ci(c,5)==0||ci(c,6)==0||ci(c,7)==0 {return bad(c,10);}
    if keep!=0&&K>1&&stride<=0 {return bad(c,11);}
    if kda!=0&&kda!=1 {return bad(c,12);}
    if keep!=0&&keep!=1 {return bad(c,13);}
    let h=bid(0);
    let seq=bid(1);
    if h<0||h>=H {return bad(c,14);}
    if seq<0||seq>=ns {return bad(c,15);}
    let col=bid(2)*cta+tid();
    if col<0||col>=Sv {return 0;}
    let L=if Sv>=32 {8}else{Sv/4};
    let rows=Sv/L;
    let q=ci(c,0);
    let k=ci(c,1);
    let v=ci(c,2);
    let g=ci(c,3);
    let beta=ci(c,4);
    let state_in=ci(c,5);
    let dst=ci(c,6);
    let state_out=ci(c,7);
    let sq1=ci(c,11);
    let sq2=ci(c,12);
    let sq3=ci(c,13);
    let sv1=ci(c,14);
    let sv2=ci(c,15);
    let sv3=ci(c,16);
    let sb1=ci(c,17);
    let sb2=ci(c,18);
    let sb3=ci(c,19);
    let scale=cf(c,0);
    var s:f32[128]=default<f32[128]>();
    var kreg:f32[128]=default<f32[128]>();
    var qreg:f32[128]=default<f32[128]>();
    var part:f32[8]=default<f32[8]>();
    let inbase=seq*H*Sv*Sv+h*Sv*Sv+col*Sv;
    var i:i64=0;
    while i<Sv {
        s[i]=ld(state_in,inbase+i);
        i=i+1;
    }
    let iq1=h%neq;
    let iq3=seq/rq3;
    var t:i64=0;
    while t<nt {
        let qb=iq3*sq3+t*sq2+iq1*sq1;
        let vb=seq*sv3+t*sv2+h*sv1;
        let gb=seq*sb3+t*sb2+h*sb1;
        let beta_val=ld(beta,gb);
        i=0;
        while i<Sv {
            kreg[i]=ld(k,qb+i);
            qreg[i]=ld(q,qb+i);
            i=i+1;
        }
        if kda==0 {
            let gv=exp32(ld(g,gb));
            var lane:i64=0;
            while lane<L {
                var acc:f32=0 as f32;
                var r:i64=0;
                while r<rows {
                    let idx=r*L+lane;
                    acc=acc+s[idx]*kreg[idx];
                    r=r+1;
                }
                part[lane]=acc;
                lane=lane+1;
            }
            let kv=reduce(part,L);
            let delta=(ld(v,vb+col)-gv*kv)*beta_val;
            lane=0;
            while lane<L {
                var acc:f32=0 as f32;
                var r:i64=0;
                while r<rows {
                    let idx=r*L+lane;
                    s[idx]=gv*s[idx]+kreg[idx]*delta;
                    acc=acc+s[idx]*qreg[idx];
                    r=r+1;
                }
                part[lane]=acc;
                lane=lane+1;
            }
            let attn=reduce(part,L);
            st(dst,((seq*nt+t)*H+h)*Sv+col,attn*scale);
        } else {
            var lane:i64=0;
            while lane<L {
                var acc:f32=0 as f32;
                var r:i64=0;
                while r<rows {
                    let idx=r*L+lane;
                    acc=acc+exp32(ld(g,gb*Sv+idx))*s[idx]*kreg[idx];
                    r=r+1;
                }
                part[lane]=acc;
                lane=lane+1;
            }
            let kv=reduce(part,L);
            let delta=(ld(v,vb+col)-kv)*beta_val;
            lane=0;
            while lane<L {
                var acc:f32=0 as f32;
                var r:i64=0;
                while r<rows {
                    let idx=r*L+lane;
                    s[idx]=exp32(ld(g,gb*Sv+idx))*s[idx]+kreg[idx]*delta;
                    acc=acc+s[idx]*qreg[idx];
                    r=r+1;
                }
                part[lane]=acc;
                lane=lane+1;
            }
            let attn=reduce(part,L);
            st(dst,((seq*nt+t)*H+h)*Sv+col,attn*scale);
        }
        if keep!=0 {
            let slot=nt-1-t;
            if slot>=0&&slot<K {
                let ob=slot*stride+(seq*H+h)*Sv*Sv+col*Sv;
                i=0;
                while i<Sv {
                    st(state_out,ob+i,s[i]);
                    i=i+1;
                }
            }
        }
        t=t+1;
    }
    if keep==0 {
        let ob=(seq*H+h)*Sv*Sv+col*Sv;
        i=0;
        while i<Sv {
            st(state_out,ob+i,s[i]);
            i=i+1;
        }
    }
    return 0;
}
fn main(){let r=entry(0);print(r);}
