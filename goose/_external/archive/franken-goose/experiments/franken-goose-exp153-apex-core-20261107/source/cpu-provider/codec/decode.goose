// Q4_K/Q5_K/Q6_K super-block row decoding for the Apex CPU provider.
// This is the accepted codec/decode.goose byte layout restricted to the
// quantisation types that are reachable in the normal Apex Q4_K_M graph.
namespace decode;
struct TileTraits { bytes:i64, sub:i64, bias:i64 }
struct WeightTile { q:u8[32][8][256], d:f32[1], dmin:f32[1], scales:i32[8][512], mins:i32[8][512] }
extern "fg141_half" fn half(x:u16)->f32;

fn tile_traits(kind:i64)->TileTraits {
    assert(kind>=0&&kind<13);
    if kind==0 {return TileTraits{144,32,0};}
    if kind==1 {return TileTraits{176,32,0};}
    if kind==2 {return TileTraits{210,16,32};}
    return TileTraits{0,0,0};
}
fn super_kind(t:i64)->i64 {
    if t==12 {return 0;}
    if t==13 {return 1;}
    if t==14 {return 2;}
    return -1;
}
fn read16(x:const u8[:],first:i64)->u16 {return (x[first] as u16)|((x[first+1] as u16)<<8);}
fn read32(x:const u8[:],first:i64)->u32 {return (read16(x,first) as u32)|((read16(x,first+2) as u32)<<16);}
fn load32(x:const u8[:],first:i64)->u8[32] {
    let bytes_view=x[first..first+32];var out:u8[32]=default<u8[32]>();for i in 32 {out[i]=bytes_view[i];}return out;
}
fn and8(a:u8[32],mask:u8)->u8[32] {var out:u8[32]=default<u8[32]>();for i in 32 {out[i]=a[i]&mask;}return out;}
fn shr8(a:u8[32],count:u8)->u8[32] {var out:u8[32]=default<u8[32]>();for i in 32 {out[i]=a[i]>>count;}return out;}
fn bitscale(a:WeightTile&,index:i64,value:i32){a.scales[index/8][index%8]=value;}
fn bitmin(a:WeightTile&,index:i64,value:i32){a.mins[index/8][index%8]=value;}
fn code(a:WeightTile&,panel_index:i64,index:i64,value:u8){a.q[panel_index][index/32][index%32]=value;}

fn unpack_block(kind:i64,x:const u8[:],base:i64,a:WeightTile&) {
    let f=tile_traits(kind);let nb=256/f.sub;
    a.d[0]=(0 as f32);a.dmin[0]=(0 as f32);
    for j in nb {bitscale(a,j,0);bitmin(a,j,0);}
    if kind==0 {
        a.d[0]=half(read16(x,base));a.dmin[0]=half(read16(x,base+2));
        for j in 4 {
            let s0=x[base+4+j];let m0=x[base+8+j];let both=x[base+12+j];
            bitscale(a,j,(s0&63) as i32);bitmin(a,j,(m0&63) as i32);
            bitscale(a,j+4,((both&15)|((s0>>6)<<4)) as i32);bitmin(a,j+4,((both>>4)|((m0>>6)<<4)) as i32);
        }
        for j in 4 {
            let packed=load32(x,base+16+j*32);let lo=and8(packed,15);let hi=shr8(packed,4);
            a.q[0][2*j]=lo;a.q[0][2*j+1]=hi;
        }
    } else if kind==1 {
        a.d[0]=half(read16(x,base));a.dmin[0]=half(read16(x,base+2));
        for j in 4 {
            let s0=x[base+4+j];let m0=x[base+8+j];let both=x[base+12+j];
            bitscale(a,j,(s0&63) as i32);bitmin(a,j,(m0&63) as i32);
            bitscale(a,j+4,((both&15)|((s0>>6)<<4)) as i32);bitmin(a,j+4,((both>>4)|((m0>>6)<<4)) as i32);
        }
        let high=load32(x,base+16);
        for j in 4 {
            let packed=load32(x,base+48+j*32);let lo=and8(packed,15);let hi=shr8(packed,4);
            for lane in 32 {lo[lane]=lo[lane]|(((high[lane]>>((2*j) as u8))&1)<<4);hi[lane]=hi[lane]|(((high[lane]>>((2*j+1) as u8))&1)<<4);}
            a.q[0][2*j]=lo;a.q[0][2*j+1]=hi;
        }
    } else {
        assert(kind==2);
        a.d[0]=half(read16(x,base+208));
        for s in 16 {bitscale(a,s,(x[base+192+s] as! i8) as i32);}
        for h in 2 {for lane in 32 {
            let high=x[base+128+h*32+lane];let lo0=x[base+h*64+lane];let lo1=x[base+h*64+32+lane];
            a.q[0][h*4][lane]=(lo0&15)|((high&3)<<4);a.q[0][h*4+1][lane]=(lo1&15)|((high&12)<<2);
            a.q[0][h*4+2][lane]=(lo0>>4)|(high&48);a.q[0][h*4+3][lane]=(lo1>>4)|((high&192)>>2);
        }}
    }
}

// raw must contain dst.len/256 whole super-blocks in contiguous order.
fn row_entry(weight_type:i64,raw:const u8[:],dst:f32[:])->bool {
    let kind=super_kind(weight_type);
    if kind<0 {return false;}
    let f=tile_traits(kind);
    if f.bytes==0||dst.len<=0||dst.len%256!=0||raw.len<(dst.len/256)*f.bytes {return false;}
    var decoded:WeightTile=default<WeightTile>();
    for ib in dst.len/256 {
        unpack_block(kind,raw,ib*f.bytes,decoded);
        for lane in 256 {
            let sub=lane/f.sub;
            let scale=decoded.scales[sub/8][sub%8] as f32;
            let minimum=decoded.mins[sub/8][sub%8] as f32;
            let q=(decoded.q[0][lane/32][lane%32] as i32)-f.bias;
            dst[ib*256+lane]=(decoded.d[0]*scale)*(q as f32)-decoded.dmin[0]*minimum;
        }
    }
    return true;
}
fn main(){let x:u8[144]=default<u8[144]>();var out:f32[256]=default<f32[256]>();print(row_entry(12,x,out));}
