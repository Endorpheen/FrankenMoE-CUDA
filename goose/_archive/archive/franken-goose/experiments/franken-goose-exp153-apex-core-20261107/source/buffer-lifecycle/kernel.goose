extern "fg153_bl_get" fn bl_get(p:i64,k:i64,i:i64)->i64;
extern "fg153_bl_put" fn bl_put(p:i64,k:i64,i:i64,v:i64);
extern "fg153_bl_call" fn bl_call(op:i64,a:i64,b:i64,c:i64)->i64;
extern "fg153_bl_witness" fn bl_witness(mode:i64);
fn bl_ensure(ok:bool){if !ok {let failed=bl_call(99,0,0,0);assert(false);}}
fn bl_add(a:i64,b:i64)->i64{bl_ensure(a>=0&&b>=0&&a<=9223372036854775807-b);return a+b;}
fn bl_multi_create(array:i64,count:i64)->i64{
 bl_ensure(array!=0&&count>0&&count<=9223372036854775807/8);
 let ctx=bl_call(0,bl_get(0,20,0),0,0);bl_ensure(ctx!=0);
 let storage=bl_call(0,count*8,0,0);if storage==0{let freed=bl_call(1,ctx,0,0);bl_ensure(false);return 0;}
 bl_put(ctx,0,0,storage);bl_put(ctx,1,0,count);var size:i64=0;
 for i in count {let child=bl_get(array,0,i);bl_ensure(child!=0);bl_put(ctx,2,i,child);size=bl_add(size,bl_get(child,4,0));}
 return bl_call(2,bl_get(array,0,0),ctx,size);
}
fn bl_multi(mode:i64,buffer:i64,value:i64)->i64{
 bl_ensure(buffer!=0);let ctx=bl_get(buffer,1,0);bl_ensure(ctx!=0);let count=bl_get(ctx,2,0);
 if mode==3 {bl_ensure(bl_call(9,buffer,0,0)!=0);}
 for i in count {
  let child=bl_get(ctx,3,i);
  if mode==1 {let freed=bl_call(3,child,0,0);}
  if mode==2 {let cleared=bl_call(4,child,value,0);}
  if mode==3 {let used=bl_call(5,child,value,0);}
 }
 if mode==1 {let freed=bl_call(1,bl_get(ctx,8,0),0,0);let freedctx=bl_call(1,ctx,0,0);}
 return 0;
}
fn bl_entry(mode:i64,a:i64,b:i64)->i64{
 bl_witness(mode);
 if mode==0{return bl_multi_create(a,b);}if mode>=1&&mode<=3{return bl_multi(mode,a,b);}
 if mode==4 {if a!=0 {if bl_get(a,5,0)!=0 {let released=bl_call(6,a,0,0);}let deleted=bl_call(7,a,0,0);}return 0;}
 if mode==5 {bl_ensure(a!=0);if bl_get(a,4,0)!=0 {let clear=bl_call(8,a,b,0);}return 0;}
 if mode==6 {bl_ensure(a!=0);if bl_get(a,6,0)!=0 {let reset=bl_call(10,a,0,0);}return 0;}
 if mode==7 {
  bl_ensure(a!=0);bl_put(a,3,0,b);
  if bl_call(9,a,0,0)!=0 {let used=bl_call(11,a,b,0);}else {if bl_call(12,a,0,0)!=0 {let meta=bl_call(13,a,b,0);}}
  return 0;
 }
 if mode==8 {
  bl_ensure(a!=0);if bl_call(12,a,0,0)==0&&bl_get(a,4,0)==0{return 0;}if bl_get(a,7,0)==0{return 0;}
  let base=bl_call(14,a,0,0);bl_ensure(base!=0);return base;
 }
 if mode==9{return 1;}bl_ensure(false);return 0;
}
fn main(){print(bl_entry(9,0,0));}
