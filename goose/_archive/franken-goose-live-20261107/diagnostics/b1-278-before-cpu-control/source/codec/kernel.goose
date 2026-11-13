import .packed_simd;
struct Plan { mode:i64,kind:i64,first:i64,stride:i64,start:i64,count:i64,nk:i64,row0:i64,col0:i64,out0:i64,slab:i64,route_first:i64,slots:i64,blocks:i64,y_stride:i64,origin:i64,input_first:i64,output_base:i64,dense:i64,defer_bias:i64,width:i64,output_rows:i64,ith:i64,nth:i64,enabled:i64,forced:i64,out_stride1:i64,out_stride2:i64,out_stride3:i64,n11:i64,n12:i64,n13:i64,n02:i64,n03:i64,nb02:i64,nb03:i64,context:i64,need_quant:i64,src_nb0:i64,src_nb1:i64,src_nb2:i64,src_nb3:i64,workspace_mod:i64,wdata_bytes:i64 }
struct TileTraits { bytes:i64,sub:i64,has_min:i64,bias:i64,act:i64 }
struct Route { slot:i32,token:i32 }
struct Q8 { d:f32,q:i8[32][8],sums:i16[16] }
struct WeightTile { q:u8[32][8][256],d:f32[256],dmin:f32[256],scales:i32[8][512],mins:i32[8][512] }
struct ActivationTile { q:i8[32][8][256],sums:i32[8][512],d:f32[256] }
struct Workspace { a:WeightTile,b:ActivationTile,acc:f32[8][8192] }
extern "fg141_half" fn half(x:u16)->f32;
extern "fmaf" fn fma(x:f32,y:f32,z:f32)->f32;
extern "fg141_witness" fn witness(stage:i64,kind:i64,units:i64);
fn tile_traits(kind:i64)->TileTraits {
    assert(kind>=0&&kind<13);
    if kind==0 {return TileTraits{144,32,1,0,0};}
    if kind==1 {return TileTraits{176,32,1,0,0};}
    if kind==2 {return TileTraits{210,16,0,32,1};}
    if kind==3 {return TileTraits{110,16,0,4,1};}
    if kind==4 {return TileTraits{84,16,1,0,0};}
    if kind==5 {return TileTraits{136,32,0,128,0};}
    if kind==6 {return TileTraits{66,32,0,128,1};}
    if kind==7 {return TileTraits{74,16,0,128,1};}
    if kind==8 {return TileTraits{82,16,0,128,1};}
    if kind==9 {return TileTraits{98,32,0,128,1};}
    if kind==10 {return TileTraits{110,32,0,128,1};}
    if kind==11 {return TileTraits{50,32,0,128,1};}
    return TileTraits{56,16,0,128,1};
}
fn minimum(a:i64,b:i64)->i64 {return if a<b {a} else {b};}
fn round16(a:i64)->i64 {return ((a+15)/16)*16;}
fn read16(x:const u8[:],first:i64)->u16 {return (x[first] as u16)|((x[first+1] as u16)<<8);}
fn read32(x:const u8[:],first:i64)->u32 {return (read16(x,first) as u32)|((read16(x,first+2) as u32)<<16);}
fn load32(x:const u8[:],first:i64)->u8[32] {
    let bytes_view=x[first..first+32];var out:u8[32]=default<u8[32]>();for i in 32 {out[i]=bytes_view[i];}return out;
}
fn bitscale(a:WeightTile&,index:i64,value:i32){a.scales[index/8][index%8]=value;}
fn bitmin(a:WeightTile&,index:i64,value:i32){a.mins[index/8][index%8]=value;}
fn code(a:WeightTile&,panel_index:i64,index:i64,value:u8){a.q[panel_index][index/32][index%32]=value;}
fn as_unsigned(x:i8[32])->u8[32] {var out:u8[32]=default<u8[32]>();for i in 32 {out[i]=x[i] as! u8;}return out;}
fn debias(x:u8[32],bias:u8)->i8[32] {var out:i8[32]=default<i8[32]>();for i in 32 {out[i]=(x[i]-bias) as! i8;}return out;}
fn biased(x:i8[32])->i8[32] {var out:i8[32]=default<i8[32]>();for i in 32 {out[i]=((x[i] as! u8)^128) as! i8;}return out;}
fn pair_scales(low:i16,high:i16)->i16[16] {var out:i16[16]=default<i16[16]>();for i in 8 {out[i]=low;out[i+8]=high;}return out;}
fn splat32(value:i32)->i32[8] {var out:i32[8]=default<i32[8]>();for i in 8 {out[i]=value;}return out;}
fn sub32(x:i32[8],y:i32[8])->i32[8] {var out:i32[8]=default<i32[8]>();for i in 8 {out[i]=x[i]-y[i];}return out;}
fn hsum(x:i32[8])->i32 {let p=packed_simd::pair_sum32(x);return (p[0]+p[1])+(p[2]+p[3]);}
fn scaled(d:f32,x:i32[8])->f32[8] {var out:f32[8]=default<f32[8]>();for i in 8 {out[i]=d*(x[i] as f32);}return out;}
fn signed_pairs(w:i8[32],y:i8[32])->i16[16] {
    var mag:u8[32]=default<u8[32]>();var sy:i8[32]=default<i8[32]>();
    for i in 32 {let wb=w[i] as! u8;let yb=y[i] as! u8;let mask=((0 as u8)-(wb>>7)) as! u8;let neg=((0 as u8)-wb) as! u8;let nz=((0 as u8)-((wb|neg)>>7)) as! u8;mag[i]=((wb^mask)-mask) as! u8;sy[i]=((((yb^mask)-mask) as! u8)&nz) as! i8;}
    return packed_simd::maddubs(mag,sy);
}
fn grid8(a:WeightTile&,panel_index:i64,offset:i64,grid:u8[8],sign:u8) {
    for lane in 8 {let mask=((0 as u8)-((sign>>(lane as u8))&1)) as! u8;code(a,panel_index,offset+lane,(((grid[lane]^mask)-mask)+128) as! u8);}
}
fn tern8(a:WeightTile&,panel_index:i64,offset:i64,grid:i8[8],delta:i16) {
    for lane in 8 {code(a,panel_index,offset+lane,(128+delta+8*(grid[lane] as i16)) as! u8);}
}

