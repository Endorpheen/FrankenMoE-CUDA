// matrix-closure: Goose closure of the active Apex CUDA matrix prefill path.
//
// The frozen matrix candidate (../apex_matrix.goose) already owns Q4_K/Q6_K
// dequant, q8_1 dot and MMVQ dispatch.  This module adds the parts that were
// still native when prefill actually runs:
//   * q8_1 activation quantization and MMQ block packing (quantize.cu
//     quantize_q8_1 / quantize_mmq_q8_1, q8_1 layouts only),
//   * MMQ launch planning: J_best selection, shared-memory bytes, native
//     tile counts, stream-k block count/fixup, q8_1 column strides, padding,
//   * the tiled Q4_K/Q6_K MMQ numerical kernel (weight and activation tile
//     staging plus the dp4a-equivalent dot),
//   * an explicit F16/F32 prefill router that names cuBLAS as the external
//     vendor primitive instead of silently pretending Goose owns it.
//
// The tensor-core MMA instruction issue is *not* claimed here.  The tiled
// kernel below is a correct functional MMQ-shaped kernel: the same q8_1 block
// layout, Goose tiles and Goose dot, but scalar arithmetic.  Performance is a
// separate parent GPU gate.
//
// Frozen ggml layouts used:
//   block_q4_K      144 B: half d, half dmin, u8 scales[12], u8 qs[128]
//   block_q6_K      210 B: u8 ql[128], u8 qh[64], i8 scales[16], half d
//   block_q8_1       36 B: half d, half s, i8 qs[32]
//   block_q8_1_mmq  144 B: 4 x (half d, half s) or 4 x float d, i8 qs[128]
// Q4_K uses MMQ_Q8_1_DS_LAYOUT_DS4, Q6_K uses MMQ_Q8_1_DS_LAYOUT_D4.

extern "fgc_half" fn half(x:u16)->f32;
extern "fgc_to_half" fn to_half(x:f32)->u16;
extern "fgc_from_bits" fn from_bits(x:u32)->f32;
extern "fgc_bits" fn bits(x:f32)->u32;
extern "fgc_roundf" fn roundf(x:f32)->f32;
extern "fgc_f2i8" fn f2i8(x:f32)->u8;
extern "fgc_witness" fn witness(op:i64,value:i64);

fn imin(a:i64,b:i64)->i64 {return if a<b {a} else {b};}
fn align_up(x:i64,align:i64)->i64 {assert(align>0);return ((x+align-1)/align)*align;}
fn read16(x:const u8[:],i:i64)->u16 {assert(i>=0&&i+1<x.len);return (x[i] as u16)|((x[i+1] as u16)<<8);}
fn read32(x:const u8[:],i:i64)->u32 {assert(i>=0&&i+3<x.len);return (read16(x,i) as u32)|((read16(x,i+2) as u32)<<16);}
fn write16(x:u8[:],i:i64,v:u16) {assert(i>=0&&i+1<x.len);x[i]=v as! u8;x[i+1]=(v>>8) as! u8;}
fn write32(x:u8[:],i:i64,v:u32) {assert(i>=0&&i+3<x.len);x[i]=v as! u8;x[i+1]=(v>>8) as! u8;x[i+2]=(v>>16) as! u8;x[i+3]=(v>>24) as! u8;}
fn f16(x:const u8[:],i:i64)->f32 {return half(read16(x,i));}
fn f32at(x:const u8[:],i:i64)->f32 {return from_bits(read32(x,i));}
fn s8(x:const u8[:],i:i64)->i64 {assert(i>=0&&i<x.len);return ((x[i] as! i8) as i64);}
fn bbytes(kind:i64)->i64 {return if kind==12 {144} else if kind==14 {210} else {0};}
fn row_stride_for(kind:i64)->i64 {return if kind==12 {160} else if kind==14 {224} else {0};}

