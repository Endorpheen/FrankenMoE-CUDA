// Apex CUDA IQ4_NL row dequantization host planning (Goose), from
// ggml-cuda/convert.cu: dequantize_row_iq4_nl_cuda.
//
// Contract:
//   nb32 = k / QK4_NL (32) ; nb = ceil(k / QK_K) = ceil(k / 256)
//   grid = (nb,1,1) ; block = 32 ; the kernel also receives nb32 for its guard
// A k that is not a multiple of 32 is rejected here instead of leaving a tail
// partially defined as the native function does.
//
// Packet layout written by the bridge:
//   (0,0) source pointer (0,1) element count k (0,2) destination pointer
//   (0,3) nb32, filled in by this module for the launch
extern "fg252h_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg252h_plan" fn plan_ptr(packet:i64)->i64;
extern "fg252h_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg252h_launch" fn launch(packet:i64)->i64;
extern "fg252h_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}
fn cld(a:u64,b:u64)->u64{
 ensure((b!=(0 as u64)),1);
 var q:u64=a/b;
 if (a%b)!=(0 as u64){q=q+(1 as u64);}
 return q;
}

fn run(packet:i64)->bool{
 let src=get(packet,0,0);
 let k=get(packet,0,1);
 let dst=get(packet,0,2);
 ensure((src!=(0 as u64))&&(dst!=(0 as u64)),20);
 ensure(k>(0 as u64),21);
 ensure((k%(32 as u64))==(0 as u64),30);
 ensure(k<=(4294967295 as u64),22);
 let nb:i64=(cld(k,256 as u64)) as i64;
 let nb32:i64=((k/(32 as u64)) as i64);
 let p=plan_ptr(packet);
 put(p,0,3,nb32 as u64);
 put(p,1,0,nb as u64);put(p,1,1,32 as u64);
 ensure(launch(packet)==(0 as i64),23);
 return true;
}

fn main(){run(0);}
