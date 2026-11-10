// vec_dot_q4_K_q8_1_impl_mmq in Goose (APX: P5, the inner kernel of the 30% mul_mat_q block).
//
// Native (vecdotq.cuh:534):
//     float sumf_d = 0, sumf_m = 0;
//     for (i = 0; i < QR4_K*VDR_Q4_K_Q8_1_MMQ/QI8_1; ++i) {      // 2*8/8 = 2 iterations
//         int sumi_d = 0;
//         for (j = 0; j < QI8_1; ++j)                            // 8
//             sumi_d = ggml_cuda_dp4a((v[j] >> (4*i)) & 0x0F0F0F0F, u[i*QI8_1 + j], sumi_d);
//         float2 ds8f = __half22float2(ds8[i]);
//         sumf_d += ds8f.x * (sc[i] * sumi_d);
//         sumf_m += ds8f.y *   m[i];
//     }
//     float2 dm4f = __half22float2(dm4);
//     return dm4f.x*sumf_d - dm4f.y*sumf_m;
//
// The `dp4a` is ported here as real arithmetic rather than kept as a primitive: it is four byte
// products plus an accumulator, and doing it in Goose is what makes this a port instead of a wrapper.
// The byte extractions sign-extend, because `b` holds signed q8 values while `a` holds masked nibbles.
//
// The two half2 -> float2 conversions stay primitives: they are format decoding, not arithmetic, and the
// bridge already owns the half type.
//
// Accumulation uses i64 even where native uses int32. Every product is at most 15*127 in magnitude and
// there are 32 of them, so the value never approaches the int32 limit -- using a wider type cannot change
// the result, only remove a class of overflow I would otherwise have to reason about.

extern "fg235v_fail" fn fail(code:i64);
extern "fg235v_ld_i32" fn ld_i32(base:u64,idx:i64)->i64;
extern "fg235v_ld_u8" fn ld_u8(base:u64,idx:i64)->i64;

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// byte k of a 32-bit word, sign-extended as int8
fn byte_s(v:i64,k:i64)->i64{
 let b:i64=(v>>(k*8 as i64))&0xFF as i64;
 if b>=0x80 as i64{return b-0x100 as i64;}
 b
}

// byte k as unsigned
fn byte_u(v:i64,k:i64)->i64{
 (v>>(k*8 as i64))&0xFF as i64
}

// dp4a: dot product of four signed byte pairs, accumulated
fn dp4a(a:i64,b:i64,acc:i64)->i64{
 var s:i64=acc;
 var k:i64=0;
 while k<4 as i64{
  s=s+byte_s(a,k)*byte_s(b,k);
  k=k+1 as i64;
 }
 s
}

// vd_q4k: computes the dot product. sc and m are uint8 arrays, dsx/dsy the per-i float halves.
fn vd_q4k(v:u64,u:u64,sc:u64,m:u64,dsx:u64,dsy:u64,dm4x:f32,dm4y:f32,n_i:i64)->f32{
 ensure(n_i>=0,1300);
 var sumf_d:f32=0.0;
 var sumf_m:f32=0.0;
 var i:i64=0;
 while i<n_i{
  var sumi_d:i64=0;
  var j:i64=0;
  while j<8 as i64{
   let a:i64=(ld_i32(v,j)>>(4 as i64*i))&0x0F0F0F0F as i64;
   let b:i64=ld_i32(u,i*8 as i64+j);
   sumi_d=dp4a(a,b,sumi_d);
   j=j+1 as i64;
  }
  let sci:i64=ld_u8(sc,i);
  let mi:i64=ld_u8(m,i);
  let sxf:f32=ld_f32(dsx,i);
  let syf:f32=ld_f32(dsy,i);
  sumf_d=sumf_d+sxf*((sci as f32)*to_f32(sumi_d));
  sumf_m=sumf_m+syf*(mi as f32);
  i=i+1 as i64;
 }
 dm4x*sumf_d-dm4y*sumf_m
}

extern "fg235v_ld_f32" fn ld_f32(base:u64,idx:i64)->f32;
extern "fg235v_to_f32" fn to_f32(v:i64)->f32;

fn main(){
 vd_q4k(0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0.0,0.0,0 as i64);
 vd_q4k(8 as u64,16 as u64,24 as u64,32 as u64,40 as u64,48 as u64,1.0,0.5,2 as i64);
}