// ------------------------------ Q4_K ---------------------------------------
fn q4k_scale(blk:const u8[:],base:i64,j:i64)->i64 {
    let s=base+4;
    if j<4 {return (blk[s+j] as i64)&63;}
    return ((blk[s+j+4] as i64)&15)|(((blk[s+j-4] as i64)>>6)<<4);
}
fn q4k_min(blk:const u8[:],base:i64,j:i64)->i64 {
    let s=base+4;
    if j<4 {return (blk[s+j+4] as i64)&63;}
    return ((blk[s+j+4] as i64)>>4)|(((blk[s+j] as i64)>>6)<<4);
}
fn q4k_nib(blk:const u8[:],base:i64,g:i64,r:i64)->i64 {
    let il=g/2;
    let hi=g%2;
    let qb=(blk[base+16+32*il+r] as i64);
    return if hi==0 {(qb&15)} else {(qb>>4)};
}

// ------------------------------ Q6_K ---------------------------------------
fn q6k_raw(blk:const u8[:],base:i64,lane:i64)->i64 {
    let ip=lane/128;
    let within=lane%128;
    let il=within%32;
    let chunk=within/32;
    let qo=if (chunk%2)==0 {0} else {32};
    let ql=(blk[base+64*ip+il+qo] as i64);
    let lo=if chunk<2 {(ql&15)} else {(ql>>4)};
    let qh=(blk[base+128+32*ip+il] as i64);
    return lo|(((qh>>(2*chunk))&3)<<4);
}

// -------------------- MMQ q8_1 activation packing ---------------------------
fn mmvq_q_byte(x:const f32[:],i:i64,amax:f32)->u8 {
    if amax==(0 as f32) {return 0;}
    let d=amax/(127 as f32);
    return f2i8(roundf(x[i]/d));
}
fn mmq_q_byte(x:const f32[:],i:i64,amax:f32)->u8 {
    if amax==(0 as f32) {return 0;}
    let dinv=(127 as f32)/amax;
    return f2i8(roundf(x[i]*dinv));
}
fn mmvq_header(amax:f32,sum:f32,out:u8[:],off:i64)->i64 {
    witness(120,off);
    if off<0||off+36>out.len {return 0;}
    let d=amax/(127 as f32);
    write16(out,off,to_half(d));
    write16(out,off+2,to_half(sum));
    return 1;
}
fn mmq_header(amax:f32,sum:f32,layout:i64,out:u8[:],off:i64,gi:i64)->i64 {
    witness(121,gi);
    if off<0||off+144>out.len||gi<0||gi>=4 {return 0;}
    if layout==0 {
        let d=if amax==(0 as f32) {(0 as f32)} else {(1 as f32)/((127 as f32)/amax)};
        write16(out,off+gi*4,to_half(d));
        write16(out,off+gi*4+2,to_half(sum));
        return 1;
    }
    if layout==1 {
        let d=if amax==(0 as f32) {(0 as f32)} else {(1 as f32)/((127 as f32)/amax)};
        write32(out,off+gi*4,bits(d));
        return 1;
    }
    return 0;
}
fn q8_elem(mode:i64,x:const f32[:],i:i64,amax:f32,out:u8[:],off:i64)->i64 {
    witness(122,mode);
    if i<0||i>=x.len||off<0||off>=out.len {return 0;}
    out[off]=if mode==0 {mmvq_q_byte(x,i,amax)} else {mmq_q_byte(x,i,amax)};
    return 1;
}
// Sequential reference for one MMVQ 32-value block (36 bytes).
fn mmvq_q8_block(x:const f32[:],out:u8[:])->i64 {
    witness(100,0);
    if x.len<32||out.len<36 {return 0;}
    var amax:f32=(0 as f32);
    var sum:f32=(0 as f32);
    var i:i64=0;
    while i<32 {let v=x[i];let a=if v<(0 as f32) {(0 as f32)-v} else {v};if a>amax {amax=a;}sum=sum+v;i+=1;}
    i=0;
    while i<32 {out[4+i]=mmvq_q_byte(x,i,amax);i+=1;}
    witness(123,0);
    let d=amax/(127 as f32);
    write16(out,0,to_half(d));
    write16(out,2,to_half(sum));
    return 1;
}
// Sequential reference for one MMQ 128-value block (144 bytes).
// layout 0 = DS4 (Q4_K), 1 = D4 (Q6_K).
fn mmq_q8_block(x:const f32[:],out:u8[:],layout:i64)->i64 {
    witness(101,layout);
    if x.len<128||out.len<144 {return 0;}
    var g:i64=0;
    while g<4 {
        let off=g*32;
        var amax:f32=(0 as f32);
        var sum:f32=(0 as f32);
        var i:i64=0;
        while i<32 {let v=x[off+i];let a=if v<(0 as f32) {(0 as f32)-v} else {v};if a>amax {amax=a;}sum=sum+v;i+=1;}
        i=0;
        while i<32 {out[16+g*32+i]=mmq_q_byte(x,off+i,amax);i+=1;}
        let ok=mmq_header(amax,sum,layout,out,0,g);
        if ok!=1 {return 0;}
        g+=1;
    }
    return 1;
}

