// Apex CUDA cooperative softmax host plan in Goose, from ggml-cuda/softmax.cu.
//
// This module covers ONLY the cooperative branch. It validates the native
// preconditions and rejects everything else instead of planning a different
// algorithm:
//   nbytes_shared > smpbo          (else the shared-memory softmax applies)   -> 25
//   cooperative launch supported   (else the low-shared path applies)         -> 26
//   ncols / nrows > 8192           (native heuristic)                         -> 27
//   scale == 1                     (required by the native condition)         -> 28
//   max_bias == 0                  (required by the native condition)         -> 29
// Plan: block = 8*WARP_SIZE = 256, grid = nsm, shared = 2*32 floats = 256 bytes.
//
// Packet: (0,0) dst,(0,1) x,(0,2) ncols,(0,3) nrows,(0,4) smpbo,(0,5) nsm,
//         (0,6) coop support,(0,7) scale bits,(0,8) max_bias bits.
extern "fg285h_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg285h_plan" fn plan_ptr(packet:i64)->i64;
extern "fg285h_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg285h_launch" fn launch(packet:i64)->i64;
extern "fg285h_u32f" fn u32f(b:u32)->f32;
extern "fg285h_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn run(packet:i64)->bool{
 let dst=get(packet,0,0);
 let x=get(packet,0,1);
 let ncols=get(packet,0,2) as i64;
 let nrows=get(packet,0,3) as i64;
 let smpbo=get(packet,0,4) as i64;
 let nsm=get(packet,0,5) as i64;
 let coop=get(packet,0,6) as i64;
 let scale:f32=u32f(get(packet,0,7) as u32);
 let maxb:f32=u32f(get(packet,0,8) as u32);
 ensure(dst!=(0 as u64)&&x!=(0 as u64),20);
 ensure(ncols>0 as i64&&nrows>0 as i64&&nsm>0 as i64,21);
 let padn:i64=((ncols+(32 as i64)-(1 as i64))/(32 as i64))*(32 as i64);
 let nbs:i64=(padn+(32 as i64))*(4 as i64);
 ensure(nbs>smpbo as i64,25);
 ensure(coop!=0 as i64,26);
 ensure((ncols/nrows)>8192 as i64,27);
 ensure(scale==(1.0 as f32),28);
 ensure(maxb==(0.0 as f32),29);
 let p=plan_ptr(packet);
 put(p,1,0,256 as u64);put(p,1,1,nsm as u64);put(p,1,2,256 as u64);put(p,1,3,1 as u64);
 ensure(launch(packet)==(0 as i64),23);
 return true;
}

fn main(){run(0);}
