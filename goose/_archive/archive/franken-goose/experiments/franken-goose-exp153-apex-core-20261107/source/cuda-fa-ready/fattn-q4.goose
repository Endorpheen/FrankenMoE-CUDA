// worker-12: Apex FlashAttention vector kernel, Q4_0 K and Q4_0 V, FA on.
//
// Source-backed port of ggml-cuda/fattn-vec.cuh (flash_attn_ext_vec) plus the
// Q4_0 helpers from fattn-common.cuh (vec_dot_fattn_vec_KQ_q4_0,
// quantize_q8_1_to_shared, dequantize_V_q4_0).  The arithmetic, the online
// softmax order, the warp/lane mapping, the shared-memory layout and the
// output reduction are Goose.  Only raw device primitives (pointer loads,
// thread/block ids, warp shuffles, barriers, dp4a, shared slots) are extern.
//
// Template parameters of the native kernel are runtime values here so the
// same generated unit serves D in {64,128,256} and ncols in {1,2}.  The
// qualifier adapter marks every function that touches a device-only primitive
// as __device__ and keeps pure helpers __host__ __device__ so the host
// fixture can exercise the emitted mathematics.

struct V4 { v:f32[4] }
struct QReg { i:u32[2][2], ds:f32[2][4] }
struct VKQReg { v:f32[2][4][2] }

extern "dfa_ld_u8"  fn ld8(p:i64,o:i64)->u32;
extern "dfa_ld_u16" fn ld16(p:i64,o:i64)->u32;
extern "dfa_ld_u32" fn ld32(p:i64,o:i64)->u32;
extern "dfa_ld_f32" fn ldf(p:i64,o:i64)->f32;
extern "dfa_st_f32" fn stf(p:i64,o:i64,v:f32);
extern "dfa_half"   fn h2f(v:u32)->f32;
extern "dfa_tid"    fn tid(axis:i64)->i64;
extern "dfa_bid"    fn bid(axis:i64)->i64;
extern "dfa_gdim"   fn gdim(axis:i64)->i64;
extern "dfa_shfl_xor_f32" fn shflx(v:f32,m:i64)->f32;
extern "dfa_sync"   fn sync();
extern "dfa_syncwarp" fn syncwarp();
extern "dfa_dp4a"   fn dp4a(a:i32,b:i32,c:i32)->i32;
extern "dfa_sh_f32" fn shf(i:i64)->f32;
extern "dfa_sh_set_f32" fn shsetf(i:i64,v:f32);
extern "dfa_sh_u32" fn shu(i:i64)->u32;
extern "dfa_sh_set_u32" fn shsetu(i:i64,v:u32);
extern "dfa_neg_half_flt_max" fn neg_half_flt_max()->f32;
extern "dfa_f2i" fn f2i(x:f32)->i32;
extern "expf"   fn exp32(x:f32)->f32;
extern "tanhf"  fn tanh32(x:f32)->f32;
extern "powf"   fn pow32(a:f32,b:f32)->f32;
extern "roundf" fn round32(x:f32)->f32;
extern "fabsf"  fn abs32(x:f32)->f32;
extern "fmaxf"  fn max32(a:f32,b:f32)->f32;
extern "fminf"  fn min32(a:f32,b:f32)->f32;

fn max_i(a:i64,b:i64)->i64 { return if a<b {b} else {a}; }
fn min_i(a:i64,b:i64)->i64 { return if a<b {a} else {b}; }
fn maxf(a:f32,b:f32)->f32 { return max32(a,b); }
fn kq_offset()->f32 { return (3.0 as f32)*(0.6931 as f32); }

// ---------------------------------------------------------------------------
// Pure mathematics.  These are host+device and are the exact code the device
// kernel runs, so the fixture can compare them with an independent reference.
// ---------------------------------------------------------------------------

fn alibi_slope(max_bias:f32,h:i64,n_head_log2:i64,m0:f32,m1:f32)->f32 {
    if max_bias<=0.0 { return 1.0 as f32; }
    let base=if h<n_head_log2 {m0} else {m1};
    let exph=if h<n_head_log2 {h+1} else {2*(h-n_head_log2)+1};
    return pow32(base,exph as f32);
}

fn q4_group(q:u32,shift:i64,d:f32)->V4 {
    var out:V4=default<V4>();
    for l in 4 { out.v[l]=q4_nibble_at(q,l,shift,d); }
    return out;
}

fn q4_nibble_at(q:u32,l:i64,shift:i64,d:f32)->f32 {
    let b=(q >> ((8*l) as u32)) & (255 as u32);
    let nib=if shift==0 {b & (15 as u32)} else {b >> (4 as u32)};
    return d*(((nib as i32)-8) as f32);
}

