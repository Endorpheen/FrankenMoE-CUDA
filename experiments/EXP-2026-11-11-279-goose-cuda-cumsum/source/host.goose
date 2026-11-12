// Apex CUDA cumsum host plan in Goose, from ggml-cuda/cumsum.cu.
//
// num_warps = ceil(ne00 / warp_size); block = min(num_warps*warp_size, 256);
// warps = block/warp_size; shmem = (block + warps + 2) * 4 bytes;
// grid = (ne01, ne02, ne03).
//
// Packet: (0,0) dst,(0,1) src,(0,2) ne00,(0,3) ne01,(0,4) ne02,(0,5) ne03,
//         (0,6) warp_size.
extern "fg279h_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg279h_plan" fn plan_ptr(packet:i64)->i64;
extern "fg279h_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg279h_launch" fn launch(packet:i64)->i64;
extern "fg279h_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn run(packet:i64)->bool{
 let dst=get(packet,0,0);
 let src=get(packet,0,1);
 let ne00=get(packet,0,2) as i64;
 let ne01=get(packet,0,3) as i64;
 let ne02=get(packet,0,4) as i64;
 let ne03=get(packet,0,5) as i64;
 let ws=get(packet,0,6) as i64;
 ensure(dst!=(0 as u64)&&src!=(0 as u64),20);
 ensure(ne00>0 as i64&&ne01>0 as i64&&ne02>0 as i64&&ne03>0 as i64,21);
 ensure(ws>0 as i64&&ws<=1024 as i64,22);
 let nw:i64=(ne00+(ws-(1 as i64)))/(ws);
 var blk:i64=nw*ws;
 if blk>256 as i64{ blk=256 as i64; }
 let warps:i64=blk/ws;
 let shm:i64=(blk+warps+(2 as i64))*(4 as i64);
 let p=plan_ptr(packet);
 put(p,1,0,blk as u64);put(p,1,1,shm as u64);
 put(p,1,2,ne01 as u64);put(p,1,3,ne02 as u64);put(p,1,4,ne03 as u64);
 ensure(launch(packet)==(0 as i64),23);
 return true;
}

fn main(){run(0);}
