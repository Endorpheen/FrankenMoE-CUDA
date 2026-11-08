// Apex CUDA matrix path, Goose numerical core (worker-11).
//
// Scope: the Q4_K_M hybrid graph keeps attention/router/shared-expert matmuls
// on CUDA while the routed experts run on the CPU (cpu-moe).  The reachable
// CUDA matrix work is therefore:
//   * F16/F32 GEMV and GEMM for router + shared-expert + attention projections
//   * Q4_K/Q6_K row dequantisation (cast weights, zero points)
//   * Q4_K/Q6_K dot against pre-quantised q8_1 activations (MMVQ core)
//   * MMVQ/MMQ dispatch, tile accounting and shared-memory layout selection
// The tensor-core MMA instruction itself stays in the vendor/native mma.cuh
// primitive; every numerical loop, packing decision and tile bound below is
// Goose.  A mechanical qualifier pass turns the emitted C into device code.
//
// Block layouts are the frozen ggml structures from /inputs/native:
//   block_q4_K: half d, half dmin, u8 scales[12], u8 qs[128]   (144 bytes)
//   block_q6_K: u8 ql[128], u8 qh[64], i8 scales[16], half d  (210 bytes)
//   block_q8_1: half d, half s, i8 qs[32]                    (36 bytes)
// All arithmetic below is scalar and keeps the native evaluation order per
// lane/sub-blk; the host corpus checks it against an independent reference.

extern "fgm_half" fn half(x:u16)->f32;
extern "fgm_to_half" fn to_half(x:f32)->u16;
extern "fgm_from_bits" fn from_bits(x:u32)->f32;
extern "fgm_bits" fn bits(x:f32)->u32;
extern "fgm_witness" fn witness(op:i64,value:i64);

fn add(a:i64,b:i64)->i64 {assert(a>=0&&b>=0&&a<=9223372036854775807-b);return a+b;}
fn read16(x:const u8[:],i:i64)->u16 {assert(i>=0&&i+1<x.len);return (x[i] as u16)|((x[i+1] as u16)<<8);}
fn read32(x:const u8[:],i:i64)->u32 {assert(i>=0&&i+3<x.len);return (read16(x,i) as u32)|((read16(x,i+2) as u32)<<16);}
fn f16(x:const u8[:],i:i64)->f32 {return half(read16(x,i));}
fn f32at(x:const u8[:],i:i64)->f32 {return from_bits(read32(x,i));}
fn s8(x:const u8[:],i:i64)->i64 {assert(i>=0&&i<x.len);return ((x[i] as! i8) as i64);}

// ---------------------------- Q4_K -----------------------------------------
fn q4k_scale(blk:const u8[:],base:i64,j:i64)->i64 {
    let s=base+4;
    if j<4 {return (blk[s+j] as i64)&63;}
    let lo=(blk[s+j+4] as i64)&15;
    let hi=((blk[s+j-4] as i64)>>6)<<4;
    return lo|hi;
}
fn q4k_min(blk:const u8[:],base:i64,j:i64)->i64 {
    let s=base+4;
    if j<4 {return (blk[s+j+4] as i64)&63;}
    let lo=(blk[s+j+4] as i64)>>4;
    let hi=((blk[s+j] as i64)>>6)<<4;
    return lo|hi;
}
fn q4k_lane(blk:const u8[:],base:i64,lane:i64)->f32 {
    let il=lane/64;
    let within=lane%64;
    let r=within%32;
    let hi=within/32;
    let qb=(blk[base+16+32*il+r] as i64);
    let nib=if hi==0 {(qb&15)} else {(qb>>4)};
    let sub=2*il+hi;
    let d=f16(blk,base);
    let dmin=f16(blk,base+2);
    let sc=q4k_scale(blk,base,sub) as f32;
    let m=q4k_min(blk,base,sub) as f32;
    return (d*sc)*(nib as f32)-dmin*m;
}