// ---------------------- MMQ tile dot (dp4a-equivalent) ----------------------
fn q4k_dot_mmq(w:const u8[:],wbase:i64,q8:const u8[:],qbase:i64)->f32 {
    let d=f16(w,wbase);
    let dmin=f16(w,wbase+2);
    var sumd:f32=(0 as f32);
    var summ:f32=(0 as f32);
    var g:i64=0;
    while g<8 {
        let blk=g/4;
        let gi=g%4;
        let hdr=qbase+blk*144+gi*4;
        let d8=f16(q8,hdr);
        let s=f16(q8,hdr+2);
        let qs=qbase+blk*144+16+gi*32;
        var sumi:i64=0;
        var r:i64=0;
        while r<32 {sumi+=q4k_nib(w,wbase,g,r)*s8(q8,qs+r);r+=1;}
        let sc=q4k_scale(w,wbase,g) as f32;
        let m=q4k_min(w,wbase,g) as f32;
        sumd=sumd+d8*((sumi as f32)*sc);
        summ=summ+s*m;
        g+=1;
    }
    return d*sumd-dmin*summ;
}
fn q6k_dot_mmq(w:const u8[:],wbase:i64,q8:const u8[:],qbase:i64)->f32 {
    let d6=f16(w,wbase+208);
    var sum:f32=(0 as f32);
    var g:i64=0;
    while g<16 {
        let t=g/2;
        let blk=t/4;
        let gi=t%4;
        let d8=f32at(q8,qbase+blk*144+gi*4);
        let qs=qbase+blk*144+16+gi*32+(g%2)*16;
        let sc=s8(w,wbase+192+g) as f32;
        var sumi:i64=0;
        var r:i64=0;
        while r<16 {let q=q6k_raw(w,wbase,g*16+r)-32;sumi+=q*s8(q8,qs+r);r+=1;}
        sum=sum+d8*((sumi as f32)*sc);
        g+=1;
    }
    return d6*sum;
}
fn mmq_dot(kind:i64,w:const u8[:],wbase:i64,q8:const u8[:],qbase:i64,out:f32[:])->i64 {
    witness(102,kind);
    if out.len<1 {return 0;}
    let bs=bbytes(kind);
    if bs==0||wbase<0||wbase+bs>w.len||qbase<0||qbase+288>q8.len {return 0;}
    out[0]=if kind==12 {q4k_dot_mmq(w,wbase,q8,qbase)} else {q6k_dot_mmq(w,wbase,q8,qbase)};
    return 1;
}