fn q4_lane_term(kbytes:u32,kd:f32,q8:u32,dsx:f32,dsy:f32)->f32 {
    let v=kbytes & (0x0F0F0F0F as u32);
    let sumi=dp4a(v as! i32,q8 as! i32,0);
    return kd*((sumi as f32)*dsx-dsy);
}

fn q4_dot(Krow:i64,qa:u32,qb:u32,
          dsxa:f32,dsya:f32,dsxb:f32,dsyb:f32,
          D:i64,nthreads:i64,lane:i64)->f32 {
    var sum=0.0 as f32;
    var k0=0 as i64;
    while k0 < D/4 {
        let k=k0+(if nthreads==32 {lane} else {lane%nthreads});
        let ib=k/8;
        let iqs4=k%4;
        let shift=k&4;
        let raw=ld32(Krow,ib*18+2+iqs4*4);
        let wi=k0/nthreads;
        let q8=if wi==0 {qa} else {qb};
        let dsx=if wi==0 {dsxa} else {dsxb};
        let dsy=if wi==0 {dsya} else {dsyb};
        let kd=h2f(ld16(Krow,ib*18));
        let masked=(raw >> (shift as u32)) & (0x0F0F0F0F as u32);
        let sumi=dp4a(masked as! i32,q8 as! i32,0);
        sum+=kd*((sumi as f32)*dsx-dsy);
        k0+=nthreads;
    }
    return sum;
}

fn deq4(V:i64,i0:i64)->V4 {
    let ib=i0/32;
    let iqs=i0%16;
    let shift=(i0%32)/16;
    let base=V+ib*18;
    let d=h2f(ld16(base,0));
    let q=ld32(base,2+iqs);
    return q4_group(q,shift,d);
}

fn deq4_scalar(V:i64,i0:i64,l:i64)->f32 {
    let ib=i0/32;
    let iqs=i0%16;
    let shift=(i0%32)/16;
    let base=V+ib*18;
    let d=h2f(ld16(base,0));
    let q=ld32(base,2+iqs);
    return q4_nibble_at(q,l,shift,d);
}

fn q8_amax4(v0:f32,v1:f32,v2:f32,v3:f32)->f32 {
    return max32(max32(abs32(v0),abs32(v1)),max32(abs32(v2),abs32(v3)));
}
fn q8_sum4(v0:f32,v1:f32,v2:f32,v3:f32)->f32 {
    return v0+v1+v2+v3;
}
fn q8_lane_byte(tidx:i64)->i64 { return 16*tidx; }
fn q8_round(v:f32,d:f32)->i32 {
    if d==0.0 { return 0; }
    return f2i(round32(v/d));
}
fn q8_pack(v0:f32,v1:f32,v2:f32,v3:f32,d:f32)->u32 {
    let q0=(q8_round(v0,d) as u32) & (255 as u32);
    let q1=(q8_round(v1,d) as u32) & (255 as u32);
    let q2=(q8_round(v2,d) as u32) & (255 as u32);
    let q3=(q8_round(v3,d) as u32) & (255 as u32);
    return q0|(q1<<(8 as u32))|(q2<<(16 as u32))|(q3<<(24 as u32));
}

fn sm_max_new(cur:f32,x:f32)->f32 { return maxf(cur,x+kq_offset()); }
fn sm_scale(old_max:f32,new_max:f32)->f32 { return exp32(old_max-new_max); }
fn sm_prob(x:f32,m:f32)->f32 { return exp32(x-m); }
fn sm_mix(acc:f32,scale:f32,prob:f32)->f32 { return acc*scale+prob; }
fn sm_value(acc:f32,scale:f32,v:f32,w:f32)->f32 { return acc*scale+v*w; }
fn apply_mask(sum:f32,slope:f32,mask_bits:u32)->f32 { return sum+slope*h2f(mask_bits); }
fn apply_softcap(sum:f32,cap:f32)->f32 { if cap==0.0 { return sum; } return cap*tanh32(sum); }

// ---------------------------------------------------------------------------
// Device-only pieces.
// ---------------------------------------------------------------------------

fn warp_sum(v:f32,width:i64)->f32 {
    var x=v;
    var off=width/2;
    while off>0 { x+=shflx(x,off); off=off/2; }
    return x;
}
fn warp_max(v:f32)->f32 {
    var x=v;
    var off=16 as i64;
    while off>0 { x=maxf(x,shflx(x,off)); off=off/2; }
    return x;
}

