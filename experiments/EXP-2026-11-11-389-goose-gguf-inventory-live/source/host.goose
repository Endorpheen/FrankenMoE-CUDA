// Apex GGUF inventory decisions in Goose (llama_model_loader, tensor walk).
//
//   duplicate   a tensor name may appear once; a second occurrence invalidates the model
//   dominant    the file type is decided by the most frequent tensor type, first one winning a tie
//   step        the running count of a type after adding one
//
// Packet:
//   k=0  0 op, 1 a, 2 b, 3 c
//   k=1  0 out (two int64)
//   k=2  0 done, 1 ret
//
// Records: 0 decide (a = op, b = ret0, c = ret1)
extern "fg389k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg389k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg389k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg389k_stw" fn stw(base:u64,byte:i64,v:i64);
extern "fg389k_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn at(arr:u64,i:i64)->i64{return ld(arr,i*(8 as i64));}
fn setat(arr:u64,i:i64,v:i64){stw(arr,i*(8 as i64),v);}

fn emit(out:u64,cntp:u64,kind:i64,a:i64,b:i64,c:i64){
 let n:i64=at(cntp,0 as i64);
 setat(out,n*(4 as i64),kind);
 setat(out,n*(4 as i64)+1 as i64,a);
 setat(out,n*(4 as i64)+2 as i64,b);
 setat(out,n*(4 as i64)+3 as i64,c);
 setat(cntp,0 as i64,n+(1 as i64));
}

fn run(packet:i64)->bool{
 let op:i64=get(packet,0,0) as i64;
 let a:i64=get(packet,0,1) as i64;
 let b:i64=get(packet,0,2) as i64;
 let c:i64=get(packet,0,3) as i64;
 let out:u64=get(packet,1,0);
 let cntp:u64=get(packet,2,0);
 var r0:i64=0 as i64;
 var r1:i64=0 as i64;

 if op==(0 as i64){
  // duplicate name
  r0=if a!=(0 as i64){1 as i64}else{0 as i64};
 }else{
  if op==(1 as i64){
   // dominant type: strictly greater count wins, so the first type keeps a tie
   if b>c{
    r0=a;
    r1=b;
   }else{
    r0=(0 as i64)-(1 as i64);
    r1=c;
   }
  }else{
   if op==(2 as i64){
    // a type's running count after one more tensor
    r0=a+(1 as i64);
   }else{
    ensure(false,60);
   }
  }
 }
 emit(out,cntp,0 as i64,op,r0,r1);
 put(packet,2,0,1 as u64);
 put(packet,2,1,r0 as u64);
 put(packet,2,2,r1 as u64);
 return true;
}

fn main(){run(0);}