fn unpack_block(kind:i64,x:const u8[:],base:i64,panel_index:i64,di:i64,a:WeightTile&,gxx:const u8[8][:],gxs:const u8[8][:],gs:const u8[8][:],g3x:const u8[4][:],g3s:const u8[4][:],g1:const i8[8][:],signs:const u8[:],palette:const i8[:]) {
    let f=tile_traits(kind);let nb=256/f.sub;let si=panel_index*nb;
    a.dmin[di]=(0 as f32);
    for j in nb {bitmin(a,si+j,0);}
    if kind==0||kind==1 {
        a.d[di]=half(read16(x,base));a.dmin[di]=half(read16(x,base+2));
        for j in 4 {
            let s0=x[base+4+j];let m0=x[base+8+j];let both=x[base+12+j];
            bitscale(a,si+j,(s0&63) as i32);bitmin(a,si+j,(m0&63) as i32);
            bitscale(a,si+j+4,((both&15)|((s0>>6)<<4)) as i32);bitmin(a,si+j+4,((both>>4)|((m0>>6)<<4)) as i32);
        }
        let qbase=base+(if kind==0 {16} else {48});
        for j in 4 {
            let packed=load32(x,qbase+j*32);var lo=packed_simd::and8(packed,15);var hi=packed_simd::shr8(packed,4);
            if kind==1 {let high=load32(x,base+16);for lane in 32 {lo[lane]=lo[lane]|(((high[lane]>>((2*j) as u8))&1)<<4);hi[lane]=hi[lane]|(((high[lane]>>((2*j+1) as u8))&1)<<4);}}
            a.q[panel_index][2*j]=lo;a.q[panel_index][2*j+1]=hi;
        }
    } else if kind==2 {
        a.d[di]=half(read16(x,base+208));for s in 16 {bitscale(a,si+s,(x[base+192+s] as! i8) as i32);}
        for h in 2 {for lane in 32 {
            let high=x[base+128+h*32+lane];let lo0=x[base+h*64+lane];let lo1=x[base+h*64+32+lane];
            a.q[panel_index][h*4][lane]=(lo0&15)|((high&3)<<4);a.q[panel_index][h*4+1][lane]=(lo1&15)|((high&12)<<2);
            a.q[panel_index][h*4+2][lane]=(lo0>>4)|(high&48);a.q[panel_index][h*4+3][lane]=(lo1>>4)|((high&192)>>2);
        }}
    } else if kind==3 {
        a.d[di]=half(read16(x,base+108));
        for j in 4 {
            let lo=x[base+96+j];let hi=x[base+100+j];let packed=x[base+104+j];
            bitscale(a,si+j,(((lo&15)|((packed&3)<<4)) as i32)-32);
            bitscale(a,si+4+j,(((hi&15)|(((packed>>2)&3)<<4)) as i32)-32);
            bitscale(a,si+8+j,(((lo>>4)|(((packed>>4)&3)<<4)) as i32)-32);
            bitscale(a,si+12+j,(((hi>>4)|(((packed>>6)&3)<<4)) as i32)-32);
        }
        for h in 2 {for group in 4 {let raw=load32(x,base+32+h*32);let high=load32(x,base);var out:u8[32]=default<u8[32]>();for lane in 32 {out[lane]=((raw[lane]>>((2*group) as u8))&3)|(((high[lane]>>((h*4+group) as u8))&1)<<2);}a.q[panel_index][h*4+group]=out;}}
    } else if kind==4 {
        a.d[di]=half(read16(x,base+80));a.dmin[di]=half(read16(x,base+82));
        for s in 16 {bitscale(a,si+s,(x[base+s]&15) as i32);bitmin(a,si+s,(x[base+s]>>4) as i32);}
        for h in 2 {let raw=load32(x,base+16+h*32);for group in 4 {a.q[panel_index][h*4+group]=packed_simd::and8(packed_simd::shr8(raw,(2*group) as u8),3);}}
    } else if kind==5 {
        a.d[di]=half(read16(x,base));let high=read16(x,base+2);
        for s in 8 {let packed=((x[base+4+s/2]>>((4*(s%2)) as u8))&15) as u16;bitscale(a,si+s,((packed|(((high>>((2*s) as u16))&3)<<4)) as i32)-32);}
        var lookup:u8[32]=default<u8[32]>();for lane in 16 {let value=((palette[lane] as i16)+128) as! u8;lookup[lane]=value;lookup[lane+16]=value;}
        for group in 4 {let raw=load32(x,base+8+group*32);let lo=packed_simd::shuffle8(lookup,packed_simd::and8(raw,15));let hi=packed_simd::shuffle8(lookup,packed_simd::shr8(raw,4));for lane in 16 {a.q[panel_index][2*group][lane]=lo[lane];a.q[panel_index][2*group][lane+16]=hi[lane];a.q[panel_index][2*group+1][lane]=lo[lane+16];a.q[panel_index][2*group+1][lane+16]=hi[lane+16];}}
    } else if kind==6 {
        a.d[di]=half(read16(x,base))*(0.125 as f32);
        for sub in 8 {let info=read32(x,base+2+sub*8+4);bitscale(a,si+sub,(2*(info>>28)+1) as i32);for group in 4 {grid8(a,panel_index,sub*32+group*8,gxx[x[base+2+sub*8+group] as i64],signs[((info>>((group*7) as u32))&127) as i64]);}}
    } else if kind==7||kind==8 {
        a.d[di]=half(read16(x,base))*(0.125 as f32);let scales=base+(if kind==7 {66} else {74});
        for sub in 8 {
            bitscale(a,si+2*sub,(2*(x[scales+sub]&15)+1) as i32);bitscale(a,si+2*sub+1,(2*(x[scales+sub]>>4)+1) as i32);
            for group in 4 {
                if kind==7 {let info=read16(x,base+2+(sub*4+group)*2);grid8(a,panel_index,sub*32+group*8,gxs[(info&511) as i64],signs[(info>>9) as i64]);}
                else {let index=(x[base+2+sub*4+group] as u16)|(((x[base+66+sub] as u16)<<((8-2*group) as u16))&768);grid8(a,panel_index,sub*32+group*8,gs[index as i64],x[base+34+sub*4+group]);}
            }
        }
    } else if kind==9 {
        a.d[di]=half(read16(x,base))*(0.25 as f32);
        for sub in 8 {let info=read32(x,base+66+sub*4);bitscale(a,si+sub,(2*(info>>28)+1) as i32);for group in 4 {let lo=g3x[x[base+2+sub*8+group*2] as i64];let hi=g3x[x[base+2+sub*8+group*2+1] as i64];var grid:u8[8]=default<u8[8]>();for lane in 4 {grid[lane]=lo[lane];grid[lane+4]=hi[lane];}grid8(a,panel_index,sub*32+group*8,grid,signs[((info>>((group*7) as u32))&127) as i64]);}}
    } else if kind==10 {
        a.d[di]=half(read16(x,base));
        for sub in 8 {let packed=x[base+106+sub/2];bitscale(a,si+sub,(1+2*((packed>>((4*(sub%2)) as u8))&15)) as i32);for group in 4 {
            let high=x[base+66+sub] as u16;let lo=(x[base+2+sub*8+group*2] as u16)|((high<<((8-2*group) as u16))&256);let hi=(x[base+2+sub*8+group*2+1] as u16)|((high<<((7-2*group) as u16))&256);
            let g0=g3s[lo as i64];let g2=g3s[hi as i64];var grid:u8[8]=default<u8[8]>();for lane in 4 {grid[lane]=g0[lane];grid[lane+4]=g2[lane];}grid8(a,panel_index,sub*32+group*8,grid,x[base+74+sub*4+group]);
        }}
    } else if kind==11 {
        a.d[di]=half(read16(x,base))*(0.125 as f32);
        for sub in 8 {let high=read16(x,base+34+sub*2);bitscale(a,si+sub,(2*((high>>12)&7)+1) as i32);let delta:i16=if (high&32768)!=0 {(-1 as i16)} else {(1 as i16)};for group in 4 {let index=(x[base+2+sub*4+group] as u16)|(((high>>((3*group) as u16))&7)<<8);tern8(a,panel_index,sub*32+group*8,g1[index as i64],delta);}}
    } else {
        assert(kind==12);let s0=read16(x,base+48);let s1=read16(x,base+50);let s2=read16(x,base+52);let s3=read16(x,base+54);
        let d=(s0>>12)|((s1>>8)&240)|((s2>>4)&3840)|(s3&61440);a.d[di]=half(d)*(0.125 as f32);
        for sub in 8 {
            let scales=read16(x,base+48+(sub/2)*2);let sh=(6*(sub%2)) as u16;bitscale(a,si+sub*2,(2*((scales>>sh)&7)+1) as i32);bitscale(a,si+sub*2+1,(2*((scales>>(sh+3))&7)+1) as i32);
            for group in 4 {let high=x[base+32+sub*2+group/2] as u16;let shift=(if group%2==0 {8} else {4}) as u16;let index=(x[base+sub*4+group] as u16)|((high<<shift)&1792);let delta:i16=if ((high>>((if group%2==0 {3} else {7}) as u16))&1)!=0 {(-1 as i16)} else {(1 as i16)};tern8(a,panel_index,sub*32+group*8,g1[index as i64],delta);}
        }
    }
}
fn unpack_panel(p:const Plan&,x:const u8[:],w:Workspace&,gxx:const u8[8][:],gxs:const u8[8][:],gs:const u8[8][:],g3x:const u8[4][:],g3s:const u8[4][:],g1:const i8[8][:],signs:const u8[:],palette:const i8[:]) {
    let f=tile_traits(p.kind);let nb=256/f.sub;let a:WeightTile& = &w.a;
    assert(p.first>=0&&p.stride>=f.bytes&&p.start>=0&&p.count>=0&&p.count<=256&&p.nk>=1&&p.nk<=16&&round16(p.count)*p.nk<=256);
    assert(p.nk==1||p.count<=16);
    let ones16=packed_simd::splat16(1);var ones:i8[32]=default<i8[32]>();for lane in 32 {ones[lane]=1;}
    for slab in p.nk {for row in round16(p.count) {
        let panel_index=row*p.nk+slab;let di=slab*16+row;
        if row<p.count {unpack_block(p.kind,x,p.origin+(p.first+row)*p.stride+(p.start+slab)*f.bytes,panel_index,di,a,gxx,gxs,gs,g3x,g3s,g1,signs,palette);}
        else {a.d[di]=(0 as f32);a.dmin[di]=(0 as f32);for j in nb {bitscale(a,panel_index*nb+j,0);bitmin(a,panel_index*nb+j,0);}for j in 8 {a.q[panel_index][j]=default<u8[32]>();}}
        if f.act!=0 {
            var correction:i32=0;
            for j in 8 {
                let sums=packed_simd::madd16(ones16,packed_simd::maddubs(a.q[panel_index][j],ones));
                if f.sub==32 {correction+=128*a.scales[(panel_index*nb+j)/8][(panel_index*nb+j)%8]*(hsum(sums)-(32*(f.bias as i32)));}
                else {let lo=(sums[0]+sums[1])+(sums[2]+sums[3]);let hi=(sums[4]+sums[5])+(sums[6]+sums[7]);let s=panel_index*nb+2*j;correction+=128*a.scales[s/8][s%8]*(lo-16*(f.bias as i32))+128*a.scales[(s+1)/8][(s+1)%8]*(hi-16*(f.bias as i32));}
            }
            bitmin(a,panel_index*nb,correction);
        }
    }}
    witness(0,p.kind,p.count*p.nk);
}