// ---------------------------- Q6_K -----------------------------------------
fn q6k_raw(blk:const u8[:],base:i64,lane:i64)->i64 {
    let ip=lane/128;
    let within=lane%128;
    let il=within%32;
    let chunk=within/32;
    let qo=if (chunk%2)==0 {0} else {32};
    let ql=(blk[base+64*ip+il+qo] as i64);
    let lo=if chunk<2 {(ql&15)} else {(ql>>4)};
    let qh=(blk[base+128+32*ip+il] as i64);
    let sh=2*chunk;
    let hi=((qh>>sh)&3)<<4;
    return lo|hi;
}
fn q6k_scale(blk:const u8[:],base:i64,lane:i64)->i64 {
    let ip=lane/128;
    let within=lane%128;
    let il=within%32;
    let chunk=within/32;
    return 8*ip+il/16+2*chunk;
}
fn q6k_lane(blk:const u8[:],base:i64,lane:i64)->f32 {
    let d=f16(blk,base+208);
    let sc=s8(blk,base+192+q6k_scale(blk,base,lane)) as f32;
    let q=q6k_raw(blk,base,lane)-32;
    return d*sc*(q as f32);
}

// ---------------------- whole-row dequantisation ----------------------------
fn dequant_row(weights:const u8[:],dst:f32[:],kind:i64)->i64 {
    let bs=if kind==12 {144} else if kind==14 {210} else {0};
    if bs==0 {return 0;}
    if dst.len<=0||dst.len%256!=0 {return 0;}
    let blocks=dst.len/256;
    if weights.len<blocks*bs {return 0;}
    var ib:i64=0;
    while ib<blocks {
        let base=ib*bs;
        var lane:i64=0;
        while lane<256 {
            let v=if kind==12 {q4k_lane(weights,base,lane)} else {q6k_lane(weights,base,lane)};
            dst[ib*256+lane]=v;
            lane+=1;
        }
        ib+=1;
    }
    witness(40,blocks);
    return blocks;
}

// ------------------------ q8_1 dot (MMVQ core) ------------------------------
fn q4k_dot_sb(weights:const u8[:],sb:i64,q8:const u8[:])->f32 {
    let base=sb*144;
    let q8base=sb*8*36;
    let d=f16(weights,base);
    let dmin=f16(weights,base+2);
    var sumd:f32=(0 as f32);
    var summ:f32=(0 as f32);
    var j:i64=0;
    while j<8 {
        let qb=q8base+j*36;
        let d8=f16(q8,qb);
        var sumi:i64=0;
        var sumu:i64=0;
        let il=j/2;
        let hi=j%2;
        var r:i64=0;
        while r<32 {
            let byte=(weights[base+16+32*il+r] as i64);
            let nib=if hi==0 {(byte&15)} else {(byte>>4)};
            let u=s8(q8,qb+4+r);
            sumi+=nib*u;
            sumu+=u;
            r+=1;
        }
        let sc=q4k_scale(weights,base,j) as f32;
        let m=q4k_min(weights,base,j) as f32;
        sumd=sumd+d8*((sumi as f32)*sc);
        summ=summ+d8*((sumu as f32)*m);
        j+=1;
    }
    return d*sumd-dmin*summ;
}
fn q6k_dot_sb(weights:const u8[:],sb:i64,q8:const u8[:])->f32 {
    let base=sb*210;
    let q8base=sb*8*36;
    let d=f16(weights,base+208);
    var sum:f32=(0 as f32);
    var j:i64=0;
    while j<16 {
        let d8=f16(q8,q8base+(j/2)*36);
        let sc=s8(weights,base+192+j) as f32;
        var sumi:i64=0;
        var r:i64=0;
        while r<16 {
            let lane=j*16+r;
            let q=q6k_raw(weights,base,lane)-32;
            let u=s8(q8,q8base+(j/2)*36+4+(j%2)*16+r);
            sumi+=q*u;
            r+=1;
        }
        sum=sum+d8*((sumi as f32)*sc);
        j+=1;
    }
    return d*sum;
}

// --------------------------- F16/F32 GEMV -----------------------------------
fn gemv_f32_row(weights:const u8[:],x:const u8[:],y:f32[:],k:i64,row:i64)->i64 {
    if k<=0||row<0 {return 0;}
    if weights.len<(row+1)*k*4||x.len<k*4 {return 0;}
    var s:f32=(0 as f32);
    var i:i64=0;
    while i<k {s=s+f32at(weights,(row*k+i)*4)*f32at(x,i*4);i+=1;}
    y[0]=s;
    witness(41,row);
    return 1;
}
fn gemv_f16_row(weights:const u8[:],x:const u8[:],y:f32[:],k:i64,row:i64)->i64 {
    if k<=0||row<0 {return 0;}
    if weights.len<(row+1)*k*2||x.len<k*4 {return 0;}
    var s:f32=(0 as f32);
    var i:i64=0;
    while i<k {s=s+f16(weights,(row*k+i)*2)*f32at(x,i*4);i+=1;}
    y[0]=s;
    witness(42,row);
    return 1;
}

