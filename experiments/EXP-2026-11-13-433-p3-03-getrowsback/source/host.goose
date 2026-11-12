// GET_ROWS_BACK host plan in Goose, from ggml_cuda_op_get_rows_back.
// block = 256; grid = (ceil(ncols/256), min(nrows_dst, 65535), 1).
extern "fg433h_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg433h_plan" fn plan_ptr(packet:i64)->i64;
extern "fg433h_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg433h_launch" fn launch(packet:i64)->i64;
extern "fg433h_fail" fn fail(c:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn run(packet:i64)->bool{
 let grad=get(packet,0,0);
 let rows=get(packet,0,1);
 let dst=get(packet,0,2);
 let ncols=get(packet,0,3) as i64;
 let nrows_grad=get(packet,0,4) as i64;
 let nrows_dst=get(packet,0,5) as i64;
 ensure(grad!=(0 as u64)&&rows!=(0 as u64)&&dst!=(0 as u64),20);
 ensure(ncols>(0 as i64),21);
 ensure(nrows_grad>=(0 as i64)&&nrows_dst>(0 as i64),22);
 let bs:i64=256 as i64;
 let gx:i64=(ncols+bs-(1 as i64))/bs;
 var gy:i64=nrows_dst;
 if gy>(65535 as i64){gy=65535 as i64;}
 let p=plan_ptr(packet);
 put(p,1,0,gx as u64);put(p,1,1,gy as u64);put(p,1,2,1 as u64);
 put(p,1,3,bs as u64);put(p,1,4,1 as u64);put(p,1,5,1 as u64);
 put(p,2,0,ncols as u64);put(p,2,1,nrows_grad as u64);put(p,2,2,nrows_dst as u64);
 ensure(launch(packet)==(0 as i64),23);
 return true;
}

fn main(){run(0);}
