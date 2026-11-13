// EXP170s SSM convolution host policy:native shape/type/stride validation and exact
// serial-vs-long launch selection. Replicates every ggml_cuda_op_ssm_conv / ssm_conv_f32_cuda
// assert and adds the checks the native host omits but its kernels require (out nb0==4,
// weight rows,window coverage,launch caps,i32 offset domain). tail_mode arg5:0/absent=bounded
// corrected tail load (default),1=native memory-trace (needs padded storage).
extern "fg170s_arg" fn arg(packet:i64,k:i64)->i64;
extern "fg170s_plan_ptr" fn plan_ptr(packet:i64)->i64;
extern "fg170s_hget" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg170s_hput" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg170s_ti" fn ti(t:i64,k:i64,i:i64)->i64;
extern "fg170s_contiguous" fn contiguous(t:i64)->bool;
extern "fg170s_f32_type" fn f32_type()->i64;
extern "fg170s_launch" fn launch(packet:i64);
extern "fg170s_fail" fn fail(code:i64);
fn ensure(v:bool,c:i64){if !v{fail(c);}}
fn U()->u64{return 18446744073709551615 as u64;}
fn mul(a:u64,b:u64)->u64{ensure(a==0||b<=U()/a,1);return a*b;}
fn add(a:u64,b:u64)->u64{ensure(a<=U()-b,1);return a+b;}
fn ceil(a:u64,b:u64)->u64{ensure(b!=0,2);return a/b+if a%b!=0{1 as u64}else{0 as u64};}
fn copy_tensor(p:i64,t:i64,which:i64){ensure(t!=0,5);ensure(ti(t,2,0)==f32_type(),5);ensure(ti(t,1,0)!=0,5);ensure(ti(t,4,0)==4,5);put(p,0,which,ti(t,1,0) as u64);var i:i64=0;while i<4{let n=ti(t,3,i);let stride=ti(t,4,i);ensure(n>0,6);ensure(n<=2147483647,6);ensure(stride>=0,6);ensure(stride<=2147483647,6);ensure((stride as u64)%4==0,6);put(p,1,which*4+i,n as u64);put(p,2,which*4+i,(stride as u64)/4);i+=1;}ti(t,9,0);}
fn nelem(p:i64,which:i64)->u64{return mul(mul(get(p,1,which*4),get(p,1,which*4+1)),mul(get(p,1,which*4+2),get(p,1,which*4+3)));}
fn run(packet:i64){
 let p=plan_ptr(packet);
 let dst=arg(packet,2);
 let bias_node=arg(packet,3);
 let silu_dst=arg(packet,4);
 let mode=arg(packet,0);
 ensure(arg(packet,1)!=0,3);
 ensure(dst!=0,3);
 let fuse_silu=silu_dst!=0;
 let fuse_bias=bias_node!=0;
 if mode==1{ensure(fuse_silu,3);}else{ensure(mode==0,3);ensure(!fuse_silu,3);}
 if fuse_bias{ensure(fuse_silu,7);}
 let src0=ti(dst,0,0);
 let src1=ti(dst,0,1);
 ensure(src0!=0,3);
 ensure(src1!=0,3);
 let out=if fuse_silu{silu_dst}else{dst};
 copy_tensor(p,src0,0);
 copy_tensor(p,src1,1);
 copy_tensor(p,out,3);
 let nc=get(p,1,4);
 var ok=false;
 if nc==3{ok=true;}
 if nc==4{ok=true;}
 if nc==5{ok=true;}
 if nc==7{ok=true;}
 if nc==9{ok=true;}
 if nc==15{ok=true;}
 ensure(ok,9);
 let nr=get(p,1,1);
 let n_t=get(p,1,13);
 let n_s=get(p,1,14);
 ensure(get(p,1,12)==nr,4);
 ensure(get(p,2,12)==1,14);
 ensure(get(p,2,13)>=nr,14);
 ensure(get(p,2,1)==get(p,1,0),10);
 ensure(get(p,1,0)>=add(n_t,nc-1),12);
 ensure(get(p,1,2)>=n_s,12);
 ensure(get(p,1,5)==nr,13);
 ensure(get(p,2,5)>=nc,13);
 ensure(mul(get(p,2,1),128)<=2147483647,16);
 ensure(add(mul(get(p,2,1),128),add(n_t,nc))<=2147483647,16);
 ensure(add(mul(get(p,2,5),128),nc)<=2147483647,16);
 ensure(mul(n_s-1,get(p,2,2))<=2147483647,17);
 ensure(mul(n_s-1,get(p,2,14))<=2147483647,17);
 ensure(nr%128==0,11);
 ensure(nr/128<=65535,15);
 let is_long=n_t>32;
 if is_long{ensure(ceil(n_t,32)<=65535,15);}
 if fuse_bias{
  let bias=if ti(bias_node,0,0)==dst{ti(bias_node,0,1)}else{ti(bias_node,0,0)};
  copy_tensor(p,bias,2);
  ensure(contiguous(bias),8);
  ensure(nelem(p,2)==nr,8);
 }
 let bounded=arg(packet,5)!=1;
 put(p,3,0,if fuse_silu{1 as u64}else{0 as u64});
 put(p,3,1,if fuse_bias{1 as u64}else{0 as u64});
 put(p,3,2,nc);
 put(p,3,3,nr);
 put(p,3,4,n_t);
 put(p,3,5,n_s);
 put(p,3,6,if is_long{1 as u64}else{0 as u64});
 put(p,3,7,if bounded{1 as u64}else{0 as u64});
 put(p,3,8,n_s);
 put(p,3,9,nr/128);
 put(p,3,10,if is_long{ceil(n_t,32)}else{0 as u64});
 put(p,3,11,if is_long{mul(128 as u64,(nc+31)*4)}else{0 as u64});
 put(p,3,12,128 as u64);
 put(p,3,13,32 as u64);
 put(p,3,14,get(p,1,0));
 put(p,3,15,0 as u64);
 launch(packet);
}
fn main(){run(0);}