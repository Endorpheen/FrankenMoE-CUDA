// Apex CUDA MMQ Q8_1 activation quantizer (Goose), from
// ggml-cuda/quantize.cu: quantize_mmq_q8_1.
//
// Scope: the active Apex configuration, which is ds_layout = DS4 with scatter =
// false on the quantize path and scatter = true for the compact-row scatter
// variant. The D4 and D2S6 layouts and the FP4 scatter are a different data layout
// and stay native; the host module refuses those so the parent falls back.
//
// Everything algorithmic is Goose: the 4-value thread stride, the guarded float4
// load, the partial absolute max, the scale-group shuffle reduction over
// vals_per_scale/8, the sum accumulation and its shuffle reduction over
// vals_per_sum/8, d_inv and the four rounded int8 values, the block index for both
// the direct and the scatter form, the char4 write and the half2 scale/sum store.
// Extern symbols are only the raw float4 load, the int32/int8/half2 stores, the
// shuffle primitive and the rounding/conversion primitives. No original kernel,
// GGML struct or C++ routine is called.
//
// Layout of block_q8_1_mmq (144 bytes): a 16-byte union of {float d4[4],
// half2 ds4[4], half d2s6[8]} followed by int8 qs[128]. For DS4 the union holds
// four half2 (d,s) pairs, so ds4[i] is the 4 bytes at 4*i. qs starts at byte 16 and
// the native writes 4 int8 values at a time as one 32-bit word at qs + iqs.
//
// Raw ABI: u64 values are global byte addresses; s01..s03 are element strides.
extern "fg180q_ld_f4x" fn ld_f4x(src:const f32&,byte:u64)->f32;
extern "fg180q_ld_f4y" fn ld_f4y(src:const f32&,byte:u64)->f32;
extern "fg180q_ld_f4z" fn ld_f4z(src:const f32&,byte:u64)->f32;
extern "fg180q_ld_f4w" fn ld_f4w(src:const f32&,byte:u64)->f32;
extern "fg180q_ld_i32" fn ld_i32(src:u64,byte:u64)->i64;
fn ld_i32_ro(src:u64,byte:u64)->i64{return ld_i32(src,byte);}
extern "fg180q_st_i32" fn st_i32(dst:u64,byte:u64,v:i32);
extern "fg180q_st_f32" fn st_f32(dst:u64,byte:u64,v:f32);
extern "fg180q_st_half2" fn st_half2(dst:u64,byte:u64,a:f32,b:f32);
extern "fg180q_shfl_xor" fn shfl_xor(v:f32,mask:u32,off:i32)->f32;
extern "fg180q_round" fn rnd(v:f32)->f32;
extern "fg180q_fabs" fn fabsv(v:f32)->f32;
extern "fg180q_i32" fn to_i32(v:f32)->i32;
extern "fg180q_pdl_sync" fn pdl_sync();
extern "fg180q_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// fmaxf / fadd keep the native evaluation order: a plain compare-select for the
// max and a left-to-right addition chain for the sum.
fn fmax2(a:f32,b:f32)->f32{return if a<b{b}else{a};}
// Reduction ladders with compile-time offsets, matching the native `#pragma unroll`
// loops. ds==2 uses the 8/4/2/1 ladder, everything else 4/2/1. Keeping them as two
// straight-line bodies lets the backend emit the same shuffle sequence as NVCC
// instead of a data-dependent loop.
fn red_max(a0:f32,ds:i32)->f32{
 var a:f32=a0;
 if ds==2{
  a=fmax2(a,shfl_xor(a,4294967295 as u32,8));
  a=fmax2(a,shfl_xor(a,4294967295 as u32,4));
  a=fmax2(a,shfl_xor(a,4294967295 as u32,2));
  a=fmax2(a,shfl_xor(a,4294967295 as u32,1));
 }else{
  a=fmax2(a,shfl_xor(a,4294967295 as u32,4));
  a=fmax2(a,shfl_xor(a,4294967295 as u32,2));
  a=fmax2(a,shfl_xor(a,4294967295 as u32,1));
 }
 return a;
}
fn red_sum(s0:f32,ds:i32)->f32{
 var s:f32=s0;
 if ds==2{
  s=s+shfl_xor(s,4294967295 as u32,8);
  s=s+shfl_xor(s,4294967295 as u32,4);
  s=s+shfl_xor(s,4294967295 as u32,2);
  s=s+shfl_xor(s,4294967295 as u32,1);
 }else{
  s=s+shfl_xor(s,4294967295 as u32,4);
  s=s+shfl_xor(s,4294967295 as u32,2);
  s=s+shfl_xor(s,4294967295 as u32,1);
 }
 return s;
}

