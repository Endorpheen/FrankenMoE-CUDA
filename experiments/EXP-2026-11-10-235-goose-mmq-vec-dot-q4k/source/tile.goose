// The MMQ tile loop for Q4_K in Goose (APX: P5).
//
// Native (mmq-vec-dot.cuh:905, ggml_cuda_mmq_vec_dot_q4_K_q8_1_dp4a):
//
//   for (k01 = 0; k01 < MMQ_TILE_NE_K; k01 += QR4_K*VDR = 16) {
//     k0 = k00 + k01;
//     for (j0 = 0; j0 < J; j0 += nwarps) {
//       j = j0 + threadIdx.y;
//       for (i0 = 0; i0 < I; i0 += warp_size) {
//         i = i0 + threadIdx.x;
//         sc = (uint8_t*)&x_sc[i*(MMQ_TILE_NE_K/8) + i/8 + k0/32] + 2*(k01/16);
//         sum[j0/nwarps*I/warp_size + i0/warp_size] += vd(&x_qs[i*(MMQ_TILE_NE_K+1) + k0/2],
//                                                          &y_qs[j*MMQ_TILE_Y_K + k01], sc, sc+8,
//                                                          x_dm[i], &y_ds[j*MMQ_TILE_Y_K + k01/QI8_1]);
//       }
//     }
//   }
//
// Porting the tile loop, not just the inner product, keeps the boundary at one call per thread instead
// of one per element. That matters: EXP223 showed a per-element crossing is what makes a port lose
// regardless of how good the inner code is.
//
// All strides are passed in rather than recomputed, so the module does not need to know the tile geometry
// constants. The accumulator is `sum`, which the native code ADDS into -- the += is why this returns
// nothing and writes in place.

extern "fg235t_fail" fn fail(code:i64);
extern "fg235t_ld_i32" fn ld_i32(base:u64,idx:i64)->i64;
extern "fg235t_ld_f32" fn ld_f32(base:u64,idx:i64)->f32;
extern "fg235t_st_f32" fn st_f32(base:u64,idx:i64,v:f32);
extern "fg235t_vd" fn vd(v:u64,u:u64,sc:u64,m:u64,dm4x:f32,dm4y:f32,dsx:u64,dsy:u64)->f32;
extern "fg235t_split_h2" fn split_h2(src:u64,idx:i64,dstx:u64,dsty:u64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// tile_q4k accumulates into sum. Arguments are the pointers and strides the native loop uses.
fn tile_q4k(x_qs:u64,x_dm:u64,x_sc:u64,y_qs:u64,y_ds:u64,sum:u64,
            k00:i64,J:i64,nwarps:i64,I:i64,warp_size:i64,tile_ne_k:i64,i8_1:i64,
            tx:i64,ty:i64,scratch:u64)->i64{
 ensure(J>=0&&I>=0&&warp_size>0&&nwarps>0,1400);
 ensure(tile_ne_k>0&&i8_1>0,1401);
 var k01:i64=0;
 var calls:i64=0;
 while k01<tile_ne_k{
  let k0:i64=k00+k01;
  var j0:i64=0;
  while j0<J{
   let j:i64=j0+ty;
   var i0:i64=0;
   while i0<I{
    let i:i64=i0+tx;
    // x_sc is an `int *`, so &x_sc[idx] is idx*4 bytes; the native code then adds 2*(k01/16) BYTES.
    // Mixing those up is the first bug I had here: multiplying the byte term by 4 again.
    let sc_idx:i64=i*((tile_ne_k/(8 as i64)))+(i/(8 as i64))+(k0/(32 as i64));
    let sc_byte:i64=sc_idx*(4 as i64)+(2 as i64)*(k01/(16 as i64));
    split_h2(x_dm,i,scratch,scratch+4 as i64);
    split_h2(y_ds,j*(tile_ne_k+(tile_ne_k/i8_1))+k01/i8_1,scratch+8 as i64,scratch+12 as i64);
    let xo:i64=i*(tile_ne_k+1 as i64)+(k0/(2 as i64));
    let yo:i64=j*(tile_ne_k+i8_1)+k01;
    let r:f32=vd((x_qs as i64+xo*(4 as i64)) as u64,
                 (y_qs as i64+yo*(4 as i64)) as u64,
                 (x_sc as i64+sc_byte) as u64, (x_sc as i64+sc_byte+8 as i64) as u64,
                 ld_f32(scratch,0 as i64),ld_f32(scratch,1 as i64),
                 scratch+24 as i64,scratch+28 as i64);
    let si:i64=(j0/nwarps)*(I/warp_size)+i0/warp_size;
    st_f32(sum,si,ld_f32(sum,si)+r);
    calls=calls+1 as i64;
    i0=i0+warp_size;
   }
   j0=j0+nwarps;
  }
  k01=k01+(16 as i64);
 }
 calls
}

fn main(){
 tile_q4k(0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,
          0 as i64,0 as i64,1 as i64,0 as i64,32 as i64,256 as i64,8 as i64,
          0 as i64,0 as i64,0 as u64);
}