// ---------------------------- F16/F32 GEMM ----------------------------------
fn gemm_f32_row(a:const u8[:],b:const u8[:],c:f32[:],m:i64,n:i64,k:i64,row:i64)->i64 {
    if m<=0||n<=0||k<=0||row<0||row>=m {return 0;}
    if a.len<m*k*4||b.len<k*n*4||c.len<n {return 0;}
    var j:i64=0;
    while j<n {
        var s:f32=(0 as f32);
        var t:i64=0;
        while t<k {s=s+f32at(a,(row*k+t)*4)*f32at(b,(t*n+j)*4);t+=1;}
        c[j]=s;
        j+=1;
    }
    witness(43,row);
    return 1;
}
fn gemm_f16_row(a:const u8[:],b:const u8[:],c:f32[:],m:i64,n:i64,k:i64,row:i64)->i64 {
    if m<=0||n<=0||k<=0||row<0||row>=m {return 0;}
    if a.len<m*k*2||b.len<k*n*4||c.len<n {return 0;}
    var j:i64=0;
    while j<n {
        var s:f32=(0 as f32);
        var t:i64=0;
        while t<k {s=s+f16(a,(row*k+t)*2)*f32at(b,(t*n+j)*4);t+=1;}
        c[j]=s;
        j+=1;
    }
    witness(44,row);
    return 1;
}

// ------------------------- MMVQ / MMQ dispatch ------------------------------
// ggml_cuda_should_use_mmvq restricted to the two K-quants the Apex Q4_K_M
// cast actually stores on the CUDA device.
fn mmvq_should_use(kind:i64,cc:i64,ne11:i64)->i64 {
    if kind!=12&&kind!=14 {return 0;}
    if ne11<=0 {return 0;}
    if cc==890 {return if ne11<=8 {1} else {0};}                    // Ada
    if cc==1200 {
        if kind==12 {return if ne11<=5 {1} else {0};}              // Blackwell
        return if ne11<=7 {1} else {0};
    }
    if cc==870 {return if ne11<=1 {1} else {0};}                    // Orin
    if cc==700 {
        if kind==12 {return if ne11<=5 {1} else {0};}              // Volta
        return if ne11<=7 {1} else {0};
    }
    return if ne11<=8 {1} else {0};                                  // generic
}
// calc_nwarps for the generic MMVQ parameter table.
fn mmvq_nwarps(kind:i64,ncols:i64,table:i64)->i64 {
    if ncols<=0 {return 1;}
    if table==0 {
        if ncols<=4 {return 4;}
        if ncols<=8 {return 2;}
        return 1;
    }
    return 1;
}
fn mmq_layout(kind:i64)->i64 {if kind==12 {return 1;} if kind==14 {return 4;} return 0;}
fn mmq_sram_stride(layout:i64)->i64 {
    if layout==1 {return 84;}                 // Q8_1: 2*32 + 2*32/4 + 4
    if layout==4 {return 76;}                 // Q6_K: 2*32 + 32/32 + 32/8 + 7
    return 0;
}
// config for the Ampere+ NVIDIA tables: nthreads 256, I 128, J preserved,
// occupancy 1, K_vram MMQ_ITER_K, stream-k on.
fn mmq_config(kind:i64,J:i64,fallback:i64,out:f32[:])->i64 {
    if out.len<10 {return 0;}
    if kind!=12&&kind!=14 {return 0;}
    if J<8||J>128||J%8!=0 {return 0;}
    let layout=mmq_layout(kind);
    let rpw=if (J>=48&&J%16==0) {32} else {16};
    out[0]=256 as f32;
    out[1]=1 as f32;
    out[2]=128 as f32;
    out[3]=J as f32;
    out[4]=layout as f32;
    out[5]=256 as f32;
    out[6]=1 as f32;
    out[7]=rpw as f32;
    out[8]=mmq_sram_stride(layout) as f32;
    out[9]=fallback as f32;
    return 1;
}
// tile_x_sizes {qs, df, sc} from the dp4a MMQ macros.
fn mmq_tile_x_sizes(kind:i64,I:i64,out:f32[:])->i64 {
    if out.len<3||I<=0||I%32!=0 {return 0;}
    if kind==12 {
        out[0]=((I*32+I) as f32);
        out[1]=(I as f32);
        out[2]=((I*32/8+I/8) as f32);
        return 1;
    }
    if kind==14 {
        out[0]=((I*64+I) as f32);
        out[1]=((I+I/32) as f32);
        out[2]=((I*32/8+I/8) as f32);
        return 1;
    }
    return 0;
}
fn mmq_needs_fallback(nrows:i64)->i64 {return if nrows%128!=0 {1} else {0};}
// Shared-memory bound check for the dp4a MMQ tile: qs ints + df ints +
// packed scale bytes must fit in I*sram_stride ints.
fn mmq_sram_check(kind:i64,I:i64,stride:i64,out:f32[:])->i64 {
    if out.len<6||I<=0||I%32!=0||stride<=0 {return 0;}
    var qs:i64=0;
    var df:i64=0;
    var sc:i64=0;
    if kind==12 {qs=I*32+I;df=I;}
    else if kind==14 {qs=I*64+I;df=I+I/32;}
    else {return 0;}
    sc=I*32/8+I/8;
    let need=qs+df+(sc+3)/4;
    let cap=I*stride;
    out[0]=qs as f32;
    out[1]=df as f32;
    out[2]=sc as f32;
    out[3]=need as f32;
    out[4]=cap as f32;
    out[5]=if need<=cap {(1 as f32)} else {(0 as f32)};
    return if need<=cap {1} else {0};
}

