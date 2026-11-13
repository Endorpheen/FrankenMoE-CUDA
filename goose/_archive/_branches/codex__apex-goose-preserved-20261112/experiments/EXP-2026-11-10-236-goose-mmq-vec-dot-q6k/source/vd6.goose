// vec_dot_q6_K_q8_1_impl_mmq in Goose (APX: P5). Q6_K is 9.7% of Apex kernel time per the census.
//
// Native (vecdotq.cuh:656):
//     sc_packed = get_int_b4(sc, 0);  sc_reg = (int8_t*)&sc_packed;
//     for (i0 = 0; i0 < VDR_Q6_K_Q8_1_MMQ; i0 += 4) {            // VDR = 8 -> i0 in {0,4}
//         int2 sumi_d = {0,0};
//         for (i = i0; i < i0+2; ++i) {
//             sumi_d.x = dp4a(v[2i+0], u[2i+0], sumi_d.x);
//             sumi_d.x = dp4a(v[2i+1], u[2i+1], sumi_d.x);
//             sumi_d.y = dp4a(v[2i+4], u[2i+4], sumi_d.y);
//             sumi_d.y = dp4a(v[2i+5], u[2i+5], sumi_d.y);
//         }
//         sumf_d += d8[i0/4] * (sc_reg[i0/2+0]*sumi_d.x + sc_reg[i0/2+1]*sumi_d.y);
//     }
//     return d6 * sumf_d;
//
// Differences from the Q4_K port that matter:
//   * the scale is a SIGNED int8 loaded from a packed 32-bit word, not a uint8 array -- so the extraction
//     has to sign-extend, and getting that wrong flips the sign of half the scale terms;
//   * there are TWO accumulators per step (x and y) fed from different halves of the tile, and they are
//     combined with different scale bytes. Swapping them produces a plausible number.
//   * no half2 anywhere: d6 and d8 are plain floats, so this module needs no conversion primitive at all.
//
// dp4a is declared as a primitive for the same reason as in EXP235: written out longhand the emission does
// not reach IDP.4A and costs 19% there. The same defect would apply here.

extern "fg236k_fail" fn fail(code:i64);
extern "fg236k_dp4a" fn dp4a(a:i64,b:i64,acc:i64)->i64;
extern "fg236k_ld_i32" fn ld_i32(base:u64,idx:i64)->i64;
extern "fg236k_ld_i8" fn ld_i8(base:u64,idx:i64)->i64;
extern "fg236k_ld_f32" fn ld_f32(base:u64,idx:i64)->f32;
extern "fg236k_to_f32" fn to_f32(v:i64)->f32;

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// vd_q6k: d6 is the q6 scale, d8 points at one float per i0/4 step (two steps).
fn vd_q6k(v:u64,u:u64,sc:u64,d6:f32,d8:u64,n_steps:i64)->f32{
 ensure(n_steps>=0,1600);
 let sc_packed:i64=ld_i32(sc,0 as i64);
 var sumf_d:f32=0.0;
 var i0:i64=0;
 while i0<n_steps*(4 as i64){
  var sx:i64=0;
  var sy:i64=0;
  var i:i64=i0;
  while i<i0+2 as i64{
   sx=dp4a(ld_i32(v,2 as i64*i+0 as i64),ld_i32(u,2 as i64*i+0 as i64),sx);
   sx=dp4a(ld_i32(v,2 as i64*i+1 as i64),ld_i32(u,2 as i64*i+1 as i64),sx);
   sy=dp4a(ld_i32(v,2 as i64*i+4 as i64),ld_i32(u,2 as i64*i+4 as i64),sy);
   sy=dp4a(ld_i32(v,2 as i64*i+5 as i64),ld_i32(u,2 as i64*i+5 as i64),sy);
   i=i+1 as i64;
  }
  // sc_reg[k] is byte k of sc_packed, sign-extended.
  let scx:i64=(sc_packed>>(8 as i64*(i0/(2 as i64)+0 as i64)))&0xFF as i64;
  let scy:i64=(sc_packed>>(8 as i64*(i0/(2 as i64)+1 as i64)))&0xFF as i64;
  let scxs:i64=sext8(scx);
  let scys:i64=sext8(scy);
  let acc:i64=scxs*sx+scys*sy;
  sumf_d=sumf_d+ld_f32(d8,i0/(4 as i64))*to_f32(acc);
  i0=i0+4 as i64;
 }
 d6*sumf_d
}

// sign-extend a byte held in the low 8 bits
fn sext8(b:i64)->i64{
 if b>=0x80 as i64{return b-0x100 as i64;}
 b
}

fn main(){
 vd_q6k(0 as u64,0 as u64,0 as u64,1.0,0 as u64,0 as i64);
 vd_q6k(8 as u64,16 as u64,24 as u64,0.5,32 as u64,2 as i64);
}