// --------------------------- tile staging -----------------------------------
// Copy `rows` weight rows of one 256-value super-block into a row-major shared
// tile.  s01 is the source row stride in blocks.
fn stage_weights(kind:i64,w:const u8[:],s01:i64,kbx0:i64,ibase:i64,rows:i64,dst:u8[:],row_stride:i64)->i64 {
    witness(103,kind);
    let bs=bbytes(kind);
    if bs==0||rows<=0||rows>32||row_stride<bs||dst.len<rows*row_stride {return 0;}
    var i:i64=0;
    while i<rows {
        let src=((ibase+i)*s01+kbx0)*bs;
        if src<0||src+bs>w.len {return 0;}
        var b:i64=0;
        while b<bs {dst[i*row_stride+b]=w[src+b];b+=1;}
        while b<row_stride {dst[i*row_stride+b]=0 as u8;b+=1;}
        i+=1;
    }
    return rows;
}
// Copy `cols` q8_1 MMQ columns (2 blocks = 256 K values) from the packed
// [k_block][column] layout into a column-major shared tile.
fn stage_q8(q8:const u8[:],ncols:i64,kblock0:i64,jbase:i64,cols:i64,dst:u8[:])->i64 {
    witness(104,cols);
    if ncols<=0||cols<=0||cols>8||dst.len<cols*288 {return 0;}
    var c:i64=0;
    while c<cols {
        var kk:i64=0;
        while kk<2 {
            let src=((kblock0+kk)*ncols+(jbase+c))*144;
            if src<0||src+144>q8.len {return 0;}
            var b:i64=0;
            while b<144 {dst[c*288+kk*144+b]=q8[src+b];b+=1;}
            kk+=1;
        }
        c+=1;
    }
    return cols;
}
fn sram_bound(kind:i64,I:i64,J:i64,out:f32[:])->i64 {
    witness(105,kind);
    let rs=row_stride_for(kind);
    if out.len<3||rs==0||I<=0||J<=0 {return 0;}
    let need=I*rs+J*288;
    out[0]=need as f32;
    out[1]=9472 as f32;
    out[2]=if need<=9472 {(1 as f32)} else {(0 as f32)};
    return if need<=9472 {1} else {0};
}

// ------------------------- MMQ launch planning ------------------------------
fn nbytes_shared(J:i64,I:i64,stride:i64,nthreads:i64)->i64 {
    return J*4+I*stride*4+align_up(J*144,nthreads*4);
}
fn plan_mmq(kind:i64,ne00:i64,ne01:i64,ne11:i64,smpbo:i64,cc:i64,nsm:i64,out:f32[:])->i64 {
    witness(106,kind);
    if out.len<24 {return 0;}
    var z:i64=0;while z<24 {out[z]=0 as f32;z+=1;}
    if kind!=12&&kind!=14 {return 0;}
    if ne00<=0||ne01<=0||ne11<=0||ne00%256!=0 {return 0;}
    let I=128;
    let nthreads=256;
    let layout=if kind==12 {0} else {1};
    let sramlayout=if kind==12 {1} else {4};
    let stride=if kind==12 {84} else {76};
    let fallback=if ne01%128!=0 {1} else {0};
    let ne10p=align_up(ne00,512);
    var Jbest:i64=0;
    var ntbest:i64=9223372036854775807;
    var J:i64=8;
    while J<=128&&ntbest>1 {
        if nbytes_shared(J,I,stride,nthreads)<=smpbo {
            let ntx=(ne11+J-1)/J;
            if ntx<ntbest {Jbest=J;ntbest=ntx;}
        }
        J+=8;
    }
    if Jbest==0 {Jbest=8;}
    let nty=(ne01+I-1)/I;
    let ntx=(ne11+Jbest-1)/Jbest;
    let ntiles=nty*ntx;
    let nsm_n=if nsm>0 {nsm} else {1};
    let waves=(ntiles+nsm_n-1)/nsm_n;
    let eff=if nsm_n*waves>0 {(100*ntiles)/(nsm_n*waves)} else {0};
    let nwork=ntiles*((ne00+255)/256);
    let skblocks=if eff>=90&&cc>=750 {ntiles} else {imin(nsm_n,nwork)};
    let fixup=if skblocks>0&&ntiles%skblocks!=0 {1} else {0};
    let chunk=nthreads*4;
    let q8pad=align_up(Jbest*144,chunk);
    let gi=32;
    let gj=8;
    let gnty=(ne01+gi-1)/gi;
    let gntx=(ne11+gj-1)/gj;
    let rs=row_stride_for(kind);
    let gshared=gi*rs+gj*288;
    out[0]=1 as f32;
    out[1]=Jbest as f32;
    out[2]=nthreads as f32;
    out[3]=I as f32;
    out[4]=layout as f32;
    out[5]=sramlayout as f32;
    out[6]=stride as f32;
    out[7]=nbytes_shared(Jbest,I,stride,nthreads) as f32;
    out[8]=nty as f32;
    out[9]=ntx as f32;
    out[10]=ntiles as f32;
    out[11]=gnty as f32;
    out[12]=gntx as f32;
    out[13]=gi as f32;
    out[14]=gj as f32;
    out[15]=gshared as f32;
    out[16]=fallback as f32;
    out[17]=ne10p as f32;
    out[18]=(ne10p/128) as f32;
    out[19]=q8pad as f32;
    out[20]=skblocks as f32;
    out[21]=fixup as f32;
    out[22]=2 as f32;
    out[23]=0 as f32;
    return 1;
}

