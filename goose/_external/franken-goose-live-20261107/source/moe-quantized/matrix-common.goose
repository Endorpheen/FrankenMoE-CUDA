import .packed_simd;
struct Task { mode:i64, kind:i64, blocks:i64, rows:i64, weight_stride:i64, first:i64, cols:i64, slots:i64, dst_stride1:i64, dst_stride2:i64, route_first:i64, input_contiguous:i64, input_row_blocks:i64, input_stride1:i64, input_stride2:i64, output_stride:i64 }
struct Route { slot:i32, token:i32 }
struct Q2 { d:u16, q:u8[16] }
struct Q5 { d:u16, high:u8[4], q:u8[16] }
struct NL4 { d:u16, q:u8[16] }
struct IQ2X { d:u16, q:u8[64] }
struct IQ2S { d:u16, q:u8[64], high:u8[8], scales:u8[8] }
struct Y0 { d:u16, q:i8[32] }
struct YK { d:f32, q:i8[32][8], sums:i16[16] }
struct Decoded { q:u8[32], mask:u8[32], scales:i16[16] }
extern "fg139_half_float" fn half(x:u16)->f32;
extern "fmaf" fn fma(a:f32,b:f32,c:f32)->f32;
extern "fg139_iq4_tail_fused" fn iq4_tail_fused()->bool;
extern "fg139_math_witness" fn witness(mode:i64,kind:i64,blocks:i64,rows:i64,cols:i64);
fn addf(a:f32[8],b:f32[8])->f32[8] {var out:f32[8]=default<f32[8]>();for i in 8 {out[i]=a[i]+b[i];}return out;}
fn hsum(a:f32[8])->f32 {return ((a[4]+a[0])+(a[6]+a[2]))+((a[5]+a[1])+(a[7]+a[3]));}
fn sub16(a:i16[16],b:i16[16])->i16[16] {var out:i16[16]=default<i16[16]>();for i in 16 {out[i]=a[i]-b[i];}return out;}
fn signed_bytes(a:u8[32])->i8[32] {var out:i8[32]=default<i8[32]>();for i in 32 {out[i]=a[i] as! i8;}return out;}
fn flip(y:i8[32],mask:u8[32])->i8[32] {var out:i8[32]=default<i8[32]>();for i in 32 {out[i]=(((y[i] as! u8)^mask[i])-mask[i]) as! i8;}return out;}
fn signed_product(w:i8[32],y:i8[32])->i32[8] {
    var positive:u8[32]=default<u8[32]>();var sy:i8[32]=default<i8[32]>();
    for i in 32 {
        positive[i]=(if w[i]<0 {-(w[i] as i16)} else {w[i] as i16}) as! u8;
        sy[i]=(if w[i]<0 {-(y[i] as i16)} else if w[i]==0 {0} else {y[i] as i16}) as! i8;
    }
    return packed_simd::madd16(packed_simd::splat16(1),packed_simd::maddubs(positive,sy));
}
fn pair_scale(low:i16,high:i16)->i16[16] {var out:i16[16]=default<i16[16]>();for i in 8 {out[i]=low;out[i+8]=high;}return out;}
fn word32(bytes:u8[64],first:i64)->u32 {return (bytes[first] as u32)|((bytes[first+1] as u32)<<8)|((bytes[first+2] as u32)<<16)|((bytes[first+3] as u32)<<24);}

