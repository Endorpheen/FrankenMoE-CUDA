// roll_f32_cuda index mapping in Goose (APX: P3-03 ROLL).
//
// Native (roll.cu:5-45): each destination element reads from a shifted source element.
//
//     i0 = idx % ne00;  i1 = (idx / ne00) % ne01;  i2 = (idx / (ne00*ne01)) % ne02;  ...
//     dk = wrap_index(ik - sk, nek)
//     wrap_index(v, ne) = v < 0 ? v + ne : (v >= ne ? v - ne : v)
//
// Two things worth stating rather than assuming:
//
//  1. The `%` and `/` above are on NON-NEGATIVE operands only -- `idx` is a thread index and the divisors are
//     extents -- so Goose's Euclidean modulo and C's truncating one agree here. This is exactly the case the
//     MODULO_SEMANTICS sweep is meant to certify, and it is checked, not hoped for.
//  2. `wrap_index` corrects by a SINGLE extent. It is not a general modulo: it is only correct while
//     `|sk| < nek`. The kernel relies on that, and a general `%` would hide a broken precondition by silently
//     accepting any shift. The guard below asserts the bound so a caller that violates it fails loudly.

extern "fg247r_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn wrap_index(v:i64,ne:i64)->i64{
 if v<0 as i64{return v+ne;}
 if v>=ne{return v-ne;}
 v
}

// map returns the source linear index for destination index `idx`.
fn map(idx:i64,ne00:i64,ne01:i64,ne02:i64,ne03:i64,s0:i64,s1:i64,s2:i64,s3:i64)->i64{
 ensure(idx>=0 as i64,2800);
 ensure(ne00>0 as i64&&ne01>0 as i64&&ne02>0 as i64&&ne03>0 as i64,2801);
 // single-extent correction requires the shift to be smaller than the extent
 ensure(s0>-ne00&&s0<ne00,2802);
 ensure(s1>-ne01&&s1<ne01,2803);
 ensure(s2>-ne02&&s2<ne02,2804);
 ensure(s3>-ne03&&s3<ne03,2805);
 let n_elements:i64=ne00*ne01*ne02*ne03;
 ensure(idx<n_elements,2806);
 let i0:i64=idx%ne00;
 let i1:i64=(idx/ne00)%ne01;
 let i2:i64=(idx/(ne00*ne01))%ne02;
 let i3:i64=(idx/(ne00*ne01*ne02))%ne03;
 let d0:i64=wrap_index(i0-s0,ne00);
 let d1:i64=wrap_index(i1-s1,ne01);
 let d2:i64=wrap_index(i2-s2,ne02);
 let d3:i64=wrap_index(i3-s3,ne03);
 d3*ne00*ne01*ne02+d2*ne00*ne01+d1*ne00+d0
}

fn main(){
 map(0 as i64,1 as i64,1 as i64,1 as i64,1 as i64,0 as i64,0 as i64,0 as i64,0 as i64);
 map(5 as i64,4 as i64,3 as i64,2 as i64,1 as i64,1 as i64,2 as i64,0 as i64,0 as i64);
 map(7 as i64,4 as i64,3 as i64,1 as i64,1 as i64,3 as i64,1 as i64,0 as i64,0 as i64);
}
