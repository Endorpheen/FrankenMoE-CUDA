// Active Apex Q4_Kx8 AVX2 repack provider, ported from:
//   inputs/native/ggml/src/ggml-cpu/repack.cpp:3177 (make_block_q4_Kx8)
//   inputs/native/ggml/src/ggml-cpu/repack.cpp:3572 (repack_q4_K_to_q4_K_8_bl)
//   inputs/native/ggml/src/ggml-cpu/arch/x86/repack.cpp:290 (quantize_mat_q8_K_4x8)
//   inputs/native/ggml/src/ggml-cpu/arch/x86/repack.cpp:1464 (gemv)
//   inputs/native/ggml/src/ggml-cpu/arch/x86/repack.cpp:2042 (gemm)
// SIMD operations are scalarized in Goose; per-lane FMA order, integer grouping
// and q4_Kx8/q8_Kx4 layout match the AVX2 reference.
struct Q4Scales { scales:u8[8], mins:u8[8] }
extern "fg143_half" fn half(x:u16)->f32;
extern "fg143_from_bits" fn from_bits(x:u32)->f32;
extern "fg_q8k_float_bits" fn float_bits(x:f32)->u32;
extern "fmaf" fn fma(a:f32,b:f32,c:f32)->f32;
extern "fg006_repack_witness" fn witness(mode:i64,blocks:i64);

fn rd16(a:const u8[:],i:i64)->i64 {
    assert(i>=0&&i<=a.len-2);
    return (a[i] as i64)|((a[i+1] as i64)<<8);
}
fn rd32(a:const u8[:],i:i64)->i64 {
    assert(i>=0&&i<=a.len-4);
    return (a[i] as i64)|((a[i+1] as i64)<<8)|((a[i+2] as i64)<<16)|((a[i+3] as i64)<<24);
}
fn wr16(a:u8[:],i:i64,v:i64) {
    assert(i>=0&&i<=a.len-2);
    a[i]=(v&255) as! u8;a[i+1]=((v>>8)&255) as! u8;
}
fn wr32(a:u8[:],i:i64,v:u32) {
    assert(i>=0&&i<=a.len-4);
    a[i]=(v&255) as! u8;a[i+1]=((v>>8)&255) as! u8;
    a[i+2]=((v>>16)&255) as! u8;a[i+3]=((v>>24)&255) as! u8;
}
fn unpack_group(x:const u8[:],base:i64)->Q4Scales {
    var u:i32[4]=default<i32[4]>();
    for k in 4 {u[k]=rd32(x,base+4*k) as! i32;}
    let m1:i32=0x3f3f3f3f;let m2:i32=0x0f0f0f0f;let m3:i32=0x03030303;
    u[3]=((u[2]>>4)&m2)|(((u[1]>>6)&m3)<<4);
    let aux:i32=u[1]&m1;
    u[1]=(u[2]&m2)|(((u[0]>>6)&m3)<<4);
    u[2]=aux;u[0]=u[0]&m1;
    var out:Q4Scales=default<Q4Scales>();
    for k in 4 {
        out.scales[k]=(u[0]>>((8*k) as u32)) as! u8;
        out.scales[4+k]=(u[1]>>((8*k) as u32)) as! u8;
        out.mins[k]=(u[2]>>((8*k) as u32)) as! u8;
        out.mins[4+k]=(u[3]>>((8*k) as u32)) as! u8;
    }
    return out;
}
fn nearest(x:f32)->i32 {
    let shifted:f32=x+(12582912.0 as f32);
    return ((float_bits(shifted)&0x007fffff) as i32)-0x00400000;
}

