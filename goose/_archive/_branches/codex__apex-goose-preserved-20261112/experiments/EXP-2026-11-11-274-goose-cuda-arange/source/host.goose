// Apex CUDA arange host plan in Goose, from ggml-cuda/arange.cu.
//
// Contract:
//   steps = (int64_t)ceil((stop - start) / step)   (float division, ceil, cast)
//   steps must equal the destination element count
//   num_blocks = ceil(ne0 / CUDA_ARANGE_BLOCK_SIZE), block = 256
//
// Packet layout: (0,0) dst pointer, (0,1) ne0, (0,2) nelements,
//                (0,3) start bits, (0,4) step bits, (0,5) stop bits.
extern "fg274h_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg274h_plan" fn plan_ptr(packet:i64)->i64;
extern "fg274h_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg274h_launch" fn launch(packet:i64)->i64;
extern "fg274h_u32f" fn u32f(b:u32)->f32;
extern "fg274h_ceilf" fn ceilf(x:f32)->f32;
extern "fg274h_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn run(packet:i64)->bool{
 let dst=get(packet,0,0);
 let ne0=get(packet,0,1);
 let ne=get(packet,0,2);
 let start:f32=u32f(get(packet,0,3) as u32);
 let step:f32=u32f(get(packet,0,4) as u32);
 let stop:f32=u32f(get(packet,0,5) as u32);
 ensure(dst!=(0 as u64),20);
 ensure(ne0>(0 as u64),21);
 ensure(step!=(0.0 as f32),22);
 let cx:f32=(stop-start)/step;
 let steps:i64=ceilf(cx) as i64;
 ensure(steps==(ne as i64),23);
 let nb:i64=(((ne0 as i64)+(256 as i64)-(1 as i64))/(256 as i64));
 let p=plan_ptr(packet);
 put(p,1,0,nb as u64);put(p,1,1,256 as u64);
 ensure(launch(packet)==(0 as i64),24);
 return true;
}

fn main(){run(0);}