// The parent fixes ds and scatter per launch, so four thin wrappers pin them and
// let the backend specialise the body the way the native template does.
fn fg180q_mmq_d4_direct(x:const f32&,ids:u64,y:u64,ne00:i64,s01:i64,s02:i64,s03:i64,ne0:i64,ne1:i64,ne2:i64,n_used:i32,bx:i32,by:i32,bz:i32,tx:i32,nt:i32,gx:i32,gyv:i32){
 fg180q_mmq(x,ids,y,ne00,s01,s02,s03,ne0,ne1,ne2,n_used,0 as i32,0 as i32,bx,by,bz,tx,nt,gx,gyv);
}
fn fg180q_mmq_d4_scatter(x:const f32&,ids:u64,y:u64,ne00:i64,s01:i64,s02:i64,s03:i64,ne0:i64,ne1:i64,ne2:i64,n_used:i32,bx:i32,by:i32,bz:i32,tx:i32,nt:i32,gx:i32,gyv:i32){
 fg180q_mmq(x,ids,y,ne00,s01,s02,s03,ne0,ne1,ne2,n_used,0 as i32,1 as i32,bx,by,bz,tx,nt,gx,gyv);
}
fn fg180q_mmq_ds4_direct(x:const f32&,ids:u64,y:u64,ne00:i64,s01:i64,s02:i64,s03:i64,ne0:i64,ne1:i64,ne2:i64,n_used:i32,bx:i32,by:i32,bz:i32,tx:i32,nt:i32,gx:i32,gyv:i32){
 fg180q_mmq(x,ids,y,ne00,s01,s02,s03,ne0,ne1,ne2,n_used,1 as i32,0 as i32,bx,by,bz,tx,nt,gx,gyv);
}
fn fg180q_mmq_ds4_scatter(x:const f32&,ids:u64,y:u64,ne00:i64,s01:i64,s02:i64,s03:i64,ne0:i64,ne1:i64,ne2:i64,n_used:i32,bx:i32,by:i32,bz:i32,tx:i32,nt:i32,gx:i32,gyv:i32){
 fg180q_mmq(x,ids,y,ne00,s01,s02,s03,ne0,ne1,ne2,n_used,1 as i32,1 as i32,bx,by,bz,tx,nt,gx,gyv);
}
fn fg180q_mmq_d2s6_direct(x:const f32&,ids:u64,y:u64,ne00:i64,s01:i64,s02:i64,s03:i64,ne0:i64,ne1:i64,ne2:i64,n_used:i32,bx:i32,by:i32,bz:i32,tx:i32,nt:i32,gx:i32,gyv:i32){
 fg180q_mmq(x,ids,y,ne00,s01,s02,s03,ne0,ne1,ne2,n_used,2 as i32,0 as i32,bx,by,bz,tx,nt,gx,gyv);
}
fn fg180q_mmq_d2s6_scatter(x:const f32&,ids:u64,y:u64,ne00:i64,s01:i64,s02:i64,s03:i64,ne0:i64,ne1:i64,ne2:i64,n_used:i32,bx:i32,by:i32,bz:i32,tx:i32,nt:i32,gx:i32,gyv:i32){
 fg180q_mmq(x,ids,y,ne00,s01,s02,s03,ne0,ne1,ne2,n_used,2 as i32,1 as i32,bx,by,bz,tx,nt,gx,gyv);
}

