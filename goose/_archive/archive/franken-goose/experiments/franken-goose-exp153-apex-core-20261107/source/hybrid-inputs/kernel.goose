extern "fg153_hi_get" fn hi_get(p:i64,k:i64,i:i64)->i64;
extern "fg153_hi_put" fn hi_put(p:i64,k:i64,v:i64);
extern "fg153_hi_ne" fn hi_ne(t:i64,axis:i64)->i64;
extern "fg153_hi_call" fn hi_call(op:i64,p:i64,a:i64,b:i64,c:i64,d:i64)->i64;
extern "fg153_hi_witness" fn hi_witness(mode:i64);
fn hi_ensure(ok:bool){if !ok {let failed=hi_call(99,0,0,0,0,0);assert(false);}}
fn hi_streams(p:i64)->i64 {if hi_get(p,3,0)!=0{return 1;}let n=hi_get(p,4,0);hi_ensure(n>0);return n;}
fn hi_build(p:i64)->i64 {
 let rs=hi_call(0,p,0,0,0,0);let count=hi_get(p,0,0);hi_ensure(count>0);
 let copy=hi_call(1,p,26,count,0,0);hi_put(p,0,copy);let marked=hi_call(2,p,copy,0,0,0);
 let main=hi_call(3,p,copy,hi_get(p,1,0),0,0);hi_put(p,1,main);
 let tail=hi_call(3,p,copy,count-1,hi_call(4,p,copy,0,0,0),0);hi_put(p,2,tail);
 hi_put(p,3,hi_get(p,5,0));hi_put(p,4,hi_get(p,6,0));
 let attn=hi_call(5,p,0,0,0,0);hi_ensure(hi_get(p,7,0)==0);
 hi_put(p,5,hi_call(6,p,0,0,0,0));hi_put(p,6,hi_call(6,p,1,0,0,0));
 let streams=hi_streams(p);let tokens=hi_get(p,2,0);var kind:i64=0;if hi_get(p,8,0)!=0 {kind=1;}
 let mask=hi_call(7,p,kind,hi_get(p,9,0),tokens/streams,streams);hi_put(p,7,mask);hi_put(p,8,mask);
 hi_put(p,9,hi_call(6,p,2,0,0,0));hi_put(p,10,hi_call(6,p,3,0,0,0));return hi_call(8,p,0,0,0,0);
}
fn hi_set(p:i64)->i64 {
 let k=hi_call(10,p,0,0,0,0);let v=hi_call(10,p,1,0,0,0);let mask=hi_call(10,p,2,0,0,0);
 if hi_get(p,10,0)!=0 {let rot=hi_call(10,p,3,0,0,0);}
 if hi_get(p,11,0)!=0 {let rot=hi_call(10,p,4,0,0,0);}
 let count=hi_get(p,0,0);if hi_get(p,12,0)!=0 {
  let host=hi_call(11,p,0,0,0,0);hi_ensure(host!=0);
  hi_ensure(count>=0&&count<=hi_ne(hi_get(p,12,0),0));
  var i:i64=0;while i<count {let stored=hi_call(12,p,i,hi_call(13,p,i,0,0,0),0,0);i+=1;}
 }
 return 0;
}
fn hi_reuse(p:i64)->i64 {
 let changed=hi_call(14,p,0,0,0,0);var ok:i64=1;
 if hi_ne(hi_get(p,13,0),0)!=hi_get(p,2,0) {ok=0;}
 let mask=hi_get(p,14,0);let streams=hi_streams(p);
 if hi_ne(mask,0)!=hi_get(p,9,0) {ok=0;}
 if hi_ne(mask,1)!=hi_get(p,2,0)/streams {ok=0;}
 if hi_ne(mask,2)!=1 {ok=0;}
 if hi_ne(mask,3)!=streams {ok=0;}
 if hi_ne(hi_get(p,12,0),0)!=hi_get(p,0,0) {ok=0;}
 if hi_ne(hi_get(p,15,0),0)!=hi_get(p,1,0) {ok=0;}
 if hi_get(p,16,0)!=hi_get(p,5,0) {ok=0;}
 if hi_get(p,17,0)!=hi_get(p,6,0) {ok=0;}
 return ok;
}
fn hi_entry(mode:i64,p:i64)->i64 {
 hi_witness(mode);if mode==0{return hi_build(p);}if mode==1{return hi_set(p);}if mode==2{return hi_reuse(p);}if mode==3{return 1;}hi_ensure(false);return 0;
}
fn main(){print(hi_entry(3,0));}
