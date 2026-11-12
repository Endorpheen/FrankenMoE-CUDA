// Apex CUDA softmax backward host plan in Goose, from ggml-cuda/softmax.cu.
//
// block = (WARP_SIZE,1,1), grid = (nrows,1,1), no shared memory.
// Packet: (0,0) dst,(0,1) grad,(0,2) dstf,(0,3) ncols,(0,4) nrows,(0,5) scale bits.
extern "fg284h_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg284h_plan" fn plan_ptr(packet:i64)->i64;
extern "fg284h_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg284h_launch" fn launch(packet:i64)->i64;
extern "fg284h_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn run(packet:i64)->bool{
 let dst=get(packet,0,0);
 let grad=get(packet,0,1);
 let dstf=get(packet,0,2);
 let ncols=get(packet,0,3) as i64;
 let nrows=get(packet,0,4) as i64;
 ensure(dst!=(0 as u64)&&grad!=(0 as u64)&&dstf!=(0 as u64),20);
 ensure(ncols>0 as i64&&nrows>0 as i64,21);
 let p=plan_ptr(packet);
 put(p,1,0,32 as u64);put(p,1,1,nrows as u64);
 ensure(launch(packet)==(0 as i64),23);
 return true;
}

fn main(){run(0);}
