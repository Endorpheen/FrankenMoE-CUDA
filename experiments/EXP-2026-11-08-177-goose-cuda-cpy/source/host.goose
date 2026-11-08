// Apex CUDA CPY host planning and routing (Goose), from ggml-cuda/cpy.cu.
//
// Scope, exactly ggml_cuda_cpy's branch selection and the float/float kernel
// sizing the Apex profile uses:
//   - the equal-element assert and the same-type-contiguous memcpy branch,
//   - ggml_cuda_cpy_as_memcpy_2d: the contiguous-prefix walk, the "rows" dim, the
//     upper-dims-must-be-1 rule and the two pitch inequalities,
//   - can_be_transposed and the scalar/transposed kernel choice,
//   - grid/block sizing for the scalar, contiguous and transposed kernels.
//
// Everything above is Goose. The parent C++ keeps raw tensor-field reads, the
// plan arena and the launch adapter. Device-side copies and the transposed body
// live in device.goose. Transport branches (single memcpy and 2D memcpy) are
// requested through the plan so the parent issues exactly the native SDK call.
//
// Caller-owned bounds: this module proves the shape/stride contract and the
// representable byte spans. Allocation sizes belong to the graph allocator.
extern "fg177h_arg" fn arg(packet:i64,k:i64)->i64;
extern "fg177h_plan" fn plan_ptr(packet:i64)->i64;
extern "fg177h_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg177h_ti" fn ti(t:i64,k:i64,i:i64)->i64;
extern "fg177h_launch" fn launch(packet:i64)->i64;
extern "fg177h_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}
fn U()->u64{return 18446744073709551615 as u64;}
fn add(a:u64,b:u64)->u64{ensure(a<=U()-b,1);return a+b;}
fn mul(a:u64,b:u64)->u64{ensure(a==0||b<=U()/a,2);return a*b;}
fn itg(a:i64)->u64{ensure(a>=0,3);return a as u64;}
fn cld(a:u64,b:u64)->u64{ensure(b!=0,4);return a/b+if a%b!=0{1 as u64}else{0 as u64};}
fn nev(t:i64,i:i64)->i64{return ti(t,3,i);}
fn nbv(t:i64,i:i64)->i64{return ti(t,4,i);}
fn esz(t:i64)->i64{return ti(t,5,0);}

// ggml_cuda_cpy_as_memcpy_2d, reproduced exactly. Returns 0 when it does not
// apply, else 1 and writes width/height/pitches into plan slots 5/6/7/8.
fn memcpy2d(p:i64,s0:i64,s1:i64)->i64{
 // require matching type and shape
 if ti(s0,2,0)!=ti(s1,2,0){return 0;}
 var k:i64=0;
 while k<4{if nev(s0,k)!=nev(s1,k){return 0;}k+=1;}
 let e0:u64=itg(esz(s0));
 var blk:u64=e0;
 var d:i64=0;
 while d<4{
  if itg(nbv(s0,d))!=blk||itg(nbv(s1,d))!=blk{break;}
  blk=mul(blk,itg(nev(s0,d)));
  d+=1;
 }
 // d == 0: nothing contiguous; d == 4: fully contiguous (parent memcpy handles it)
 if d==0||d==4{return 0;}
 var u:i64=d+1;
 while u<4{if nev(s0,u)!=1{return 0;}u+=1;}
 let width:u64=blk;
 let height:u64=itg(nev(s0,d));
 let spitch:u64=itg(nbv(s0,d));
 let dpitch:u64=itg(nbv(s1,d));
 if !(spitch>=width&&dpitch>=width){return 0;}
 put(p,5,0,width);put(p,5,1,height);put(p,5,2,spitch);put(p,5,3,dpitch);
 put(p,5,4,d as u64);
 return 1;
}

fn run(packet:i64)->bool{
 // ggml_cuda_cpy receives the two tensors as arguments, not through the graph,
 // so the packet carries them directly: arg(0) is src1 (the destination) and
 // arg(1) is src0 (the source), matching the native parameter order.
 let s1=arg(packet,0);
 let s0=arg(packet,1);
 ensure(s0!=0&&s1!=0,10);
 let ne0=ti(s0,7,0);
 let ne1=ti(s1,7,0);
 if ne0!=ne1{return false;}
 let p=plan_ptr(packet);
 // Same type and both contiguous: the native branch is a single device memcpy.
 if ti(s0,2,0)==ti(s1,2,0)&&ti(s0,8,0)==1&&ti(s1,8,0)==1{
  put(p,0,0,0 as u64);
  // k=9 is a packet-level query answered by the bridge, so it takes `packet`,
  // not a tensor address.
  let nb=itg(ti(packet,9,0));
  put(p,0,1,nb);
  ensure(launch(packet)==0,20);
  return true;
 }
 if memcpy2d(p,s0,s1)==1{
  put(p,0,0,1 as u64);
  ensure(launch(packet)==0,21);
  return true;
 }
 // Element path. The Apex profile is F32->F32 on both sides.
 let e0=esz(s0);let e1=esz(s1);
 if e0!=4||e1!=4{return false;}
 let nb01=nbv(s0,1);let nb02=nbv(s0,2);let nb03=nbv(s0,3);
 let can_tr:bool=nb01==e0&&nev(s0,3)==1&&nb02==nev(s0,0)*nev(s0,1)*e0;
 let cont:bool=ti(s0,8,0)==1&&ti(s1,8,0)==1;
 let bs:u64=64 as u64;
 let g=cld(itg(ne0),bs);
 ensure(g<=2147483647 as u64,22);
 put(p,0,0,2 as u64);
 put(p,0,1,g);put(p,0,2,bs);
 put(p,1,0,itg(ne0));
 put(p,1,1,itg(nev(s0,0)));put(p,1,2,itg(nev(s0,1)));put(p,1,3,itg(nev(s0,2)));
 put(p,1,4,nbv(s0,0) as u64);put(p,1,5,nbv(s0,1) as u64);
 put(p,1,6,nbv(s0,2) as u64);put(p,1,7,nbv(s0,3) as u64);
 put(p,2,0,itg(nev(s1,0)));put(p,2,1,itg(nev(s1,1)));put(p,2,2,itg(nev(s1,2)));
 put(p,2,3,nbv(s1,0) as u64);put(p,2,4,nbv(s1,1) as u64);
 put(p,2,5,nbv(s1,2) as u64);put(p,2,6,nbv(s1,3) as u64);
 // Route 3 is the contiguous fast path, 2 the general scalar walk, 4 the
 // transposed tile kernel selected by can_be_transposed on equal-type copies.
 let route:u64=if can_tr&&ti(s0,2,0)==ti(s1,2,0){4 as u64}else{if cont&&ti(s0,2,0)==ti(s1,2,0){3 as u64}else{2 as u64}};
 put(p,3,0,route);
 ensure(launch(packet)==0,23);
 return true;
}

fn main(){run(0);}
