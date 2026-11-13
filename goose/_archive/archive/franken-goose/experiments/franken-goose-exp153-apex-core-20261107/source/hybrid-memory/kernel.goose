extern "fg153_hm_get" fn hm_get(p:i64,k:i64,i:i64)->i64;
extern "fg153_hm_put" fn hm_put(p:i64,k:i64,i:i64,v:i64);
extern "fg153_hm_call" fn hm_call(op:i64,p:i64,a:i64,b:i64,c:i64,d:i64)->i64;
extern "fg153_hm_witness" fn hm_witness(mode:i64);
fn hm_ensure(ok:bool) {if !ok {let failed=hm_call(99,0,0,0,0,0);assert(false);}}
fn hm_fail(status:i64)->bool {return status==2||status==3;}
fn hm_combine(a:i64,b:i64)->i64 {
 if hm_fail(a) {return a;}if hm_fail(b) {return b;}
 if a==0||b==0 {return 0;}return 1;
}
fn hm_batch(p:i64,n:i64,all:i64)->i64 {
 let reset=hm_call(0,p,0,0,0,0);
 while true {
  var tokens:i64=0;
  if all!=0 {tokens=hm_call(1,p,n,0,0,0);}
  else {var tail=hm_get(p,3,0);hm_ensure(tail>=0&&tail<4294967295);if tail>0 {tail+=1;}var sequential:i64=0;if hm_get(p,2,0)!=1 {sequential=1;}tokens=hm_call(2,p,n,sequential,tail,0);}
  if tokens==0 {break;}
  let pushed=hm_call(3,p,0,0,0,0);
 }
 if hm_get(p,0,0)<hm_get(p,1,0) {return hm_call(8,p,0,0,0,0);}
 if hm_call(5,p,0,0,0,0)==0 {return hm_call(8,p,1,0,0,0);}
 if hm_call(6,p,0,0,0,0)==0 {return hm_call(8,p,2,0,0,0);}
 return hm_call(7,p,0,0,0,0);
}
fn hm_entry(mode:i64,p:i64,a:i64,b:i64,c:i64,d:i64)->i64 {
 hm_witness(mode);
 if mode==0 {return hm_batch(p,a,b);}
 if mode==1 {return hm_call(9,p,0,0,0,0);}
 if mode==2 {return hm_call(10,p,a,b,0,0);}
 if mode==3 {return hm_call(11,p,0,0,0,0);}
 if mode==4 {let x=hm_call(12,p,0,a,0,0);let y=hm_call(12,p,1,a,0,0);return 0;}
 if mode==5 {if hm_call(13,p,1,a,b,c)==0 {return 0;}return hm_call(13,p,0,a,b,c);}
 if mode==6 {let x=hm_call(14,p,0,a,b,c);let y=hm_call(14,p,1,a,b,c);return 0;}
 if mode==7 {let x=hm_call(15,p,0,a,0,0);let y=hm_call(15,p,1,a,0,0);return 0;}
 if mode==8 {let x=hm_call(16,p,0,a,b,c);let y=hm_call(16,p,1,a,b,c);return 0;}
 if mode==9 {let x=hm_call(17,p,0,a,b,c);let y=hm_call(17,p,1,a,b,c);return 0;}
 if mode==10 {let x=hm_call(18,p,0,a,0,0);let y=hm_call(18,p,1,a,0,0);return if x>y {x} else {y};}
 if mode==11 {let x=hm_call(19,p,0,a,0,0);let y=hm_call(19,p,1,a,0,0);return if x<y {x} else {y};}
 if mode==12 {return hm_combine(a,b);}
 if mode==13 {return if hm_fail(a) {1} else {0};}
 if mode==14 {
  hm_ensure(hm_get(p,10,0)==0);let x=hm_call(30,p,0,0,0,0);let y=hm_call(30,p,1,0,0,0);
  let next=hm_get(p,11,0)+1;hm_put(p,11,0,next);return if next<hm_get(p,12,0) {1} else {0};
 }
 if mode==15 {hm_ensure(!hm_fail(hm_get(p,10,0)));let x=hm_call(31,p,0,0,0,0);let y=hm_call(31,p,1,0,0,0);return if x!=0&&y!=0 {1} else {0};}
 if mode==16 {return hm_get(p,10,0);}
 if mode==17 {hm_ensure(hm_get(p,10,0)==0);let index=hm_get(p,11,0);hm_ensure(index>=0&&index<hm_get(p,12,0));return hm_call(33,p,index,0,0,0);}
 if mode==18 {let x=hm_call(42,p,0,a,b,0);let y=hm_call(42,p,1,a,b,0);return 0;}
 if mode==19 {
  if hm_call(43,p,0,a,b,0)!=0 {return hm_call(44,p,0,0,0,0);}
  if hm_call(43,p,1,a,b,0)!=0 {let clear=hm_call(45,p,a,0,0,0);return hm_call(44,p,0,0,0,0);}
  return 0;
 }
 if mode==20 {
  for side in 2 {
   let loaded=hm_call(46,p,side,0,0,0);var index:i64=0;
   while index<hm_get(p,20,0) {
    let key=hm_get(p,21,index);let value=hm_get(p,22,index);let old=hm_call(48,p,key,0,0,0);
    hm_ensure(value>=0&&old>=0&&old<=9223372036854775807-value);
    let stored=hm_call(49,p,key,old+value,0,0);index+=1;
   }
  }
  return 0;
 }
 hm_ensure(false);return 0;
}
fn main() {print(hm_entry(12,0,0,1,0,0));}
