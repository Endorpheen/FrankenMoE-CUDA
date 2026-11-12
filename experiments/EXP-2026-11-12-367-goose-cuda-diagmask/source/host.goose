// Apex CUDA diag_mask_inf host plan in Goose, from ggml-cuda/diagmask.cu.
//
// ncols_x = src0->ne[0]; nrows_x = ggml_nrows(src0); rows_per_channel = ne01;
// n_past comes from the op parameters; block = (1, CUDA_DIAG_MASK_INF_BLOCK_SIZE);
// grid = (nrows_x, ceil(ncols_x/block), 1).
//
// Packet: (0,0) x,(0,1) dst,(0,2) ncols_x,(0,3) nrows_x,(0,4) rows_per_channel,
// (0,5) n_past.
extern "fg367h_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg367h_plan" fn plan_ptr(packet:i64)->i64;
extern "fg367h_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg367h_launch" fn launch(packet:i64)->i64;
extern "fg367h_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn run(packet:i64)->bool{
 let x=get(packet,0,0);
 let dst=get(packet,0,1);
 let ncols_x=get(packet,0,2) as i64;
 let nrows_x=get(packet,0,3) as i64;
 let rows_per_channel=get(packet,0,4) as i64;
 let n_past=get(packet,0,5) as i64;
 ensure(x!=(0 as u64)&&dst!=(0 as u64),20);
 ensure(ncols_x>0 as i64&&nrows_x>0 as i64,21);
 ensure(rows_per_channel>0 as i64,22);
 let block_size:i64=256 as i64;
 let grid_y:i64=(ncols_x+block_size-(1 as i64))/block_size;
 let p=plan_ptr(packet);
 put(p,1,0,nrows_x as u64);
 put(p,1,1,grid_y as u64);
 put(p,1,2,block_size as u64);
 put(p,1,3,n_past as u64);
 ensure(launch(packet)==(0 as i64),23);
 return true;
}

fn main(){run(0);}
