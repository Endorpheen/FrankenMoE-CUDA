// Apex CUDA FWHT host planning (Goose), from ggml-cuda/fwht.cu:
// ggml_cuda_op_fwht_impl.
//
// Scope: the warp-path sizes 64/128/256/512 and their launch geometry
//   rows_per_block = 4 ; grid = (ceil(rows/4), 1, 1) ; block = (warp, 4, 1)
//   scale = 1/sqrt(n)
// The wide block path (n >= 1024) and non-F32 sources return false so the parent
// keeps them native.
//
// Packet layout written by the bridge:
//   (0,0) src ptr (0,1) dst ptr (0,2) n (0,3) rows (0,4) source is F32 (0,5) warp
extern "fg183h_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg183h_plan" fn plan_ptr(packet:i64)->i64;
extern "fg183h_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg183h_launch" fn launch(packet:i64)->i64;
extern "fg183h_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}
fn cld(a:u64,b:u64)->u64{ensure(b!=0,1);return a/b+if a%b!=0{1 as u64}else{0 as u64};}

fn run(packet:i64)->bool{
 let src=get(packet,0,0);
 let dst=get(packet,0,1);
 ensure(src!=0&&dst!=0,20);
 // Only the F32-source warp path is transferred.
 if get(packet,0,4)!=1{return false;}
 let n=get(packet,0,2) as i64;
 if !(n==64||n==128||n==256||n==512){return false;}
 let rows=get(packet,0,3) as i64;
 let warp=get(packet,0,5) as i64;
 ensure(rows>0&&warp==32,21);
 let per:u64=4 as u64;
 let nb=cld(rows as u64,per);
 ensure(nb<=(2147483647 as u64),22);
 let p=plan_ptr(packet);
 put(p,1,0,nb);put(p,1,1,warp as u64);put(p,1,2,per);
 put(p,2,0,n as u64);put(p,2,1,rows as u64);
 ensure(launch(packet)==0,23);
 return true;
}

fn main(){run(0);}
