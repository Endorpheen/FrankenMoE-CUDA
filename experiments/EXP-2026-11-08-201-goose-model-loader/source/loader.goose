// Model-loader decisions in Goose (APX: model loader).
//
// From llama-model-loader.cpp:881-928. This is the check that decides whether a tensor found in
// the GGUF matches the shape the model asked for, and it is exactly what caught a wrong
// is_recr value in EXP200 ("blk.1.attn_qkv.weight has wrong shape").

extern "fg201l_fail" fn fail(code:i64);
extern "fg201l_ld_ne" fn ld_ne(desc:u64,i:i64)->i64;
extern "fg201l_st_i64" fn st_i64(base:u64,byte:i64,v:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// 4 as i64 is written as a literal everywhere: a `const` lives in gg_globals, which the emitter
// initialises inside gg_program -- and a bridge that calls a generated function directly never
// runs that, so a const reads as zero. Same trap EXP199 hit with W.

// nelements: product of the first `n` dims of a descriptor array.
fn nelements(desc:u64,n:i64)->i64{
 ensure(n>=0&&n<=4 as i64,10);
 var p:i64=1;
 var i:i64=0;
 while i<n{
  let v:i64=ld_ne(desc,i);
  ensure(v>=0,11);
  p=p*v;
  i+=1;
 }
 return p;
}

// dims_ok: llama-model-loader.cpp:907-915, the non-reshape branch.
// A requested dim must match exactly; dims the request does not mention must be 1.
fn dims_ok(desc:u64,nreq:i64,r0:i64,r1:i64,r2:i64,r3:i64)->i64{
 ensure(nreq>=0&&nreq<=4 as i64,20);
 var i:i64=0;
 while i<4 as i64{
  let want:i64=if i==0 as i64{r0}else{if i==1 as i64{r1}else{if i==2 as i64{r2}else{r3}}};
  let have:i64=ld_ne(desc,i);
  let expect:i64=if i<nreq{want}else{1 as i64};
  if have!=expect{return 0 as i64;}
  i+=1;
 }
 return 1 as i64;
}

// shape_ok: the two branches of check_tensor_dims. allow_reshape compares element counts only.
fn shape_ok(allow_reshape:i64,desc:u64,nreq:i64,total_req:i64,
            r0:i64,r1:i64,r2:i64,r3:i64)->i64{
 ensure(total_req>=0,30);
 if allow_reshape!=0{
  if nelements(desc,4 as i64)==total_req{return 1 as i64;}
  return 0 as i64;
 }
 return dims_ok(desc,nreq,r0,r1,r2,r3);
}

fn main(){
 nelements(0 as u64,1 as i64);
 dims_ok(0 as u64,1 as i64,1 as i64,1 as i64,1 as i64,1 as i64);
 shape_ok(0 as i64,0 as u64,1 as i64,1 as i64,1 as i64,1 as i64,1 as i64,1 as i64);
}
