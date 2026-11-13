// Apex CUDA add_id in Goose, from ggml-cuda/add-id.cu.
//
// Device side: id load, row pointers from byte strides, and the elementwise
// src0_row[i0] + src1_row[i0] loop with the blockDim stride.
// Host side: threads = min(ne00, 768), grid = (ne01, ne02), and the byte-stride
// contract nb00 == nb10 == nb20 == 4.
//
// All index arithmetic is Goose. Externs are a 32-bit id load, a float load and
// a float store.
extern "fg277d_ld_f32" fn ld_f32(x:u64,o:u64)->f32;
extern "fg277d_st_f32" fn st_f32(dst:u64,o:u64,v:f32);
extern "fg277d_ld_i32" fn ld_i32(x:u64,o:u64)->i32;
extern "fg277d_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn add_id(src0:u64,src1:u64,src2:u64,dst:u64,ne0:i64,ne1:i64,
          nb01:i64,nb02:i64,nb11:i64,nb21:i64,i1:i64,i2:i64,thr:i64,tid:i64){
 ensure(ne0>0 as i64&&ne1>0 as i64,20);
 ensure(thr>0 as i64,21);
 let i11:i64=(ld_i32(src2,((i1*4 as i64+i2*nb21) as u64))) as i64;
 let nb1:i64=ne0*4 as i64;
 let nb2:i64=ne1*nb1;
 let drow:i64=i1*nb1+i2*nb2;
 let s0row:i64=i1*nb01+i2*nb02;
 let s1row:i64=i11*nb11;
 var i0:i64=tid;
 while i0<ne0 as i64{
  st_f32(dst,((drow+i0*4 as i64) as u64),
         ld_f32(src0,((s0row+i0*4 as i64) as u64))+ld_f32(src1,((s1row+i0*4 as i64) as u64)));
  i0=i0+thr;
 }
}

// Retained entry; the parent never executes main.
fn main(){add_id(0,0,0,0,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64);}
