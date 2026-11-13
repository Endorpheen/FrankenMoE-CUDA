// Apex CUDA snake/softcap host plan in Goose, from snake.cu and softcap.cu.
//
// Both kernels: block = 256, grid = ceil(n / 256), where n is total = T*C for
// snake and k = nelements for softcap.
//
// Packet: (0,0) dst,(0,1) x,(0,2) n,(0,3) mode (0 = snake, 1 = softcap).
extern "fg286h_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg286h_plan" fn plan_ptr(packet:i64)->i64;
extern "fg286h_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg286h_launch" fn launch(packet:i64)->i64;
extern "fg286h_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn run(packet:i64)->bool{
 let dst=get(packet,0,0);
 let x=get(packet,0,1);
 let n=get(packet,0,2) as i64;
 let mode=get(packet,0,3) as i64;
 ensure(dst!=(0 as u64)&&x!=(0 as u64),20);
 ensure(n>0 as i64,21);
 ensure(mode==(0 as i64)||mode==(1 as i64),22);
 let nb:i64=(n+(256 as i64)-(1 as i64))/(256 as i64);
 let p=plan_ptr(packet);
 put(p,1,0,256 as u64);put(p,1,1,nb as u64);
 ensure(launch(packet)==(0 as i64),23);
 return true;
}

fn main(){run(0);}
