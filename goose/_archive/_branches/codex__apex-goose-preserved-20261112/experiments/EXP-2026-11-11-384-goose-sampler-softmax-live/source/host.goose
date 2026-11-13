// Apex sampler softmax in Goose (llama_sampler_softmax_impl).
//
// The normaliser only: the maximum (taken from the first element when the array is already sorted),
// exp(logit - max) per element accumulated left to right, then one division per element. The order of
// operations is preserved exactly, and exp comes from the standard library through `expf`, so the
// result is bit-identical to the native loop.
//
// Packet:
//   k=0  0 n, 1 sorted, 2 logits (f32 array), 3 out (f32 array)
//   k=1  0 sum (f32 bits in the low word), 1 max (f32 bits)
//   k=2  0 done
//
// Records: 0 plan (a = n, b = sum bits, c = max bits)
extern "fg384k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg384k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg384k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg384k_stw" fn stw(base:u64,byte:i64,v:i64);
extern "fg384k_fail" fn fail(code:i64);
extern "gs_expf" fn expf32(x:f32)->f32;
extern "fg384k_ldf" fn ldf(base:u64,byte:i64)->f32;
extern "fg384k_stf" fn stf(base:u64,byte:i64,v:f32);
extern "fg384k_bits" fn bits(x:f32)->i64;

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
 let n:i64=get(packet,0,0) as i64;
 let sorted:i64=get(packet,0,1) as i64;
 let logits:u64=get(packet,0,2);
 let outp:u64=get(packet,0,3);
 let out:u64=get(packet,1,0);
 let cntp:u64=get(packet,2,0);

 ensure(n>(0 as i64),10);

 // the maximum: the first element when the array is sorted, otherwise a scan
 var max_l:f32=ldf(logits,0 as i64);
 if sorted==(0 as i64){
  var i:i64=1 as i64;
  while i<n{
   let v:f32=ldf(logits,i*(4 as i64));
   if v>max_l{ max_l=v; }
   i+=1 as i64;
  }
 }

 // exp(logit - max), accumulated left to right exactly as the native loop does
 var cum:f32=0.0 as f32;
 var i:i64=0 as i64;
 while i<n{
  let p:f32=expf32(ldf(logits,i*(4 as i64))-max_l);
  stf(outp,i*(4 as i64),p);
  cum=cum+p;
  i+=1 as i64;
 }

 i=0 as i64;
 while i<n{
  stf(outp,i*(4 as i64),ldf(outp,i*(4 as i64))/cum);
  i+=1 as i64;
 }

 emit(out,cntp,0 as i64,n,bits(cum),bits(max_l));
 put(packet,2,0,1 as u64);
 return true;
}

fn main(){run(0);}
