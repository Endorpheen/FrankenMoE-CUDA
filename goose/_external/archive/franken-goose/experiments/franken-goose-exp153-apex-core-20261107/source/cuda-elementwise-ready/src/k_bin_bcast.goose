// Apex CUDA binary elementwise broadcast. Mirrors binbcast.cu:k_bin_bcast.
// op: 0 ADD, 1 SUB, 2 MUL, 3 DIV. Collapse and launch geometry are chosen by
// the host launcher exactly as launch_bin_bcast_pack does; this kernel keeps
// the source per-element loop order and overflow guards.
extern "apex_tid_x"  fn tid_x()->i64;
extern "apex_tid_y"  fn tid_y()->i64;
extern "apex_tid_z"  fn tid_z()->i64;
extern "apex_bid_x"  fn bid_x()->i64;
extern "apex_bid_y"  fn bid_y()->i64;
extern "apex_bid_z"  fn bid_z()->i64;
extern "apex_bdim_x" fn bdim_x()->i64;
extern "apex_bdim_y" fn bdim_y()->i64;
extern "apex_bdim_z" fn bdim_z()->i64;
extern "apex_gdim_x" fn gdim_x()->i64;

fn bin_eval(op:i64,a:f32,b:f32)->f32 {
  if op==0 {return a+b;}
  if op==1 {return a-b;}
  if op==2 {return a*b;}
  return a/b;
}

fn bin_bcast_f32(op:i64, s0:f32[:], s1:f32[:], dst:f32[:],
                 ne0:i64, ne1:i64, ne2:i64, ne3:i64,
                 ne10:i64, ne11:i64, ne12:i64, ne13:i64,
                 d1:i64, d2:i64, d3:i64,
                 s00:i64, s01:i64, s02:i64, s03:i64,
                 s10:i64, s11:i64, s12:i64, s13:i64) {
  let i0s=bdim_x()*bid_x()+tid_x();
  let i1=bdim_y()*bid_y()+tid_y();
  let iz=bdim_z()*bid_z()+tid_z();
  let i2=iz/ne3;
  let i3=iz-i2*ne3;
  if i0s>=ne0||i1>=ne1||i2>=ne2||i3>=ne3 {return;}
  let i11=i1%ne11;
  let i12=i2%ne12;
  let i13=i3%ne13;
  let is0=i3*s03+i2*s02+i1*s01;
  let is1=i13*s13+i12*s12+i11*s11;
  let idst=i3*d3+i2*d2+i1*d1;
  let stride=bdim_x()*gdim_x();
  var i0=i0s;
  while i0<ne0 {
    let i10=i0%ne10;
    dst[idst+i0]=bin_eval(op,s0[is0+i0*s00],s1[is1+i10*s10]);
    if ne0-i0<=stride {break;}
    i0+=stride;
  }
}

fn main() {
  var a:f32[256]=default<f32[256]>();
  var b:f32[256]=default<f32[256]>();
  var d:f32[256]=default<f32[256]>();
  bin_bcast_f32(0,a,b,d,256,1,1,1,256,1,1,1,0,0,0,1,256,256,256,1,256,256,256);
  print(d[0]);
}
