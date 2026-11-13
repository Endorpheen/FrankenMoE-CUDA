// Apex CUDA MROPE/IMROPE, F32 input and F16 KV-cache output. Same numerical
// path as rope_multi_f32; only the destination store is narrowed exactly like
// rope.cu:rope_norm<...,float,half> / rope_multi dst_type == F16.
extern "apex_tid_x"  fn tid_x()->i64;
extern "apex_tid_y"  fn tid_y()->i64;
extern "apex_bid_x"  fn bid_x()->i64;
extern "apex_bid_y"  fn bid_y()->i64;
extern "apex_bdim_x" fn bdim_x()->i64;
extern "apex_bdim_y" fn bdim_y()->i64;
extern "apex_powf"   fn pow32(a:f32,b:f32)->f32;
extern "apex_cosf"   fn cos32(x:f32)->f32;
extern "apex_sinf"   fn sin32(x:f32)->f32;
extern "apex_logf"   fn log32(x:f32)->f32;
extern "apex_fmaxf"  fn max32(a:f32,b:f32)->f32;
extern "apex_fminf"  fn min32(a:f32,b:f32)->f32;
extern "apex_f32_to_f16" fn to_half(x:f32)->u16;

fn write16(a:u8[:],i:i64,v:u16) {
  assert(i>=0&&i<=a.len-2);
  a[i]=v as! u8;
  a[i+1]=(v>>8) as! u8;
}
fn read_pos(a:const u8[:],i:i64)->i64 {
  assert(i>=0&&i<=a.len-4);
  let b=i*4;
  let u=(a[b] as u32)|((a[b+1] as u32)<<8)|((a[b+2] as u32)<<16)|((a[b+3] as u32)<<24);
  return (u as! i32) as i64;
}
fn rope_yarn_ramp(low:f32,high:f32,i0:i64)->f32 {
  let y=((i0/2) as f32-low)/max32((0.001 as f32),high-low);
  return (1 as f32)-min32((1 as f32),max32((0 as f32),y));
}
fn yarn(theta_extrap:f32,freq_scale:f32,corr0:f32,corr1:f32,i0:i64,ext_factor:f32,amplitude:f32)->f32[2] {
  let theta_interp=freq_scale*theta_extrap;
  var theta=theta_interp;
  var mscale=amplitude;
  if ext_factor!=(0 as f32) {
    let ramp_mix=rope_yarn_ramp(corr0,corr1,i0)*ext_factor;
    theta=theta_interp*((1 as f32)-ramp_mix)+theta_extrap*ramp_mix;
    mscale=mscale*((1 as f32)+(0.1 as f32)*log32((1 as f32)/freq_scale));
  }
  var out:f32[2]=default<f32[2]>();
  out[0]=cos32(theta)*mscale;
  out[1]=sin32(theta)*mscale;
  return out;
}

fn rope_multi_f32_f16(x:f32[:], dst:u8[:], pos:const u8[:],
                      ne00:i64, ne01:i64, ne02:i64,
                      s01:i64, s02:i64, s03:i64, s1:i64, s2:i64, s3:i64,
                      n_dims:i64, n_offs:i64,
                      freq_scale:f32, ext_factor:f32, attn_factor:f32,
                      corr0:f32, corr1:f32, theta_scale:f32,
                      sec0:i64, sec1:i64, sec2:i64, sec3:i64, is_imrope:i64) {
  let i0=2*(bdim_y()*bid_y()+tid_y());
  if i0>=ne00 {return;}
  let row_dst=bdim_x()*bid_x()+tid_x();
  let i3=row_dst/(ne01*ne02);
  let i2=(row_dst-i3*ne01*ne02)/ne01;
  let i1=row_dst-i3*ne01*ne02-i2*ne01;
  let idst=i0/2+i1*s1+i2*s2+i3*s3;
  let ix=i0/2+i1*s01+i2*s02+i3*s03;
  if i0<n_offs||i0>=n_offs+n_dims {
    write16(dst,(idst+i0/2)*2,to_half(x[ix+i0/2]));
    write16(dst,(idst+i0/2+1)*2,to_half(x[ix+i0/2+1]));
    return;
  }
  let iw=i0-n_offs;
  let sect_dims=sec0+sec1+sec2+sec3;
  let sec_w=sec1+sec0;
  let sector=(iw/2)%sect_dims;
  var theta_base:f32=(0 as f32);
  if is_imrope!=0 {
    if sector%3==1&&sector<3*sec1 {theta_base=(read_pos(pos,i2+ne02) as f32)*pow32(theta_scale,(iw as f32)/(2 as f32));}
    else if sector%3==2&&sector<3*sec2 {theta_base=(read_pos(pos,i2+ne02*2) as f32)*pow32(theta_scale,(iw as f32)/(2 as f32));}
    else if sector%3==0&&sector<3*sec0 {theta_base=(read_pos(pos,i2) as f32)*pow32(theta_scale,(iw as f32)/(2 as f32));}
    else {theta_base=(read_pos(pos,i2+ne02*3) as f32)*pow32(theta_scale,(iw as f32)/(2 as f32));}
  } else {
    if sector<sec0 {theta_base=(read_pos(pos,i2) as f32)*pow32(theta_scale,(iw as f32)/(2 as f32));}
    else if sector>=sec0&&sector<sec_w {theta_base=(read_pos(pos,i2+ne02) as f32)*pow32(theta_scale,(iw as f32)/(2 as f32));}
    else if sector>=sec_w&&sector<sec_w+sec2 {theta_base=(read_pos(pos,i2+ne02*2) as f32)*pow32(theta_scale,(iw as f32)/(2 as f32));}
    else {theta_base=(read_pos(pos,i2+ne02*3) as f32)*pow32(theta_scale,(iw as f32)/(2 as f32));}
  }
  let cs=yarn(theta_base,freq_scale,corr0,corr1,iw,ext_factor,attn_factor);
  let x0=x[ix+n_offs/2];
  let x1=x[ix+n_offs/2+n_dims/2];
  write16(dst,(idst+n_offs/2)*2,to_half(x0*cs[0]-x1*cs[1]));
  write16(dst,(idst+n_offs/2+n_dims/2)*2,to_half(x0*cs[1]+x1*cs[0]));
}

fn main() {
  var x:f32[4096]=default<f32[4096]>();
  var d:u8[8192]=default<u8[8192]>();
  var p:u8[64]=default<u8[64]>();
  rope_multi_f32_f16(x,d,p,128,32,1,128,4096,4096,128,4096,4096,128,0,(1 as f32),(0 as f32),(1 as f32),(0 as f32),(0 as f32),(10000 as f32),16,24,24,24,1);
  print(x[0]+(d[0] as f32));
}