// ds_layout: 1 = DS4 (the Apex path), 0 = D4, 2 = D2S6.
// scatter: 0 = direct, 1 = compact rows.
fn fg180q_mmq(x:const f32&,ids:u64,y:u64,
 ne00:i64,s01:i64,s02:i64,s03:i64,ne0:i64,ne1:i64,ne2:i64,n_used:i32,
 ds:i32,scatter:i32,bx:i32,by:i32,bz:i32,tx:i32,nt:i32,gx:i32,gyv:i32){
 // vals_per_scale / vals_per_sum for this layout, divided by 8 as in native.
 let vps:i32=if ds==2{64 as i32}else{32 as i32};
 let vpsum:i32=if ds==2{16 as i32}else{32 as i32};
 let i0:i64=((nt as i64)*(by as i64)+(tx as i64))*(4 as i64);
 if i0<ne0{
  ensure(ne0>0&&ne1>0&&ne2>0&&ne00>0,10);
  var base:i64=0;
  if scatter!=0{
   base=(bx as i64)*s02;
  }else{
   // ne2 is 1 for the Apex projectile shapes and small otherwise. Specialising the
   // trivial case removes two int64 divisions from the hot path; the general case
   // keeps the exact quotient/remainder.
   let bzi:i32=bz;
   let ne2i:i32=ne2 as i32;
   var i2:i64=0;
   var i3:i64=0;
   if ne2i==1{
    i2=0;i3=bzi as i64;
   }else{
    i2=(bzi%ne2i) as i64;
    i3=(bzi/ne2i) as i64;
   }
   var i01:i64=bx as i64;
   if ids!=0{
    i01=ld_i32_ro(ids,((bx as i64)*(4 as i64)) as u64);
   }
   base=i3*s03+i2*s02+i01*s01;
  }
  // i0 >= 0, so >> 7 is the exact quotient and & 127 the exact remainder.
  let kb32:i32=((i0>>(7 as i64)) as i32);
  let k_block:i64=kb32 as i64;
  let iqs:i64=i0&(127 as i64);
  var xi0:f32=0.0;var xi1:f32=0.0;var xi2:f32=0.0;var xi3:f32=0.0;
  if i0<ne00{
   let o:i64=base+i0;
   let ob:u64=(o*(4 as i64)) as u64;
   xi0=ld_f4x(x,ob);
   xi1=ld_f4y(x,ob);
   xi2=ld_f4z(x,ob);
   xi3=ld_f4w(x,ob);
  }
  var amax:f32=fabsv(xi0);
  amax=fmax2(amax,fabsv(xi1));
  amax=fmax2(amax,fabsv(xi2));
  amax=fmax2(amax,fabsv(xi3));
  amax=red_max(amax,ds);
  var sum:f32=0.0;
  if ds!=0{
   sum=xi0+xi1;
   sum=sum+xi2;
   sum=sum+xi3;
   sum=red_sum(sum,ds);
  }
  let d_inv:f32=127.0/amax;
  pdl_sync();
  let qx:i32=to_i32(rnd(xi0*d_inv));
  let qy:i32=to_i32(rnd(xi1*d_inv));
  let qz:i32=to_i32(rnd(xi2*d_inv));
  let qw:i32=to_i32(rnd(xi3*d_inv));
  // char4 packed little-endian, as the native `char4 q` store produces.
  let m:i32=255 as i32;
  let qword:i32=(qx&m)|((qy&m)<<(8 as i32))|((qz&m)<<(16 as i32))|((qw&m)<<(24 as i32));
  let d:f32=1.0/d_inv;
  let nwrite:i32=if scatter!=0{n_used}else{1 as i32};
  var slot:i32=0;
  while slot<nwrite{
   var ib:i64=0;
   if scatter!=0{
    let idx:i64=(bx as i64)*(n_used as i64)+(slot as i64);
    let ii:i64=ld_i32_ro(ids,(idx*(4 as i64)) as u64);
    ib=k_block*ne1+ii;
   }else{
    // gx*gy*nt/128 fits in 32 bits for every supported launch; compute it there.
    let bpc32:i32=((gx*gyv*nt)>>(7 as i32));
    let blocks_per_channel:i64=bpc32 as i64;
    let ib0:i64=(bz as i64)*blocks_per_channel;
    ib=ib0+k_block*ne1+(bx as i64);
   }
   ensure(ib>=0,11);
   let yb:i64=ib*(144 as i64);
   // qs starts at byte 16; four int8 values are written as one word at qs+iqs.
   st_i32(y,(yb+(16 as i64)+iqs) as u64,qword);
   if ds==2{
    if (iqs&(15 as i64))==0&&96>iqs{
     st_f32(y,(yb+(4 as i64)+((2 as i64)+(iqs>>(4 as i64)))*2) as u64,sum);
     if (iqs&(63 as i64))==0{st_f32(y,(yb+(4 as i64)+(iqs>>(6 as i64))*2) as u64,d);}
    }
   }else{if (iqs&(31 as i64))==0{
    if ds==1{
     st_half2(y,(yb+(iqs>>(5 as i64))*(4 as i64)) as u64,d,sum);
    }else{
     st_f32(y,(yb+(iqs>>(5 as i64))*(4 as i64)) as u64,d);
    }
   }}
   slot+=1;
  }
 }
}

fn main(){
 let z:u64=0;
 var a:f32=0.0 as f32;
 fg180q_mmq_ds4_direct(a,z,z,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,1 as i64,1 as i64,0 as i32,0,0,0,0,32 as i32,1 as i32,1 as i32);
 fg180q_mmq_ds4_scatter(a,z,z,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,1 as i64,1 as i64,0 as i32,0,0,0,0,32 as i32,1 as i32,1 as i32);
}
