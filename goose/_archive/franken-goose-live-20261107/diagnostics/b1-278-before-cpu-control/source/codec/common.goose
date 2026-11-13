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
