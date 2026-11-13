// fg171r Apex RoPE device: Goose re-implementation of the native rope.cu pair
// kernels (rope_norm / rope_neox / rope_multi, forward and backward). One thread
// owns one even element index i0 = 2*bi. Grid/thread indices are supplied by the
// bridge: bi = blockDim.y*blockIdx.y + threadIdx.y, row_dst = blockDim.x*blockIdx.x
// + threadIdx.x, native launch shape block(1,256,1) / grid(nr,ceil(ne00/512),1).
// Dispatch mirrors native order: mode&2 -> NEOX half pairs; else mode&8 ->
// MROPE/IMROPE half pairs with section theta; VISION(24) is rejected upstream and
// returns untouched; otherwise NORMAL adjacent pairs. No metadata dims are baked
// into the kernel; every scalar comes from the fg171r plan.
struct F2{x:f32,y:f32}
extern "fg171r_dget" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg171r_dfl" fn fl(p:i64,i:i64)->f32;
extern "fg171r_f32_load" fn f32load(p:u64,i:u64)->f32;
extern "fg171r_f32_store" fn f32store(p:u64,i:u64,v:f32);
extern "fg171r_f16_load" fn f16load(p:u64,i:u64)->f32;
extern "fg171r_f16_store" fn f16store(p:u64,i:u64,v:f32);
extern "fg171r_i32_load" fn i32load(p:u64,i:u64)->i32;
extern "fg171r_sin" fn sinf(v:f32)->f32;
extern "fg171r_cos" fn cosf(v:f32)->f32;
extern "fg171r_pow" fn powf(a:f32,b:f32)->f32;
extern "fg171r_log" fn logf(v:f32)->f32;
extern "fg171r_lc" fn lc();
extern "fg171r_psync" fn psync();
fn fmaxg(a:f32,b:f32)->f32{return if a>b{a}else{b};}
fn fming(a:f32,b:f32)->f32{return if a<b{a}else{b};}
fn xld(p:u64,h:bool,i:u64)->f32{return if h{f16load(p,i)}else{f32load(p,i)};}
fn xst(p:u64,h:bool,i:u64,v:f32){if h{f16store(p,i,v);}else{f32store(p,i,v);}}
// native rope_yarn_ramp: y=(i0/2-low)/max(0.001f,high-low); 1-min(1,max(0,y))
fn ramp(low:f32,high:f32,iw:i64)->f32{let y=((iw/2) as f32-low)/fmaxg(0.001,high-low);return 1.0-fming(1.0,fmaxg(0.0,y));}
// native rope_yarn<forward>: freq_scale interpolation, extrapolation ramp mix, mscale magnitude
fn yarn(theta_extrap:f32,freq_scale:f32,low:f32,high:f32,iw:i64,ext_factor:f32,attn_factor:f32,forward:bool)->F2{
 var theta=freq_scale*theta_extrap;
 var mscale=attn_factor;
 if ext_factor!=0.0{
  let mix=ramp(low,high,iw)*ext_factor;
  theta=theta*(1.0-mix)+theta_extrap*mix;
  mscale=mscale*(1.0+0.1*logf(1.0/freq_scale));
 }
 let c=cosf(theta)*mscale;
 var s=sinf(theta)*mscale;
 if !forward{s*=-1.0;}
 return F2{c,s};
}
fn rope(p:i64,bi:i64,row_dst:i64){
 let mode=get(p,3,0) as i64;
 let neox=(mode&2)!=0;
 let multi=!neox&&(mode&8)!=0;
 if neox{lc();}
 let ne00=get(p,1,0) as i64;
 let i0=2*bi;
 if i0>=ne00{return;}
 if mode==24{return;}
 let ne01=get(p,1,1) as i64;
 let ne02=get(p,1,2) as i64;
 let prod=ne01*ne02;
 let i3=row_dst/prod;
 let i2=(row_dst-i3*prod)/ne01;
 let i1=row_dst-i3*prod-i2*ne01;
 let n_dims=get(p,3,1) as i64;
 let n_offs=get(p,3,2) as i64;
 let xs1=get(p,2,1) as i64;let xs2=get(p,2,2) as i64;let xs3=get(p,2,3) as i64;
 let ds1=get(p,2,5) as i64;let ds2=get(p,2,6) as i64;let ds3=get(p,2,7) as i64;
 let bx=i1*xs1+i2*xs2+i3*xs3;
 let bd=i1*ds1+i2*ds2+i3*ds3;
 if neox||multi{psync();}
 let xh=get(p,3,11)!=0;
 let dh=get(p,3,12)!=0;
 let xp=get(p,0,0);let dp=get(p,0,1);let pp=get(p,0,2);let fp=get(p,0,3);
 if i0<n_offs||i0>=n_offs+n_dims{
  if get(p,3,8)!=0{return;}
  let v0=xld(xp,xh,(bx+i0) as u64);
  let v1=xld(xp,xh,(bx+i0+1) as u64);
  xst(dp,dh,(bd+i0) as u64,v0);
  xst(dp,dh,(bd+i0+1) as u64,v1);
  return;
 }
 let iw=i0-n_offs;
 let ts=fl(p,0);
 var theta_base:f32=0.0;
 if multi{
  let s0=get(p,3,3) as i64;let s1=get(p,3,4) as i64;let s2=get(p,3,5) as i64;let s3=get(p,3,6) as i64;
  let sect=s0+s1+s2+s3;
  let sector=(iw/2)%sect;
  let ne02u=ne02 as u64;
  let i2u=i2 as u64;
  let pw=powf(ts,iw as f32/2.0);
  if get(p,3,10)!=0{
   if sector%3==1&&sector<3*s1{theta_base=(i32load(pp,i2u+ne02u) as f32)*pw;}
   else if sector%3==2&&sector<3*s2{theta_base=(i32load(pp,i2u+2*ne02u) as f32)*pw;}
   else if sector%3==0&&sector<3*s0{theta_base=(i32load(pp,i2u) as f32)*pw;}
   else{theta_base=(i32load(pp,i2u+3*ne02u) as f32)*pw;}
  }else{
   let sw=s0+s1;
   if sector<s0{theta_base=(i32load(pp,i2u) as f32)*pw;}
   else if sector>=s0&&sector<sw{theta_base=(i32load(pp,i2u+ne02u) as f32)*pw;}
   else if sector>=sw&&sector<sw+s2{theta_base=(i32load(pp,i2u+2*ne02u) as f32)*pw;}
   else if sector>=sw+s2{theta_base=(i32load(pp,i2u+3*ne02u) as f32)*pw;}
  }
 }else{
  theta_base=(i32load(pp,i2 as u64) as f32)*powf(ts,iw as f32/2.0);
 }
 let has_ff=get(p,3,9)!=0;
 let ff:f32=if has_ff{f32load(fp,(iw/2) as u64)}else{1.0};
 let cs=yarn(theta_base/ff,fl(p,1),fl(p,4),fl(p,5),iw,fl(p,2),fl(p,3),get(p,3,7)!=0);
 var x0:f32=0.0;var x1:f32=0.0;var d0:u64=0;var d1:u64=0;
 if neox||multi{
  let c0=i0/2+n_offs/2;
  let c1=c0+n_dims/2;
  x0=xld(xp,xh,(bx+c0) as u64);
  x1=xld(xp,xh,(bx+c1) as u64);
  d0=(bd+c0) as u64;
  d1=(bd+c1) as u64;
 }else{
  x0=xld(xp,xh,(bx+i0) as u64);
  x1=xld(xp,xh,(bx+i0+1) as u64);
  d0=(bd+i0) as u64;
  d1=(bd+i0+1) as u64;
 }
 xst(dp,dh,d0,x0*cs.x-x1*cs.y);
 xst(dp,dh,d1,x0*cs.y+x1*cs.x);
}
// retained entry via dummy call; the parent never executes dummy main
fn main(){rope(0,0,0);}
