// EXP171s Apex softmax host policy: validation, tensor projection (bytes to elements),
// launch selection. Mirrors ggml_cuda_op_soft_max/_back host logic; no tensor arithmetic.
extern "fg171s_arg" fn arg(packet:i64,k:i64)->i64;
extern "fg171s_plan_ptr" fn plan_ptr(packet:i64)->i64;
extern "fg171s_hget" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg171s_hput" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg171s_hfloat" fn put_float(p:i64,i:i64,v:f32);
extern "fg171s_ti" fn ti(t:i64,k:i64,i:i64)->i64;
extern "fg171s_tf" fn tf(t:i64,i:i64)->f32;
extern "fg171s_contiguous" fn contiguous(t:i64)->bool;
extern "fg171s_f32_type" fn f32_type()->i64;
// Host CPU powf: only for the native host-side m0/m1 precompute (bit parity).
extern "fg171s_powf" fn hpowf(b:f32,e:f32)->f32;
extern "fg171s_launch" fn launch(packet:i64);
extern "fg171s_fail" fn fail(code:i64);
fn ensure(v:bool,c:i64){if !v{fail(c);}}
fn U()->u64{return 18446744073709551615 as u64;}
fn mul(a:u64,b:u64)->u64{ensure(a==0||b<=U()/a,1);return a*b;}
fn magic(p:i64,key:i64,d:u64){ensure(d>0&&d<=4294967295,3);var l:u64=0;while l<32&&((1 as u64)<<l)<d{l+=1;}let mp=((1 as u64)<<32)*(((1 as u64)<<l)-d)/d+1;ensure(mp<=4294967295,4);put(p,4,key*3,mp);put(p,4,key*3+1,l);put(p,4,key*3+2,d);}
fn copy_f32(p:i64,t:i64,which:i64){ensure(t!=0&&ti(t,2,0)==f32_type()&&ti(t,1,0)!=0&&ti(t,4,0)==4,5);put(p,0,which,ti(t,1,0) as u64);var i:i64=0;while i<4{let n=ti(t,3,i);let s=ti(t,4,i);ensure(n>0&&n<=2147483647&&s>=0&&(s as u64)%4==0,6);put(p,1,which*4+i,n as u64);put(p,2,which*4+i,(s as u64)/4);i+=1;}ti(t,9,0);}
// Mask element size is 4 (F32) or 2 (F16, GGML_TYPE_F16==1); strides land in mask elements.
fn copy_mask(p:i64,t:i64,which:i64)->u64{ensure(t!=0&&ti(t,1,0)!=0,5);let f16=ti(t,2,0)!=f32_type();ensure(!f16||ti(t,2,0)==1,5);let es=if f16{2 as u64}else{4 as u64};ensure(ti(t,4,0)==(es as i64),5);put(p,0,which,ti(t,1,0) as u64);var i:i64=0;while i<4{let n=ti(t,3,i);let s=ti(t,4,i);ensure(n>0&&n<=2147483647&&s>=0&&(s as u64)%es==0,6);put(p,1,which*4+i,n as u64);put(p,2,which*4+i,(s as u64)/es);i+=1;}ti(t,9,0);return if f16{1 as u64}else{0 as u64};}
fn run(packet:i64){
 let mode=arg(packet,0);
 let p=plan_ptr(packet);
 let node=arg(packet,2);
 ensure(mode>=0&&mode<2&&node!=0&&arg(packet,1)!=0,9);
 ensure(ti(node,2,0)==f32_type(),10);
 put(p,3,0,mode as u64);
 if mode==1{back(p,node);return;}
 let x=ti(node,0,0);
 ensure(x!=0,7);
 copy_f32(p,x,0);
 copy_f32(p,node,1);
 ensure(contiguous(x)&&contiguous(node),16);
 var i:i64=0;
 while i<4{ensure(get(p,1,i)==get(p,1,4+i),12);i+=1;}
 let ncols=get(p,1,0);
 let ne01=get(p,1,1);
 let ne02=get(p,1,2);
 let ne03=get(p,1,3);
 put_float(p,0,tf(node,0));
 let max_bias=tf(node,1);
 put_float(p,1,max_bias);
 let mt=ti(node,0,1);
 var f16:u64=0;
 var ne12:u64=1;
 var ne13:u64=1;
 var nb11:u64=0;
 var nb12:u64=0;
 var nb13:u64=0;
 if mt!=0{
  f16=copy_mask(p,mt,2);
  ensure(get(p,1,8)>=ncols&&get(p,1,9)>=ne01,21);
  ne12=get(p,1,10);
  ne13=get(p,1,11);
  ensure(ne02%ne12==0&&ne03%ne13==0,22);
  nb11=get(p,2,9);
  nb12=get(p,2,10);
  nb13=get(p,2,11);
  put(p,3,9,1);
 }
 put(p,3,8,f16);
 put(p,3,11,ne12);
 put(p,3,12,ne13);
 put(p,3,13,nb11);
 put(p,3,14,nb12);
 put(p,3,15,nb13);
 let st=ti(node,0,2);
 if st!=0{
  copy_f32(p,st,3);
  ensure(contiguous(st)&&get(p,1,12)==ne02,27);
  put(p,3,10,1);
 }
 var nhl:u64=1;
 while nhl*2<=ne02{nhl*=2;}
 put(p,3,16,nhl);
 put_float(p,2,hpowf(2.0,(0.0-max_bias)/(nhl as f32)));
 put_float(p,3,hpowf(2.0,(0.0-max_bias/2.0)/(nhl as f32)));
 ensure(ne02<=65535&&ne03<=65535,23);
 let nrows=mul(mul(ne01,ne02),ne03);
 ensure(nrows<=2147483647,24);
 var nth:u64=32;
 while nth<ncols&&nth<1024{nth*=2;}
 let pad=(ncols+31)/32*32;
 let nbytes=(pad+32)*4;
 var ush:u64=1;
 var shared:u64=nbytes;
 if nbytes>49152{ush=0;shared=128;}
 put(p,3,2,ne01);
 put(p,3,3,ne02);
 put(p,3,4,ne03);
 put(p,3,5,nth);
 put(p,3,6,shared);
 put(p,3,7,ush);
 put(p,3,17,nrows);
 put(p,3,18,if max_bias<=0.0{0 as u64}else{1 as u64});
 magic(p,0,ne12);
 magic(p,1,ne13);
 launch(packet);
}
fn back(p:i64,node:i64){
 let grad=ti(node,0,0);
 let dstf=ti(node,0,1);
 ensure(grad!=0&&dstf!=0,7);
 copy_f32(p,grad,0);
 copy_f32(p,node,1);
 copy_f32(p,dstf,4);
 ensure(contiguous(grad)&&contiguous(dstf)&&contiguous(node),13);
 var i:i64=0;
 while i<4{ensure(get(p,1,i)==get(p,1,4+i)&&get(p,1,i)==get(p,1,16+i),14);i+=1;}
 ensure(tf(node,1)==0.0,25);
 put_float(p,0,tf(node,0));
 let nrows=mul(mul(get(p,1,1),get(p,1,2)),get(p,1,3));
 ensure(nrows<=2147483647,24);
 put(p,3,1,get(p,1,0));
 put(p,3,17,nrows);
 put(p,3,5,32);
 put(p,3,6,0);
 launch(packet);
}
fn main(){run(0);}