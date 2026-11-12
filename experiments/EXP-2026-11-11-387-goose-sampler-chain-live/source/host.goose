// Apex sampler chain orchestration in Goose (llama_sampler_chain_apply / _accept).
//
// The chain visits samplers in insertion order and skips two kinds:
//   backend  every backend sampler is skipped once the chain itself was initialised as a backend chain,
//            and that "already skipped" state carries forward to the following samplers
//   apply    a sampler without an apply hook is skipped
// accept visits every sampler without exception and then advances the sample counter.
//
// Packet:
//   k=0  0 op, 1 a, 2 b
//   k=1  0 out
//   k=2  0 done, 1 ret
//
// Records: 0 decide (a = op, b = ret)
extern "fg387k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg387k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg387k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg387k_stw" fn stw(base:u64,byte:i64,v:i64);
extern "fg387k_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn at(arr:u64,i:i64)->i64{return ld(arr,i*(8 as i64));}
fn setat(arr:u64,i:i64,v:i64){stw(arr,i*(8 as i64),v);}

fn emit(out:u64,cntp:u64,kind:i64,a:i64,b:i64){
 let n:i64=at(cntp,0 as i64);
 setat(out,n*(3 as i64),kind);
 setat(out,n*(3 as i64)+1 as i64,a);
 setat(out,n*(3 as i64)+2 as i64,b);
 setat(cntp,0 as i64,n+(1 as i64));
}

fn run(packet:i64)->bool{
 let op:i64=get(packet,0,0) as i64;
 let a:i64=get(packet,0,1) as i64;
 let b:i64=get(packet,0,2) as i64;
 let out:u64=get(packet,1,0);
 let cntp:u64=get(packet,2,0);
 var ret:i64=0 as i64;

 if op==(0 as i64){
  // skip a backend sampler only while the chain is still in backend mode
  ret=if a!=(0 as i64)&&b!=(0 as i64){1 as i64}else{0 as i64};
 }else{
  if op==(1 as i64){
   // a sampler without an apply hook is skipped, and the backend mode is off from here on
   ret=if a==(0 as i64){1 as i64}else{0 as i64};
  }else{
   if op==(2 as i64){
    // accept reaches every sampler
    ret=b;
   }else{
    if op==(3 as i64){
     // the sample counter advances by one
     ret=a+(1 as i64);
    }else{
     ensure(false,60);
    }
   }
  }
 }
 emit(out,cntp,0 as i64,op,ret);
 put(packet,2,0,1 as u64);
 put(packet,2,1,ret as u64);
 return true;
}

fn main(){run(0);}
