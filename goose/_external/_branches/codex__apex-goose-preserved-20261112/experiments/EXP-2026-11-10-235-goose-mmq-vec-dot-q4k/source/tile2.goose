// MMQ Q4_K tile loop in Goose, redesigned (APX: P5).
//
// The first version (tile.goose) was verified equal to native and 2.7x slower. The recorded reason was
// that it round-tripped values through a scratch buffer and took geometry as runtime arguments. This
// version removes the scratch entirely: every conversion is a primitive that RETURNS the float it needs,
// so nothing crosses memory and nvcc can keep the values in registers across the loop.
//
// The accumulator is still read-modify-written, because `sum` is a pointer the native code also mutates in
// place -- but it is touched once per element, which is what native does too.
//
// Geometry arrives as i64 arguments. Because the whole module is `__forceinline__` into a caller that
// passes literals, nvcc folds them; the difference from the first version is that there is no memory in
// the inner path any more, which is what actually cost the 171%.

extern "fg236t_fail" fn fail(code:i64);
extern "fg236t_dp4a" fn dp4a(a:i64,b:i64,acc:i64)->i64;
extern "fg236t_ld_i32" fn ld_i32(base:u64,idx:i64)->i64;
extern "fg236t_ld_u8" fn ld_u8(base:u64,idx:i64)->i64;
extern "fg236t_ld_f32" fn ld_f32(base:u64,idx:i64)->f32;
extern "fg236t_st_f32" fn st_f32(base:u64,idx:i64,v:f32);
extern "fg236t_dm_x" fn dm_x(t:u64,idx:i64)->f32;
extern "fg236t_dm_y" fn dm_y(t:u64,idx:i64)->f32;

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// inner product. ds_x/ds_y are primitives called per i, so no array crosses memory.
fn vd_p(v:u64,u:u64,sc:u64,m:u64,ds:u64,dsbase:i64,dmx:f32,dmy:f32,n_i:i64)->f32{
 var sumf_d:f32=0.0;
 var sumf_m:f32=0.0;
 var i:i64=0;
 while i<n_i{
  var sumi_d:i64=0;
  var j:i64=0;
  while j<8 as i64{
   let aw:i64=ld_i32(v,j);
   let a:i64=(aw>>(4 as i64*i))&0x0F0F0F0F as i64;
   sumi_d=dp4a(a,ld_i32(u,i*8 as i64+j),sumi_d);
   j=j+1 as i64;
  }
  let sxf:f32=dm_x(ds,dsbase+i);
  let syf:f32=dm_y(ds,dsbase+i);
  sumf_d=sumf_d+sxf*((ld_u8(sc,i) as f32)*to_f32(sumi_d));
  sumf_m=sumf_m+syf*(ld_u8(m,i) as f32);
  i=i+1 as i64;
 }
 dmx*sumf_d-dmy*sumf_m
}

extern "fg236t_to_f32" fn to_f32(v:i64)->f32;

fn tile_q4k(x_qs:u64,x_dm:u64,x_sc:u64,y_qs:u64,y_ds:u64,sum:u64,
            k00:i64,J:i64,nwarps:i64,I:i64,warp_size:i64,tile_ne_k:i64,i8_1:i64,
            tx:i64,ty:i64)->i64{
 ensure(J>=0&&I>=0&&warp_size>0&&nwarps>0,1500);
 ensure(tile_ne_k>0&&i8_1>0,1501);
 let ytk:i64=tile_ne_k+tile_ne_k/i8_1;
 var calls:i64=0;
 var k01:i64=0;
 while k01<tile_ne_k{
  let k0:i64=k00+k01;
  var j0:i64=0;
  while j0<J{
   let j:i64=j0+ty;
   var i0:i64=0;
   while i0<I{
    let i:i64=i0+tx;
    let sc_idx:i64=i*(tile_ne_k/(8 as i64))+(i/(8 as i64))+(k0/(32 as i64));
    let sc_byte:i64=sc_idx*(4 as i64)+(2 as i64)*(k01/(16 as i64));
    let xo:i64=i*(tile_ne_k+1 as i64)+(k0/(2 as i64));
    let yo:i64=j*ytk+k01;
    let dsb:i64=j*ytk+k01/i8_1;
    let dmx:f32=dm_x(x_dm,i);
    let dmy:f32=dm_y(x_dm,i);
    let r:f32=vd_p((x_qs as i64+xo*(4 as i64)) as u64,
                   (y_qs as i64+yo*(4 as i64)) as u64,
                   (x_sc as i64+sc_byte) as u64,
                   (x_sc as i64+sc_byte+8 as i64) as u64,
                   y_ds,dsb,dmx,dmy,2 as i64);
    let si:i64=j0/nwarps*I/warp_size+i0/warp_size;
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
          0 as i64,0 as i64,1 as i64,0 as i64,32 as i64,32 as i64,8 as i64,0 as i64,0 as i64);
}