fn quantize_q8(src:i64,scale:f32,ni:i64,tidx:i64,yq_base:i64,ds_base:i64,ioff:i64) {
    let active=(ni==32)||(tidx<ni);
    var v0=0.0 as f32;
    var v1=0.0 as f32;
    var v2=0.0 as f32;
    var v3=0.0 as f32;
    if active {
        v0=scale*ldf(src,q8_lane_byte(tidx));
        v1=scale*ldf(src,q8_lane_byte(tidx)+4);
        v2=scale*ldf(src,q8_lane_byte(tidx)+8);
        v3=scale*ldf(src,q8_lane_byte(tidx)+12);
    }
    var amax=q8_amax4(v0,v1,v2,v3);
    var sum=q8_sum4(v0,v1,v2,v3);
    amax=max32(amax,shflx(amax,4));
    sum+=shflx(sum,4);
    amax=max32(amax,shflx(amax,2));
    sum+=shflx(sum,2);
    amax=max32(amax,shflx(amax,1));
    sum+=shflx(sum,1);
    let d=amax/(127.0 as f32);
    let packed=q8_pack(v0,v1,v2,v3,d);
    shsetu(yq_base+ioff+tidx,packed);
    if (tidx%8)==0 && active {
        let di=(ioff+tidx)/8;
        shsetf(ds_base+2*di,d);
        shsetf(ds_base+2*di+1,sum);
    }
}

fn zero_q_tmp(j:i64,D:i64,tidx:i64) {
    var i0=0 as i64;
    while i0<D/4 {
        let i=i0+tidx;
        if i0+32<=D/4 || i<D/4 { shsetu(j*D+i,0); }
        i0+=32;
    }
    if tidx<D/32 {
        shsetf(j*D+D+2*tidx,0.0);
        shsetf(j*D+D+2*tidx+1,0.0);
    }
}

