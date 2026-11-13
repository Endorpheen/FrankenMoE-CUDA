// Apex CUDA GET_ROWS device path (Goose), from ggml-cuda/getrows.cu.
//
// Scope: the float fast paths of get_rows_cuda_float, which is what the fixed
// Apex profile executes (GPU census shows k_get_rows_float_vec<float> 330
// launches and k_get_rows_float<float,float> 336):
//   fg176g_vec    <- k_get_rows_float_vec<dst_t>    (same src/dst type, 16B vector)
//   fg176g_scalar <- k_get_rows_float<src0_t,dst_t> (element path, any cast pair)
//
// Everything algorithmic is Goose: the z-loop over ne11*ne12 with the
// gridDim.z stride, the fastdivide of z into (i11,i12), the src1 row index read,
// the src0 row base, the i00 accumulation with the gridDim.y*blockDim.x stride,
// and the int4 / element copies. Extern symbols are only raw loads and stores of
// the element or vector width, the __umulhi used by the native fastdiv, and the
// PDL primitives. No original kernel, GGML struct or C++ routine is called.
//
// FastDiv: the native init_fastdiv_values packs <mp, L, divisor> and fast_div_modulo
// returns (fastdiv(n), n - fastdiv(n)*divisor) where fastdiv is
// (umulhi(n, mp) + n) >> L. Both are reproduced exactly, including the L pack.
//
// Raw ABI: every u64 is a global byte address; offsets below are byte offsets.
// s1/s2/s3 are destination strides in ELEMENTS of the copy width; s10..s12 are
// src1 strides in I32 elements. nb01..nb03 are src0 byte strides. ne00v is the
// vectorised row length (ne00/VEC). No allocation sizes are visible here; the
// graph allocator owns them, exactly as on the native path.
extern "fg176g_ld_f32" fn ld_f32(src:u64,byte:u64)->f32;
extern "fg176g_st_f32" fn st_f32(dst:u64,byte:u64,v:f32);
extern "fg176g_ld_i32" fn ld_i32(src:u64,byte:u64)->i64;
extern "fg176g_ld_v4" fn ld_v4(src:u64,byte:u64,o:i32)->i32;
extern "fg176g_st_v4" fn st_v4(dst:u64,byte:u64,o:i32,v:i32);
extern "fg176g_mulhi" fn mulhi(a:u32,b:u32)->u32;
extern "fg176g_pdl_sync" fn pdl_sync();
extern "fg176g_pdl_lc" fn pdl_lc();
extern "fg176g_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}
// Native packs <mp,L,divisor> as a three-word POD.
struct FastDiv { mp:u32, shift:u32, divisor:u32 }
// k_get_rows_float_vec: one thread copies one 16-byte vector of a row.
//   for (z = blockIdx.z; z < ne11*ne12; z += gridDim.z)
//     i10 = blockIdx.x; (i11,i12) = fast_div_modulo(z, ne12_fdv)
//     i01 = src1[i10*s10 + i11*s11 + i12*s12]
//     dst_row  = dst + i10*s1 + i11*s2 + i12*s3
//     src0_row = src0 + i01*nb01 + i11*nb02 + i12*nb03
//     for (i = blockIdx.y*blockDim.x + threadIdx.x; i < ne00v; i += gridDim.y*blockDim.x)
//         dst_row[i] = src0_row[i]      // int4 units
fn fg176g_vec(src0:u64,src1:u64,d:u64,ne00v:i64,ne11:i64,ne12:i64,fd:FastDiv,
 s1:i64,s2:i64,s3:i64,nb01:i64,nb02:i64,nb03:i64,s10:i64,s11:i64,s12:i64,
 bx:i32,by:i32,bz:i32,gx:i32,gy:i32,gz:i32,tx:i32,nt:i32){
 pdl_sync();
 ensure(nt>0&&gy>0&&gz>0&&ne12>0,10);
 let nz:i64=ne11*ne12;
 var z:i64=bz as i64;
 while z<nz{
  let i10:i64=bx as i64;
  // fast_div_modulo((uint32_t)z, ne12_fdv)
  let zu:u32=z as u32;
  let hi:u32=mulhi(zu,fd.mp);
  let q:u32=(hi+zu)>>(fd.shift as u32);
  let i11:i64=q as i64;
  let i12:i64=(z as u32-q*fd.divisor) as i64;
  let idx:i64=i10*s10+i11*s11+i12*s12;
  let i01:i64=ld_i32(src1,(idx*(4 as i64)) as u64);
  let drow:i64=i10*s1+i11*s2+i12*s3;
  let srow:i64=i01*nb01+i11*nb02+i12*nb03;
  ensure(drow>=0&&srow>=0,11);
  var i:i64=(by as i64)*(nt as i64)+(tx as i64);
  let stride:i64=(gy as i64)*(nt as i64);
  while i<ne00v{
   let e:i64=i*(16 as i64);
   // Native casts the element row pointer to int4* and indexes it:
   //   dst_row[i] with dst_row = (int4*)(dst + drow*sizeof(dst_t))
   // so the byte base is drow * element size (4 for F32), NOT drow * 16.
   // s1/s2/s3 and nb01..nb03 are element/byte strides respectively, and e is the
   // byte offset of the i-th 16-byte vector inside the row.
   let doff:u64=((drow*(4 as i64)) as u64)+(e as u64);
   let soff:u64=((srow as u64)+(e as u64));
   st_v4(d,doff,0,ld_v4(src0,soff,0));
   st_v4(d,doff,1,ld_v4(src0,soff,1));
   st_v4(d,doff,2,ld_v4(src0,soff,2));
   st_v4(d,doff,3,ld_v4(src0,soff,3));
   i+=stride;
  }
  z+=gz as i64;
 }
}
// k_get_rows_float: one thread copies one element, with the native cast.
//   ggml_cuda_cast<dst_t>(src0_t) is float(x) for a float source and
//   int32_t(x) for an integer source; the fixed Apex path is F32->F32.
fn fg176g_scalar(src0:u64,src1:u64,d:u64,ne00:i64,ne11:i64,ne12:i64,fd:FastDiv,
 s1:i64,s2:i64,s3:i64,nb01:i64,nb02:i64,nb03:i64,s10:i64,s11:i64,s12:i64,
 es:i32,bx:i32,by:i32,bz:i32,gy:i32,gz:i32,tx:i32,nt:i32){
 pdl_sync();
 ensure(nt>0&&gy>0&&gz>0&&ne12>0,20);
 ensure(es==4,22);
 let nz:i64=ne11*ne12;
 var z:i64=bz as i64;
 while z<nz{
  let i10:i64=bx as i64;
  let zu:u32=z as u32;
  let hi:u32=mulhi(zu,fd.mp);
  let q:u32=(hi+zu)>>(fd.shift as u32);
  let i11:i64=q as i64;
  let i12:i64=(z as u32-q*fd.divisor) as i64;
  let idx:i64=i10*s10+i11*s11+i12*s12;
  let i01:i64=ld_i32(src1,(idx*(4 as i64)) as u64);
  let drow:i64=i10*s1+i11*s2+i12*s3;
  let srow:i64=i01*nb01+i11*nb02+i12*nb03;
  ensure(drow>=0&&srow>=0,21);
  var i:i64=(by as i64)*(nt as i64)+(tx as i64);
  let stride:i64=(gy as i64)*(nt as i64);
  let ew:i64=es as i64;
  // es==4 is the F32->F32 Apex path. The width is a per-launch constant, so its
  // guard is hoisted above the element loop; leaving it inside bloated the
  // emitted frame until the scalar kernel overflowed its stack on real hardware.
  while i<ne00{
   let e:i64=i*ew;
   st_f32(d,((drow*ew) as u64)+(e as u64),ld_f32(src0,(srow as u64)+(e as u64)));
   i+=stride;
  }
  z+=gz as i64;
 }
}
// Retained entries; the parent never executes main. Distinct trailing arguments
// keep the emitter from folding equal calls together.
fn main(){
 let z:u64=0;
 let f=FastDiv{1 as u32,0 as u32,1 as u32};
 fg176g_vec(z,z,z,0 as i64,0 as i64,0 as i64,f,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,1 as i32,1 as i32,1 as i32,1 as i32,1 as i32,1 as i32,1 as i32,1 as i32);
 fg176g_scalar(z,z,z,0 as i64,0 as i64,0 as i64,f,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,1 as i32,1 as i32,1 as i32,1 as i32,1 as i32,1 as i32,1 as i32,1 as i32);
}
