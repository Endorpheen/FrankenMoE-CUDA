// Apex CUDA MM IDs helper host planning (Goose), from ggml-cuda/mmid.cu:
// launch_mm_ids_helper and ggml_cuda_launch_mm_ids_helper.
//
// Scope: the 8-expert-used specialisation that the Apex profile instantiates, plus
// its assert set and launch geometry:
//   num_blocks = (n_experts, 1, 1), block = warp_size, shared = n_tokens*4 bytes.
// Other widths and the generic (template 0) variant stay native; the host returns
// false for them so the parent keeps its own path.
//
// Everything above is Goose. The parent keeps the pointer marshalling, the plan
// arena, the shared-memory limit call and the launch.
//
// Packet layout written by the bridge:
//   (0,0) ids ptr (0,1) ids_src1 ptr (0,2) ids_dst ptr (0,3) expert_bounds ptr
//   (1,0) n_experts (1,1) n_tokens (1,2) n_expert_used (1,3) nchannels_y
//   (1,4) si1 (1,5) sis1 (1,6) write_inverse (1,7) warp_size
//   (1,8) shared-bytes limit (smpbo)
extern "fg181h_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg181h_plan" fn plan_ptr(packet:i64)->i64;
extern "fg181h_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg181h_launch" fn launch(packet:i64)->i64;
extern "fg181h_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}
fn U()->u64{return 18446744073709551615 as u64;}
fn mul(a:u64,b:u64)->u64{ensure(a==0||b<=U()/a,1);return a*b;}

fn run(packet:i64)->bool{
 let ids=get(packet,0,0);
 let src1=get(packet,0,1);
 let dst=get(packet,0,2);
 let bounds=get(packet,0,3);
 ensure(ids!=0&&src1!=0&&dst!=0&&bounds!=0,20);
 let n_used=get(packet,1,2) as i64;
 // Only the 8-wide specialisation is transferred.
 if n_used!=8{return false;}
 let n_experts=get(packet,1,0) as i64;
 let n_tokens=get(packet,1,1) as i64;
 let warp=get(packet,1,7) as i64;
 ensure(n_experts>0&&n_tokens>0,21);
 ensure(warp==32,22);
 // Native: n_tokens < 2^22 and n_expert_used < 2^10 for the packed store.
 ensure(n_tokens<(4194304 as i64),23);
 ensure(n_used<(1024 as i64),24);
 // Shared-memory budget: n_tokens*4 bytes must fit the device's opt-in limit.
 let shared:u64=mul(n_tokens as u64,4 as u64);
 ensure(shared<=get(packet,1,8),25);
 let p=plan_ptr(packet);
 put(p,1,0,n_experts as u64);put(p,1,1,warp as u64);put(p,1,2,shared);
 put(p,2,0,n_tokens as u64);put(p,2,1,n_used as u64);
 put(p,2,2,get(packet,1,3));      // nchannels_y
 put(p,2,3,get(packet,1,4));      // si1
 put(p,2,4,get(packet,1,5));      // sis1
 put(p,2,5,get(packet,1,6));      // write_inverse
 ensure(launch(packet)==0,26);
 return true;
}

fn main(){run(0);}