fn make_block(dst:u8[:],src:const u8[:],nrows:i64,nblocks:i64,interleave:i64) {
    assert(interleave==8);
    assert(nrows>0&&nrows%8==0&&nblocks>0);
    assert(dst.len==(nrows/8)*nblocks*1152);
    assert(src.len==nrows*nblocks*144);
    for g in nrows/8 {
        for x in nblocks {
            let out=(g*nblocks+x)*1152;
            for j in 8 {
                let si=((g*8+j)*nblocks+x)*144;
                wr16(dst,out+2*j,rd16(src,si));
                wr16(dst,out+16+2*j,rd16(src,si+2));
            }
            for i in 128 {
                let row=i%8;
                let si=((g*8+row)*nblocks+x)*144;
                let so=16+(i/8)*8;
                let do=out+128+i*8;
                for k in 8 {dst[do+k]=src[si+so+k];}
            }
            var s:u8[8]=default<u8[8]>();
            var m:u8[8]=default<u8[8]>();
            for i in 4 {
                for j in 8 {
                    let si=((g*8+j)*nblocks+x)*144;
                    s[j]=src[si+4+i]&63;
                    m[j]=src[si+4+i+4]&63;
                }
                let o=out+32+i*12;
                dst[o+0]=(s[0]&63)+((s[4]&48)<<2);
                dst[o+1]=(s[1]&63)+((s[5]&48)<<2);
                dst[o+2]=(s[2]&63)+((s[6]&48)<<2);
                dst[o+3]=(s[3]&63)+((s[7]&48)<<2);
                dst[o+4]=(m[0]&63)+((m[4]&48)<<2);
                dst[o+5]=(m[1]&63)+((m[5]&48)<<2);
                dst[o+6]=(m[2]&63)+((m[6]&48)<<2);
                dst[o+7]=(m[3]&63)+((m[7]&48)<<2);
                dst[o+8]=(s[4]&15)+((m[4]&15)<<4);
                dst[o+9]=(s[5]&15)+((m[5]&15)<<4);
                dst[o+10]=(s[6]&15)+((m[6]&15)<<4);
                dst[o+11]=(s[7]&15)+((m[7]&15)<<4);
            }
            for i in 4 {
                for j in 8 {
                    let si=((g*8+j)*nblocks+x)*144;
                    s[j]=((src[si+4+i]&192)>>2)|(src[si+4+i+8]&15);
                    m[j]=((src[si+4+i+4]&192)>>2)|((src[si+4+i+8]&240)>>4);
                }
                let o=out+32+i*12+48;
                dst[o+0]=(s[0]&63)+((s[4]&48)<<2);
                dst[o+1]=(s[1]&63)+((s[5]&48)<<2);
                dst[o+2]=(s[2]&63)+((s[6]&48)<<2);
                dst[o+3]=(s[3]&63)+((s[7]&48)<<2);
                dst[o+4]=(m[0]&63)+((m[4]&48)<<2);
                dst[o+5]=(m[1]&63)+((m[5]&48)<<2);
                dst[o+6]=(m[2]&63)+((m[6]&48)<<2);
                dst[o+7]=(m[3]&63)+((m[7]&48)<<2);
                dst[o+8]=(s[4]&15)+((m[4]&15)<<4);
                dst[o+9]=(s[5]&15)+((m[5]&15)<<4);
                dst[o+10]=(s[6]&15)+((m[6]&15)<<4);
                dst[o+11]=(s[7]&15)+((m[7]&15)<<4);
            }
        }
    }
    witness(0,nrows/8*nblocks);
}

fn quantize_mat(x:const f32[:],y:u8[:],k:i64) {
    assert(k>0&&k%256==0);
    let nb=k/256;
    assert(x.len==4*k);
    assert(y.len==nb*1168);
    var srcv:f32[1024]=default<f32[1024]>();
    var iscale:f32[4]=default<f32[4]>();
    for i in nb {
        for row in 4 {
            var amax:f32=(0.0 as f32);
            var pos:bool=false;
            for p in 256 {
                let v=x[row*k+i*256+p];
                srcv[row*256+p]=v;
                let av=if v<(0.0 as f32) {-v} else {v};
                if av>amax {amax=av;}
            }
            if amax>(0.0 as f32) {
                for p in 256 {if srcv[row*256+p]==amax {pos=true;}}
            }
            let isc=if amax==(0.0 as f32) {(0.0 as f32)} else if pos {(-127.0 as f32)/amax} else {(127.0 as f32)/amax};
            iscale[row]=isc;
            let base=i*1168+row*4;
            let d=if amax==(0.0 as f32) {(0.0 as f32)} else {(1.0 as f32)/isc};
            wr32(y,base,float_bits(d));
        }
        for j in 256*4 {
            let row=(j%32)/8;
            let off=(j/32)*8+(j%8);
            let q=nearest(srcv[row*256+off]*iscale[row]);
            y[i*1168+16+j]=(q&255) as! u8;
        }
        for row in 4 {
            for t in 16 {
                var sum:i32=0;
                for u in 8 {sum+=(y[i*1168+16+((2*t)*32+row*8+u)] as! i8) as i32;}
                for u in 8 {sum+=(y[i*1168+16+((2*t+1)*32+row*8+u)] as! i8) as i32;}
                let idx=row*4+(t/4)*16+(t%4);
                wr16(y,i*1168+1040+2*idx,sum);
            }
        }
    }
    witness(1,nb);
}

fn block_scales(vx:const u8[:],bb:i64)->i32[128] {
    var out:i32[128]=default<i32[128]>();
    for g in 8 {
        let sm=unpack_group(vx,bb+32+12*g);
        for j in 8 {out[g*8+j]=(sm.scales[j] as i32);out[64+g*8+j]=(sm.mins[j] as i32);}
    }
    return out;
}

