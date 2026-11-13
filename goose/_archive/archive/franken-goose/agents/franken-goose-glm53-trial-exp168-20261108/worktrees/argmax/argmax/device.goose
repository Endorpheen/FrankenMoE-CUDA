// Argmax F32->I32 device lane logic;exact native argmax_f32 semantics:strict >,native butterfly order,-FLT_MAX/-1 sentinel.
struct MI{v:f32,c:i32}
extern "fg169a_load" fn load(p:u64,i:u64)->f32;
extern "fg169a_store" fn store(p:u64,i:u64,v:i32);
extern "fg169a_sloadf" fn sloadf(i:u64)->f32;
extern "fg169a_sstoref" fn sstoref(i:u64,v:f32);
extern "fg169a_sloadi" fn sloadi(i:u64)->i32;
extern "fg169a_sstorei" fn sstorei(i:u64,v:i32);
extern "fg169a_shuffle" fn shuffle(v:f32,offset:i32)->f32;
extern "fg169a_ishuffle" fn ishuffle(v:i32,offset:i32)->i32;
extern "fg169a_barrier" fn barrier();
// Native butterfly:__shfl_xor_sync full mask,offsets 16,8,4,2,1,strict > keeps own value on ties,NaN never wins.
fn warp_pair(m:MI)->MI{
 var x:MI=m;
 var offset:i32=16;
 while offset>0{
  let v=shuffle(x.v,offset);
  let c=ishuffle(x.c,offset);
  if v>x.v{x.v=v;x.c=c;}
  offset>>=1;
 }
 return x;
}
// One block reduces one row of contiguous F32 x into dst[row]:row=blockIdx.x,tid=threadIdx.x,bs=blockDim.x.
fn argmax_lane(x:u64,dst:u64,ncols:i64,row:i64,tid:i32,bs:i32){
 var maxval:f32=-340282346638528859811704183484516925440.0;
 var argmax:i32=-1;
 let base=row*ncols;
 var col:i64=tid as i64;
 while col<ncols{
  let val=load(x,(base+col) as u64);
  if val>maxval{maxval=val;argmax=col as! i32;}
  col+=bs as i64;
 }
 var best:MI=MI{maxval,argmax};
 best=warp_pair(best);
 let lane=tid%32;
 let warp_id=tid/32;
 let n_warps=bs/32;
 if n_warps>1{
  if lane==0{sstoref(warp_id as u64,best.v);sstorei(warp_id as u64,best.c);}
  barrier();
  if warp_id==0{
   if lane<n_warps{best=MI{sloadf(lane as u64),sloadi(lane as u64)};}
   best=warp_pair(best);
  }
 }
 if warp_id==0&&lane==0{store(dst,row as u64,best.c);}
}
// Dummy retained entry;parent must not execute it.
fn main(){argmax_lane(0 as u64,0 as u64,0,0,0,32);}