extern "fg143_codec_from_bits" fn from_bits(x:u32)->f32;
extern "ldexpf" fn ldexp32(x:f32,p:i32)->f32;
fn super_kind(t:i64)->i64 {
    if t==12 {return 0;}if t==13 {return 1;}if t==14 {return 2;}if t==11 {return 3;}if t==10 {return 4;}if t==23 {return 5;}if t==16 {return 6;}if t==17 {return 7;}if t==22 {return 8;}if t==18 {return 9;}if t==21 {return 10;}if t==19 {return 11;}if t==29 {return 12;}return -1;
}
fn compact_width(t:i64)->i64 {if t==41 {return 128;}if t==42||t==40 {return 64;}if t==15 {return 256;}return 32;}
fn compact_bytes(t:i64)->i64 {
    if t==2||t==20||t==41||t==42 {return 18;}if t==3 {return 20;}if t==6 {return 22;}if t==7 {return 24;}if t==8 {return 34;}if t==9||t==40 {return 36;}if t==39 {return 17;}if t==15 {return 292;}return 0;
}
fn fp4(index:u8)->f32 {
    let v=index&7;let magnitude=if v<=4 {v as i32} else if v==5 {6} else if v==6 {8} else {12};return (if index<8 {magnitude} else {-magnitude}) as f32;
}
fn ue4m3(x:u8)->f32 {
    if x==0||x==127 {return (0 as f32);}let exp=((x>>3)&15) as i32;let mantissa=(x&7) as f32;
    if exp==0 {return ldexp32(mantissa,-9)*(0.5 as f32);}return ldexp32((1 as f32)+mantissa/(8 as f32),exp-7)*(0.5 as f32);
}
fn compact_value(t:i64,x:const u8[:],base:i64,lane:i64,palette:const i8[:])->f32 {
    if t==15 {return from_bits(read32(x,base))*((x[base+4+lane] as! i8) as f32);}
    if t==39 {let exponent=x[base];let d=from_bits(if exponent<2 {(0x00200000 as u32)<<(exponent as u32)} else {((exponent-1) as u32)<<23});let bits=x[base+1+lane%16];let value=if lane<16 {bits&15} else {bits>>4};return fp4(value)*d;}
    if t==40 {let sub=lane/16;let i=lane%16;let bits=x[base+4+sub*8+i%8];let value=if i<8 {bits&15} else {bits>>4};return fp4(value)*ue4m3(x[base+sub]);}
    let d=half(read16(x,base));
    if t==41 {let bit=(x[base+2+lane/8]>>((lane%8) as u8))&1;return if bit!=0 {d} else {-d};}
    if t==42 {let q=(x[base+2+lane/4]>>((2*(lane%4)) as u8))&3;return ((q as i32)-1) as f32*d;}
    if t==8 {return ((x[base+2+lane] as! i8) as f32)*d;}if t==9 {return ((x[base+4+lane] as! i8) as f32)*d;}
    if t==2||t==3||t==20 {let qbase=base+(if t==3 {4} else {2});let bits=x[qbase+lane%16];let q=if lane<16 {bits&15} else {bits>>4};if t==20 {return (palette[q as i64] as f32)*d;}if t==2 {return (((q as i32)-8) as f32)*d;}return (q as f32)*d+half(read16(x,base+2));}
    assert(t==6||t==7);let high=read32(x,base+(if t==7 {4} else {2}));let bits=x[base+(if t==7 {8} else {6})+lane%16];let q=((if lane<16 {bits&15} else {bits>>4}) as u32)|(((high>>(lane as u32))&1)<<4);
    if t==6 {return (((q as i32)-16) as f32)*d;}return (q as f32)*d+half(read16(x,base+2));
}
fn row_entry(weight_type:i64,raw:const u8[:],dst:f32[:],gxx:const u8[8][:],gxs:const u8[8][:],gs:const u8[8][:],g3x:const u8[4][:],g3s:const u8[4][:],g1:const i8[8][:],signs:const u8[:],palette:const i8[:])->bool {
    let kind=super_kind(weight_type);
    if kind<0 {let width=compact_width(weight_type);let bytes=compact_bytes(weight_type);if bytes==0 {return false;}assert(dst.len%width==0&&raw.len>=(dst.len/width)*bytes);for ib in dst.len/width {for lane in width {dst[ib*width+lane]=compact_value(weight_type,raw,ib*bytes,lane,palette);}}return true;}
    let f=tile_traits(kind);assert(dst.len%256==0&&raw.len>=(dst.len/256)*f.bytes);var decoded:WeightTile=default<WeightTile>();
    for ib in dst.len/256 {
        unpack_block(kind,raw,ib*f.bytes,0,0,decoded,gxx,gxs,gs,g3x,g3s,g1,signs,palette);
        for lane in 256 {
            let sub=lane/f.sub;let scale=decoded.scales[sub/8][sub%8] as f32;let minimum=decoded.mins[sub/8][sub%8] as f32;let q=(decoded.q[0][lane/32][lane%32] as i32)-f.bias as i32;
            dst[ib*256+lane]=(decoded.d[0]*scale)*(q as f32)-decoded.dmin[0]*minimum;
        }
    }
    return true;
}
fn main(){let x:u8[18]=default<u8[18]>();var out:f32[32]=default<f32[32]>();let g8:u8[8][1]=default<u8[8][1]>();let g4:u8[4][1]=default<u8[4][1]>();let g1:i8[8][1]=default<i8[8][1]>();let signs:u8[128]=default<u8[128]>();let palette:i8[16]=default<i8[16]>();let ok=row_entry(2,x,out,g8,g8,g8,g4,g4,g1,signs,palette);print(ok);}
