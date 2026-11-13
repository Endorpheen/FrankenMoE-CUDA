// Apex CUDA Q5_K row dequantization host planning (Goose), from
// ggml-cuda/convert.cu: dequantize_row_q5_K_cuda.
//
// Scope: the element-count contract and the launch geometry.
//   nb = k / QK_K  (QK_K = 256) ; grid = (nb, 1, 1) ; block = 64
// The native function does not validate k beyond the integer division, so a k
// that is not a multiple of QK_K silently drops the tail there. This module
// rejects it instead of copying that behaviour, because a partial block would
// leave the destination unwritten and that is not a property worth preserving.
//
// Packet layout written by the bridge:
//   (0,0) source pointer (0,1) block count k/256 (0,2) destination pointer
extern "fg249h_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg249h_plan" fn plan_ptr(packet:i64)->i64;
extern "fg249h_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg249h_launch" fn launch(packet:i64)->i64;
extern "fg249h_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn run(packet:i64)->bool{
 let src=get(packet,0,0);
 let dst=get(packet,0,2);
 let nb=get(packet,0,1);
 ensure(src!=0&&dst!=0,20);
 ensure(nb>0,21);
 ensure(nb<=(2147483647 as u64),22);
 let p=plan_ptr(packet);
 put(p,1,0,nb);put(p,1,1,64 as u64);
 ensure(launch(packet)==0,23);
 return true;
}

fn main(){run(0);}
