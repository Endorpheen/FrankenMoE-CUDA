// Transposed CPY host plan in Goose.
// grid = (ceil(ne01/32), ceil(ne00/32), ceil(nmat/8)); block = (32,8,1).
extern "fg434h_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg434h_plan" fn plan_ptr(packet:i64)->i64;
extern "fg434h_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg434h_launch" fn launch(packet:i64)->i64;
extern "fg434h_fail" fn fail(c:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn run(packet:i64)->bool{
 let cx=get(packet,0,0);
 let cdst=get(packet,0,1);
 let ne00=get(packet,0,2) as i64;
 let ne01=get(packet,0,3) as i64;
 let ne=get(packet,0,4) as i64;
 ensure(cx!=(0 as u64)&&cdst!=(0 as u64),20);
 ensure(ne00>(0 as i64)&&ne01>(0 as i64)&&ne>(0 as i64),21);
 let n:i64=ne00*ne01;
 ensure(ne%n==(0 as i64),22);
 let nmat:i64=ne/n;
 let bs:i64=32 as i64;
 let gx:i64=(ne01+bs-(1 as i64))/bs;
 let gy:i64=(ne00+bs-(1 as i64))/bs;
 let gz:i64=(nmat+(8 as i64)-(1 as i64))/(8 as i64);
 let p=plan_ptr(packet);
 put(p,1,0,gx as u64);put(p,1,1,gy as u64);put(p,1,2,gz as u64);
 put(p,1,3,32 as u64);put(p,1,4,8 as u64);put(p,1,5,1 as u64);
 ensure(launch(packet)==(0 as i64),23);
 return true;
}

fn main(){run(0);}