// ------------------------------- entry -------------------------------------
fn entry(mode:i64,w:const u8[:],x:const u8[:],y:f32[:],p0:i64,p1:i64,p2:i64,p3:i64)->i64 {
    witness(mode,0);
    if mode==0 {
        if y.len<1 {return 0;}
        if p0<0 {return 0;}
        let base=(p0/256)*144;
        if w.len<base+144 {return 0;}
        y[0]=q4k_lane(w,base,p0%256);
        return 1;
    }
    if mode==1 {
        if y.len<1 {return 0;}
        if p0<0 {return 0;}
        let base=(p0/256)*210;
        if w.len<base+210 {return 0;}
        y[0]=q6k_lane(w,base,p0%256);
        return 1;
    }
    if mode==2 {return gemv_f32_row(w,x,y,p1,p2);}
    if mode==3 {return gemv_f16_row(w,x,y,p1,p2);}
    if mode==4 {return gemm_f32_row(w,x,y,p0,p1,p2,p3);}
    if mode==5 {return gemm_f16_row(w,x,y,p0,p1,p2,p3);}
    if mode==6 {
        if y.len<1||p0<0 {return 0;}
        if w.len<(p0+1)*144||x.len<(p0+1)*8*36 {return 0;}
        y[0]=q4k_dot_sb(w,p0,x);
        return 1;
    }
    if mode==7 {
        if y.len<1||p0<0 {return 0;}
        if w.len<(p0+1)*210||x.len<(p0+1)*8*36 {return 0;}
        y[0]=q6k_dot_sb(w,p0,x);
        return 1;
    }
    if mode==8 {return mmvq_should_use(p0,p1,p2);}
    if mode==9 {return mmvq_nwarps(p0,p1,p2);}
    if mode==10 {return mmq_config(p0,p1,p2,y);}
    if mode==11 {return mmq_tile_x_sizes(p0,p1,y);}
    if mode==12 {return mmq_needs_fallback(p0);}
    if mode==15 {return mmq_sram_check(p0,p1,p2,y);}
    if mode==13 {return dequant_row(w,y,12);}
    if mode==14 {return dequant_row(w,y,14);}
    return 0;
}

fn main() {
    // Keep every mode reachable so the whole-program emitter emits every
    // callable unit.  Zero-filled buffers satisfy all length guards.
    var w:u8[4096]=default<u8[4096]>();
    var x:u8[4096]=default<u8[4096]>();
    var y:f32[1024]=default<f32[1024]>();
    var acc:i64=0;
    var mode:i64=0;
    while mode<16 {
        acc+=entry(mode,w,x,y,0,8,0,0);
        mode+=1;
    }
    print(acc);
}