fn gemv(n:i64,s:f32[:],bs:i64,vx:const u8[:],vy:const u8[:],nr:i64,nc:i64) {
    assert(n>0&&n%256==0&&nr>0&&nc>0&&nc%8==0);
    let nb=n/256;
    assert(vx.len==(nc/8)*nb*1152);
    assert(vy.len==nr*nb*292);
    for y in nr {
        for x in nc/8 {
            var acc:f32[8]=default<f32[8]>();
            var accm:f32[8]=default<f32[8]>();
            for b in nb {
                let ab=(y*nb+b)*292;
                let bb=(x*nb+b)*1152;
                let yd=from_bits(rd32(vy,ab) as! u32);
                let sm=block_scales(vx,bb);
                var q8p:i32[8]=default<i32[8]>();
                for sb in 8 {
                    let b0=(rd16(vy,ab+260+4*sb) as! i16) as i32;
                    let b1=(rd16(vy,ab+262+4*sb) as! i16) as i32;
                    q8p[sb]=b0+b1;
                }
                for j in 8 {
                    var isum:i32=0;
                    for g in 4 {
                        for u in 4 {
                            for i in 8 {
                                let byte=vx[bb+128+(4*g+u)*64+j*8+i];
                                let lo=(byte&15) as i32;
                                let hi=(byte>>4) as i32;
                                let idx=g*64+u*8+i;
                                isum+=lo*((vy[ab+4+idx] as! i8) as i32)*sm[(2*g)*8+j];
                                isum+=hi*((vy[ab+4+idx+32] as! i8) as i32)*sm[(2*g+1)*8+j];
                            }
                        }
                    }
                    var msum:i32=0;
                    for sb in 8 {msum+=sm[64+sb*8+j]*q8p[sb];}
                    acc[j]=fma(isum as f32,half(rd16(vx,bb+2*j) as! u16)*yd,acc[j]);
                    accm[j]=fma(msum as f32,half(rd16(vx,bb+16+2*j) as! u16)*yd,accm[j]);
                }
            }
            for j in 8 {s[y*nr+x*8+j]=acc[j]-accm[j];}
        }
    }
    witness(2,nr*nb*(nc/8));
}

fn gemm(n:i64,s:f32[:],bs:i64,vx:const u8[:],vy:const u8[:],nr:i64,nc:i64) {
    assert(n>0&&n%256==0&&nr>0&&nr%4==0&&nc>0&&nc%8==0);
    let nb=n/256;
    assert(vx.len==(nc/8)*nb*1152);
    assert(vy.len==(nr/4)*nb*1168);
    for y in nr/4 {
        for x in nc/8 {
            var acc:f32[4*8]=default<f32[4*8]>();
            var accm:f32[4*8]=default<f32[4*8]>();
            for b in nb {
                let ab=(y*nb+b)*1168;
                let bb=(x*nb+b)*1152;
                let sm=block_scales(vx,bb);
                for m in 4 {
                    let yd=from_bits(rd32(vy,ab+4*m) as! u32);
                    var q8:i8[256]=default<i8[256]>();
                    for p in 256 {
                        let g=p/8;
                        let o=p%8;
                        q8[p]=vy[ab+16+g*32+m*8+o] as! i8;
                    }
                    var q8p:i32[8]=default<i32[8]>();
                    for sb in 8 {
                        var sum:i32=0;
                        for u in 32 {sum+=(q8[sb*32+u] as i32);}
                        q8p[sb]=sum;
                    }
                    for g in 4 {
                        for j in 8 {
                            var isum:i32=0;
                            for u in 4 {
                                for i in 8 {
                                    let byte=vx[bb+128+(4*g+u)*64+j*8+i];
                                    let lo=(byte&15) as i32;
                                    let hi=(byte>>4) as i32;
                                    let idx=g*64+u*8+i;
                                    isum+=lo*(q8[idx] as i32)*sm[(2*g)*8+j];
                                    isum+=hi*(q8[idx+32] as i32)*sm[(2*g+1)*8+j];
                                }
                            }
                            let msum=sm[64+(2*g)*8+j]*q8p[2*g]+sm[64+(2*g+1)*8+j]*q8p[2*g+1];
                            acc[m*8+j]=fma(isum as f32,half(rd16(vx,bb+2*j) as! u16)*yd,acc[m*8+j]);
                            accm[m*8+j]=fma(msum as f32,half(rd16(vx,bb+16+2*j) as! u16)*yd,accm[m*8+j]);
                        }
                    }
                }
            }
            for m in 4 {for j in 8 {s[(y*4+m)*bs+x*8+j]=acc[m*8+j]-accm[m*8+j];}}
        }
    }
    witness(3,(nr/4)*nb*(nc/8));
}

fn entry(mode:i64,a:i64)->i64 {
    return mode+a;
}
// Keeps every exported numerical unit reachable for the compiler; the runtime
// calls the generated functions directly and never executes this main.
fn main() {
    let a:u8[1]=default<u8[1]>();
    let b:u8[1]=default<u8[1]>();
    let f:f32[1]=default<f32[1]>();
    make_block(a,b,8,1,8);
    quantize_mat(f,a,256);
    gemv(256,f,1,b,b,1,8);
    gemm(256,f,1,b,b,4,8);
    print(entry(0,0));
}