// --------------------------- explicit routing -------------------------------
// 0 fallback native MMQ, 1 Goose MMVQ, 2 vendor cuBLAS (F16/F32 prefill),
// 3 Goose MMQ, 4 vendor cuBLAS other, 10 CPU MoE / not a CUDA matrix.
fn route_matmul(kind:i64,batch:i64,dense:i64,cc:i64,smpbo:i64,ne00:i64,ne11:i64,nsm:i64,out:f32[:])->i64 {
    witness(107,kind);
    if nsm<0 {return 0;}
    if out.len<2 {return 0;}
    out[0]=0 as f32;
    out[1]=batch as f32;
    if dense==0 {out[0]=10 as f32;return 10;}
    if kind==12||kind==14 {
        if batch>0&&batch<=8 {out[0]=1 as f32;return 1;}
        if ne00>0&&ne00%256==0&&smpbo>=49152&&cc>=750 {out[0]=3 as f32;return 3;}
        out[0]=0 as f32;
        return 0;
    }
    if kind==0||kind==1 {
        if batch>8 {out[0]=2 as f32;return 2;}
        out[0]=4 as f32;
        return 4;
    }
    out[0]=4 as f32;
    return 4;
}

// ------------------------ F16/F32 GEMM row core -----------------------------
fn gemm_row(kind:i64,a:const u8[:],b:const u8[:],c:f32[:],m:i64,n:i64,k:i64,row:i64,stride:i64)->i64 {
    witness(108,row);
    if m<=0||n<=0||k<=0||row<0||row>=m||stride<=0 {return 0;}
    let ab=if kind==1 {2} else {4};
    if a.len<m*k*ab||b.len<k*n*4||c.len<(n-1)*stride+1 {return 0;}
    var j:i64=0;
    while j<n {
        var s:f32=(0 as f32);
        var t:i64=0;
        while t<k {
            let av=if kind==1 {f16(a,(row*k+t)*2)} else {f32at(a,(row*k+t)*4)};
            s=s+av*f32at(b,(j*k+t)*4);
            t+=1;
        }
        c[j*stride]=s;
        j+=1;
    }
    return 1;
}

// ------------------------------- entry --------------------------------------
fn entry(mode:i64,a:const u8[:],b:const u8[:],c:u8[:],d:f32[:],p0:i64,p1:i64,p2:i64,p3:i64)->i64 {
    witness(109,mode);
    if mode==0 {return mmq_dot(p0,a,p1,b,p2,d);}
    if mode==1 {return sram_bound(p0,p1,p2,d);}
    return 0;
}

fn main() {
    var w:u8[8192]=default<u8[8192]>();
    var x:u8[8192]=default<u8[8192]>();
    var o:u8[8192]=default<u8[8192]>();
    var o2:u8[8192]=default<u8[8192]>();
    var f:f32[1024]=default<f32[1024]>();
    var xf:f32[512]=default<f32[512]>();
    var acc:i64=0;
    acc+=mmvq_q8_block(xf,o);
    acc+=mmq_q8_block(xf,o,0);
    acc+=q8_elem(0,xf,0,0 as f32,o,4);
    acc+=q8_elem(1,xf,0,0 as f32,o,16);
    acc+=mmvq_header(0 as f32,0 as f32,o,0);
    acc+=mmq_header(0 as f32,0 as f32,0,o,0,0);
    acc+=mmq_dot(0,w,0,o,0,f);
    acc+=stage_weights(0,w,1,0,0,1,o,160);
    acc+=stage_q8(o,1,0,0,1,o2);
    acc+=sram_bound(0,32,8,f);
    acc+=plan_mmq(0,256,128,8,49152,800,64,f);
    acc+=route_matmul(0,1,0,800,49152,256,8,64,f);
    acc+=gemm_row(0,w,o,f,1,1,1,0,1);
    acc+=entry(0,w,o,o2,f,0,0,0,0);
    acc+=entry(1,w,o,o2,f,0,32,8,0);
    print(acc);
}
