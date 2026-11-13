// pad_f32 index mapping in Goose (APX: P3-03 PAD).
//
// Native (pad.cu:13-84): for each destination element, either copy from src or write zero.
//
//     inside = (i0 >= lp0 && i0 < ne0-rp0) && ... for all four dims
//     !circular: inside -> src_idx = i03*s03 + i02*s02 + i01*s01 + i00*s00, with i0k = ik - lpk
//                otherwise dst = 0
//     circular : always copy, with i0k = wrap_around(ik - lpk, nek - lpk - rpk)
//
//     wrap_around(coord, size) = (coord + size) % size
//
// The trap is `wrap_around` on negatives. In C, `%` on a negative operand yields a negative result, so
// `coord % size` alone would index BEFORE the array for every wrapped position on the low side. The `+ size`
// fixes the common case, but it only works because `|coord| < size` here: one `+ size` cannot rescue
// `coord <= -size`. That bound holds for this kernel because coord is `ik - lpk` with `0 <= ik < nek` and
// `lpk < nek`, so `coord > -nek`. Porting it as a general modulo would be wrong, and porting it without the
// `+ size` would produce negative indices that read garbage rather than faulting.
//
// Returns the source index, or -1 when the destination element must be zero.

extern "fg246p_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// C's % TRUNCATES toward zero, so a negative dividend keeps its sign. Goose's % is Euclidean and always
// returns a non-negative result. Transcribing the native line directly therefore diverges for every
// wrapped position whose (coord + size) is negative -- 30 of 10234 checks in the first gate run, with the
// module returning +5 where C returns -5. Emulate the C rule explicitly.
fn cmod(a:i64,m:i64)->i64{
 if a<0 as i64{return -((0 as i64-a)%m);}
 a%m
}
fn wrap_around(coord:i64,size:i64)->i64{cmod(coord+size,size)}

fn map(i0:i64,i1:i64,i2:i64,i3:i64,s00:i64,s01:i64,s02:i64,s03:i64,
       lp0:i64,rp0:i64,lp1:i64,rp1:i64,lp2:i64,rp2:i64,lp3:i64,rp3:i64,
       ne0:i64,ne1:i64,ne2:i64,ne3:i64,circular:i64)->i64{
 ensure(circular==0 as i64||circular==1 as i64,2700);
 ensure(s00>0 as i64&&s01>0 as i64&&s02>0 as i64&&s03>0 as i64,2701);
 if circular==0 as i64{
  var inside:bool=true;
  if !(i0>=lp0&&i0<ne0-rp0){inside=false;}
  if !(i1>=lp1&&i1<ne1-rp1){inside=false;}
  if !(i2>=lp2&&i2<ne2-rp2){inside=false;}
  if !(i3>=lp3&&i3<ne3-rp3){inside=false;}
  if !inside{return 0 as i64-1 as i64;}
  let i00:i64=i0-lp0;
  let i01:i64=i1-lp1;
  let i02:i64=i2-lp2;
  let i03:i64=i3-lp3;
  return i03*s03+i02*s02+i01*s01+i00*s00;
 }
 // circular: on a torus, every element is copied
 let n00:i64=ne0-lp0-rp0;
 let n01:i64=ne1-lp1-rp1;
 let n02:i64=ne2-lp2-rp2;
 let n03:i64=ne3-lp3-rp3;
 ensure(n00>0 as i64&&n01>0 as i64&&n02>0 as i64&&n03>0 as i64,2702);
 let i00:i64=wrap_around(i0-lp0,n00);
 let i01:i64=wrap_around(i1-lp1,n01);
 let i02:i64=wrap_around(i2-lp2,n02);
 let i03:i64=wrap_around(i3-lp3,n03);
 i03*s03+i02*s02+i01*s01+i00*s00
}

fn main(){
 map(0 as i64,0 as i64,0 as i64,0 as i64,1 as i64,1 as i64,1 as i64,1 as i64,
     1 as i64,1 as i64,1 as i64,1 as i64,1 as i64,1 as i64,1 as i64,1 as i64,
     8 as i64,8 as i64,1 as i64,1 as i64,0 as i64);
 map(3 as i64,2 as i64,0 as i64,0 as i64,2 as i64,16 as i64,128 as i64,1024 as i64,
     1 as i64,1 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,
     8 as i64,4 as i64,1 as i64,1 as i64,1 as i64);
}
