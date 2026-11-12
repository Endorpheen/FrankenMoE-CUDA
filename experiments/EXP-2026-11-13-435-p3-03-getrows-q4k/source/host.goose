// Q4_K GET_ROWS host plan in Goose.
// block = 32; grid = (ne10, min(nsb,65535), min(ne11*ne12,65535)).
extern "fg435h_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg435h_plan" fn plan_ptr(packet:i64)->i64;
extern "fg435h_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg435h_launch" fn launch(packet:i64)->i64;
extern "fg435h_fail" fn fail(c:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn run(packet:i64)->bool{
 let src0=get(packet,0,0);
 let src1=get(packet,0,1);
 let dst=get(packet,0,2);
 let ne00=get(packet,0,3) as i64;
 let ne11=get(packet,0,4) as i64;
 let ne12=get(packet,0,5) as i64;
 let ne10=get(packet,0,6) as i64;
 ensure(src0!=(0 as u64)&&src1!=(0 as u64)&&dst!=(0 as u64),20);
 ensure(ne00>(0 as i64)&&ne00%(256 as i64)==(0 as i64),21);
 ensure(ne11>(0 as i64)&&ne12>(0 as i64)&&ne10>(0 as i64),22);
 let nsb:i64=ne00/(256 as i64);
 var gy:i64=nsb;
 if gy>(65535 as i64){gy=65535 as i64;}
 var gz:i64=ne11*ne12;
 if gz>(65535 as i64){gz=65535 as i64;}
 let p=plan_ptr(packet);
 put(p,1,0,ne10 as u64);put(p,1,1,gy as u64);put(p,1,2,gz as u64);
 put(p,1,3,32 as u64);put(p,1,4,1 as u64);put(p,1,5,1 as u64);
 ensure(launch(packet)==(0 as i64),23);
 return true;
}

fn main(){run(0);}