fn entry(Q:i64,K:i64,V:i64,mask:i64,sinks:i64,kvmax:i64,dst:i64,meta:i64,
         scale:f32,max_bias:f32,m0:f32,m1:f32,n_head_log2:i64,logit_softcap:f32,
         ne00:i64,ne01z:i64,ne02:i64,ne03:i64,
         nb01:i64,nb02:i64,nb03:i64,
         ne10:i64,ne11:i64,ne12:i64,ne13:i64,
         nb11:i64,nb12:i64,nb13:i64,
         nb21:i64,nb22:i64,nb23:i64,
         ne31:i64,ne32:i64,ne33:i64,
         nb31:i64,nb32:i64,nb33:i64,
         ncols:i64,has_mask:i64,has_sinks:i64,has_kvmax:i64,use_softcap:i64)->f32 {
    let D=ne00;
    let tidy=tid(1);
    let tidx=tid(0);
    let tid=tidy*32+tidx;
    let bidx=bid(0);
    let bidy=bid(1);
    let bidz=bid(2);
    let gdimy=gdim(1);
    let gdimx=gdim(0);
    let ic0=bidx*ncols;
    let sequence=bidz/ne02;
    let head=bidz-sequence*ne02;
    let gqa_ratio=ne02/ne12;
    let kg=head/gqa_ratio;

    var Qc:i64=Q+nb03*sequence+nb02*head+nb01*ic0;
    var Kc:i64=K+nb13*sequence+nb12*kg;
    var Vc:i64=V+nb23*sequence+nb22*kg;
    var maskc:i64=0 as i64;
    if has_mask!=0 { maskc=mask+nb33*(sequence%ne33)+nb31*ic0; }
    let slope=alibi_slope(max_bias,head,n_head_log2,m0,m1);

    let nwarp=4 as i64;
    let norig=32 as i64;
    let nKQ=if D/4<32 {D/4} else {32};
    let nV=nKQ;
    let vrows=4 as i64;
    let vcols=norig/nV;
    let nekq=ncols*D;
    let necomb=nwarp*vcols*D;
    let kqsize=if nekq>necomb {nekq} else {necomb};
    let maxbase=kqsize;
    let sumbase=kqsize+64;
    let nvkq=D/(2*nV);

    var qr:QReg=default<QReg>();
    var vkq:VKQReg=default<VKQReg>();
    var kq_reg:f32[2]=default<f32[2]>();
    var kq_max:f32[2]=default<f32[2]>();
    var kq_sum:f32[2]=default<f32[2]>();
    var kq_k:f32[2]=default<f32[2]>();
    var kq_max_new:f32[2]=default<f32[2]>();
    for j in 2 { kq_max[j]=neg_half_flt_max(); kq_sum[j]=0.0; }

    let ni=if D/4<32 {D/4} else {32};
    var j0=0 as i64;
    while j0<ncols {
        let j=j0+tidy;
        if !(j0+nwarp>ncols && j>=ncols) {
            let in_range=!(ncols>1 && ic0+j>=ne01z);
            if !in_range { zero_q_tmp(j,D,tidx); }
            else {
                var qi=0 as i64;
                while qi<D/4 {
                    quantize_q8(Qc+j*nb01+q8_lane_byte(qi),scale,ni,tidx,j*D,j*D+D,qi);
                    qi+=ni;
                }
            }
        }
        j0+=nwarp;
    }
    sync();

    var lj=0 as i64;
    while lj<ncols {
        var i0=0 as i64;
        while i0<D/4 {
            let i=i0+(if nKQ==32 {tidx} else {tidx%nKQ});
            let wi=i0/nKQ;
            qr.i[lj][wi]=shu(lj*D+i);
            let dsi=i/8;
            qr.ds[lj][2*wi]=shf(lj*D+D+2*dsi);
            qr.ds[lj][2*wi+1]=shf(lj*D+D+2*dsi+1);
            i0+=nKQ;
        }
        lj+=1;
    }
    sync();

    let kmaxv=if has_kvmax!=0 {ld32(kvmax,4*(sequence*gdimx+bidx)) as i64} else {ne11};
    Kc+=bidy*128*nb11;
    Vc+=bidy*128*nb21;
    maskc+=2*(bidy*128);
    var kk=bidy*128;
    while kk<kmaxv {
        for j in 2 { kq_max_new[j]=kq_max[j]; }

        var ik0=0 as i64;
        while ik0<nKQ {
            let i_KQ=tidy*32+(if nKQ==32 {0 as i64} else {tidx-(tidx%nKQ)})+ik0;
            var jj=0 as i64;
            while jj<ncols {
                let in_range=!(ncols>1 && ic0+jj>=ne01z);
                let partial=q4_dot(Kc+i_KQ*nb11,qr.i[jj][0],qr.i[jj][1],
                    qr.ds[jj][0],qr.ds[jj][1],qr.ds[jj][2],qr.ds[jj][3],
                    D,nKQ,tidx);
                var sum=warp_sum(partial,nKQ);
                if use_softcap!=0 { sum=apply_softcap(sum,logit_softcap); }
                if has_mask!=0 && in_range {
                    sum=apply_mask(sum,slope,ld16(maskc,2*(jj*ne11+i_KQ)));
                }
                kq_max_new[jj]=sm_max_new(kq_max_new[jj],sum);
                let keeper=if nKQ==32 {tidx} else {tidx%nKQ};
                if keeper==ik0 { kq_reg[jj]=sum; }
                jj+=1;
            }
            ik0+=1;
        }

        var uj=0 as i64;
        while uj<ncols {
            var off=nKQ;
            while off<32 { kq_max_new[uj]=maxf(kq_max_new[uj],shflx(kq_max_new[uj],off)); off=off*2; }
            let kmax_scale=sm_scale(kq_max[uj],kq_max_new[uj]);
            kq_max[uj]=kq_max_new[uj];
            kq_reg[uj]=sm_prob(kq_reg[uj],kq_max[uj]);
            kq_sum[uj]=sm_mix(kq_sum[uj],kmax_scale,kq_reg[uj]);
            shsetf(uj*128+tid,kq_reg[uj]);
            var s=0 as i64;
            while s<nvkq {
                vkq.v[uj][s][0]=vkq.v[uj][s][0]*kmax_scale;
                vkq.v[uj][s][1]=vkq.v[uj][s][1]*kmax_scale;
                s+=1;
            }
            uj+=1;
        }
        syncwarp();

        var k0=0 as i64;
        while k0<32 {
            let k=tidy*32+k0+(if nV==32 {0 as i64} else {tidx/nV});
            for j in 2 { if j<ncols { kq_k[j]=shf(j*128+k); } }
            var iv0=0 as i64;
            while iv0<D/2 {
                let idx0=2*iv0+(if nV==32 {tidx} else {tidx%nV})*4;
                let tmp=deq4(Vc+k*nb21,idx0);
                var i1=0 as i64;
                while i1<vrows/2 {
                    var jj=0 as i64;
                    while jj<ncols {
                        let slot=iv0/nV+i1;
                        vkq.v[jj][slot][0]=sm_value(vkq.v[jj][slot][0],1.0,tmp.v[2*i1],kq_k[jj]);
                        vkq.v[jj][slot][1]=sm_value(vkq.v[jj][slot][1],1.0,tmp.v[2*i1+1],kq_k[jj]);
                        jj+=1;
                    }
                    i1+=1;
                }
                iv0+=nV*vrows/2;
            }
            k0+=vcols;
        }
        syncwarp();

        kk+=gdimy*128;
        Kc+=gdimy*128*nb11;
        Vc+=gdimy*128*nb21;
        maskc+=2*(gdimy*128);
    }

    if has_sinks!=0 && bidy==0 {
        let sink=ldf(sinks,4*head);
        var j0=0 as i64;
        while j0<ncols {
            let j=j0+tidy;
            if !(j0+nwarp>ncols && j>=ncols) {
                let newmax=maxf(sink,kq_max[j]);
                let sc=sm_scale(kq_max[j],newmax);
                kq_max[j]=newmax;
                let add=if tidx==0 {sm_prob(sink,kq_max[j])} else {0.0 as f32};
                kq_sum[j]=sm_mix(kq_sum[j],sc,add);
                var s=0 as i64;
                while s<nvkq {
                    vkq.v[j][s][0]=vkq.v[j][s][0]*sc;
                    vkq.v[j][s][1]=vkq.v[j][s][1]*sc;
                    s+=1;
                }
            }
            j0+=nwarp;
        }
    }

    for j in 2 {
        if j<ncols && tidy==0 {
            shsetf(maxbase+j*32+tidx,neg_half_flt_max());
            shsetf(sumbase+j*32+tidx,0.0);
        }
    }
    sync();
    for j in 2 { if j<ncols && tidx==0 { shsetf(maxbase+j*32+tidy,kq_max[j]); } }
    sync();

    var jv=0 as i64;
    while jv<ncols {
        let jvalid=!(ncols>1 && ic0+jv>=ne01z);
        if jvalid {
            var kqmn=shf(maxbase+jv*32+tidx);
            kqmn=warp_max(kqmn);
            let sc=sm_scale(kq_max[jv],kqmn);
            kq_max[jv]=kqmn;
            var s=0 as i64;
            while s<nvkq {
                vkq.v[jv][s][0]=vkq.v[jv][s][0]*sc;
                vkq.v[jv][s][1]=vkq.v[jv][s][1]*sc;
                s+=1;
            }
            var iv0=0 as i64;
            while iv0<D/2 {
                let shbase=tidy*(vcols*D/2)+(if nV==32 {0 as i64} else {(tidx/nV)*(D/2)});
                let shidx=shbase+iv0+(if nV==32 {tidx} else {tidx%nV})*(vrows/2);
                shsetf(2*shidx,vkq.v[jv][iv0/nV][0]);
                shsetf(2*shidx+1,vkq.v[jv][iv0/nV][1]);
                shsetf(2*(shidx+1),vkq.v[jv][iv0/nV+1][0]);
                shsetf(2*(shidx+1)+1,vkq.v[jv][iv0/nV+1][1]);
                iv0+=nV*vrows/2;
            }
            kq_sum[jv]=kq_sum[jv]*sc;
            kq_sum[jv]=warp_sum(kq_sum[jv],32);
            if tidx==0 { shsetf(sumbase+jv*32+tidy,kq_sum[jv]); }
            sync();
            if 128<=D || tid<D {
                var total=shf(sumbase+jv*32+tidx);
                total=warp_sum(total,32);
                var i0=0 as i64;
                while i0<D {
                    var dv=0.0 as f32;
                    var w=0 as i64;
                    while w<nwarp {
                        var v=0 as i64;
                        while v<vcols {
                            dv+=shf(w*(vcols*D)+v*D+i0+tid);
                            v+=1;
                        }
                        w+=1;
                    }
                    if gdimy==1 { dv=dv/total; }
                    let doff=4*((((sequence*ne01z+ic0+jv)*ne02+head)*gdimy+bidy)*D+i0+tid);
                    stf(dst,doff,dv);
                    i0+=128;
                }
            }
            if jv+1<ncols { sync(); }
        }
        jv+=1;
    }

    if gdimy!=1 && tid<ncols && (ncols==1 || ic0+tid<ne01z) {
        let mo=8*((((sequence*ne01z+ic0+tid)*ne02+head)*gdimy+bidy));
        stf(meta,mo,kq_max[tid]);
        stf(meta,mo+4,kq_sum[tid]);
    }
    return 0.0 as f32;
}

fn main() {
    let p=0 as i64;
    print(deq4_scalar(p,0,0)+q4_nibble_at(0,0,0,0.0));
    print(entry(p,p,p,p,p,p,p,p,1.0,0.0,1.0,1.0,1,0.0,
        128,1,1,1, 512,512,512, 0,1,1,1, 72,72,72, 72,72,72,
        0,0,0,0,0,0, 1,0,0,0,0));
}
