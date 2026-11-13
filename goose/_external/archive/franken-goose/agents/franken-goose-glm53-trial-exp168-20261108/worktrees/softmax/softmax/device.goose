// EXP171s Apex softmax: forward and backward numerical path (Goose port of softmax.cu).
// The plan is projected by policy.goose; effects are storage, half conversion, expf/powf
// ISA, shuffle and barrier only. All max/sum/logit/normalize arithmetic is Goose.
extern "fg171s_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg171s_fl" fn fl(p:i64,i:i64)->f32;
extern "fg171s_coeff" fn coeff(p:i64,i:i64,k:i64)->u32;
extern "fg171s_mulhi" fn mulhi(a:u32,b:u32)->u32;
extern "fg171s_load" fn load(p:u64,i:u64)->f32;
extern "fg171s_store" fn store(p:u64,i:u64,v:f32);
extern "fg171s_h2f" fn h2f(p:u64,i:u64)->f32;
extern "fg171s_expf" fn expf(v:f32)->f32;
extern "fg171s_powf" fn powf(b:f32,e:f32)->f32;
extern "fg171s_shuffle" fn shuffle(v:f32,offset:i32)->f32;
extern "fg171s_barrier" fn barrier();
// fmaxg mirrors fmaxf: a NaN operand loses against a numeric one.
fn fmaxg(a:f32,b:f32)->f32{return if a!=a{b}else{if b!=b{a}else{if a>b{a}else{b}}}}
// 2^128 overflows f32, so IEEE round-to-nearest yields -inf: native -INFINITY sentinel.
fn ninf()->f32{return -powf(2.0,128.0);}
fn warp_max(v:f32)->f32{var value=v;var shift:i32=16;while shift>0{value=fmaxg(value,shuffle(value,shift));shift>>=1;}return value;}
fn warp_sum(v:f32)->f32{var value=v;var shift:i32=16;while shift>0{value+=shuffle(value,shift);shift>>=1;}return value;}
// Shared elements 0..31 hold warp partials; the vals cache starts at element 32.
fn block_max(v:f32,shared:u64,tid:i32,bs:i32)->f32{var value=warp_max(v);if bs>32{let lane=tid%32;if lane==0{store(shared,(tid/32) as u64,value);}barrier();value=ninf();if lane<bs/32{value=load(shared,lane as u64);}value=warp_max(value);}return value;}
fn block_sum(v:f32,shared:u64,tid:i32,bs:i32)->f32{var value=warp_sum(v);if bs>32{let lane=tid%32;if lane==0{store(shared,(tid/32) as u64,value);}barrier();value=0.0;if lane<bs/32{value=load(shared,lane as u64);}value=warp_sum(value);}return value;}
fn modulo(p:i64,n:u32,key:i64)->u32{let hi=mulhi(n,coeff(p,key,0));let q=(((hi as u64)+(n as u64))>>(coeff(p,key,1) as u64)) as! u32;return n-q*coeff(p,key,2);}
// Native get_alibi_slope with the max_bias<=0 shortcut hoisted into the host active flag.
fn alibi_slope(h:u64,nhl:u64,m0:f32,m1:f32)->f32{let base=if h<nhl{m0}else{m1};let exph=if h<nhl{h+1}else{2*(h-nhl)+1};return powf(base,(exph as f32));}
// Forward entry. Bridge injects p, tid=threadIdx.x, bx/by/bz=blockIdx.xyz and the dynamic
// shared base (float elements); grid=(ne01,ne02,ne03), block=(bs,1,1), smem bytes iv[6].
fn soft_row(p:i64,tid:i32,bx:u64,by:u64,bz:u64,shared:u64){
 let ncols=get(p,3,1) as i32;
 let bs=get(p,3,5) as i32;
 let ush=get(p,3,7)==1;
 let f16=get(p,3,8)==1;
 let has_mask=get(p,3,9)==1;
 let has_sinks=get(p,3,10)==1;
 let alibi=get(p,3,18)==1;
 let scale=fl(p,0);
 let x=get(p,0,0);
 let d=get(p,0,1);
 let mask=get(p,0,2);
 let sinks=get(p,0,3);
 let ne01=get(p,3,2);
 let ne02=get(p,3,3);
 let rowx=bx+by*ne01+bz*ne01*ne02;
 let base=rowx*(ncols as u64);
 let i12=modulo(p,by as u32,0) as u64;
 let i13=modulo(p,bz as u32,1) as u64;
 var mp:u64=mask;
 var slope:f32=1.0;
 if has_mask{
  mp=mask+bx*get(p,3,13)+i12*get(p,3,14)+i13*get(p,3,15);
  if alibi{slope=alibi_slope(by,get(p,3,16),fl(p,2),fl(p,3));}
 }
 var maxv:f32=ninf();
 if has_sinks{maxv=load(sinks,by);}
 var col0:i32=0;
 while col0<ncols{
  let col=col0+tid;
  if col<ncols{
   let ci=base+(col as u64);
   let xv=load(x,ci)*scale;
   var val:f32=xv;
   if has_mask{
    let mv=if f16{h2f(mp,col as u64)}else{load(mp,col as u64)};
    val=xv+slope*mv;
   }
   if ush{store(shared,32+(col as u64),val);}else{store(d,ci,val);}
   maxv=fmaxg(maxv,val);
  }
  col0+=bs;
 }
 maxv=block_max(maxv,shared,tid,bs);
 var tmp:f32=0.0;
 col0=0;
 while col0<ncols{
  let col=col0+tid;
  if col<ncols{
   let vi=if ush{32+(col as u64)}else{base+(col as u64)};
   let cached=if ush{load(shared,vi)}else{load(d,vi)};
   let ev=expf(cached-maxv);
   if ush{store(shared,vi,ev);}else{store(d,vi,ev);}
   tmp+=ev;
  }
  col0+=bs;
 }
 if bs>32{barrier();}
 tmp=block_sum(tmp,shared,tid,bs);
 if has_sinks{tmp+=expf(load(sinks,by)-maxv);}
 let inv=1.0/tmp;
 col0=0;
 while col0<ncols{
  let col=col0+tid;
  if col<ncols{
   let vi=if ush{32+(col as u64)}else{base+(col as u64)};
   let v=if ush{load(shared,vi)}else{load(d,vi)};
   store(d,base+(col as u64),v*inv);
  }
  col0+=bs;
 }
}
// Backward entry (soft_max_back): grid=(rows,1,1), block=(32,1,1), no shared.
fn back_row(p:i64,tid:i32,row:u64){
 let ncols=get(p,3,1) as i32;
 let scale=fl(p,0);
 let g=get(p,0,0);
 let d=get(p,0,1);
 let dstf=get(p,0,4);
 let base=row*(ncols as u64);
 var dgf:f32=0.0;
 var col=tid;
 while col<ncols{dgf+=load(dstf,base+(col as u64))*load(g,base+(col as u64));col+=32;}
 dgf=warp_sum(dgf);
 col=tid;
 while col<ncols{store(d,base+(col as u64),scale*(load(g,base+(col as u64))-dgf)*load(dstf,base+(col as u64)));col+=32;}
}
fn main(){soft_row(0,0,0 as u64,0 as u64,0 as u64,0 as u64);back_row(0,0,0 as u64);}